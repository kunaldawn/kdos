/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   job.c — the programs that do the work, and waiting for them
 *
 * A LONG RUN IS A CHILD WITH A PIPE. `kdos update apply --progress 3` writes
 * one line per event to descriptor 3 (update.c says which), and this file
 * reads those lines as they come and hands each to bus.c, which turns them
 * into a reply, a Progress change or a JobRemoved. The run is finished when
 * the child has exited AND its pipe is at end of file, whichever comes last,
 * so no line written just before the exit is lost.
 *
 * A SHORT RUN IS WAITED FOR. `kdos-bootctl status`, `try` and `mark-good`
 * each read or write one small file on the ESP and exit; run_sync waits for
 * one with the bus unattended for those milliseconds rather than splitting
 * every caller into two halves.
 *
 * Every child execs an argument vector, never a shell, starts with the
 * signals a process starts with, and has /dev/null for its input. Its output
 * is this daemon's stderr, which the supervisor files in syslog.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sysupdated.h"

/* A child starts with an empty mask: this process blocks SIGCHLD and SIGTERM
 * for its signalfd, and a blocked mask survives exec. */
static void child_setup(void)
{
	sigset_t empty;
	sigemptyset(&empty);
	sigprocmask(SIG_SETMASK, &empty, NULL);
	kb_child_reset_signals();
	int null = open("/dev/null", O_RDONLY);
	if (null >= 0) {
		dup2(null, STDIN_FILENO);
		if (null > STDERR_FILENO)
			close(null);
	}
}

Child *child_spawn(const char *const *argv, int progress, int kind)
{
	int p[2] = { -1, -1 };

	if (progress && pipe2(p, O_CLOEXEC) != 0) {
		su_log("pipe: %s", strerror(errno));
		return NULL;
	}
	pid_t pid = fork();
	if (pid < 0) {
		su_log("fork: %s", strerror(errno));
		if (progress) {
			close(p[0]);
			close(p[1]);
		}
		return NULL;
	}
	if (pid == 0) {
		child_setup();
		dup2(STDERR_FILENO, STDOUT_FILENO);
		if (progress) {
			/* Descriptor 3 exactly, open across the exec. */
			if (p[1] == 3)
				fcntl(3, F_SETFD, 0);
			else if (dup2(p[1], 3) < 0)
				_exit(127);
		}
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	Child *c = kb_calloc(1, sizeof(*c));
	c->pid = pid;
	c->kind = kind;
	c->fd = -1;
	if (progress) {
		close(p[1]);
		c->fd = p[0];
		fcntl(c->fd, F_SETFL, O_NONBLOCK);
	}
	c->next = su.children;
	su.children = c;
	return c;
}

void child_poll_fds(struct pollfd *pfd, int *n, int cap)
{
	for (Child *c = su.children; c && *n < cap; c = c->next)
		if (c->fd >= 0) {
			pfd[*n].fd = c->fd;
			pfd[*n].events = POLLIN;
			pfd[*n].revents = 0;
			(*n)++;
		}
}

static void child_finish(Child *c)
{
	Child **pp = &su.children;
	while (*pp && *pp != c)
		pp = &(*pp)->next;
	if (*pp)
		*pp = c->next;
	bus_child_done(c);
	if (c->call)
		sd_bus_message_unref(c->call);
	free(c);
}

/* Lines, as they arrive; a line longer than the buffer is delivered in
 * pieces rather than dropped. */
void child_on_readable(int fd)
{
	Child *c = su.children;
	while (c && c->fd != fd)
		c = c->next;
	if (!c)
		return;
	for (;;) {
		ssize_t r = read(fd, c->line + c->len,
				 sizeof(c->line) - 1 - c->len);
		if (r < 0 && errno == EINTR)
			continue;
		if (r < 0 && errno == EAGAIN)
			return;
		if (r <= 0) {
			if (c->len) {
				c->line[c->len] = 0;
				c->len = 0;
				bus_child_line(c, c->line);
			}
			close(c->fd);
			c->fd = -1;
			if (c->exited)
				child_finish(c);
			return;
		}
		c->len += (size_t)r;
		for (;;) {
			char *nl = memchr(c->line, '\n', c->len);
			if (!nl && c->len < sizeof(c->line) - 1)
				break;
			size_t ln = nl ? (size_t)(nl - c->line) : c->len;
			char one[sizeof(c->line)];
			memcpy(one, c->line, ln);
			one[ln] = 0;
			size_t used = nl ? ln + 1 : ln;
			memmove(c->line, c->line + used, c->len - used);
			c->len -= used;
			bus_child_line(c, one);
		}
	}
}

void child_reap(void)
{
	int st;
	pid_t pid;

	while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
		Child *c = su.children;
		while (c && c->pid != pid)
			c = c->next;
		if (!c)
			continue;
		c->exited = 1;
		c->status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
		if (c->fd < 0)
			child_finish(c);
	}
}

/* Every child is asked to stop, as the daemon itself stops: `kdos update`
 * finishes the package it is on and installs no other. */
void child_cancel(void)
{
	for (Child *c = su.children; c; c = c->next)
		if (!c->exited)
			kill(c->pid, SIGTERM);
}

/*
 * A short run, waited for: stdout into `out`, stderr into `err` (or this
 * daemon's stderr when `err` is NULL). The exit status, 127 when it could not
 * run, -1 when it could not be started.
 */
int run_sync(const char *const *argv, KbBuf *out, KbBuf *err)
{
	int po[2], pe[2] = { -1, -1 };
	KbBuf sink = {0};
	int rc;

	if (!out)
		out = &sink;
	if (pipe2(po, O_CLOEXEC) != 0)
		return -1;
	if (err && pipe2(pe, O_CLOEXEC) != 0) {
		close(po[0]);
		close(po[1]);
		return -1;
	}
	pid_t pid = fork();
	if (pid < 0) {
		close(po[0]);
		close(po[1]);
		if (err) {
			close(pe[0]);
			close(pe[1]);
		}
		return -1;
	}
	if (pid == 0) {
		child_setup();
		dup2(po[1], STDOUT_FILENO);
		if (err)
			dup2(pe[1], STDERR_FILENO);
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(po[1]);
	if (err)
		close(pe[1]);
	struct pollfd pfd[2] = {
		{ .fd = po[0], .events = POLLIN },
		{ .fd = err ? pe[0] : -1, .events = POLLIN },
	};
	int open_fds = err ? 2 : 1;
	char buf[8192];
	while (open_fds > 0) {
		if (poll(pfd, 2, -1) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		for (int i = 0; i < 2; i++) {
			if (pfd[i].fd < 0 || !(pfd[i].revents &
					       (POLLIN | POLLHUP | POLLERR)))
				continue;
			ssize_t r = read(pfd[i].fd, buf, sizeof(buf));
			if (r < 0 && errno == EINTR)
				continue;
			if (r <= 0) {
				close(pfd[i].fd);
				pfd[i].fd = -1;
				open_fds--;
				continue;
			}
			kb_buf_add(i ? err : out, buf, (size_t)r);
		}
	}
	for (int i = 0; i < 2; i++)
		if (pfd[i].fd >= 0)
			close(pfd[i].fd);
	int st;
	rc = -1;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			goto out;
	rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
out:
	kb_buf_str(out, "");
	if (err)
		kb_buf_str(err, "");
	kb_buf_free(&sink);
	return rc;
}

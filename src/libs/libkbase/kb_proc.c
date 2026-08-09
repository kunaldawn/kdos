/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkbase — argv building and process spawning
 *
 * No system(), no popen(), no shell. See the note in kbase.h.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "kbase.h"

#define ARGF_MAX 4096

int kb_proc_verbose;

void kb_argv_add(KbArgv *a, const char *s)
{
	if (a->n >= KB_MAX_ARGV - 1)
		kb_die("argument list too long");
	a->v[a->n++] = s;
}

void kb_argv_addf(KbArgv *a, const char *fmt, ...)
{
	char *buf = kb_calloc(1, ARGF_MAX);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, ARGF_MAX, fmt, ap);
	va_end(ap);
	kb_argv_add(a, buf);
}

void kb_argv_end(KbArgv *a)
{
	a->v[a->n] = NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */

static pid_t spawn(const KbArgv *a, int outfd)
{
	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			if (outfd < 0)
				dup2(null, STDOUT_FILENO);
			if (!kb_proc_verbose)
				dup2(null, STDERR_FILENO);
			if (null > STDERR_FILENO)
				close(null);
		}
		if (outfd >= 0) {
			dup2(outfd, STDOUT_FILENO);
			close(outfd);
		}
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	return pid;
}

static int reap(pid_t pid)
{
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

int kb_run(const KbArgv *a)
{
	return reap(spawn(a, -1));
}

int kb_run_tty(const KbArgv *a)
{
	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	return reap(pid);
}

int kb_run_capture(const KbArgv *a, char *buf, size_t n)
{
	int fd[2];
	size_t got = 0;
	pid_t pid;
	int rc;

	buf[0] = '\0';
	if (pipe(fd) < 0)
		kb_die("pipe: %s", strerror(errno));
	/* The read end must NOT survive into the child, or the child's own copy
	 * keeps the pipe readable forever: when the buffer fills and we close
	 * ours, the child blocks in write() instead of taking EPIPE, and we
	 * block in waitpid(). `kpkg install zig` deadlocked exactly there —
	 * a 1.1 MB `tar -tf` listing against a 1 MB buffer. */
	fcntl(fd[0], F_SETFD, FD_CLOEXEC);
	pid = spawn(a, fd[1]);
	close(fd[1]);
	for (;;) {
		ssize_t r = read(fd[0], buf + got, n - 1 - got);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		got += (size_t)r;
		if (got >= n - 1)
			break;
	}
	buf[got] = '\0';
	close(fd[0]);
	rc = reap(pid);
	while (got && (buf[got - 1] == '\n' || buf[got - 1] == '\r'))
		buf[--got] = '\0';
	return rc;
}

/* Same, into a growing buffer: no ceiling to overflow and nothing truncated.
 * A package manifest is the whole reason — zig's is 1.1 MB and a short one
 * writes a database entry that owns fewer files than the package installed. */
int kb_run_capture_buf(const KbArgv *a, KbBuf *out)
{
	int fd[2];
	pid_t pid;
	char chunk[65536];

	if (pipe(fd) < 0)
		kb_die("pipe: %s", strerror(errno));
	fcntl(fd[0], F_SETFD, FD_CLOEXEC);
	pid = spawn(a, fd[1]);
	close(fd[1]);
	for (;;) {
		ssize_t r = read(fd[0], chunk, sizeof(chunk));
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			break;
		kb_buf_add(out, chunk, (size_t)r);
	}
	close(fd[0]);
	return reap(pid);
}

void kb_run_detach(const KbArgv *a)
{
	pid_t pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		if (fork() == 0) {
			int null = open("/dev/null", O_RDWR);
			if (null >= 0) {
				dup2(null, STDIN_FILENO);
				dup2(null, STDOUT_FILENO);
				dup2(null, STDERR_FILENO);
				if (null > STDERR_FILENO)
					close(null);
			}
			setsid();
			execvp(a->v[0], (char *const *)a->v);
		}
		_exit(0);
	}
	reap(pid);
}

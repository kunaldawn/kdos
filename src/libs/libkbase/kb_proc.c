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
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
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

/*
 * Run with stdout going to a FILE.
 *
 * This is the `>` that would otherwise need a shell — and the reason there is no
 * shell anywhere in these programs is that a redirect built by string
 * concatenation puts every path it touches one quote away from being a command.
 * The file is created here, in the parent, so a failure to create it is reported
 * as itself rather than as a mysterious child exit.
 */
int kb_run_to_file(const KbArgv *a, const char *path)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;
	int rc = reap(spawn(a, fd));
	close(fd);
	return rc;
}

/*
 * Run with `in` on the child's stdin. The one reason this exists: a password
 * must not travel in argv, where /proc/<pid>/cmdline publishes it to every
 * process on the machine for as long as the child lives — kdos-lock pipes one
 * into kdos-checkpass.
 *
 * The write is guarded against SIGPIPE rather than assumed safe: a child that
 * exits without reading (a bad invocation, a missing binary) would otherwise
 * kill the CALLER, which for a lock screen means the lock client dying and the
 * session staying locked forever.
 */
static int feed(const KbArgv *a, const char *in, size_t n, bool keep_stdout)
{
	int fd[2];
	pid_t pid;

	if (pipe(fd) < 0)
		kb_die("pipe: %s", strerror(errno));
	/* The write end must not survive into the child, or its own copy keeps
	 * the pipe open and the child never sees EOF on stdin. */
	fcntl(fd[1], F_SETFD, FD_CLOEXEC);

	pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);
		if (!keep_stdout) {
			int null = open("/dev/null", O_RDWR);
			if (null >= 0) {
				dup2(null, STDOUT_FILENO);
				if (!kb_proc_verbose)
					dup2(null, STDERR_FILENO);
				if (null > STDERR_FILENO)
					close(null);
			}
		}
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	close(fd[0]);

	void (*old)(int) = signal(SIGPIPE, SIG_IGN);
	size_t off = 0;
	while (off < n) {
		ssize_t w = write(fd[1], in + off, n - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			break;		/* the child is gone; reap tells us */
		}
		off += (size_t)w;
	}
	close(fd[1]);
	signal(SIGPIPE, old);
	return reap(pid);
}

int kb_run_feed(const KbArgv *a, const char *in, size_t n)
{
	return feed(a, in, n, false);
}

/* A pager is the case kb_run_feed cannot serve: it has to be fed on stdin AND
 * keep the terminal it draws on. Writing to /dev/null is right for a checker
 * and silent for a pager, which is a failure that looks like an empty
 * document. */
int kb_run_feed_tty(const KbArgv *a, const char *in, size_t n)
{
	return feed(a, in, n, true);
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

/*
 * Is `user` (whose primary group is `primary`) a member of `group`?
 *
 * /etc/group is parsed directly rather than through getgrnam(): musl's NSS is
 * files-only anyway, and the callers are root daemons deciding whether to obey
 * a request — loading a name-service module to answer a question one file read
 * can is more code running as root, not less.
 *
 * The group's OWN gid counts as well as its member list. A user whose primary
 * group IS wheel never appears in that list, and a check that missed it would
 * refuse exactly the accounts an installer creates.
 */
int kb_user_in_group(const char *user, gid_t primary, const char *group)
{
	FILE *f = fopen("/etc/group", "r");
	if (!f)
		return 0;

	int ok = 0;
	char line[4096];
	while (!ok && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *save = NULL;
		char *gname = strtok_r(line, ":", &save);
		if (!gname || strcmp(gname, group))
			continue;
		strtok_r(NULL, ":", &save);		/* the password field */
		char *gid_s = strtok_r(NULL, ":", &save);
		char *members = strtok_r(NULL, ":", &save);

		if (gid_s && (gid_t)strtoul(gid_s, NULL, 10) == primary)
			ok = 1;

		for (char *m = members ? strtok_r(members, ",", &save) : NULL;
		     m && !ok; m = strtok_r(NULL, ",", &save))
			ok = !strcmp(m, user);
	}
	fclose(f);
	return ok;
}

/*
 * Is this machine virtual, by the DMI vendor string?
 *
 * The idle timers and the lid policy both default to OFF here, and both for
 * the same reason: a blanked screen over VNC is indistinguishable from a
 * crashed session, and a VM's lid event is a stray ACPI report rather than
 * somebody closing a laptop.
 *
 * The vendor string is the only test that works from an unprivileged process
 * with no hypervisor-specific code. It is a list of names and it will miss a
 * hypervisor nobody here has run — which fails in the safe direction: the
 * timers stay on, as they would on hardware.
 */
int kb_in_vm(void)
{
	static const char *const V[] = {
		"QEMU", "Bochs", "VirtualBox", "VMware", "innotek",
		"Microsoft Corporation",	/* Hyper-V */
		"Parallels", "Amazon EC2",
		"Google",			/* GCE */
	};
	char vendor[128] = { 0 };
	FILE *f = fopen("/sys/class/dmi/id/sys_vendor", "re");

	if (!f)
		return 0;
	if (!fgets(vendor, sizeof(vendor), f))
		vendor[0] = '\0';
	fclose(f);

	for (size_t i = 0; i < sizeof(V) / sizeof(V[0]); i++)
		if (strstr(vendor, V[i]))
			return 1;
	return 0;
}

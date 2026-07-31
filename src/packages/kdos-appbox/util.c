/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * Process, path and locking helpers.
 *
 * Everything here execs directly — there is no system() and no shell anywhere
 * in this program. That is deliberate: app names, package names and file
 * arguments all reach this code from .desktop files and the command line, and
 * a shell in the middle turns any of them into an injection point.
 */

#include "kdos-appbox.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void die(const char *fmt, ...)
{
	va_list ap;
	fputs("kdos-appbox: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void warn(const char *fmt, ...)
{
	va_list ap;
	fputs("kdos-appbox: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void *xmalloc(size_t n)
{
	void *p = calloc(1, n);
	if (!p)
		die("out of memory");
	return p;
}

char *xstrdup(const char *s)
{
	char *p = strdup(s);
	if (!p)
		die("out of memory");
	return p;
}

void argv_add(Argv *a, const char *s)
{
	if (a->n >= MAX_ARGV - 1)
		die("argument list too long");
	a->v[a->n++] = s;
}

void argv_addf(Argv *a, const char *fmt, ...)
{
	char *buf = xmalloc(MAX_LINE);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, MAX_LINE, fmt, ap);
	va_end(ap);
	argv_add(a, buf);
}

void argv_end(Argv *a)
{
	a->v[a->n] = NULL;
}

static pid_t spawn(const Argv *a, int outfd)
{
	pid_t pid = fork();
	if (pid < 0)
		die("fork: %s", strerror(errno));
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			if (outfd < 0)
				dup2(null, STDOUT_FILENO);
			if (!g_verbose)
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

int run_quiet(const Argv *a)
{
	return reap(spawn(a, -1));
}

int run_capture(const Argv *a, char *buf, size_t n)
{
	int fd[2];
	size_t got = 0;
	pid_t pid;
	int rc;

	buf[0] = '\0';
	if (pipe(fd) < 0)
		die("pipe: %s", strerror(errno));
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

/*
 * Fire and forget, with the double fork that keeps the caller from ever
 * collecting a zombie or — worse — blocking on one. gdbus's default reply
 * timeout is 25 seconds, and a notification must never be able to gate an
 * app launch behind that.
 */
void run_detach(const Argv *a)
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

const char *runtime_dir(void)
{
	const char *d = getenv("XDG_RUNTIME_DIR");
	return (d && *d) ? d : "/tmp";
}

const char *home_dir(void)
{
	const char *d = getenv("HOME");
	return (d && *d) ? d : "/root";
}

char *path_join(const char *a, const char *b)
{
	size_t n = strlen(a) + strlen(b) + 2;
	char *p = xmalloc(n);
	snprintf(p, n, "%s/%s", a, b);
	return p;
}

int mkdir_p(const char *path)
{
	char *tmp = xstrdup(path);
	char *p;
	for (p = tmp + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
			free(tmp);
			return -1;
		}
		*p = '/';
	}
	if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
		free(tmp);
		return -1;
	}
	free(tmp);
	return 0;
}

int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

int read_file(const char *path, char *buf, size_t n)
{
	int fd = open(path, O_RDONLY);
	ssize_t r;
	if (fd < 0)
		return -1;
	r = read(fd, buf, n - 1);
	close(fd);
	if (r < 0)
		return -1;
	buf[r] = '\0';
	return (int)r;
}

int write_file(const char *path, const char *data)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	size_t len = strlen(data);
	ssize_t w;
	if (fd < 0)
		return -1;
	w = write(fd, data, len);
	close(fd);
	return (w == (ssize_t)len) ? 0 : -1;
}

int lock_file(const char *path, int nonblock)
{
	int fd = open(path, O_WRONLY | O_CREAT, 0600);
	if (fd < 0)
		return -1;
	if (flock(fd, LOCK_EX | (nonblock ? LOCK_NB : 0)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/* Stage timings, so "why did that take so long" stays answerable afterwards. */
void tracef(const char *fmt, ...)
{
	char *path = path_join(runtime_dir(), "kdos-appbox.trace");
	FILE *f = fopen(path, "a");
	struct timespec ts;
	va_list ap;

	free(path);
	if (!f)
		return;
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(f, "%ld.%06ld ", (long)ts.tv_sec, ts.tv_nsec / 1000);
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

/*
 * Desktop notification straight over the bus — the host has no libnotify.
 * Only reachable at all because the session bus lives at a fixed path in
 * $XDG_RUNTIME_DIR, which the appbox shares.
 */
void notify(const char *summary, const char *body)
{
	Argv a = {0};
	argv_add(&a, "gdbus");
	argv_add(&a, "call");
	argv_add(&a, "--timeout");
	argv_add(&a, "2");
	argv_add(&a, "--session");
	argv_add(&a, "--dest");
	argv_add(&a, "org.freedesktop.Notifications");
	argv_add(&a, "--object-path");
	argv_add(&a, "/org/freedesktop/Notifications");
	argv_add(&a, "--method");
	argv_add(&a, "org.freedesktop.Notifications.Notify");
	argv_add(&a, "KDOS");
	argv_add(&a, "0");
	argv_add(&a, "distributor-logo-kdos");
	argv_add(&a, summary);
	argv_add(&a, body);
	argv_add(&a, "[]");
	argv_add(&a, "{}");
	argv_add(&a, "8000");
	argv_end(&a);
	run_detach(&a);
}

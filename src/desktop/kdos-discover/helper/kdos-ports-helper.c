/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-ports-helper — one host port into this slot, prebuilt or built
 *   here, or one installed on request out of it
 *
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <port>
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-helper remove <port>
 *   pkexec /usr/libexec/kdos-discover/kdos-ports-builder install \
 *       [--build-deps=prebuilt|source] <port>
 *
 * Discover's KDOS backend runs one of these, through pkexec, when somebody
 * installs host software the running system lacks or removes a port that
 * was installed on request. Each is the whole of the root side of its
 * button, so it takes one verb and one port name, and the name is checked
 * before it means anything: pkexec's grant names a program and its first
 * argument, never the rest, and /usr/bin/kdos under the same grant would
 * reach every subcommand.
 *
 * ONE SOURCE, TWO PROGRAMS, THREE GRANTS. kdos-ports-helper takes the port
 * prebuilt from the signed binhost, under io.kdos.ports.install, and removes
 * one under io.kdos.ports.remove. kdos-ports-builder, this file compiled
 * with KPH_BUILD, builds the port on this computer under io.kdos.ports.build,
 * which asks for the password every time: it runs a recipe's build.sh as
 * root, for minutes or hours, and a kept grant for the first would otherwise
 * cover it. pkexec picks the action by the program's path and its first
 * argument (exec.path, exec.argv1), so the two routes are two programs, and
 * each action of the helper names its verb: an action naming the path alone
 * would match either verb, whichever pkexec read first.
 *
 * THE REMOVAL IS `kdos update remove`'S. It refuses a port that is not in
 * /var/lib/kdos/world (part of the system) and one an installed package
 * depends on, and runs `kpkg remove`, which takes the port off the world set
 * and the origin record too, on the port and on each package it brought that
 * nothing else installed needs, then drops their files from the binhost
 * cache.
 *
 * THE INSTALL IS `kdos update install`'S, never a second copy of it. That
 * command owns everything the route decides: the signed index `kdos update
 * fetch` keeps in /var/lib/kdos/binhost, downloading each missing package and
 * checking it against that index, the build environment of
 * /etc/kdos/chroot.env, fetching and building the sources of a source
 * install, recording each package's route in /var/lib/kdos/origin, and adding
 * the port to /var/lib/kdos/world so the next update keeps it. A copy here
 * would be a second answer to each of those, and the first to drift. The
 * builder passes --build-deps only when asked; without it, `build-deps =` in
 * /etc/kdos/update.conf decides.
 *
 * CANCEL IS STDIN CLOSING. Once pkexec has run this program it is root, and
 * Discover, the user, may not signal it or anything it starts; what Discover
 * can still do is close the write end of the pipe on this program's standard
 * input, which no root process can stop it doing. So the caller holds stdin
 * open for as long as it wants the step to go on, and anything it writes
 * there is read and dropped. `kdos update` runs in a process group of its
 * own, with /dev/null for its input; this program watches stdin and the
 * child together, and end of file or a hang-up on stdin, or SIGTERM, SIGINT
 * or SIGHUP to this program, sends SIGTERM to that whole group, so kpkg, the
 * recipe's build and its compilers stop with it, then SIGKILL to the group
 * if anything is left after 60 seconds. A package whose install was under
 * way is left as far as it got, and the packages before it stay installed;
 * the port joins the world set only once everything installed. A cancelled
 * run says `==> <port>: cancelled`, writes no catalogue and exits 130, and
 * it exits only once the group is gone, so Discover's refresh after it never
 * meets a run still holding the package lock. Run from a terminal, the
 * helper takes Ctrl-D or Ctrl-C as the same cancel; run with stdin on
 * /dev/null it cancels at once.
 *
 * THE CATALOGUE IS WRITTEN AGAIN after an install or a removal, so
 * Discover's next refresh lists the program as it now is. A catalogue that
 * could not be written leaves the port as the step left it and is said, not
 * failed.
 *
 * A FAILED BUILD KEEPS ITS LOG in /var/log/kdos-build/<port>.log, which
 * the builder writes as it runs: every line `kdos update` and the build
 * print goes to Discover on the stream it was written to, and to that file
 * too. Discover shows the build's output nowhere, and a failed build's work
 * directory is removed (KPKG_CLEAN_FAILED), so without the file a desktop
 * user has nothing to learn the cause from; `kdos update` names it at the
 * end of the failure line Discover shows. The file is readable by anyone,
 * replaced by the next build of the port, and removed when the run
 * succeeds.
 *
 * The environment pkexec hands over is replaced by a fixed PATH, the C
 * locale, KDOS_UPDATE_FRONT=discover (so `kdos update` words its next step
 * as the button, not as a command to type) and, for the builder,
 * KDOS_UPDATE_LOG naming the log, before anything runs, so nothing the
 * caller set reaches a root process.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"

#define SAFE_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

/* A kpkg package name: the characters `kdos update` accepts for the world
 * set, never starting with `.` or `-`, so the name cannot be read as an
 * option by anything it is handed to. */
static int kph_name_ok(const char *s)
{
	if (!*s || strlen(s) >= 128 || *s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '+' || *c == '.' ||
		      *c == '_' || *c == '-'))
			return 0;
	return 1;
}

#ifdef KPH_BUILD
#define KPH_NAME "kdos-ports-builder"
#define KPH_USAGE "install [--build-deps=prebuilt|source] <port>"
#else
#define KPH_NAME "kdos-ports-helper"
#define KPH_USAGE "install|remove <port>"
#endif

/* How long the group has after SIGTERM before SIGKILL: long enough for kpkg
 * to finish writing the file it is on, short of leaving a build running
 * minutes after somebody pressed Cancel. */
#define KPH_STOP_MS 60000

#ifdef KPH_BUILD
#define KPH_LOG_DIR "/var/log/kdos-build"
#endif

/* All of `buf` to `fd`, retried across interruptions and short writes; a
 * reader gone away (EPIPE) or any other error ends the copy to that fd. */
static void kph_put(int fd, const char *buf, size_t n)
{
	while (n) {
		ssize_t w = write(fd, buf, n);
		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0)
			return;
		buf += w;
		n -= (size_t)w;
	}
}

/* One read of the child's pipe `*from` copied to `to` and to `log`: 1 when
 * something was copied, 2 when the pipe was empty, 0 with `*from` closed and
 * set to -1 at end of file or an error. */
static int kph_copy(int *from, int to, int log)
{
	char buf[4096];
	ssize_t got = read(*from, buf, sizeof(buf));
	if (got < 0 && (errno == EINTR || errno == EAGAIN))
		return 2;
	if (got <= 0) {
		close(*from);
		*from = -1;
		return 0;
	}
	kph_put(to, buf, (size_t)got);
	if (log >= 0)
		kph_put(log, buf, (size_t)got);
	return 1;
}

/* Milliseconds left until `end` on the monotonic clock, never below 0. */
static int kph_ms_left(const struct timespec *end)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	long ms = (end->tv_sec - now.tv_sec) * 1000L +
		  (end->tv_nsec - now.tv_nsec) / 1000000L;
	return ms < 0 ? 0 : (int)ms;
}

/*
 * `a` in a process group of its own, stopped with its group when stdin
 * closes (CANCEL IS STDIN CLOSING above). The child's exit status, 128 plus
 * the signal that ended it, -1 when it could not be started or watched, or
 * -2 when the run was cancelled before it finished; in every case the child
 * has been reaped. With `log` open, the child writes into two pipes in place
 * of this program's stdout and stderr, and each chunk read from one is
 * copied to the stream it stands for and to `log`, so Discover reads
 * progress and errors where it always does.
 */
static int kph_run(const KbArgv *a, int log)
{
	sigset_t set, old;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);
	sigprocmask(SIG_BLOCK, &set, &old);
	int sfd = signalfd(-1, &set, SFD_CLOEXEC);
	if (sfd < 0) {
		sigprocmask(SIG_SETMASK, &old, NULL);
		fprintf(stderr, KPH_NAME ": signalfd: %s\n", strerror(errno));
		return -1;
	}
	/* A caller gone away closes stdout too; the next line written must
	 * not end this program before its group is stopped. */
	signal(SIGPIPE, SIG_IGN);

	/* out[0], err[0]: the read ends this program copies from; out[1],
	 * err[1]: the write ends the child takes as its stdout and stderr. */
	int out[2] = {-1, -1}, err[2] = {-1, -1};
	if (log >= 0 && (pipe2(out, O_CLOEXEC) != 0 ||
			 pipe2(err, O_CLOEXEC) != 0)) {
		fprintf(stderr, KPH_NAME ": pipe: %s\n", strerror(errno));
		for (int i = 0; i < 2; i++) {
			if (out[i] >= 0)
				close(out[i]);
			if (err[i] >= 0)
				close(err[i]);
		}
		close(sfd);
		sigprocmask(SIG_SETMASK, &old, NULL);
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, KPH_NAME ": fork: %s\n", strerror(errno));
		if (log >= 0) {
			close(out[0]);
			close(out[1]);
			close(err[0]);
			close(err[1]);
		}
		close(sfd);
		sigprocmask(SIG_SETMASK, &old, NULL);
		return -1;
	}
	if (pid == 0) {
		setpgid(0, 0);
		if (log >= 0) {
			dup2(out[1], STDOUT_FILENO);
			dup2(err[1], STDERR_FILENO);
		}
		int null = open("/dev/null", O_RDONLY);
		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			if (null != STDIN_FILENO)
				close(null);
		}
		kb_child_reset_signals();
		execvp(a->v[0], (char *const *)a->v);
		_exit(127);
	}
	/* Both sides set the group, so no signal sent before the child ran
	 * its own setpgid can miss it. */
	setpgid(pid, pid);
	if (log >= 0) {
		close(out[1]);
		close(err[1]);
		fcntl(out[0], F_SETFL, O_NONBLOCK);
		fcntl(err[0], F_SETFL, O_NONBLOCK);
	}

	int in = fcntl(STDIN_FILENO, F_GETFD) >= 0 ? STDIN_FILENO : -1;
	int stopping = 0, killed = 0, st = 0;
	struct timespec end = {0, 0};
	for (;;) {
		pid_t r = waitpid(pid, &st, WNOHANG);
		if (r == pid || (r < 0 && errno == ECHILD))
			break;
		struct pollfd p[4] = {
			{.fd = sfd, .events = POLLIN},
			{.fd = stopping ? -1 : in, .events = POLLIN},
			{.fd = out[0], .events = POLLIN},
			{.fd = err[0], .events = POLLIN},
		};
		int n = poll(p, 4, stopping && !killed ? kph_ms_left(&end) : -1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0) {
			fprintf(stderr, KPH_NAME ": poll: %s\n", strerror(errno));
			kill(-pid, SIGKILL);
			while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
				;
			if (out[0] >= 0)
				close(out[0]);
			if (err[0] >= 0)
				close(err[0]);
			close(sfd);
			sigprocmask(SIG_SETMASK, &old, NULL);
			return -1;
		}
		int stop = 0;
		if (n > 0 && p[2].revents)
			kph_copy(&out[0], STDOUT_FILENO, log);
		if (n > 0 && p[3].revents)
			kph_copy(&err[0], STDERR_FILENO, log);
		if (n == 0 && stopping && !killed) {
			kill(-pid, SIGKILL);
			killed = 1;
			continue;
		}
		if (n > 0 && (p[0].revents & POLLIN)) {
			struct signalfd_siginfo si;
			if (read(sfd, &si, sizeof(si)) == (ssize_t)sizeof(si) &&
			    si.ssi_signo != SIGCHLD)
				stop = 1;
		}
		if (n > 0 && (p[1].revents & POLLNVAL)) {
			in = -1;
		} else if (n > 0 && p[1].revents) {
			char drop[512];
			ssize_t got = read(in, drop, sizeof(drop));
			if (got == 0 ||
			    (got < 0 && errno != EINTR && errno != EAGAIN))
				stop = 1;
		}
		if (stop && !stopping) {
			stopping = 1;
			kill(-pid, SIGTERM);
			clock_gettime(CLOCK_MONOTONIC, &end);
			end.tv_sec += KPH_STOP_MS / 1000;
		}
	}
	/* What the child wrote before it exited is still in the pipes. A
	 * process of its group that outlived it may hold a write end open, so
	 * the drain stops at the first empty read rather than waiting for the
	 * end of file. */
	while (out[0] >= 0 && kph_copy(&out[0], STDOUT_FILENO, log) == 1)
		;
	while (err[0] >= 0 && kph_copy(&err[0], STDERR_FILENO, log) == 1)
		;
	if (out[0] >= 0)
		close(out[0]);
	if (err[0] >= 0)
		close(err[0]);
	close(sfd);
	sigprocmask(SIG_SETMASK, &old, NULL);
	/* A run that finished before the stop reached it is a run that
	 * finished. */
	if (!stopping || (WIFEXITED(st) && WEXITSTATUS(st) == 0))
		return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
	/* `kdos update` gone is not its group gone: kpkg or a compiler may
	 * outlive it by a moment, and the lock is not free until they do. */
	struct timespec last;
	clock_gettime(CLOCK_MONOTONIC, &last);
	last.tv_sec += KPH_STOP_MS / 1000 + 5;
	while (kill(-pid, 0) == 0 && kph_ms_left(&last) > 0) {
		if (!killed && kph_ms_left(&end) == 0) {
			kill(-pid, SIGKILL);
			killed = 1;
		}
		usleep(100000);
	}
	return -2;
}

int main(int argc, char **argv)
{
	KbArgv install = {0}, cat = {0};
	const char *port;

#ifdef KPH_BUILD
	const char *deps = NULL;
	if (argc == 4 && (!strcmp(argv[2], "--build-deps=prebuilt") ||
			  !strcmp(argv[2], "--build-deps=source"))) {
		deps = argv[2];
		argv[2] = argv[3];
		argc = 3;
	}
#endif
	int removing = 0;
#ifndef KPH_BUILD
	removing = argc == 3 && !strcmp(argv[1], "remove");
#endif
	if (argc != 3 || (strcmp(argv[1], "install") && !removing)) {
		fprintf(stderr, "usage: " KPH_NAME " " KPH_USAGE "\n");
		return 2;
	}
	port = argv[2];
	if (!kph_name_ok(port)) {
		fprintf(stderr, KPH_NAME ": '%s' is not a port name\n", port);
		return 2;
	}
	if (geteuid() != 0) {
		fprintf(stderr, KPH_NAME ": run through pkexec\n");
		return 2;
	}

	clearenv();
	setenv("PATH", SAFE_PATH, 1);
	setenv("LC_ALL", "C", 1);
	setenv("KDOS_UPDATE_FRONT", "discover", 1);

	int log = -1;
#ifdef KPH_BUILD
	char logpath[sizeof(KPH_LOG_DIR) + 140];
	snprintf(logpath, sizeof(logpath), KPH_LOG_DIR "/%s.log", port);
	if (mkdir(KPH_LOG_DIR, 0755) != 0 && errno != EEXIST)
		fprintf(stderr, KPH_NAME ": %s: %s\n", KPH_LOG_DIR,
			strerror(errno));
	log = open(logpath, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW |
			    O_CLOEXEC, 0644);
	if (log < 0)
		fprintf(stderr, KPH_NAME ": %s: %s; the build's output goes "
				"to Discover alone\n", logpath,
			strerror(errno));
	else {
		fchmod(log, 0644);
		setenv("KDOS_UPDATE_LOG", logpath, 1);
	}
#endif

	kb_argv_add(&install, "kdos");
	kb_argv_add(&install, "update");
	kb_argv_add(&install, removing ? "remove" : "install");
#ifdef KPH_BUILD
	kb_argv_add(&install, "--source");
	if (deps)
		kb_argv_add(&install, deps);
#else
	if (!removing)
		kb_argv_add(&install, "--prebuilt");
#endif
	kb_argv_add(&install, port);
	kb_argv_end(&install);
	int rc = kph_run(&install, log);
#ifdef KPH_BUILD
	if (log >= 0) {
		close(log);
		if (rc == 0)
			unlink(logpath);
	}
#endif
	if (rc == -2) {
		printf("==> %s: cancelled\n", port);
		return 130;
	}
	if (rc != 0)
		return 1;

	kb_argv_add(&cat, "kdos-discover-catalog");
	kb_argv_end(&cat);
	if (kb_run_tty(&cat) != 0)
		fprintf(stderr, KPH_NAME ": %s is %s and the AppStream "
				"catalogue was not rewritten; Discover lists it "
				"as it was until the next system update\n",
			port, removing ? "removed" : "installed");
	printf("==> %s %s\n", port, removing ? "removed" : "installed");
	return 0;
}

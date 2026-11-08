/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-sysupdated — org.freedesktop.sysupdate1 over the A/B root slots
 *
 * THE GAP THIS CLOSES. Discover's SystemdSysupdate backend is how Plasma shows
 * an operating-system update: it Pings org.freedesktop.sysupdate1, lists the
 * targets, reads each one's AppStream file, version and newest version, and
 * runs Acquire then Install, showing progress for the first. KDOS has no
 * systemd-sysupdated and no systemd; it has `kdos update` and kdos-bootctl.
 * This daemon is the bus face of those two, so a stock Discover drives a KDOS
 * update with no patch. It serves the API Discover 6.8.2 calls — the XML is
 * vendored in tests/ and the selftest diffs this daemon against it — and owns
 * no other systemd name.
 *
 *   Manager    ListTargets (one: host), ListJobs, ListAppStream, JobRemoved
 *   Target     GetVersion, GetAppStream, CheckNew, List, Describe, Vacuum,
 *              Acquire (all the long work: `kdos update apply --acquire`),
 *              Install (the short arm: `kdos update apply --arm`)
 *   Job        Id, Type, Offline, Progress, Cancel
 *
 * It also serves io.kdos.Slots1 — the slots, the trial, the job's progress,
 * Try and Bless — for the System Versions page and the update notifier, and
 * it is the root side of the two-mark bless: it runs `kdos-bootctl mark-good`
 * once /run/kdos/bless/ holds both marks (bless.c).
 *
 * IT DOES NO UPDATE WORK ITSELF. Every check, download, install and boot
 * decision is a `kdos update` or `kdos-bootctl` run, exec'd with an argument
 * vector; this file only waits for them without blocking the bus. A long
 * Acquire is a child process with a progress pipe, and the bus keeps
 * answering while it runs.
 *
 * NOTHING IS SERVED ON localhost:3129. Discover asks that address for an
 * update's size; with nothing listening it shows 0, and the size the update
 * document records is stated in the AppStream text (appstream.c) instead.
 *
 *   kdos-sysupdated [--session] [--root DIR] [--proc DIR]
 *                   [--bless-after S] [--tick S] [--refresh-after S]
 *
 * `--session` serves the session bus, which is where Discover looks with
 * DISCOVER_TEST_SYSUPDATE=1, and `--root` puts every file it reads and writes
 * under a fixture; together they are how it is tested without root.
 *
 * It exits 1 when its bus goes away, for the supervisor to restart it with
 * the bus, and 3 when another process owns one of its names, which a restart
 * cannot change. SIGHUP rereads every file it serves from.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "sysupdated.h"

Su su;

void su_log(const char *fmt, ...)
{
	va_list ap;
	fputs("kdos-sysupdated: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

/* A system path under --root. */
void su_path(const char *rel, char *out, size_t cap)
{
	snprintf(out, cap, "%s%s", su.root, rel);
}

static void usage(void)
{
	fprintf(stderr,
		"usage: kdos-sysupdated [--session] [--root DIR] [--proc DIR]\n"
		"                       [--bless-after SECONDS] [--tick SECONDS]\n"
		"                       [--refresh-after SECONDS]\n");
}

static int num_arg(const char *s, int lo, int hi, int *out)
{
	char *end;
	long v = strtol(s, &end, 10);
	if (*end || v < lo || v > hi)
		return -1;
	*out = (int)v;
	return 0;
}

static int parse_args(int argc, char **argv)
{
	su.bless_after = 60;
	su.tick = 5;
	su.refresh_after = 3600;
	kb_strlcpy(su.proc, "/proc", sizeof(su.proc));
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(a, "--session")) {
			su.session = 1;
		} else if (!strcmp(a, "--root") && v) {
			if (v[0] != '/') {
				fprintf(stderr, "kdos-sysupdated: --root takes an "
						"absolute path\n");
				return -1;
			}
			kb_strlcpy(su.root, v, sizeof(su.root));
			size_t n = strlen(su.root);
			while (n > 1 && su.root[n - 1] == '/')
				su.root[--n] = 0;
			if (!strcmp(su.root, "/"))
				su.root[0] = 0;
			i++;
		} else if (!strcmp(a, "--proc") && v) {
			kb_strlcpy(su.proc, v, sizeof(su.proc));
			i++;
		} else if (!strcmp(a, "--bless-after") && v) {
			if (num_arg(v, 1, 3600, &su.bless_after))
				return -1;
			i++;
		} else if (!strcmp(a, "--tick") && v) {
			if (num_arg(v, 1, 300, &su.tick))
				return -1;
			i++;
		} else if (!strcmp(a, "--refresh-after") && v) {
			if (num_arg(v, 0, 86400 * 7, &su.refresh_after))
				return -1;
			i++;
		} else {
			return -1;
		}
	}
	return 0;
}

/*
 * The directories this daemon writes, created root-owned and 0755: the
 * bless marks are files only root can create, which is what makes a mark
 * mean anything, and the AppStream file must be readable by the Discover of
 * any user.
 */
static void make_dirs(void)
{
	char p[1024];
	su_path("/run/kdos", p, sizeof(p));
	kb_mkdir_p(p);
	su_path("/run/kdos/bless", p, sizeof(p));
	kb_mkdir_p(p);
	chmod(p, 0755);
	su_path("/run/kdos/sysupdate", p, sizeof(p));
	kb_mkdir_p(p);
	chmod(p, 0755);
}

/*
 * The update document's directory, watched so a nightly check, a CheckNew
 * refresh or a `kdos update` typed at a shell is reflected at once — in the
 * AppStream file Discover fetches next and in the Slots1 properties the
 * notifier watches. Renamed into place by its writer, so IN_MOVED_TO is the
 * event; IN_CLOSE_WRITE covers a writer that does not rename.
 */
static int doc_watch(int ifd)
{
	char p[1024];
	su_path("/var/lib/kdos", p, sizeof(p));
	kb_mkdir_p(p);
	return inotify_add_watch(ifd, p, IN_MOVED_TO | IN_CLOSE_WRITE);
}

static void on_doc_inotify(int ifd)
{
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	int doc = 0, st = 0;
	for (;;) {
		ssize_t n = read(ifd, buf, sizeof(buf));
		if (n <= 0)
			break;
		for (char *p = buf; p < buf + n;) {
			struct inotify_event *e = (struct inotify_event *)p;
			if (e->len && !strcmp(e->name, "update.json"))
				doc = 1;
			if (e->len && !strcmp(e->name, "update.state"))
				st = 1;
			p += sizeof(*e) + e->len;
		}
	}
	if (doc)
		state_reload_doc();
	else if (st) {
		state_recompute();
		state_emit();
	}
}

int main(int argc, char **argv)
{
	if (parse_args(argc, argv) != 0) {
		usage();
		return 2;
	}
	if (!su.session && geteuid() != 0) {
		fprintf(stderr, "kdos-sysupdated: serves the system bus as root; "
				"--session serves a test bus\n");
		return 2;
	}
	/* The children's stderr is this daemon's, which the supervisor sends
	 * to syslog: a failed update is read there, line by line. */
	kb_proc_verbose = 1;
	signal(SIGPIPE, SIG_IGN);
	umask(022);

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (sfd < 0 || ifd < 0) {
		su_log("signalfd/inotify: %s", strerror(errno));
		return 1;
	}

	make_dirs();
	int b = bus_setup();
	if (b == -2)
		return 3;	/* another process owns the names */
	if (b != 0)
		return 1;
	doc_watch(ifd);
	bless_setup();
	state_reload_boot();
	state_reload_doc();
	bless_evaluate();
	su_log("serving %s and %s on the %s bus%s%s", SU_BUS, SL_BUS,
	       su.session ? "session" : "system", su.root[0] ? ", root " : "",
	       su.root);
	/* A document this root did not write — the first boot of a slot, the
	 * boot a rollback returns to — or none at all is rewritten from this
	 * root now, with the tree and index already on the machine and no
	 * network (state.c, state_doc_current). */
	if (!state_doc_current()) {
		su_log("the update document is not this system's; checking "
		       "against the tree in place");
		bus_refresh(0);
	}

	time_t next_tick = time(NULL) + su.tick;
	int quit = 0;
	while (!quit) {
		int r;
		while ((r = sd_bus_process(su.bus, NULL)) > 0)
			;
		if (r < 0) {
			su_log("the bus went away: %s", strerror(-r));
			child_cancel();
			return 1;
		}

		struct pollfd pfd[64];
		int n = 0;
		pfd[n].fd = sd_bus_get_fd(su.bus);
		pfd[n].events = (short)sd_bus_get_events(su.bus);
		n++;
		pfd[n].fd = sfd;
		pfd[n].events = POLLIN;
		n++;
		pfd[n].fd = ifd;
		pfd[n].events = POLLIN;
		n++;
		int bfd = bless_inotify_fd();
		if (bfd >= 0) {
			pfd[n].fd = bfd;
			pfd[n].events = POLLIN;
			n++;
		}
		int fixed = n;
		child_poll_fds(pfd, &n, 64);

		uint64_t usec = 0;
		int timeout = (int)((next_tick - time(NULL)) * 1000);
		if (timeout < 0)
			timeout = 0;
		if (sd_bus_get_timeout(su.bus, &usec) >= 0 && usec != UINT64_MAX) {
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			uint64_t now = (uint64_t)ts.tv_sec * 1000000ULL +
				       (uint64_t)ts.tv_nsec / 1000;
			int bt = usec > now ? (int)((usec - now + 999) / 1000) : 0;
			if (bt < timeout)
				timeout = bt;
		}

		if (poll(pfd, (nfds_t)n, timeout) < 0 && errno != EINTR) {
			su_log("poll: %s", strerror(errno));
			return 1;
		}

		if (pfd[1].revents & POLLIN) {
			struct signalfd_siginfo si;
			while (read(sfd, &si, sizeof(si)) == sizeof(si)) {
				if (si.ssi_signo == SIGCHLD)
					child_reap();
				else if (si.ssi_signo == SIGHUP) {
					state_reload_boot();
					state_reload_doc();
				} else
					quit = 1;
			}
		}
		if (pfd[2].revents & POLLIN)
			on_doc_inotify(ifd);
		if (bfd >= 0 && (pfd[3].revents & POLLIN))
			bless_on_inotify();
		for (int i = fixed; i < n; i++)
			if (pfd[i].revents & (POLLIN | POLLHUP | POLLERR))
				child_on_readable(pfd[i].fd);
		child_reap();

		if (time(NULL) >= next_tick) {
			next_tick = time(NULL) + su.tick;
			bless_tick();
		}
	}
	su_log("stopping");
	child_cancel();
	sd_bus_flush(su.bus);
	return 0;
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-oomd — kill the memory hog before the desktop wedges
 *
 * The kernel's OOM killer fires when an ALLOCATION fails, which on a machine
 * with swap or heavy reclaim is minutes after the desktop stopped answering —
 * the whole session spent thrashing while the kernel still technically had
 * pages to hand out. PSI is the earlier signal: /proc/pressure/memory says the
 * machine is STALLING on memory, which is what a wedged desktop feels like,
 * and the appbox makes that failure likely here (a browser plus a slicer in
 * one 8 GB machine).
 *
 * So this blocks on a PSI trigger — write the threshold into the pressure
 * file, poll(2) for POLLPRI; the kernel's own trigger API, no sampling loop —
 * and when it fires, kills the largest-RSS process that is neither the desktop
 * nor the plumbing, SIGKILL then process_mrelease(2) so the pages come back
 * NOW rather than whenever the zombie is reaped. Boxed processes are preferred
 * as victims: an alien app is the likely culprit, is supervised by podman, and
 * relaunches in seconds, while a host process is more often session state.
 *
 * THE SHAPE IS kdos-powerd's: a root daemon in the foreground under ksvc, a
 * socket in /run answering one word per connection (`ping`, `status`) gated by
 * SO_PEERCRED — root and wheel — so `kdos doctor` has something to ask.
 * Killing takes no client and no argument: there is nothing in the protocol
 * that names a process, so there is nothing to aim.
 *
 * `--fixture <dir>` points every /proc read at a recorded tree and prints who
 * WOULD be killed without signalling anyone — the kdos stutter seam, and the
 * only way selection logic this consequential gets tested at all.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred, pipe2 */
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

#include "kbase.h"

#define KO_SOCKET   "/run/kdos-oomd.sock"
#define KO_GROUP    "wheel"
/* 150 ms of full stall in a 1 s window: every runnable thread stuck on memory
 * for 15% of the last second. A desktop is already visibly hitching there. */
#define KO_TRIGGER  "full 150000 1000000"
#define KO_COOLDOWN 10.0	/* seconds between kills, at most */

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_process_mrelease
#define SYS_process_mrelease 448
#endif

static const char *g_proc = "/proc";
static volatile sig_atomic_t g_stop;
static int g_kills;
static char g_last[160];

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static const char *sock_path(void)
{
	const char *p = getenv("KDOS_OOMD_SOCKET");
	return p && *p ? p : KO_SOCKET;
}

/* ── reading ───────────────────────────────────────────────────────────── */

static char *slurp(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

static char *slurp(const char *fmt, ...)
{
	char path[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);
	size_t n = 0;
	return kb_read_all(path, &n);
}

/* avg10 only, the same reasoning as kdos stutter: a wedge is a moment, and
 * avg60 has already averaged it away by the time anyone reads it. */
static void read_pressure(double *some, double *full)
{
	*some = *full = -1.0;
	char *data = slurp("%s/pressure/memory", g_proc);
	if (!data)
		return;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		const char *avg = strstr(line, "avg10=");
		if (!avg)
			continue;
		if (!strncmp(line, "some", 4))
			*some = atof(avg + 6);
		else if (!strncmp(line, "full", 4))
			*full = atof(avg + 6);
	}
	free(data);
}

typedef struct {
	int pid;
	int ppid;
	char comm[40];
	long long rss_kb;
} KoProc;

/*
 * /proc/<pid>/stat from the LAST ')' — comm may hold spaces and parentheses
 * (`(Web Content)` is real, and a browser is exactly what this ends up
 * naming). ppid is the 4th field, rss the 24th, in pages.
 */
static int read_stat(int pid, KoProc *out)
{
	char *data = slurp("%s/%d/stat", g_proc, pid);
	if (!data)
		return 0;
	char *close = strrchr(data, ')');
	char *open = strchr(data, '(');
	if (!close || !open || close < open) {
		free(data);
		return 0;
	}
	size_t len = (size_t)(close - open - 1);
	if (len >= sizeof(out->comm))
		len = sizeof(out->comm) - 1;
	memcpy(out->comm, open + 1, len);
	out->comm[len] = '\0';

	long long rss = 0;
	int ppid = 0;
	int got = sscanf(close + 2,
			 "%*c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u "
			 "%*d %*d %*d %*d %*d %*d %*u %*u %lld",
			 &ppid, &rss);
	free(data);
	if (got < 2)
		return 0;
	out->pid = pid;
	out->ppid = ppid;
	out->rss_kb = rss * (sysconf(_SC_PAGESIZE) / 1024);
	return 1;
}

/*
 * Which box is this process in? The conmon-argv walk kdos stutter and
 * kdos-energyd already use: no systemd means rootless podman gets no cgroup
 * delegation and the box frequently sits in `0::/`, so the ppid chain up to
 * podman's supervisor — whose argv carries `-n <name>` — is the only identity
 * there is, and it costs a few file reads on a machine already struggling.
 */
static int box_of(int pid, char *out, size_t cap)
{
	for (int hop = 0; hop < 24 && pid > 1; hop++) {
		KoProc st = {0};
		if (!read_stat(pid, &st))
			return 0;
		if (!strcmp(st.comm, "conmon")) {
			char *cmd = slurp("%s/%d/cmdline", g_proc, pid);
			if (!cmd)
				return 0;
			size_t len = 0;
			int found = 0;
			for (char *a = cmd; *a; a += len + 1) {
				len = strlen(a);
				if (!strcmp(a, "-n") && a[len + 1]) {
					kb_strlcpy(out, a + len + 1, cap);
					found = 1;
					break;
				}
			}
			free(cmd);
			return found;
		}
		pid = st.ppid;
	}
	return 0;
}

/* ── selection ─────────────────────────────────────────────────────────── */

/*
 * The protected set. pid 1, kernel threads (empty cmdline), anything that
 * opted out through oom_score_adj, this daemon itself, and the desktop's own
 * chrome plus the session's plumbing by comm — killing kdos-comp to save the
 * desktop is not a trade, and a dead pipewire takes every app's audio with it.
 * The three chrome names are argv[0]s of one binary, which is what comm
 * reports for a process exec'd through a symlink.
 *
 * The plumbing is here for the same reason and is easy to leave out, because
 * none of it is what a user would call an application: Xwayland is every X11
 * alien app at once and can be the largest process on the machine, the session
 * bus is the fixed address the whole appbox depends on, seatd owns the seat the
 * compositor holds, and the supervisors are what would restart any of it.
 */
static const char *const ko_protected[] = {
	"kdos-comp", "kdos-shell", "kdos-desk", "kdos-notifyd",
	"Xwayland", "dbus-daemon", "seatd", "ksvc",
	"kdos-powerd", "kdos-energyd", "kdos-oomd", NULL
};

static int is_protected(const KoProc *p)
{
	if (p->pid == 1)
		return 1;
	for (int i = 0; ko_protected[i]; i++)
		if (!strcmp(p->comm, ko_protected[i]))
			return 1;
	/* pipewire, pipewire-pulse, pipewire-media-session (comm-truncated). */
	if (!strncmp(p->comm, "pipewire", 8))
		return 1;

	char *cmd = slurp("%s/%d/cmdline", g_proc, p->pid);
	if (!cmd) 		/* vanished mid-scan */
		return 1;
	int kthread = !cmd[0];
	free(cmd);
	if (kthread)
		return 1;

	/* <= -500: the convention oomd and earlyoom honour. -1000 is
	 * OOM_SCORE_ADJ_MIN, "never kill"; halfway is "someone deliberately
	 * shielded this" and that decision is respected here too. */
	char *adj = slurp("%s/%d/oom_score_adj", g_proc, p->pid);
	if (adj) {
		int v = atoi(adj);
		free(adj);
		if (v <= -500)
			return 1;
	}
	return 0;
}

typedef struct {
	KoProc p;
	char box[64];
	int boxed;
} KoVictim;

static int is_number(const char *s)
{
	if (!*s)
		return 0;
	for (const char *p = s; *p; p++)
		if (*p < '0' || *p > '9')
			return 0;
	return 1;
}

/*
 * The largest-RSS candidate, with boxed processes PREFERRED but not absolute:
 * a boxed victim wins whenever it holds at least half the RSS of the biggest
 * candidate overall. An alien app is the likely culprit and relaunches in
 * seconds; a host process twice its size is still the honest kill, and an
 * absolute preference would shoot a 100 MB boxed helper while an 8 GB host
 * leak keeps the machine wedged.
 */
static int pick_victim(KoVictim *v)
{
	KoVictim any = {0}, boxed = {0};
	int self = getpid();

	int count = 0;
	char **names = kb_listdir(g_proc, &count);
	if (!names)
		return 0;
	for (int i = 0; i < count; i++) {
		if (!is_number(names[i]))
			continue;
		KoProc p = {0};
		int pid = atoi(names[i]);
		if (pid == self)
			continue;
		if (!read_stat(pid, &p) || p.rss_kb <= 0)
			continue;
		if (is_protected(&p))
			continue;
		char box[64] = "";
		if (box_of(p.ppid, box, sizeof(box))) {
			if (p.rss_kb > boxed.p.rss_kb) {
				boxed.p = p;
				boxed.boxed = 1;
				kb_strlcpy(boxed.box, box, sizeof(boxed.box));
			}
		}
		if (p.rss_kb > any.p.rss_kb)
			any.p = p;
	}
	kb_strv_free(names);

	if (!any.p.pid)
		return 0;
	if (boxed.p.pid && boxed.p.rss_kb * 2 >= any.p.rss_kb)
		*v = boxed;
	else {
		*v = any;
		v->boxed = box_of(v->p.ppid, v->box, sizeof(v->box));
	}
	return 1;
}

/* ── the kill ──────────────────────────────────────────────────────────── */

/*
 * pidfd first, so the reclaim cannot land on a recycled pid; SIGKILL; then
 * process_mrelease(2), which reaps the victim's address space from OUR context
 * instead of waiting for its (possibly memory-starved) parent to get
 * scheduled. That syscall is the difference between "the pressure ends now"
 * and "the pressure ends when the thrashing machine gets round to it". A
 * kernel without it (pre-5.15) just skips the release; the SIGKILL stands.
 */
static void kill_victim(const KoVictim *v, double some, double full)
{
	int pidfd = (int)syscall(SYS_pidfd_open, v->p.pid, 0);
	if (kill(v->p.pid, SIGKILL) != 0) {
		fprintf(stderr, "kdos-oomd: kill %d (%s): %s\n", v->p.pid,
			v->p.comm, strerror(errno));
		if (pidfd >= 0)
			close(pidfd);
		return;
	}
	if (pidfd >= 0) {
		syscall(SYS_process_mrelease, pidfd, 0);
		close(pidfd);
	}

	snprintf(g_last, sizeof(g_last), "%s%s%s%s (pid %d, %lld MB)",
		 v->p.comm, v->boxed ? " (appbox " : "",
		 v->boxed ? v->box : "", v->boxed ? ")" : "",
		 v->p.pid, v->p.rss_kb / 1024);
	g_kills++;
	/* The one line that attributes a vanished app. stderr is the
	 * supervisor's log; the PSI numbers are the why. */
	fprintf(stderr, "kdos-oomd: killed %s — memory pressure avg10 "
			"full=%.2f some=%.2f\n", g_last, full, some);
}

/* ── the allowed set ───────────────────────────────────────────────────── */

static bool uid_allowed(uid_t uid)
{
	if (uid == 0)
		return true;
	struct passwd *pw = getpwuid(uid);
	if (!pw || !pw->pw_name)
		return false;
	return kb_user_in_group(pw->pw_name, pw->pw_gid, KO_GROUP) != 0;
}

/* ── the daemon ────────────────────────────────────────────────────────── */

/*
 * Shield the daemon from the emergency it watches. Every /proc read below
 * allocates, and the moment those allocations matter is the moment memory is
 * short: a kdos-oomd the kernel's own killer picked, or one whose pages were
 * reclaimed mid-scan, is a kdos-oomd that is not there for the one minute it
 * exists for — which is why earlyoom and systemd-oomd both do exactly this.
 * Best-effort on purpose: a kernel or a rlimit that refuses either leaves the
 * daemon no worse off than it was.
 */
static void shield_self(void)
{
	int fd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);
	if (fd >= 0) {
		(void)!write(fd, "-1000\n", 6);
		close(fd);
	}
	(void)mlockall(MCL_CURRENT | MCL_FUTURE);
}

static void answer(int c)
{
	struct ucred cred = {0};
	socklen_t len = sizeof(cred);
	/* A client that connects and never speaks must not stall the trigger
	 * poll — the loop is single-threaded on purpose. */
	struct timeval tv = { .tv_sec = 2 };
	setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0 ||
	    !uid_allowed(cred.uid)) {
		(void)!write(c, "err not permitted\n", 18);
		close(c);
		return;
	}

	char buf[32] = {0};
	ssize_t n = read(c, buf, sizeof(buf) - 1);
	if (n > 0) {
		buf[n] = '\0';
		buf[strcspn(buf, "\r\n")] = '\0';
		if (!strcmp(buf, "ping")) {
			(void)!write(c, "ok\n", 3);
		} else if (!strcmp(buf, "status")) {
			char out[256];
			int m = snprintf(out, sizeof(out),
					 "ok trigger '%s', kills %d%s%s\n",
					 KO_TRIGGER, g_kills,
					 g_kills ? ", last: " : "",
					 g_kills ? g_last : "");
			(void)!write(c, out, (size_t)m);
		} else {
			(void)!write(c, "err unknown command\n", 20);
		}
	}
	close(c);
}

static int serve(void)
{
	const char *path = sock_path();
	if (geteuid() != 0) {
		/* Same rule as kdos-powerd: refused on the real socket, warned
		 * on a test one — an unprivileged daemon can neither read every
		 * process nor kill any of them. */
		if (!strcmp(path, KO_SOCKET)) {
			fprintf(stderr, "kdos-oomd: must run as root\n");
			return 1;
		}
		fprintf(stderr, "kdos-oomd: not root — kills will fail\n");
	}

	char psi_path[512];
	snprintf(psi_path, sizeof(psi_path), "%s/pressure/memory", g_proc);
	int psi = open(psi_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (psi < 0) {
		/* Refused rather than started: a daemon that cannot arm its
		 * trigger protects nothing, and saying so here is what lets
		 * the init script's own check stay honest. */
		fprintf(stderr, "kdos-oomd: %s: %s\n", psi_path,
			strerror(errno));
		return 1;
	}
	/* The kernel's trigger write wants the terminating NUL included. */
	if (write(psi, KO_TRIGGER, strlen(KO_TRIGGER) + 1) < 0) {
		fprintf(stderr, "kdos-oomd: arming PSI trigger: %s\n",
			strerror(errno));
		close(psi);
		return 1;
	}

	int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0) {
		fprintf(stderr, "kdos-oomd: socket: %s\n", strerror(errno));
		close(psi);
		return 1;
	}
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-oomd: bind %s: %s\n", path,
			strerror(errno));
		close(psi);
		close(srv);
		return 1;
	}
	/* 0666 with SO_PEERCRED as the real gate — kdos-powerd's reasoning. */
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		fprintf(stderr, "kdos-oomd: listen: %s\n", strerror(errno));
		close(psi);
		close(srv);
		return 1;
	}

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);
	shield_self();

	double last_kill = -KO_COOLDOWN;
	while (!g_stop) {
		struct pollfd fds[2] = {
			{ .fd = psi, .events = POLLPRI },
			{ .fd = srv, .events = POLLIN },
		};
		int rc = poll(fds, 2, -1);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		if (fds[0].revents & POLLERR) {
			/* The trigger fd is dead (psi=0 flipped at runtime is
			 * the only known way); a silent loop on it would spin. */
			fprintf(stderr, "kdos-oomd: PSI trigger lost\n");
			break;
		}
		if (fds[0].revents & POLLPRI) {
			/* One kill per cooldown window, however hard the
			 * trigger fires: the pressure takes seconds to drain
			 * after a kill, and a second trigger inside that drain
			 * would shoot the NEXT largest app for the same
			 * incident. */
			double now = kb_now_s();
			if (now - last_kill >= KO_COOLDOWN) {
				double some, full;
				KoVictim v = {0};
				/* Armed on the TRIGGER, not on the kill: the
				 * cooldown is how often this daemon may walk
				 * every entry in /proc, and a trigger that
				 * finds nothing killable — or a victim that
				 * will not die — would otherwise rescan once a
				 * second on a machine already stalling on
				 * memory. */
				last_kill = now;
				read_pressure(&some, &full);
				if (pick_victim(&v))
					kill_victim(&v, some, full);
			}
		}
		if (fds[1].revents & POLLIN) {
			int c = accept(srv, NULL, NULL);
			if (c >= 0)
				answer(c);
		}
	}

	close(psi);
	close(srv);
	unlink(path);
	/* A SIGTERM is how this daemon is meant to end; only the POLLERR break
	 * above is a failure, and a log that cannot tell them apart says
	 * nothing about either. */
	return g_stop ? 0 : 1;
}

/* ── fixture mode ──────────────────────────────────────────────────────── */

/*
 * One selection pass against a recorded tree, printing the choice and
 * signalling nobody. Exit 0 with a candidate, 1 without — which is what makes
 * the selection assertable from a test with no memory pressure to hand.
 */
static int fixture(const char *dir)
{
	g_proc = dir;
	double some, full;
	KoVictim v = {0};
	read_pressure(&some, &full);
	if (!pick_victim(&v)) {
		printf("no candidate\n");
		return 1;
	}
	printf("would kill %s%s%s%s (pid %d, %lld kB) — memory pressure "
	       "avg10 full=%.2f some=%.2f\n",
	       v.p.comm, v.boxed ? " (appbox " : "", v.boxed ? v.box : "",
	       v.boxed ? ")" : "", v.p.pid, v.p.rss_kb, full, some);
	return 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-oomd");

	if (argc == 3 && !strcmp(argv[1], "--fixture"))
		return fixture(argv[2]);
	if (argc > 1) {
		fprintf(stderr, "usage: kdos-oomd [--fixture DIR]\n");
		return 2;
	}
	return serve();
}

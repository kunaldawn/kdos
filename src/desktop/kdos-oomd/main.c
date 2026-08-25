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
#include <ctype.h>
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
#include "kproc.h"

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
 * Which box is this process in? libkproc owns the walk: the ppid climb to
 * podman's supervisor, whose argv carries `-n <name>`, is the only identity
 * there is here — no systemd means rootless podman gets no cgroup delegation
 * and the box frequently sits in `0::/`. It costs a few file reads on a
 * machine that is already struggling, which is why it reads /proc directly
 * rather than holding a table.
 */
static int box_of(int pid, char *out, size_t cap)
{
	return kpr_box_of_pid(pid, out, cap);
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
 * A BOX'S DECLARED BUDGET, and reading it here is what makes the key honest.
 *
 * `memory = 4G` in a box profile becomes `podman --memory`, and rootless
 * podman on a machine with no systemd frequently has no cgroup delegation to
 * enforce it with — so the flag is accepted and does nothing. A profile key
 * that cannot be enforced is one KDOS does not offer, so this is the
 * enforcement: a box over the budget it declared is PREFERRED as a victim,
 * ahead of the general rule that boxed processes are preferred.
 *
 * The profiles live in the user's home, which this daemon reads and never
 * writes. A box with no budget declared returns 0 and is judged by size alone.
 */
static unsigned long long parse_size_kb(const char *v)
{
	double n;
	char unit = 0;

	if (sscanf(v, "%lf%c", &n, &unit) < 1 || n <= 0)
		return 0;
	switch (unit) {
	case 'g': case 'G': return (unsigned long long)(n * 1024 * 1024);
	case 'm': case 'M': return (unsigned long long)(n * 1024);
	case 'k': case 'K': return (unsigned long long)n;
	default:            return (unsigned long long)(n / 1024);  /* bytes */
	}
}

/*
 * Where the box profiles are. One directory when the fixture names one;
 * otherwise every user's own, because this daemon is root and the budgets it
 * enforces were declared by whoever owns the box.
 */
static const char *profiles_root(void)
{
	const char *p = getenv("KDOS_BOX_PROFILES");
	return p && *p ? p : "";
}

static unsigned long long box_budget_kb(const char *box)
{
	char path[512], buf[4096];
	char *line, *save;
	unsigned long long kb = 0;

	/* A box name reaches this from a conmon command line, so it is checked
	 * before it becomes a path component. */
	if (!box || !*box || strlen(box) > 63)
		return 0;
	for (const char *c = box; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '.' && *c != '_' &&
		    *c != '-')
			return 0;

	if (profiles_root()[0]) {
		snprintf(path, sizeof(path), "%s/%s.conf", profiles_root(), box);
		if (kb_read_file(path, buf, sizeof(buf)) < 0)
			return 0;
	} else {
		char **users = kb_listdir("/home", NULL);
		int got = 0;
		for (char **u = users; u && *u && !got; u++) {
			snprintf(path, sizeof(path),
				 "/home/%s/.config/kdos/boxes/%s.conf", *u, box);
			got = kb_read_file(path, buf, sizeof(buf)) >= 0;
		}
		kb_strv_free(users);
		if (!got)
			return 0;
	}

	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *eq;
		while (*line == ' ' || *line == '\t')
			line++;
		if (strncmp(line, "memory", 6))
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		eq++;
		while (*eq == ' ')
			eq++;
		kb = parse_size_kb(eq);
	}
	return kb;
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
	KoVictim any = {0}, boxed = {0}, over = {0};
	/* Per-box RSS, so "over its budget" is a statement about the BOX and
	 * not about whichever of its forty processes happens to be largest. */
	struct { char name[64]; unsigned long long kb; } tally[32] = {{{0},0}};
	int ntally = 0;
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
		/*
		 * The PID, not its parent. kpr_box_of_pid climbs from the
		 * process's parent itself — conmon supervises the box from
		 * outside it and must never be reported as a member — so
		 * handing it a parent starts the walk one hop too high and
		 * every boxed process comes back unboxed.
		 */
		if (box_of(p.pid, box, sizeof(box))) {
			int t;
			if (p.rss_kb > boxed.p.rss_kb) {
				boxed.p = p;
				boxed.boxed = 1;
				kb_strlcpy(boxed.box, box, sizeof(boxed.box));
			}
			for (t = 0; t < ntally; t++)
				if (!strcmp(tally[t].name, box))
					break;
			if (t == ntally && ntally < 32) {
				kb_strlcpy(tally[ntally].name, box, 64);
				tally[ntally].kb = 0;
				ntally++;
			}
			if (t < 32)
				tally[t].kb += (unsigned long long)p.rss_kb;
		}
		if (p.rss_kb > any.p.rss_kb)
			any.p = p;
	}
	kb_strv_free(names);

	if (!any.p.pid)
		return 0;

	/*
	 * A box over the budget its own profile declared goes first — that is
	 * the whole of what `memory =` buys on a machine where podman cannot
	 * enforce it. The victim inside it is still the largest process, so
	 * the message names something a person recognises.
	 */
	for (int t = 0; t < ntally; t++) {
		unsigned long long budget = box_budget_kb(tally[t].name);
		if (!budget || tally[t].kb <= budget)
			continue;
		if (boxed.p.pid && !strcmp(boxed.box, tally[t].name)) {
			over = boxed;
		} else {
			/* the largest process of THAT box, found on a second
			 * pass — cheap, and only when a box is over budget */
			int c2 = 0;
			char **n2 = kb_listdir(g_proc, &c2);
			for (int i = 0; n2 && i < c2; i++) {
				KoProc q = {0};
				char b2[64] = "";
				if (!is_number(n2[i]))
					continue;
				if (!read_stat(atoi(n2[i]), &q) || q.rss_kb <= 0)
					continue;
				if (is_protected(&q))
					continue;
				if (!box_of(q.pid, b2, sizeof(b2)) ||
				    strcmp(b2, tally[t].name))
					continue;
				if (q.rss_kb > over.p.rss_kb) {
					over.p = q;
					over.boxed = 1;
					kb_strlcpy(over.box, b2, sizeof(over.box));
				}
			}
			kb_strv_free(n2);
		}
		if (over.p.pid) {
			*v = over;
			return 1;
		}
	}

	if (boxed.p.pid && boxed.p.rss_kb * 2 >= any.p.rss_kb)
		*v = boxed;
	else {
		*v = any;
		v->boxed = box_of(v->p.pid, v->box, sizeof(v->box));
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
	/*
	 * BOTH roots. The sampler reads through g_proc and the box identity
	 * through libkproc's own root; moving one and not the other replays a
	 * recorded machine while asking the REAL /proc who owns its processes.
	 */
	g_proc = dir;
	kpr_root_set(dir, NULL);
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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-energyd / kdos-energy — which app is spending the battery
 *
 *     $ kdos-energy
 *     KDOS energy  —  2.1 h of samples, RAPL package-0
 *
 *       firefox (appbox kdos-apps)          41.2%  ████████        gpu 22.1%
 *       kdos-comp                            9.4%  ██              gpu 61.0%
 *
 * Windows 11, macOS and Android all ship per-app battery attribution. No Linux
 * desktop does, and the reason is not the measurement — RAPL and the cycle-share
 * model are twenty years old. It is IDENTITY: "Firefox" is forty processes in
 * scattered cgroups, and no component owns enough of the system to name them.
 * On KDOS every fat application already runs inside its own container with a
 * canonical name in /usr/share/kdos/alien-apps, so the expensive half is free
 * here and only here.
 *
 * WHY A DAEMON AND NOT A COMMAND. RAPL is a free-running counter: a one-shot
 * tool can only report what happened while it was watching, which is not the
 * question anyone asks. And the counter has been root-only since Linux 5.10
 * closed PLATYPUS (CVE-2020-8694/8695) — fine-grained unprivileged reads
 * recover AES keys — so something privileged has to hold it either way. This is
 * the smallest such thing: a sampler, a ledger, and a socket that serves the
 * ledger to root and wheel. It is a SAMPLER ONLY. It sets no power limit, has
 * no write path into powercap at all, and takes no argument from any client.
 *
 * WHY THE SOCKET IS NOT AN ORACLE. What leaves this process is a per-app
 * percentage accumulated over minutes; the raw counter and the sampling
 * interval are never republished. PLATYPUS needs microsecond resolution on a
 * victim's own execution. A percentage of a two-hour window is not that, and
 * the interval is fixed by the daemon rather than requested by the client so
 * that it cannot be driven toward being one.
 * ---------------------------------
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred */
#endif
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "energyd.h"

/*
 * Ten seconds. Two constraints meet here and both are hard floors rather than
 * preferences: the RAPL counter wraps about every 36 minutes at a laptop's
 * power draw, so the interval must be far below that or a wrap cannot be told
 * from a stall; and the sampler walks every process's fd table, which is real
 * work to do on a battery — a power tool that itself costs power is a joke with
 * a long setup. Fixed, not configurable: see the oracle argument above.
 */
#define KE_INTERVAL 10.0

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static const char *sock_path(void)
{
	const char *p = getenv("KDOS_ENERGYD_SOCKET");
	return p && *p ? p : KE_SOCKET;
}

/* ── the allowed set ───────────────────────────────────────────────────── */

/*
 * Root and wheel, the same answer kdos-powerd gives to the same question, from
 * the same libkbase function. It matters that this is a real gate rather than
 * a courtesy: on a multi-user machine the app list is a list of what other
 * people are running, and this daemon's whole subject matter is what everyone
 * on the machine is doing.
 *
 * The gate is HERE and not on the socket's mode. The socket is 0666 and every
 * uid may connect; what an unauthorised one gets is `err not permitted` and a
 * closed connection. That is deliberate and is kdos-powerd's reasoning too —
 * a mode that looked like the authorisation is a mode somebody eventually
 * loosens, and SO_PEERCRED cannot be forged by the peer.
 */
static bool uid_allowed(uid_t uid)
{
	if (uid == 0)
		return true;
	struct passwd *pw = getpwuid(uid);
	if (!pw || !pw->pw_name)
		return false;
	return kb_user_in_group(pw->pw_name, pw->pw_gid, KE_GROUP) != 0;
}

/* ── the daemon ────────────────────────────────────────────────────────── */

static void answer(int c, const KeLedger *l, const KeRapl *r, bool json)
{
	KbBuf b = {0};
	ke_ledger_report(l, r, &b, json);
	if (b.p)
		(void)!write(c, b.p, b.n);
	kb_buf_free(&b);
}

static int serve(void)
{
	KeRapl rapl;
	int have_rapl = ke_rapl_open(&rapl);
	if (have_rapl != 0) {
		/* Refused rather than started. A daemon that samples an
		 * unreadable counter reports a machine that uses no energy at
		 * all, which reads as "nothing is draining the battery" — the
		 * most misleading output this program could produce. */
		fprintf(stderr, "kdos-energyd: %s\n", rapl.why);
		return 1;
	}

	const char *path = sock_path();
	int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0) {
		fprintf(stderr, "kdos-energyd: socket: %s\n", strerror(errno));
		return 1;
	}
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-energyd: bind %s: %s\n", path,
			strerror(errno));
		close(srv);
		return 1;
	}
	/* Same shape as kdos-powerd: the mode is open and SO_PEERCRED is the
	 * gate, so nothing is tempted to weaken the credential check on the
	 * grounds that the mode already handles it. */
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		fprintf(stderr, "kdos-energyd: listen: %s\n", strerror(errno));
		close(srv);
		return 1;
	}

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	KeLedger ledger = {0};
	ledger.started = kb_now_s();
	KeSample a = {0}, b = {0};
	KeSample *prev = &a, *cur = &b;

	ke_apps_load();
	ke_sample_take(prev);
	ke_rapl_delta(&rapl);		/* prime: the first call has no delta */
	double next = kb_now_s() + KE_INTERVAL;

	while (!g_stop) {
		double now = kb_now_s();
		int wait_ms = (int)((next - now) * 1000.0);
		if (wait_ms < 0)
			wait_ms = 0;

		struct pollfd pfd = { .fd = srv, .events = POLLIN };
		int rc = poll(&pfd, 1, wait_ms);
		if (rc < 0 && errno != EINTR)
			break;

		if (rc > 0 && (pfd.revents & POLLIN)) {
			int c = accept(srv, NULL, NULL);
			if (c >= 0) {
				struct ucred cred = {0};
				socklen_t len = sizeof(cred);
				/* A client that connects and never speaks would
				 * otherwise stop the sampler dead — the loop is
				 * single-threaded on purpose, and this is what
				 * keeps that safe. */
				struct timeval tv = { .tv_sec = 2 };
				setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv,
					   sizeof(tv));

				if (getsockopt(c, SOL_SOCKET, SO_PEERCRED,
					       &cred, &len) < 0 ||
				    !uid_allowed(cred.uid)) {
					(void)!write(c, "err not permitted\n", 18);
					close(c);
				} else {
					char buf[32] = {0};
					ssize_t n = read(c, buf, sizeof(buf) - 1);
					if (n > 0) {
						buf[n] = '\0';
						buf[strcspn(buf, "\r\n")] = '\0';
						if (!strcmp(buf, "ping"))
							(void)!write(c, "ok\n", 3);
						else if (!strcmp(buf, "report"))
							answer(c, &ledger, &rapl,
							       false);
						else if (!strcmp(buf, "report-json"))
							answer(c, &ledger, &rapl,
							       true);
						else
							(void)!write(c,
								"err unknown command\n",
								20);
					}
					close(c);
				}
			}
		}

		if (kb_now_s() >= next) {
			ke_sample_take(cur);
			long long e = ke_rapl_delta(&rapl);
			ke_ledger_add(&ledger, prev, cur, e);
			KeSample *t = prev;
			prev = cur;
			cur = t;
			next += KE_INTERVAL;
			/* A machine that suspended for an hour comes back with
			 * `next` far in the past; catching up would spin
			 * through a hundred immediate samples of nothing. */
			if (next < kb_now_s())
				next = kb_now_s() + KE_INTERVAL;
		}
	}

	ke_sample_free(&a);
	ke_sample_free(&b);
	close(srv);
	unlink(path);
	return 0;
}

/* ── the fixture ───────────────────────────────────────────────────────── */

/*
 * `kdos-energyd --fixture <dir>` replays recorded snapshots through the SAME
 * sampler and the same ledger the daemon uses, and prints the report.
 *
 * That is the only way an attribution engine gets tested: the real inputs are a
 * root-only counter and a machine that has to be busy in a particular way, and
 * a test that needed both would never run. `<dir>/0`, `<dir>/1`, … each hold a
 * `proc` tree, a `powercap` tree and an optional `dt` in seconds — the same
 * trick as `kdos stutter --fixture` and KDOS_PRIVACY_PROC.
 */
static int fixture(const char *dir, bool json)
{
	KeRapl rapl;
	KeLedger ledger = {0};
	KeSample a = {0}, b = {0};
	KeSample *prev = &a, *cur = &b;
	bool first = true;
	double clock = 0;
	int steps = 0;

	for (int step = 0;; step++) {
		char proc[512], power[512], dtpath[512];
		snprintf(proc, sizeof(proc), "%s/%d/proc", dir, step);
		snprintf(power, sizeof(power), "%s/%d/powercap", dir, step);
		snprintf(dtpath, sizeof(dtpath), "%s/%d/dt", dir, step);
		if (!kb_is_dir(proc))
			break;

		setenv("KDOS_ENERGY_PROC", proc, 1);
		setenv("KDOS_ENERGY_POWERCAP", power, 1);

		double dt = 10.0;
		char buf[32];
		if (kb_read_line_file(dtpath, buf, sizeof(buf)) > 0)
			dt = atof(buf);

		if (first) {
			if (ke_rapl_open(&rapl) != 0) {
				fprintf(stderr, "kdos-energyd: fixture: %s\n",
					rapl.why);
				return 2;
			}
			ke_sample_take(prev);
			prev->when = clock;
			ke_rapl_delta(&rapl);
			first = false;
			continue;
		}

		/* Repoint the domains at this step's tree, keeping the counter
		 * values: a wrap between two recorded snapshots is exactly what
		 * the fixture exists to be able to express. */
		for (int i = 0; i < rapl.n; i++) {
			const char *base = kb_basename(rapl.d[i].path);
			char np[1024];
			snprintf(np, sizeof(np), "%s/%s", power, base);
			kb_strlcpy(rapl.d[i].path, np, sizeof(rapl.d[i].path));
		}

		clock += dt;
		ke_sample_take(cur);
		cur->when = clock;
		ke_ledger_add(&ledger, prev, cur, ke_rapl_delta(&rapl));
		KeSample *t = prev;
		prev = cur;
		cur = t;
		steps++;
	}

	if (steps == 0) {
		fprintf(stderr, "kdos-energyd: %s holds no 0/proc and 1/proc\n",
			dir);
		return 2;
	}

	KbBuf out = {0};
	ke_ledger_report(&ledger, &rapl, &out, json);
	if (out.p)
		fwrite(out.p, 1, out.n, stdout);
	kb_buf_free(&out);
	ke_sample_free(&a);
	ke_sample_free(&b);
	return 0;
}

/* ── the client ────────────────────────────────────────────────────────── */

static int client(int argc, char **argv)
{
	const char *verb = "report";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--json"))
			verb = "report-json";
		else if (!strcmp(argv[i], "ping"))
			verb = "ping";
		else {
			fprintf(stderr, "usage: kdos-energy [--json|ping]\n");
			return 2;
		}
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return 1;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-energy: no kdos-energyd — `service "
				"kdos-energyd start`\n");
		close(fd);
		return 1;
	}

	char line[32];
	int n = snprintf(line, sizeof(line), "%s\n", verb);
	if (write(fd, line, (size_t)n) != n) {
		close(fd);
		return 1;
	}

	char buf[4096];
	ssize_t r;
	bool err = false, any = false;
	while ((r = read(fd, buf, sizeof(buf))) > 0) {
		if (!any && r >= 4 && !strncmp(buf, "err ", 4))
			err = true;
		any = true;
		fwrite(buf, 1, (size_t)r, err ? stderr : stdout);
	}
	close(fd);
	return err || !any ? 1 : 0;
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-energy");

	const char *me = strrchr(argv[0], '/');
	me = me ? me + 1 : argv[0];

	if (!strcmp(me, "kdos-energyd")) {
		bool json = false;
		const char *fix = NULL;
		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "--json"))
				json = true;
			else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
				fix = argv[++i];
			else {
				fprintf(stderr, "usage: kdos-energyd "
						"[--fixture DIR [--json]]\n");
				return 2;
			}
		}
		if (fix)
			return fixture(fix, json);
		return serve();
	}
	return client(argc, argv);
}

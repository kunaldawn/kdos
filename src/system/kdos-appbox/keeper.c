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
 * kdos-appbox keeper <box> — what a running box needs that outlives a launch.
 *
 * `kdos-appbox run` becomes `podman exec`, so nothing in it can hold a socket
 * open for the box. The keeper is one process per running box, started
 * detached by the first launch and gone with the box's container:
 *
 *   - THE BUS PROXY. xdg-dbus-proxy listens at <private>/dbus-proxy and
 *     forwards to the session bus only what proxy_policy() allows. The proxy
 *     is given one end of a socket pair and exits when the keeper does, so no
 *     proxy outlives the box it served.
 *   - THE SOCKET LINKS. The audio and accessibility sockets in the private
 *     directory are hard links, and a link goes on naming a dead socket when
 *     the daemon behind it restarts; the keeper watches the session's runtime
 *     directory and links again.
 *   - THE LAUNCH LEDGER. Every launch writes its pid into <box>.apps. The
 *     keeper watches each one exit: closed while the session is logging out,
 *     the line is kept and marked, because KWin closes every window before
 *     plasma-shutdown runs `session save` and the application would otherwise
 *     look as if somebody had closed it; closed any other time, the line goes.
 *
 * It ends when the container's conmon does, which is the one process whose
 * life is exactly the box's: `kdos-box stop`, `kdos-box gc` and the user
 * services' stop at the last logout all end there.
 */

#include "kdos-appbox.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define KEEPER_MAX_APPS 32

/*
 * THE POLICY. Names that a dedicated process owns get `--talk`; names that a
 * desktop process owns beside everything else it serves get `--call` rules
 * scoped to their interface and path. The difference is the proxy's own
 * rule: a unique name inherits the policy of EVERY well-known name its owner
 * holds, so `--talk=org.freedesktop.Notifications` would hand the box
 * plasmashell's unique name with talk rights — and with it
 * org.kde.PlasmaShell.evaluateScript, which the policy hides by name. Scoped
 * call rules are merged into the unique name the same way, which keeps a box
 * that looks up plasmashell's unique name to the Notifications interface.
 *
 * Hidden because nothing here names them: org.kde.KWin (ScreenShot2 answers
 * any caller, /Scripting loads scripts), org.kde.kglobalaccel,
 * org.kde.plasmashell, org.kde.klipper, org.kde.krunner, org.kde.ksmserver,
 * org.freedesktop.systemd1 — and every other name on the bus.
 */
int proxy_policy(const Profile *p, const char *const *owns, int nowns,
		 int a11y, char out[][192], int max)
{
	static const char *const fixed[] = {
		"--filter",
		"--talk=org.freedesktop.portal.*",
		"--call=org.freedesktop.Notifications="
			"org.freedesktop.Notifications.*@/org/freedesktop/Notifications",
		"--broadcast=org.freedesktop.Notifications="
			"org.freedesktop.Notifications.*@/org/freedesktop/Notifications",
		"--call=org.kde.StatusNotifierWatcher="
			"org.kde.StatusNotifierWatcher.*@/StatusNotifierWatcher",
		"--call=org.kde.StatusNotifierWatcher="
			"org.freedesktop.DBus.Properties.*@/StatusNotifierWatcher",
		"--broadcast=org.kde.StatusNotifierWatcher="
			"org.kde.StatusNotifierWatcher.*@/StatusNotifierWatcher",
		"--call=org.freedesktop.ScreenSaver="
			"org.freedesktop.ScreenSaver.*@/ScreenSaver",
		"--call=org.freedesktop.ScreenSaver="
			"org.freedesktop.ScreenSaver.*@/org/freedesktop/ScreenSaver",
		"--own=org.mpris.MediaPlayer2.*",
		NULL
	};
	int n = 0;

	for (int i = 0; fixed[i] && n < max; i++)
		snprintf(out[n++], 192, "%s", fixed[i]);
	if (a11y && n < max)
		snprintf(out[n++], 192, "--talk=org.a11y.Bus");
	if (profile_granted(p, "secrets") && n < max)
		snprintf(out[n++], 192, "--talk=org.freedesktop.secrets");
	/*
	 * THE APPLICATION'S OWN NAME. GApplication and KDBusService register
	 * the application id on the bus to stay single-instance, and one that
	 * cannot register exits; the id is the upstream desktop id, which only
	 * names a bus name when it is reverse-DNS.
	 */
	for (int i = 0; i < nowns && n + 2 <= max; i++) {
		const char *id = owns[i];

		if (!id || !strchr(id, '.') || strlen(id) > 160 ||
		    strspn(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
			       "vwxyz0123456789._-") != strlen(id) ||
		    (id[0] >= '0' && id[0] <= '9'))
			continue;
		snprintf(out[n++], 192, "--own=%s", id);
		snprintf(out[n++], 192, "--own=%s.*", id);
	}
	return n;
}

static int path_of(char *out, size_t n, const char *fmt, const char *a,
		   const char *b) __attribute__((format(printf, 3, 0)));
static int path_of(char *out, size_t n, const char *fmt, const char *a,
		   const char *b)
{
	int w = snprintf(out, n, fmt, a, b);

	return w < 0 || (size_t)w >= n ? -1 : 0;
}

static int keeper_lockpath(const char *box, char *out, size_t n)
{
	return path_of(out, n, "%s/kdos/box/.%s.keeper.lock", box_rundir(), box);
}

/* ── the proxy ─────────────────────────────────────────────────────────── */

static pid_t g_proxy;
static int g_sync = -1;

static void proxy_stop(void)
{
	if (g_sync >= 0)
		close(g_sync);
	g_sync = -1;
	if (g_proxy > 0) {
		kill(g_proxy, SIGTERM);
		waitpid(g_proxy, NULL, 0);
	}
	g_proxy = 0;
}

/*
 * The proxy gets one end of a socket pair as --fd: it writes `x` there once
 * it listens and exits when the other end closes. The arguments go through
 * --args on a pipe, so a policy with many owned names is not an argv a
 * `/proc/<pid>/cmdline` reader has to read through.
 */
/*
 * THE PROXY JOINS THE BOX'S SCOPE. Every call a box makes on the bus arrives
 * from the proxy's pid, and the desktop portal names a caller's application
 * from the `app-<launcher>-<id>-<n>.scope` its pid's cgroup is in; a proxy
 * left in the login's cgroup makes every box the host's anonymous caller,
 * and the portal's permission store records nothing per box. So the proxy is
 * moved into a leaf of its own under the scope the container was created in
 * (a scope that holds child cgroups holds no process itself). Only a scope
 * this program chose is used, and a refusal leaves the proxy where it is.
 */
static void proxy_into_scope(const char *box, long pid)
{
	KbArgv a = {0};
	char parent[1024] = "", dir[1200], procs[1300], num[32];
	int w;

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.HostConfig.CgroupParent}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, parent, sizeof(parent)) != 0)
		return;
	parent[strcspn(parent, "\r\n")] = '\0';
	if (!strstr(parent, "/delegated/app-kdos-") || strstr(parent, ".."))
		return;
	w = snprintf(dir, sizeof(dir), "/sys/fs/cgroup%s/kdos-dbus-proxy", parent);
	if (w < 0 || (size_t)w >= sizeof(dir))
		return;
	if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
		tracef("keeper %s: %s: %s", box, dir, strerror(errno));
		return;
	}
	snprintf(procs, sizeof(procs), "%s/cgroup.procs", dir);
	snprintf(num, sizeof(num), "%ld", pid);
	if (kb_write_file(procs, num) != 0)
		tracef("keeper %s: the proxy stays in the login's cgroup: %s",
		       box, strerror(errno));
}

static int proxy_start(const Profile *p, const char *pdir)
{
	char upstream[MAX_LINE], sock[MAX_LINE + 32];
	char policy[64][192];
	const char *owns[16];
	BoxLauncher *bl = kb_calloc(16, sizeof(*bl));
	int nl = box_launchers(p->name, bl, 16), np, sv[2], ap[2];
	KbBuf args = {0};
	char ready = 0;
	struct pollfd pf;
	pid_t pid;

	/* A throwaway box has no launcher of its own: it runs one of the box
	 * it was made from, and that launcher's name is the one it owns. */
	if (nl == 0 && throwaway_launcher(p->name, &bl[0]))
		nl = 1;
	for (int i = 0; i < nl; i++)
		owns[i] = bl[i].upstream;
	np = proxy_policy(p, owns, nl, box_a11y_wanted(), policy, 64);

	snprintf(upstream, sizeof(upstream), "unix:path=%s/bus", box_rundir());
	snprintf(sock, sizeof(sock), "%s/%s", pdir, BOX_PROXY_LEAF);
	unlink(sock);

	kb_buf_add(&args, upstream, strlen(upstream) + 1);
	kb_buf_add(&args, sock, strlen(sock) + 1);
	for (int i = 0; i < np; i++)
		kb_buf_add(&args, policy[i], strlen(policy[i]) + 1);
	free(bl);

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0 ||
	    pipe2(ap, O_CLOEXEC) != 0) {
		kb_buf_free(&args);
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		kb_buf_free(&args);
		return -1;
	}
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);

		if (null >= 0) {
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
		}
		/* Fixed numbers, past the three standard ones; dup2 clears
		 * close-on-exec on the copy. Each end is first moved above
		 * both targets: an end that already is 3 or 4 would otherwise
		 * keep its close-on-exec (dup2 onto itself changes nothing),
		 * or be overwritten by the other's dup2 before its own. */
		{
			int s = fcntl(sv[1], F_DUPFD_CLOEXEC, 10);
			int r = fcntl(ap[0], F_DUPFD_CLOEXEC, 10);

			if (s < 0 || r < 0 || dup2(s, 3) < 0 || dup2(r, 4) < 0)
				_exit(127);
		}
		kb_child_reset_signals();
		execlp("xdg-dbus-proxy", "xdg-dbus-proxy", "--fd=3", "--args=4",
		       (char *)NULL);
		_exit(127);
	}
	close(sv[1]);
	close(ap[0]);
	(void)!write(ap[1], args.p, args.n);
	close(ap[1]);
	kb_buf_free(&args);

	pf.fd = sv[0];
	pf.events = POLLIN;
	if (poll(&pf, 1, 5000) == 1 && read(sv[0], &ready, 1) == 1 &&
	    ready == 'x') {
		g_proxy = pid;
		g_sync = sv[0];
		proxy_into_scope(p->name, (long)pid);
		tracef("keeper %s: proxy %ld listening (%d policy words)",
		       p->name, (long)pid, np);
		return 0;
	}
	close(sv[0]);
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
	tracef("keeper %s: xdg-dbus-proxy did not come up", p->name);
	return -1;
}

/* ── the box's container ───────────────────────────────────────────────── */

static long conmon_pid(const char *box)
{
	KbArgv a = {0};
	char buf[64] = "";

	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "inspect");
	kb_argv_add(&a, "--type");
	kb_argv_add(&a, "container");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.State.Status}} {{.State.ConmonPid}}");
	kb_argv_add(&a, box);
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 ||
	    strncmp(buf, "running ", 8))
		return 0;
	return strtol(buf + 8, NULL, 10);
}

static int pid_fd(long pid)
{
#ifdef SYS_pidfd_open
	return (int)syscall(SYS_pidfd_open, (pid_t)pid, 0);
#else
	(void)pid;
	errno = ENOSYS;
	return -1;
#endif
}

/* ── the ledger ────────────────────────────────────────────────────────── */

typedef struct {
	long pid;
	int fd;		/* pidfd, or -1 where the kernel has none */
} App;

static App g_apps[KEEPER_MAX_APPS];
static int g_napps;

static int ledger_path(const char *box, char *out, size_t n)
{
	char sd[MAX_LINE];

	if (box_state_dir(sd, sizeof(sd)) != 0)
		return -1;
	return path_of(out, n, "%s/%s.apps", sd, box);
}

/*
 * WHETHER THE SESSION IS LOGGING OUT, asked of ksmserver when an application
 * exits. Its state is past Shutdown from the moment a logout starts until it
 * is cancelled, which covers KWin closing every window.
 */
static int session_ending(void)
{
	KbArgv a = {0};
	char out[64] = "";

	kb_argv_add(&a, "gdbus");
	kb_argv_add(&a, "call");
	kb_argv_add(&a, "--session");
	kb_argv_add(&a, "--timeout");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, "--dest");
	kb_argv_add(&a, "org.kde.ksmserver");
	kb_argv_add(&a, "--object-path");
	kb_argv_add(&a, "/KSMServer");
	kb_argv_add(&a, "--method");
	kb_argv_add(&a, "org.kde.KSMServerInterface.isShuttingDown");
	kb_argv_end(&a);
	return kb_run_capture(&a, out, sizeof(out)) == 0 && strstr(out, "true");
}

/*
 * Rewrite the ledger with the line for `pid` dropped, or marked as closed by
 * the logout. Under the file's own lock, which every appender takes too, so a
 * launch that lands during the rewrite is neither lost nor duplicated.
 */
static void ledger_settle(const char *box, long pid, int by_logout)
{
	char path[MAX_LINE];
	size_t len = 0;
	char *text;
	KbBuf out = {0};
	int fd;

	if (ledger_path(box, path, sizeof(path)) != 0)
		return;
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return;
	if (flock(fd, LOCK_EX) != 0) {
		close(fd);
		return;
	}
	text = kb_read_all(path, &len);
	for (char *line = text, *nl; line && *line; line = nl) {
		long lp;
		char *rest;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		lp = strtol(line, &rest, 10);
		if (lp != pid) {
			kb_buf_printf(&out, "%s\n", line);
			continue;
		}
		if (!by_logout)
			continue;
		/* `<pid> <start> run <rest>` becomes `<pid> <start>
		 * logout:<time> <rest>`. */
		{
			unsigned long long st = strtoull(rest, &rest, 10);
			char *sp;

			while (*rest == ' ')
				rest++;
			sp = strchr(rest, ' ');
			kb_buf_printf(&out, "%ld %llu logout:%ld %s\n", lp, st,
				      (long)time(NULL), sp ? sp + 1 : "");
		}
	}
	free(text);
	if (ftruncate(fd, 0) == 0 && out.n)
		(void)!pwrite(fd, out.p, out.n, 0);
	kb_buf_free(&out);
	close(fd);
}

/* Watch every running launch the ledger names that is not watched yet. */
static void ledger_watch(const char *box)
{
	char path[MAX_LINE];
	size_t len = 0;
	char *text;

	if (ledger_path(box, path, sizeof(path)) != 0)
		return;
	text = kb_read_all(path, &len);
	for (char *line = text, *nl; line && *line; line = nl) {
		long pid;
		char *rest;
		unsigned long long st;
		int known = 0;

		nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';
		pid = strtol(line, &rest, 10);
		st = strtoull(rest, &rest, 10);
		while (*rest == ' ')
			rest++;
		if (pid <= 1 || strncmp(rest, "run ", 4))
			continue;
		for (int i = 0; i < g_napps && !known; i++)
			known = g_apps[i].pid == pid;
		if (known || g_napps == KEEPER_MAX_APPS)
			continue;
		if (proc_start_time(pid) != st) {
			/* Gone before it was watched: nothing can say how it
			 * ended, and a guess would restore it. */
			ledger_settle(box, pid, 0);
			continue;
		}
		g_apps[g_napps].pid = pid;
		g_apps[g_napps].fd = pid_fd(pid);
		g_napps++;
	}
	free(text);
}

static void app_gone(const char *box, int i)
{
	ledger_settle(box, g_apps[i].pid, session_ending());
	if (g_apps[i].fd >= 0)
		close(g_apps[i].fd);
	g_apps[i] = g_apps[--g_napps];
}

/* ── the keeper ────────────────────────────────────────────────────────── */

static void watch_dirs(int in, const char *box, char *ledger_dir, size_t n)
{
	char d[MAX_LINE];

	inotify_add_watch(in, box_rundir(), IN_CREATE | IN_MOVED_TO);
	snprintf(d, sizeof(d), "%s/pulse", box_rundir());
	inotify_add_watch(in, d, IN_CREATE | IN_MOVED_TO);
	snprintf(d, sizeof(d), "%s/at-spi", box_rundir());
	inotify_add_watch(in, d, IN_CREATE | IN_MOVED_TO | IN_DELETE);
	if (box_state_dir(ledger_dir, n) == 0)
		inotify_add_watch(in, ledger_dir, IN_CLOSE_WRITE | IN_MODIFY);
	(void)box;
}

int cmd_keeper(const char *box)
{
	Profile p;
	char lockpath[MAX_LINE], pdir[MAX_LINE], ledger_dir[MAX_LINE];
	long conmon = 0;
	int lock, cfd = -1, in, restarts = 0, a11y = box_a11y_wanted();

	if (!box_name_ok(box) || box_private_dir(box, pdir, sizeof(pdir)) != 0 ||
	    keeper_lockpath(box, lockpath, sizeof(lockpath)) != 0)
		return 2;
	lock = kb_lock_file(lockpath, 1);
	if (lock < 0)
		return 0;
	profile_load(&p, box);
	signal(SIGPIPE, SIG_IGN);

	if (proxy_start(&p, pdir) != 0) {
		close(lock);
		return 1;
	}

	/* The launch starts the container before it starts the keeper, but a
	 * start can still be settling; a minute is the longest a launch waits
	 * for a box to come up. */
	for (int i = 0; i < 120 && !conmon; i++) {
		conmon = conmon_pid(box);
		if (!conmon)
			usleep(500 * 1000);
	}
	if (!conmon) {
		tracef("keeper %s: the box never ran", box);
		proxy_stop();
		close(lock);
		return 1;
	}
	cfd = pid_fd(conmon);

	in = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (in >= 0)
		watch_dirs(in, box, ledger_dir, sizeof(ledger_dir));
	rundir_link_sockets(box, p.audio, a11y);
	ledger_watch(box);
	tracef("keeper %s: watching conmon %ld", box, conmon);

	for (;;) {
		struct pollfd pf[3 + KEEPER_MAX_APPS];
		int n = 0, ci = -1, si = -1, ii = -1, ab, na, timeout = -1;

		if (cfd >= 0) {
			ci = n;
			pf[n].fd = cfd;
			pf[n++].events = POLLIN;
		} else {
			timeout = 2000;
		}
		if (g_sync >= 0) {
			si = n;
			pf[n].fd = g_sync;
			pf[n++].events = POLLIN;
		}
		if (in >= 0) {
			ii = n;
			pf[n].fd = in;
			pf[n++].events = POLLIN;
		}
		ab = n;
		na = g_napps;
		for (int i = 0; i < na; i++) {
			pf[n].fd = g_apps[i].fd;
			pf[n++].events = POLLIN;
			if (g_apps[i].fd < 0)
				timeout = 2000;
		}
		if (poll(pf, (nfds_t)n, timeout) < 0 && errno != EINTR)
			break;

		if ((ci >= 0 && pf[ci].revents) || (cfd < 0 && kill(conmon, 0) != 0))
			break;
		if (si >= 0 && pf[si].revents) {
			/* The proxy died under a running box: the box has no
			 * bus until it is back. Bounded, so a proxy that dies
			 * at once is not restarted at the speed of fork. */
			proxy_stop();
			if (++restarts > 5 || proxy_start(&p, pdir) != 0)
				tracef("keeper %s: the proxy is gone", box);
		}
		/* Downwards, because app_gone() moves the last entry into the
		 * slot it empties, and that entry has then been looked at. */
		for (int i = na - 1; i >= 0; i--) {
			int gone = g_apps[i].fd >= 0 ? pf[ab + i].revents != 0
						     : kill(g_apps[i].pid, 0) != 0;
			if (gone)
				app_gone(box, i);
		}
		if (ii >= 0 && pf[ii].revents) {
			char ev[4096];

			while (read(in, ev, sizeof(ev)) > 0)
				;
			rundir_link_sockets(box, p.audio, a11y);
			ledger_watch(box);
		}
	}

	tracef("keeper %s: the box stopped", box);
	proxy_stop();
	{
		char sock[MAX_LINE + 32];

		snprintf(sock, sizeof(sock), "%s/%s", pdir, BOX_PROXY_LEAF);
		unlink(sock);
	}
	box_state_clear(box);
	/* An ephemeral box was created --rm and podman has removed it; its
	 * stack goes too, which is the last of it. */
	if (p.persist == PERSIST_EPHEMERAL)
		pack_decompose(box);
	if (cfd >= 0)
		close(cfd);
	close(lock);
	return 0;
}

/*
 * A launch's half: a keeper running for this box, and its socket there.
 * The lock answers whether one runs — a socket file can be a dead keeper's —
 * and a stale socket goes before a new keeper is started, so a launch never
 * hands the box a bus that does not answer.
 */
int keeper_ensure(const char *box, char *why, size_t n)
{
	char lockpath[MAX_LINE], pdir[MAX_LINE], sock[MAX_LINE + 32];
	char self[MAX_LINE];
	ssize_t sl;
	int fd;

	if (box_private_dir(box, pdir, sizeof(pdir)) != 0 ||
	    keeper_lockpath(box, lockpath, sizeof(lockpath)) != 0) {
		snprintf(why, n, "the box's runtime paths do not fit");
		return -1;
	}
	snprintf(sock, sizeof(sock), "%s/%s", pdir, BOX_PROXY_LEAF);
	fd = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0) {
		unlink(sock);
		close(fd);
		fd = -1;
		if (!kb_have_prog("xdg-dbus-proxy")) {
			snprintf(why, n, "xdg-dbus-proxy is not installed, so "
				 "the box would have no session bus");
			return -1;
		}
		sl = readlink("/proc/self/exe", self, sizeof(self) - 1);
		if (sl <= 0) {
			snprintf(why, n, "cannot find this program to start "
				 "the box's keeper");
			return -1;
		}
		self[sl] = '\0';
		{
			KbArgv a = {0};

			kb_argv_add(&a, self);
			kb_argv_add(&a, "keeper");
			kb_argv_add(&a, box);
			kb_argv_end(&a);
			kb_run_detach(&a);
		}
	}
	if (fd >= 0)
		close(fd);
	for (int i = 0; i < 250; i++) {
		struct stat st;

		if (stat(sock, &st) == 0 && S_ISSOCK(st.st_mode))
			return 0;
		usleep(20 * 1000);
	}
	snprintf(why, n, "the box's bus proxy did not start within 5 s");
	return -1;
}

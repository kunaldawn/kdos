/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksvc / service — the service table and the supervisor
 *
 * The init.d scripts are shell: they are a service TABLE, and shell reads
 * better as one. The supervisor and its client are here, because three rules
 * they have to keep are the ones shell cannot.
 *
 * - **The supervisor LEADS its process group.** It calls setsid(), so `stop`'s
 *   `kill -- -$pid` reaches the daemon under it. A respawn loop in a
 *   backgrounded subshell stays in its script's process group, so the group
 *   kill addresses a group that pid does not lead, fails, and falls through to
 *   a plain kill of the supervisor — killing the watcher and ORPHANING the
 *   daemon, while `stop` reports success.
 *
 * - **The daemon command is a real argv**, never one word-split string. A
 *   command flattened into a string and re-expanded unquoted makes the
 *   splitting load-bearing, and a daemon path containing a space cannot then
 *   be expressed at all.
 *
 * - **A service name is checked before it is used.** It arrives from argv and
 *   selects a file; interpolated into a glob instead, `service start '*'`
 *   matches whatever it likes.
 *
 * - **One supervisor per name.** `supervise` on a name whose supervisor is
 *   alive starts nothing (see cmd_supervise): a second one would take over the
 *   pid file, and the first, with its daemon, could then never be stopped.
 *   Nor does it start beside a daemon an earlier supervisor left running.
 *
 * - **A supervisor never dies alone.** SIGTERM to the supervisor itself is
 *   passed on to its process group before it exits (see forward_term), so a
 *   `kill <supervisor pid>` ends the daemon too rather than leaving it running
 *   where no pid file names it.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "kdos-tools.h"

#define RESPAWN_DELAY 5

/* A run shorter than this that ends with a non-zero status is the next in a
 * row of failures; a longer one starts the count again at one. */
#define STABLE_SECS 60

/* A run shorter than this that ends with status 0 is a failure as well: the
 * daemon gave up at once (ModemManager exits 0 when its bus name is taken).
 * A longer one is a clean exit and ends the row. */
#define QUICK_SECS RESPAWN_DELAY

/* How long `stop` waits for the service's processes to leave after SIGTERM
 * before it sends SIGKILL, unless `supervise --stop-grace` names another. */
#define STOP_GRACE 5
#define MAX_GRACE 600

/* How often a supervisor started with `--restart-with` looks at the other
 * service's state file and at its own daemon, in milliseconds. */
#define WITH_POLL_MS 250

/* How long after the other service's daemon is back that this one is started
 * again: a bus that has just been exec'd has not yet opened its socket, and a
 * client started into that gap exits and waits out RESPAWN_DELAY. */
#define WITH_SETTLE_MS 1000

/* ──────────────────────────────────────────────────────────────────────── */

static char *pidfile(const char *name)
{
	char leaf[128];
	snprintf(leaf, sizeof(leaf), "%s.pid", name);
	return kb_path_join(RUN_DIR, leaf);
}

/*
 * THE PID FILE NAMES THE SUPERVISOR, AND THE SUPERVISOR OUTLIVES ITS DAEMON.
 * A daemon that aborts on start is respawned every RESPAWN_DELAY seconds by a
 * supervisor that never dies, so "the pid in the pid file is alive" is true of
 * a service that has not run for longer than a second since boot. The state
 * file is what the supervisor knows and the pid file cannot say:
 *
 *     up <daemon pid> <since> <failures> <last status> <stop grace>
 *     wait 0 <since> <failures> <last status> <stop grace>
 *
 * `up` while the daemon runs, `wait` during the delay before the next start.
 * <since> is CLOCK_BOOTTIME seconds, so a clock step at boot cannot age a run.
 * <failures> counts the failing exits in a row: a non-zero status from a run
 * shorter than STABLE_SECS, or status 0 from one shorter than QUICK_SECS. A
 * longer run that exits 0 is no failure, because a supervised `snooze` exits 0
 * after every job it runs (18_timers.sh) and waits at least until its next
 * slot first. <stop grace> is the `--stop-grace` the supervisor was started
 * with, which `stop` reads because it runs in another process. The file is
 * replaced by rename, so a reader never sees half a line.
 */
static char *statefile(const char *name)
{
	char leaf[160];
	snprintf(leaf, sizeof(leaf), "kdos-svc.%s.state", name);
	return kb_path_join(RUN_DIR, leaf);
}

static long boot_secs(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0)
		clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)ts.tv_sec;
}

static void write_state(const char *name, const char *phase, pid_t daemon,
			int fails, int last, int grace)
{
	char buf[112];
	snprintf(buf, sizeof(buf), "%s %d %ld %d %d %d\n", phase, (int)daemon,
		 boot_secs(), fails, last, grace);
	/* SIGTERM held off for the write: the supervisor exits in its handler,
	 * and one caught between the temporary file and the rename would leave
	 * the temporary file in /run for good. */
	sigset_t term, was;
	sigemptyset(&term);
	sigaddset(&term, SIGTERM);
	sigprocmask(SIG_BLOCK, &term, &was);
	char *p = statefile(name);
	kb_write_file_atomic(p, buf);
	free(p);
	sigprocmask(SIG_SETMASK, &was, NULL);
}

static void drop_state(const char *name)
{
	char *p = statefile(name);
	unlink(p);
	free(p);
}

static pid_t read_pid(const char *name)
{
	char *p = pidfile(name);
	char buf[32];
	pid_t pid = 0;
	if (kb_read_line_file(p, buf, sizeof(buf)) > 0)
		pid = (pid_t)atoi(buf);
	free(p);
	return pid > 1 ? pid : 0;
}

static int alive(pid_t pid)
{
	return pid > 1 && kill(pid, 0) == 0;
}

/*
 * Whether `pid` is the supervisor of `name`: its command line is
 * `ksvc supervise [--option VALUE]... <name> ...`. A pid file outlives a
 * supervisor killed by SIGKILL, and its number can by then belong to anything,
 * so a live pid alone does not make the service running.
 */
static int is_supervisor(pid_t pid, const char *name)
{
	if (!alive(pid))
		return 0;
	char path[48], buf[4096];
	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
	int n = kb_read_file(path, buf, sizeof(buf));
	if (n <= 0)
		return 0;
	const char *arg[64];
	int na = 0;
	for (int i = 0; i < n && na < 64; i += (int)strlen(buf + i) + 1)
		arg[na++] = buf + i;
	if (na < 3 || strcmp(arg[1], "supervise"))
		return 0;
	int i = 2;
	while (i + 1 < na && !strncmp(arg[i], "--", 2))
		i += 2;
	return i < na && !strcmp(arg[i], name);
}

/* The stop grace and the running daemon's pid from the state file: the
 * defaults (STOP_GRACE, none) when there is no file yet, or one written by a
 * ksvc that recorded no grace. *phase_up says whether the file exists and
 * reads `up`; NULL when the caller does not ask. */
static void read_state(const char *name, int *grace, pid_t *daemon,
		       int *phase_up)
{
	char line[112], phase[8];
	int dpid = 0, fails, last, g = 0;
	long since;
	*grace = STOP_GRACE;
	*daemon = 0;
	if (phase_up)
		*phase_up = 0;
	char *sp = statefile(name);
	if (kb_read_line_file(sp, line, sizeof(line)) > 0) {
		int got = sscanf(line, "%7s %d %ld %d %d %d", phase, &dpid,
				 &since, &fails, &last, &g);
		if (got >= 2 && !strcmp(phase, "up") && dpid > 1) {
			*daemon = (pid_t)dpid;
			if (phase_up)
				*phase_up = 1;
		}
		if (got == 6 && g >= 1 && g <= MAX_GRACE)
			*grace = g;
	}
	free(sp);
}

/* Whether `pid` is a process that has not yet exited, and its process group.
 * A zombie has exited: it waits only for its parent to collect it, and a
 * kill(pid, 0) that counted it would hold `stop` for the whole grace under a
 * parent slow to do that. */
static int running(pid_t pid, pid_t *pgrp)
{
	char path[48], buf[512];
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	if (pid <= 1 || kb_read_file(path, buf, sizeof(buf)) <= 0)
		return 0;
	char *r = strrchr(buf, ')');
	char state;
	int ppid, pg;
	if (!r || sscanf(r + 1, " %c %d %d", &state, &ppid, &pg) != 3)
		return 0;
	if (pgrp)
		*pgrp = (pid_t)pg;
	return state != 'Z' && state != 'X';
}

/*
 * The daemon left behind by a supervisor of `name` that has exited, or
 * 0. `sup` is the pid from the pid file. The daemon is the one the state file
 * names, and it counts only while it is still in the dead supervisor's process
 * group: a pid number is not reused while a group of that number has members,
 * so a daemon pid that has since been reused by something else cannot pass
 * the group test. SIGKILL to a supervisor, which nothing can catch, leaves
 * this; so does a supervisor whose daemon is still shutting down after the
 * SIGTERM the supervisor passed on.
 */
static pid_t orphan_daemon(const char *name, pid_t sup)
{
	if (sup <= 1 || is_supervisor(sup, name))
		return 0;
	int grace;
	pid_t d, pg;
	read_state(name, &grace, &d, NULL);
	return running(d, &pg) && pg == sup ? d : 0;
}

/*
 * The pid of the daemon `name`'s supervisor is running now, or 0 when it has
 * none: no supervisor, or one that is between a daemon's exit and its next
 * start. *restarting is set when there is a live supervisor and it is in that
 * gap, so its daemon is about to come back.
 */
static pid_t with_daemon(const char *name, int *restarting)
{
	pid_t sup = read_pid(name);
	*restarting = 0;
	if (!is_supervisor(sup, name))
		return 0;
	int grace, up;
	pid_t d;
	read_state(name, &grace, &d, &up);
	if (up && running(d, NULL))
		return d;
	*restarting = 1;
	return 0;
}

static void sleep_ms(int ms)
{
	struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
	while (nanosleep(&t, &t) < 0 && errno == EINTR)
		;
}

/*
 * A service name reaches this program from argv and is then used to build
 * paths. Anything that is not a plain name is refused rather than escaped:
 * there is nothing a slash or a glob character could legitimately mean here.
 */
static int name_ok(const char *s)
{
	if (!*s || strlen(s) > 64)
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-' &&
		    *c != '.')
			return 0;
	return 1;
}

/* NN_<something>.sh -> <something>, or NULL. */
static const char *script_name(const char *file)
{
	static char buf[128];
	size_t n = strlen(file);
	if (n < 7 || !isdigit((unsigned char)file[0]) ||
	    !isdigit((unsigned char)file[1]) || file[2] != '_' ||
	    strcmp(file + n - 3, ".sh"))
		return NULL;
	size_t len = n - 3 - 3;
	if (len >= sizeof(buf))
		return NULL;
	memcpy(buf, file + 3, len);
	buf[len] = 0;
	return buf;
}

/* Exact match first, then a substring: the order prefix and the suffix are
 * already stripped, so `ssh` reaches `70_sshd.sh` while an exactly-named
 * service can never be shadowed by a longer one that contains it. */
static char *find_script(const char *want)
{
	char **files = kb_listdir(INIT_DIR, NULL);
	char *hit = NULL;

	for (char **f = files; f && *f && !hit; f++) {
		const char *n = script_name(*f);
		if (n && !strcmp(n, want))
			hit = kb_path_join(INIT_DIR, *f);
	}
	for (char **f = files; f && *f && !hit; f++) {
		const char *n = script_name(*f);
		if (n && strstr(n, want))
			hit = kb_path_join(INIT_DIR, *f);
	}
	kb_strv_free(files);
	return hit;
}

static int run_script(const char *script, const char *action)
{
	KbArgv a = {0};
	kb_argv_add(&a, script);
	kb_argv_add(&a, action);
	kb_argv_end(&a);

	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));
	if (pid == 0) {
		execv(script, (char *const *)a.v);
		_exit(127);
	}
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

/*
 * A supervised daemon's stdout and stderr, and the supervisor's own Starting
 * and Exited lines, go to syslog under the service's name — the daemon
 * facility unless `--facility` names another, one message per line, at info —
 * and to /run/kdos-svc.<name>.log, which holds at most LOG_CAP bytes and one
 * previous generation beside it. syslog.conf routes by facility, so a daemon
 * whose own syslog() calls use authpriv needs its forwarded lines under
 * authpriv too, or they land in /var/log/messages instead of auth.log.
 *
 * Never the descriptors the supervisor was started with. Under rcS those are
 * the boot step's capture file on the /run tmpfs, open for as long as the
 * daemon lives: a chatty or crash-looping daemon fills RAM with it until the
 * reboot, and a daemon run in the foreground so that it logs to stderr
 * (chronyd -d) never reaches /var/log/messages at all.
 *
 * The file is what is left before syslogd is up and while it restarts; syslog
 * drops a message nobody is listening for.
 */
#define LOG_CAP (64 * 1024)

static const struct {
	const char *name;
	int facility;
} facilities[] = {
	{ "auth", LOG_AUTH },	  { "authpriv", LOG_AUTHPRIV },
	{ "daemon", LOG_DAEMON }, { "user", LOG_USER },
	{ "local0", LOG_LOCAL0 }, { "local1", LOG_LOCAL1 },
	{ "local2", LOG_LOCAL2 }, { "local3", LOG_LOCAL3 },
	{ "local4", LOG_LOCAL4 }, { "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 }, { "local7", LOG_LOCAL7 },
};

static int facility_by_name(const char *name)
{
	for (size_t i = 0; i < sizeof(facilities) / sizeof(facilities[0]); i++)
		if (!strcmp(facilities[i].name, name))
			return facilities[i].facility;
	return -1;
}

static void forward_lines(const char *name, int facility, int fd)
{
	char path[160], old[168];
	snprintf(path, sizeof(path), RUN_DIR "/kdos-svc.%s.log", name);
	snprintf(old, sizeof(old), "%s.old", path);

	openlog(name, LOG_NDELAY, facility);
	int out = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
	off_t size = out >= 0 ? lseek(out, 0, SEEK_END) : 0;

	FILE *in = fdopen(fd, "r");
	if (!in)
		_exit(1);
	char line[1024];
	while (fgets(line, sizeof(line), in)) {
		size_t n = strcspn(line, "\n");
		line[n] = 0;
		if (!n)
			continue;
		syslog(LOG_INFO, "%s", line);
		if (out < 0)
			continue;
		if (size + (off_t)n + 1 > LOG_CAP) {
			close(out);
			rename(path, old);
			out = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND |
					 O_CLOEXEC, 0640);
			size = 0;
			if (out < 0)
				continue;
		}
		line[n] = '\n';
		if (write(out, line, n + 1) == (ssize_t)(n + 1))
			size += (off_t)n + 1;
	}
	_exit(0);
}

/*
 * Point this process's stdio at a forwarder. The forwarder is forked from the
 * supervisor after setsid(), so it is in the supervisor's process group, and
 * it IGNORES that group's SIGTERM and SIGHUP and leaves on EOF instead. `stop`
 * signals the whole group at once: a forwarder that died with it would leave
 * the stopping daemon writing its shutdown lines into a pipe with no reader,
 * losing them from syslog and the log file and killing any daemon that does
 * not handle SIGPIPE partway through its cleanup. EOF comes when the last
 * write end closes — the supervisor's on its SIGTERM, the daemon's (and any
 * child's) on exit — and a SIGKILL of the group still ends it. The forwarder
 * is never exec'd, so the daemon inherits none of these dispositions. The
 * supervisor keeps the write end, so the forwarder outlives every respawn of
 * the daemon. A pipe or fork that fails leaves the inherited descriptors in
 * place: output going to the old place is better than a daemon that cannot
 * start.
 */
static void route_output(const char *name, int facility)
{
	int p[2];
	if (pipe(p) < 0)
		return;
	pid_t f = fork();
	if (f < 0) {
		close(p[0]);
		close(p[1]);
		return;
	}
	int null = open("/dev/null", O_RDWR);
	if (f == 0) {
		close(p[1]);
		if (null >= 0) {
			dup2(null, 0);
			dup2(null, 1);
			dup2(null, 2);
		}
		signal(SIGTERM, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		forward_lines(name, facility, p[0]);
	}
	close(p[0]);
	if (null >= 0) {
		dup2(null, 0);
		if (null > 2)
			close(null);
	}
	dup2(p[1], 1);
	dup2(p[1], 2);
	if (p[1] > 2)
		close(p[1]);
}

#define MAX_FINAL 8

/*
 * `--final-exit CODE` names an exit status that restarting cannot change: "this
 * machine is not one I can serve", "there is nothing here to watch", "my
 * configuration does not parse". A daemon that decides that only after probing
 * the hardware cannot be caught by a skip check in its script, and without
 * this the supervisor restarts it every RESPAWN_DELAY seconds for as long as
 * the machine is up. On that code the supervisor says so once, removes its pid
 * file and exits; every other status, and every death by signal, is still
 * respawned.
 *
 * `--facility NAME` is the syslog facility the forwarded output is sent
 * under (see forward_lines); daemon when it is not given.
 *
 * `--stop-grace SECS` is how long `stop` gives the service's processes to
 * exit after SIGTERM before it sends SIGKILL; STOP_GRACE when it is not
 * given. A daemon whose clean shutdown writes data out (a database flushing
 * its buffers) names a longer one.
 *
 * `--restart-with NAME` ties the daemon to another supervised service's
 * daemon. Whenever that daemon is not the one that was running when this one
 * started — it exited, was restarted, or the service was stopped — this
 * daemon is sent SIGTERM (SIGKILL after the stop grace) and started again
 * once the other is back, WITH_SETTLE_MS later. Before every start, a live
 * supervisor of NAME that is between two runs of its daemon is waited for.
 * NetworkManager passes `--restart-with dbus`: it does not exit when its bus
 * connection closes, and without this it would run on, holding no name on
 * the restarted bus, while `check` called it `[ OK ]`. Such a restart is no
 * failure and does not wait RESPAWN_DELAY.
 */

/*
 * A SIGTERM sent to the supervisor alone is passed on to its process group,
 * the daemon in it included, before the supervisor exits; ignored first, so
 * the group signal does not come back here (the log forwarder ignores it as
 * well). `stop` signals the whole group itself and creates the stop marker
 * first, and with the marker present this passes nothing on: a second SIGTERM
 * is not harmless to every daemon — llama-server takes one as "terminate
 * immediately". access() is async-signal-safe, and the path is built before
 * the handler is installed. A daemon that put itself in a group of its own is
 * signalled by its pid as well, as `stop` does; g_daemon is set only while a
 * daemon is unreaped, so the pid cannot belong to anything else.
 */
static char g_stopping[192];
static volatile pid_t g_daemon;

static char *stopmark(const char *name)
{
	char leaf[160];
	snprintf(leaf, sizeof(leaf), "kdos-svc.%s.stopping", name);
	return kb_path_join(RUN_DIR, leaf);
}

static void forward_term(int sig)
{
	(void)sig;
	signal(SIGTERM, SIG_IGN);
	if (access(g_stopping, F_OK) != 0) {
		kill(-getpgrp(), SIGTERM);
		pid_t d = g_daemon;
		if (d > 0 && getpgid(d) != getpgrp())
			kill(d, SIGTERM);
	}
	_exit(128 + SIGTERM);
}

static int cmd_supervise(int argc, char **argv)
{
	int final[MAX_FINAL];
	int nfinal = 0;
	int facility = LOG_DAEMON;
	int grace = STOP_GRACE;
	const char *with = NULL;

	while (argc >= 2) {
		if (!strcmp(argv[0], "--final-exit")) {
			char *end;
			long c = strtol(argv[1], &end, 10);
			if (*argv[1] == 0 || *end || c < 0 || c > 255)
				kb_die("--final-exit wants an exit status, "
				       "got '%s'", argv[1]);
			if (nfinal == MAX_FINAL)
				kb_die("at most %d --final-exit codes",
				       MAX_FINAL);
			final[nfinal++] = (int)c;
		} else if (!strcmp(argv[0], "--facility")) {
			facility = facility_by_name(argv[1]);
			if (facility < 0)
				kb_die("--facility wants a syslog facility "
				       "(auth, authpriv, daemon, user, "
				       "local0..local7), got '%s'", argv[1]);
		} else if (!strcmp(argv[0], "--stop-grace")) {
			char *end;
			long g = strtol(argv[1], &end, 10);
			if (*argv[1] == 0 || *end || g < 1 || g > MAX_GRACE)
				kb_die("--stop-grace wants seconds from 1 to "
				       "%d, got '%s'", MAX_GRACE, argv[1]);
			grace = (int)g;
		} else if (!strcmp(argv[0], "--restart-with")) {
			if (!name_ok(argv[1]))
				kb_die("--restart-with wants a service name, "
				       "got '%s'", argv[1]);
			with = argv[1];
		} else {
			break;
		}
		argc -= 2;
		argv += 2;
	}
	if (argc < 2)
		kb_die("usage: ksvc supervise [--final-exit CODE]... "
		       "[--facility NAME] [--stop-grace SECS] "
		       "[--restart-with NAME] <name> <command> [args...]");
	const char *name = argv[0];
	if (!name_ok(name))
		kb_die("bad service name '%s'", name);
	if (with && !strcmp(with, name))
		kb_die("--restart-with names the service itself");

	/* A second supervisor of a running name would overwrite the pid file
	 * with its own id: `stop` would then end the second alone, and the
	 * first, with its daemon, would run on with nothing left that can stop
	 * it. A second system bus is worse — it takes the socket, and every
	 * client already connected is left on a bus nobody else can reach.
	 * `service start` on a running service is therefore a no-op that says
	 * so, with status 0. */
	pid_t old = read_pid(name);
	if (is_supervisor(old, name)) {
		printf("[SKIP] %s: already supervised (pid %d)\n", name,
		       (int)old);
		return 0;
	}

	/* A daemon whose supervisor is gone is as much a running instance as
	 * one that is supervised. One still shutting down after its
	 * supervisor's SIGTERM is waited for, up to its stop grace; one that
	 * outlives that (its supervisor was SIGKILLed) is refused, because
	 * starting a second beside it is what the check above prevents. */
	pid_t left = orphan_daemon(name, old);
	if (left) {
		int g;
		pid_t d;
		read_state(name, &g, &d, NULL);
		for (int i = 0; i < g * 20 && orphan_daemon(name, old); i++)
			sleep_ms(50);
		if (orphan_daemon(name, old)) {
			printf("[FAIL] %s: daemon pid %d of a supervisor that "
			       "is gone is still running; `ksvc stop-supervised "
			       "%s` ends it\n", name, (int)left, name);
			return 1;
		}
	}

	/* A state file left by an earlier supervisor of this name would be
	 * read as this one's until its first start writes a new one, and a
	 * stop marker left by a `stop` that never finished would keep this
	 * one's SIGTERM from reaching its daemon. */
	drop_state(name);
	char *mark = stopmark(name);
	unlink(mark);
	free(mark);

	/* The child blocks on this pipe until the pid file exists, so a
	 * supervisor that stops at once removes the file its parent wrote
	 * rather than racing it and leaving a stale one behind. */
	int gate[2];
	if (pipe(gate) < 0)
		kb_die("pipe: %s", strerror(errno));

	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));

	if (pid == 0) {
		/* Its OWN session and process group. This is the whole fix:
		 * `ksvc stop` signals the group, and the group is exactly this
		 * supervisor, the daemon it spawns and their log forwarder —
		 * which ignores the SIGTERM and drains until the daemon has
		 * closed its end (see route_output). */
		setsid();

		close(gate[1]);
		char b;
		while (read(gate[0], &b, 1) < 0 && errno == EINTR)
			;
		close(gate[0]);

		route_output(name, facility);
		char *mark = stopmark(name);
		kb_strlcpy(g_stopping, mark, sizeof(g_stopping));
		free(mark);
		signal(SIGTERM, forward_term);

		int fails = 0, last = 0;
		for (;;) {
			pid_t with_pid = 0;
			if (with) {
				int restarting, waited = 0;
				while (with_pid = with_daemon(with, &restarting),
				       !with_pid && restarting) {
					waited = 1;
					sleep_ms(WITH_POLL_MS);
				}
				if (waited) {
					sleep_ms(WITH_SETTLE_MS);
					with_pid = with_daemon(with, &restarting);
				}
			}

			printf("[KDOS] (%s) Starting: %s\n", name, argv[1]);
			fflush(stdout);

			/* SIGTERM held off across the fork: the child would
			 * otherwise run forward_term until it resets the
			 * handler, and the supervisor would take it before
			 * g_daemon and the state file name the daemon. */
			sigset_t term, was;
			sigemptyset(&term);
			sigaddset(&term, SIGTERM);
			sigprocmask(SIG_BLOCK, &term, &was);
			long t0 = boot_secs();
			pid_t d = fork();
			if (d == 0) {
				signal(SIGTERM, SIG_DFL);
				sigprocmask(SIG_SETMASK, &was, NULL);
				execvp(argv[1], argv + 1);
				_exit(127);
			}
			int st = 0, bounced = 0;
			if (d > 0) {
				g_daemon = d;
				write_state(name, "up", d, fails, last, grace);
			} else {
				st = 127 << 8;
			}
			sigprocmask(SIG_SETMASK, &was, NULL);
			if (d > 0 && !with) {
				while (waitpid(d, &st, 0) < 0 && errno == EINTR)
					;
			}
			/* Polled rather than blocked on: the other service's
			 * state file is read between the looks at the daemon. */
			for (int ticks = 0; d > 0 && with; ticks++) {
				pid_t r = waitpid(d, &st, WNOHANG);
				if (r == d || (r < 0 && errno != EINTR))
					break;
				int restarting;
				if (!bounced &&
				    with_daemon(with, &restarting) != with_pid) {
					printf("[KDOS] (%s) %s restarted or "
					       "stopped: restarting %s\n", name,
					       with, name);
					fflush(stdout);
					kill(d, SIGTERM);
					bounced = 1;
					ticks = 0;
				} else if (bounced &&
					   ticks == grace * 1000 / WITH_POLL_MS) {
					kill(d, SIGKILL);
				}
				sleep_ms(WITH_POLL_MS);
			}
			g_daemon = 0;
			/* Status 0 for the wait: the daemon was ended, it did
			 * not fail, and `check` reads a non-zero last status
			 * in a wait as an exit to report. */
			if (bounced) {
				write_state(name, "wait", 0, fails, 0, grace);
				continue;
			}
			int rc = WIFEXITED(st) ? WEXITSTATUS(st)
					       : 128 + WTERMSIG(st);
			for (int i = 0; WIFEXITED(st) && i < nfinal; i++) {
				if (rc != final[i])
					continue;
				printf("[KDOS] (%s) Exited with code %d, a "
				       "final status: not restarting\n",
				       name, rc);
				fflush(stdout);
				char *pf = pidfile(name);
				unlink(pf);
				free(pf);
				drop_state(name);
				_exit(0);
			}
			long ran = boot_secs() - t0;
			if (rc == 0)
				fails = ran < QUICK_SECS ? fails + 1 : 0;
			else
				fails = ran < STABLE_SECS ? fails + 1 : 1;
			last = rc;
			write_state(name, "wait", 0, fails, last, grace);
			printf("[KDOS] (%s) Exited with code %d. Restarting in "
			       "%ds...\n", name, rc, RESPAWN_DELAY);
			fflush(stdout);
			sleep(RESPAWN_DELAY);
		}
	}

	close(gate[0]);
	char buf[32];
	snprintf(buf, sizeof(buf), "%d\n", (int)pid);
	char *p = pidfile(name);
	kb_write_file(p, buf);
	free(p);
	close(gate[1]);

	printf("[KDOS] (%s) Supervisor started (pid %d)\n", name, (int)pid);
	return 0;
}

/* Anything left of the service: the supervisor, a member of its process
 * group, or a daemon that left the group. */
static int service_left(pid_t pid, pid_t daemon)
{
	if (running(pid, NULL) || running(daemon, NULL))
		return 1;
	DIR *d = opendir("/proc");
	if (!d)
		return kill(-pid, 0) == 0;
	int left = 0;
	struct dirent *e;
	while (!left && (e = readdir(d))) {
		if (!isdigit((unsigned char)e->d_name[0]))
			continue;
		pid_t pg;
		left = running((pid_t)atoi(e->d_name), &pg) && pg == pid;
	}
	closedir(d);
	return left;
}

/*
 * SIGTERM to the supervisor's process group, then a wait of at most the stop
 * grace for every process in it to exit, polled every 50 ms, then SIGKILL to
 * whatever is left. The wait ends as soon as the group is empty, so a daemon
 * that exits at once costs no more than that; a fixed pause would cost every
 * service the same time at shutdown, and a daemon still flushing its data
 * when the pause ran out would lose it. The supervisor dies on the SIGTERM at
 * once, so it is the daemon's exit (and the log forwarder's, which follows
 * it) that the wait measures. The stop marker exists for the length of the
 * stop, so the supervisor does not pass the group a second SIGTERM (see
 * forward_term). A daemon that put itself in a group of its own is signalled
 * by its pid from the state file as well. A daemon whose supervisor is gone
 * (see orphan_daemon) is stopped the same way, through the group its
 * supervisor led.
 */
static int cmd_stop_supervised(const char *name)
{
	if (!name_ok(name))
		kb_die("bad service name '%s'", name);

	char *p = pidfile(name);
	pid_t pid = read_pid(name);

	if (!pid) {
		printf("[KDOS] (%s) Not running (no PID file)\n", name);
		free(p);
		return 1;
	}
	int sup = is_supervisor(pid, name);
	pid_t left = sup ? 0 : orphan_daemon(name, pid);
	if (!sup && !left) {
		printf("[KDOS] (%s) Process %d not found, removing stale PID "
		       "file\n", name, (int)pid);
		unlink(p);
		free(p);
		drop_state(name);
		return 1;
	}

	int grace;
	pid_t daemon;
	read_state(name, &grace, &daemon, NULL);
	pid_t dg;
	if (daemon && (!running(daemon, &dg) || dg == pid))
		daemon = 0;

	if (sup)
		printf("[KDOS] (%s) Stopping (pid %d)...\n", name, (int)pid);
	else
		printf("[KDOS] (%s) Supervisor %d is gone; stopping daemon "
		       "%d...\n", name, (int)pid, (int)left);
	char *mark = stopmark(name);
	kb_write_file(mark, "");
	if (kill(-pid, SIGTERM) < 0)
		kill(pid, SIGTERM);
	if (daemon)
		kill(daemon, SIGTERM);
	for (int i = 0; i < grace * 20 && service_left(pid, daemon); i++)
		sleep_ms(50);
	if (service_left(pid, daemon)) {
		printf("[KDOS] (%s) Still running after %ds, killing\n", name,
		       grace);
		if (kill(-pid, SIGKILL) < 0)
			kill(pid, SIGKILL);
		if (daemon)
			kill(daemon, SIGKILL);
	}
	unlink(mark);
	free(mark);
	unlink(p);
	free(p);
	drop_state(name);
	printf("[KDOS] (%s) Stopped\n", name);
	return 0;
}

/*
 * `[ OK ]` only while the daemon itself is running and not crash-looping.
 * A live supervisor whose daemon has just exited non-zero, or whose daemon
 * failed its last two starts in a row and is waiting for the next or has not
 * yet run STABLE_SECS this time, is `[FAIL]` with the exit status: from the
 * pid file alone each reads as a running service (see statefile). A
 * supervisor with no state file — one started by a ksvc that wrote none — is
 * judged by the pid file alone.
 */
static int cmd_check(const char *name)
{
	pid_t pid = name_ok(name) ? read_pid(name) : 0;
	if (!is_supervisor(pid, name)) {
		pid_t left = orphan_daemon(name, pid);
		if (left) {
			printf("[FAIL] %s  (supervisor gone; daemon pid %d "
			       "still running)\n", name, (int)left);
			return 1;
		}
		printf("[DOWN] %s\n", name);
		return 1;
	}

	char line[96], phase[8];
	int dpid = 0, fails = 0, last = 0;
	long since = 0;
	char *sp = statefile(name);
	int have = kb_read_line_file(sp, line, sizeof(line)) > 0 &&
		   sscanf(line, "%7s %d %ld %d %d", phase, &dpid, &since,
			  &fails, &last) == 5;
	free(sp);

	if (have && !strcmp(phase, "wait") && (last != 0 || fails > 1)) {
		if (fails > 1)
			printf("[FAIL] %s  (exited with code %d, %d failed "
			       "starts in a row; restarting)\n",
			       name, last, fails);
		else
			printf("[FAIL] %s  (exited with code %d; restarting)\n",
			       name, last);
		return 1;
	}
	if (have && !strcmp(phase, "up") && fails > 1 &&
	    boot_secs() - since < STABLE_SECS) {
		printf("[FAIL] %s  (crash loop: %d failed starts in a row, "
		       "last exit code %d; pid %d)\n",
		       name, fails, last, (int)pid);
		return 1;
	}
	printf("[ OK ] %s  (pid %d)\n", name, (int)pid);
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int is_disabled(const char *name)
{
	char *p = kb_path_join(DISABLED_DIR, name);
	int off = kb_path_exists(p);
	free(p);
	return off;
}

/* The columns before the status in a `list` row: two spaces, the name in
 * twenty, a space, the tag in twelve, a space. */
#define LIST_PREFIX 36

static int list_width(void)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col)
		return ws.ws_col;
	return 0;
}

/*
 * One row's STATUS column from what the script's `status` printed: the first
 * line that is not blank, then ` (+N more: service status <name>)` when more
 * lines follow, or `—` when there is none. A script's status is free-form —
 * nothing, or the several lines of `tlp-stat -s` or of two supervised daemons
 * — and written straight to the terminal it either joins the next row or
 * spills into it. On a terminal the line is cut to the width left after the
 * prefix, at a character boundary, so that a row is always one line.
 */
static void list_status(const char *name, const char *out, int width)
{
	const char *first = NULL;
	size_t flen = 0;
	int more = 0;
	for (const char *l = out; *l;) {
		const char *e = strchr(l, '\n');
		size_t n = e ? (size_t)(e - l) : strlen(l);
		size_t a = 0, b = n;
		while (a < b && isspace((unsigned char)l[a]))
			a++;
		while (b > a && isspace((unsigned char)l[b - 1]))
			b--;
		if (b > a) {
			if (!first) {
				first = l + a;
				flen = b - a;
			} else {
				more++;
			}
		}
		if (!e)
			break;
		l = e + 1;
	}
	if (!first) {
		printf("—\n");
		return;
	}
	char tail[192] = "";
	if (more)
		snprintf(tail, sizeof(tail), " (+%d more: service status %s)",
			 more, name);
	if (width > LIST_PREFIX) {
		size_t room = (size_t)(width - LIST_PREFIX - 1);
		size_t tl = strlen(tail);
		if (tl > room) {
			tail[0] = 0;
			tl = 0;
		}
		size_t cps = 0, i = 0;
		while (i < flen && cps < room - tl) {
			i++;
			while (i < flen && ((unsigned char)first[i] & 0xC0) ==
						   0x80)
				i++;
			cps++;
		}
		flen = i;
	}
	printf("%.*s%s\n", (int)flen, first, tail);
}

static int cmd_list(void)
{
	int width = list_width();
	printf("\nKDOS Services\n"
	       "─────────────────────────────────────────────────────────\n"
	       "  %-20s %-12s %s\n"
	       "─────────────────────────────────────────────────────────\n",
	       "SERVICE", "AUTOSTART", "STATUS");

	char **files = kb_listdir(INIT_DIR, NULL);
	for (char **f = files; f && *f; f++) {
		const char *n = script_name(*f);
		if (!n)
			continue;
		char *path = kb_path_join(INIT_DIR, *f);
		if (access(path, X_OK) != 0) {
			free(path);
			continue;
		}
		char name[128];
		kb_strlcpy(name, n, sizeof(name));

		char tag[24];
		snprintf(tag, sizeof(tag), "[%s]",
			 is_disabled(name) ? "DISABLED" : "ENABLED");
		/* The script's own status output is the whole answer, and its
		 * exit status adds nothing: a DOWN service exits non-zero, so a
		 * second line keyed on that status would only repeat the first
		 * in different words. */
		KbArgv a = {0};
		kb_argv_add(&a, path);
		kb_argv_add(&a, "status");
		kb_argv_end(&a);
		KbBuf out = {0};
		kb_run_capture_buf(&a, &out);
		printf("  %-20s %-12s ", name, tag);
		list_status(name, out.p ? out.p : "", width);
		kb_buf_free(&out);
		free(path);
	}
	kb_strv_free(files);
	printf("\n");
	return 0;
}

static void usage(void)
{
	printf("Usage: service <command> [service-name]\n"
	       "\n"
	       "Commands:\n"
	       "  list              List all available services and their status\n"
	       "  status <name>     Show status of a specific service\n"
	       "  start  <name>     Start a service\n"
	       "  stop   <name>     Stop a service\n"
	       "  restart <name>    Restart a service (stop then start)\n"
	       "  enable <name>     Enable a service to start at boot\n"
	       "  disable <name>    Disable a service from starting at boot\n"
	       "  help              Show this help message\n"
	       "\n"
	       "Examples:\n"
	       "  service list\n"
	       "  service status sshd\n"
	       "  service disable bluetooth\n"
	       "  service enable network\n");
}

int ksvc_main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}
	const char *cmd = argv[1];

	/* The three the init.d scripts call through service_helper. */
	if (!strcmp(cmd, "supervise"))
		return cmd_supervise(argc - 2, argv + 2);
	if (!strcmp(cmd, "stop-supervised")) {
		if (argc < 3)
			kb_die("usage: ksvc stop-supervised <name>");
		return cmd_stop_supervised(argv[2]);
	}
	if (!strcmp(cmd, "check")) {
		if (argc < 3)
			kb_die("usage: ksvc check <name>");
		return cmd_check(argv[2]);
	}

	if (!strcmp(cmd, "list"))
		return cmd_list();
	if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") ||
	    !strcmp(cmd, "-h")) {
		usage();
		return 0;
	}

	if (argc < 3) {
		printf("Usage: service %s <name>\n", cmd);
		return 1;
	}
	const char *name = argv[2];
	if (!name_ok(name)) {
		printf("Error: bad service name '%s'\n", name);
		return 1;
	}

	if (!strcmp(cmd, "enable") || !strcmp(cmd, "disable")) {
		char *script = find_script(name);
		if (!script) {
			printf("Error: Service '%s' not found\n", name);
			return 1;
		}
		free(script);
		char *flag = kb_path_join(DISABLED_DIR, name);
		if (!strcmp(cmd, "enable")) {
			unlink(flag);
			printf("Service '%s' enabled.\n", name);
		} else {
			kb_mkdir_p(DISABLED_DIR);
			kb_write_file(flag, "");
			printf("Service '%s' disabled.\n", name);
		}
		free(flag);
		return 0;
	}

	if (strcmp(cmd, "status") && strcmp(cmd, "start") &&
	    strcmp(cmd, "stop") && strcmp(cmd, "restart")) {
		printf("Error: Unknown command '%s'\n", cmd);
		usage();
		return 1;
	}

	char *script = find_script(name);
	if (!script) {
		printf("Error: Service '%s' not found\n", name);
		return 1;
	}

	int rc;
	if (!strcmp(cmd, "restart")) {
		printf("Restarting %s...\n", name);
		run_script(script, "stop");
		sleep(1);
		rc = run_script(script, "start");
	} else {
		rc = run_script(script, cmd);
	}
	free(script);
	return rc;
}

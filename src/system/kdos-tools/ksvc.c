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
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "kdos-tools.h"

#define RESPAWN_DELAY 5

/* ──────────────────────────────────────────────────────────────────────── */

static char *pidfile(const char *name)
{
	char leaf[128];
	snprintf(leaf, sizeof(leaf), "%s.pid", name);
	return kb_path_join(RUN_DIR, leaf);
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
 * already stripped, so `ssh` reaches `20_sshd.sh` while an exactly-named
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
 * facility, one message per line — and to /run/kdos-svc.<name>.log, which
 * holds at most LOG_CAP bytes and one previous generation beside it.
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

static void forward_lines(const char *name, int fd)
{
	char path[160], old[168];
	snprintf(path, sizeof(path), RUN_DIR "/kdos-svc.%s.log", name);
	snprintf(old, sizeof(old), "%s.old", path);

	openlog(name, LOG_NDELAY, LOG_DAEMON);
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
static void route_output(const char *name)
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
		forward_lines(name, p[0]);
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
 */
static int cmd_supervise(int argc, char **argv)
{
	int final[MAX_FINAL];
	int nfinal = 0;

	while (argc >= 2 && !strcmp(argv[0], "--final-exit")) {
		char *end;
		long c = strtol(argv[1], &end, 10);
		if (*argv[1] == 0 || *end || c < 0 || c > 255)
			kb_die("--final-exit wants an exit status, got '%s'",
			       argv[1]);
		if (nfinal == MAX_FINAL)
			kb_die("at most %d --final-exit codes", MAX_FINAL);
		final[nfinal++] = (int)c;
		argc -= 2;
		argv += 2;
	}
	if (argc < 2)
		kb_die("usage: ksvc supervise [--final-exit CODE]... <name> "
		       "<command> [args...]");
	const char *name = argv[0];
	if (!name_ok(name))
		kb_die("bad service name '%s'", name);

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

		route_output(name);

		for (;;) {
			printf("[KDOS] (%s) Starting: %s\n", name, argv[1]);
			fflush(stdout);

			pid_t d = fork();
			if (d == 0) {
				execvp(argv[1], argv + 1);
				_exit(127);
			}
			int st = 0;
			if (d > 0)
				while (waitpid(d, &st, 0) < 0 && errno == EINTR)
					;
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
				_exit(0);
			}
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
	if (!alive(pid)) {
		printf("[KDOS] (%s) Process %d not found, removing stale PID "
		       "file\n", name, (int)pid);
		unlink(p);
		free(p);
		return 1;
	}

	printf("[KDOS] (%s) Stopping (pid %d)...\n", name, (int)pid);
	/* The group, so the daemon goes with its supervisor. */
	if (kill(-pid, SIGTERM) < 0)
		kill(pid, SIGTERM);
	sleep(1);
	if (alive(pid)) {
		if (kill(-pid, SIGKILL) < 0)
			kill(pid, SIGKILL);
	}
	unlink(p);
	free(p);
	printf("[KDOS] (%s) Stopped\n", name);
	return 0;
}

static int cmd_check(const char *name)
{
	pid_t pid = name_ok(name) ? read_pid(name) : 0;
	if (alive(pid)) {
		printf("[ OK ] %s  (pid %d)\n", name, (int)pid);
		return 0;
	}
	printf("[DOWN] %s\n", name);
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int is_disabled(const char *name)
{
	char *p = kb_path_join(DISABLED_DIR, name);
	int off = kb_path_exists(p);
	free(p);
	return off;
}

static int cmd_list(void)
{
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
		printf("  %-20s %-12s ", name, tag);
		fflush(stdout);
		/* The script's own status line is the whole answer. The shell
		 * version appended "[UNKN] <name>" whenever the script exited
		 * non-zero — which is exactly what a DOWN service does, so
		 * every stopped service printed two lines saying different
		 * things. */
		run_script(path, "status");
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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksvc / service — the service table and the supervisor
 *
 * This was fs/usr/sbin/service plus the supervise/stop_service/check_status
 * functions in fs/etc/init.d/service_helper. The init.d scripts are still
 * shell: they are a service TABLE, and shell reads better as one. What moved
 * here is the part that was wrong in shell.
 *
 * Two bugs the shell version had, both fixed by this file existing:
 *
 * - **The supervisor was never a process-group leader.** `supervise` put the
 *   respawn loop in a backgrounded subshell and recorded ITS pid, but a
 *   subshell in a non-interactive shell stays in the script's process group.
 *   `stop_service` then did `kill -- -$pid`, which addressed a group that pid
 *   did not lead, failed, and fell through to a plain `kill $pid` — killing
 *   the supervisor and ORPHANING the daemon it was watching. `service stop`
 *   reported success and left the daemon running. Here the supervisor calls
 *   setsid(), so it genuinely leads its group and the group kill reaches the
 *   daemon.
 *
 * - **The daemon command was one word-split string.** `local command="$@"`
 *   flattened the arguments and `$command` was then expanded unquoted, so the
 *   splitting was load-bearing and a daemon path containing a space could not
 *   be expressed. ksvc takes a real argv.
 *
 * And one hole closed: `find_service` interpolated the user's argument into a
 * GLOB, so `service start '*'` matched whatever it liked. A service name here
 * is checked before it is used.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* Exact match first, then a unique substring, which is what the two globs in
 * the shell version amounted to. */
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

static int cmd_supervise(int argc, char **argv)
{
	if (argc < 2)
		kb_die("usage: ksvc supervise <name> <command> [args...]");
	const char *name = argv[0];
	if (!name_ok(name))
		kb_die("bad service name '%s'", name);

	pid_t pid = fork();
	if (pid < 0)
		kb_die("fork: %s", strerror(errno));

	if (pid == 0) {
		/* Its OWN session and process group. This is the whole fix:
		 * `ksvc stop` kills the group, and the group is exactly this
		 * supervisor plus the daemon it spawns. */
		setsid();

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
			printf("[KDOS] (%s) Exited with code %d. Restarting in "
			       "%ds...\n", name, rc, RESPAWN_DELAY);
			fflush(stdout);
			sleep(RESPAWN_DELAY);
		}
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "%d\n", (int)pid);
	char *p = pidfile(name);
	kb_write_file(p, buf);
	free(p);

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

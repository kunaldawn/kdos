/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   run.c — a match's vector, executed
 *
 * THERE IS NO SHELL ON THIS PATH. The vector match.c and library.c built is
 * handed to execvp() element for element (libkbase's KbArgv). A box name, a
 * path typed after `why`, a search for Discover and a route's words from a
 * user's menu.conf are each one argv element, so `; rm -rf ~` reaches the
 * program as an argument it will not find, never as a command.
 *
 * Konsole is the one wrapper: `konsole --hold -e <vector>`. With two or more
 * words after `-e` Konsole execs them as they are; only a single word with
 * a space in it is ever split, and every vector here has at least two.
 *
 * NOTHING RUN HERE IS WAITED FOR by the runner. KRunner sends Run without
 * waiting for an answer and closes; a runner blocked on a child would answer
 * the next Match late. A kdos verb run in the background is watched by a
 * detached process of its own, which posts its error output as a
 * notification if it fails — `kdos theme` refused by polkit must not look
 * like a key that did nothing. Applications, pages and links are started
 * fully detached: their exit says nothing about the click.
 *
 * THREE ROWS RUN NO PROGRAM. The local model's answer goes onto the clipboard
 * through Klipper's setClipboardContents; a question still being answered,
 * and `lib <words>`, open KRunner again through querySingleRunner on one
 * runner alone (`kdos` for the question, `kdos-library` for the words), once
 * KRunner has closed the window the Run came from. Each is a D-Bus call from
 * a detached child on a connection of its own, so the runner's loop never
 * waits on Plasma.
 *
 * THE ACTIVATION TOKEN KRunner hands over just before Run goes to the child
 * as XDG_ACTIVATION_TOKEN, which is how the window that opens takes focus on
 * Wayland. It is used once.
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* basu on the target; libsystemd's sd-bus is the same API. A host with
 * neither builds the matcher's tests, which never report. */
#if __has_include(<basu/sd-bus.h>)
#  include <basu/sd-bus.h>
#  define RN_HAVE_BUS 1
#elif __has_include(<systemd/sd-bus.h>)
#  include <systemd/sd-bus.h>
#  define RN_HAVE_BUS 1
#endif

#include "runner.h"

/* How long a re-query waits after Run for KRunner's window to close. */
#define RN_REQUERY_DELAY_MS 500

static char g_token[512];

void rn_run_token(const char *token)
{
	/* A token is a printable opaque string; anything else is not one and
	 * would go into the child's environment unchecked. */
	for (const char *p = token; p && *p; p++)
		if ((unsigned char)*p < 0x21 || (unsigned char)*p > 0x7e)
			return;
	kb_strlcpy(g_token, token ? token : "", sizeof(g_token));
}

static void build(const RnMatch *m, int in_terminal, KbArgv *a)
{
	memset(a, 0, sizeof(*a));
	if (in_terminal) {
		kb_argv_add(a, "konsole");
		kb_argv_add(a, "--hold");
		kb_argv_add(a, "-e");
	}
	for (int i = 0; i < m->argc; i++)
		kb_argv_add(a, m->argv[i]);
	kb_argv_end(a);
}

void rn_run_describe(const RnMatch *m, int in_terminal, KbBuf *out)
{
	KbArgv a;

	build(m, in_terminal, &a);
	for (int i = 0; i < a.n && a.v[i]; i++) {
		kb_buf_str(out, a.v[i]);
		kb_buf_str(out, "\n");
	}
}

/* A kdos verb's failure, as a notification. Runs in the watcher, on a bus
 * connection of its own. */
static void report(const RnMatch *m, int status, const char *err)
{
#ifdef RN_HAVE_BUS
	sd_bus *bus = NULL;
	sd_bus_message *call = NULL;
	char summary[256], body[512];
	size_t o = 0;

	snprintf(summary, sizeof(summary), "%s", m->text);
	/* The last lines of what it printed: the refusal is at the end. */
	{
		size_t n = strlen(err);
		const char *from = err;

		while (n && (err[n - 1] == '\n' || err[n - 1] == ' '))
			n--;
		if (n > 400) {
			from = err + n - 400;
			while (*from && *from != '\n' && from < err + n)
				from++;
		}
		for (const char *p = from; p < err + n && o + 1 < sizeof(body); p++)
			body[o++] = *p == '\t' ? ' ' : *p;
		body[o] = '\0';
	}
	if (!o)
		snprintf(body, sizeof(body), "It stopped with status %d.", status);
	if (sd_bus_open_user(&bus) < 0)
		return;
	if (sd_bus_message_new_method_call(bus, &call,
					   "org.freedesktop.Notifications",
					   "/org/freedesktop/Notifications",
					   "org.freedesktop.Notifications",
					   "Notify") >= 0 &&
	    sd_bus_message_append(call, "susssas", "KDOS", 0u, "dialog-error",
				  summary, body, 0) >= 0 &&
	    sd_bus_message_open_container(call, 'a', "{sv}") >= 0 &&
	    sd_bus_message_append(call, "{sv}", "desktop-entry", "s",
				  "org.kde.krunner") >= 0 &&
	    sd_bus_message_close_container(call) >= 0 &&
	    sd_bus_message_append(call, "i", -1) >= 0)
		sd_bus_call(bus, call, 5ULL * 1000 * 1000, NULL, NULL);
	sd_bus_message_unref(call);
	sd_bus_flush_close_unref(bus);
#else
	(void)m;
	(void)status;
	(void)err;
#endif
}

/* A call that carries strings and wants no answer but success, from a
 * detached child, made `delay_ms` after Run; a failure becomes a
 * notification. */
static void bus_call(int delay_ms, const char *what, const char *dest,
		     const char *path, const char *iface, const char *member,
		     const char *a1, const char *a2)
{
#ifdef RN_HAVE_BUS
	pid_t pid = fork();

	if (pid < 0)
		return;
	if (pid == 0) {
		if (fork() == 0) {
			sd_bus *bus = NULL;
			sd_bus_error e = SD_BUS_ERROR_NULL;
			int r;

			setsid();
			kb_child_reset_signals();
			if (delay_ms > 0) {
				struct timespec ts = { delay_ms / 1000,
						       (long)(delay_ms % 1000) *
							       1000000L };

				while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
					;
			}
			if (sd_bus_open_user(&bus) < 0)
				_exit(1);
			r = a2 ? sd_bus_call_method(bus, dest, path, iface, member,
						    &e, NULL, "ss", a1, a2)
			       : sd_bus_call_method(bus, dest, path, iface, member,
						    &e, NULL, "s", a1);
			if (r < 0) {
				static RnMatch said;

				kb_strlcpy(said.text, what, sizeof(said.text));
				report(&said, 1, e.message ? e.message
					   : "nothing answered on the bus");
			}
			sd_bus_error_free(&e);
			sd_bus_flush_close_unref(bus);
			_exit(r < 0);
		}
		_exit(0);
	}
	while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
		;
#else
	(void)delay_ms;
	(void)what;
	(void)dest;
	(void)path;
	(void)iface;
	(void)member;
	(void)a1;
	(void)a2;
#endif
}

static int watched(const RnMatch *m, int in_terminal)
{
	return !in_terminal && m->how == RN_RUN_DETACH && m->argc &&
	       (!strcmp(m->argv[0], "kdos") || !strcmp(m->argv[0], "kdos-box"));
}

int rn_run(const RnMatch *m, int in_terminal)
{
	KbArgv a;
	char env[600];
	const char *envv[1];
	int nenv = 0;
	pid_t pid;

	if (!m || !m->argc)
		return -1;
	if (m->how == RN_RUN_COPY) {
		g_token[0] = '\0';
		bus_call(0, "Copy the answer", "org.kde.klipper", "/klipper",
			 "org.kde.klipper.klipper", "setClipboardContents",
			 m->argv[0], NULL);
		return 0;
	}
	if (m->how == RN_RUN_REQUERY) {
		if (m->argc < 2)
			return -1;
		g_token[0] = '\0';
		/* KRunner closes its window once Run is sent, and a
		 * querySingleRunner that arrives before the close is shown
		 * and then hidden by it: the call waits the close out. */
		bus_call(RN_REQUERY_DELAY_MS, "Open KRunner again",
			 "org.kde.krunner", "/App", "org.kde.krunner.App",
			 "querySingleRunner", m->argv[0], m->argv[1]);
		return 0;
	}
	if (m->how == RN_RUN_TERM)
		in_terminal = 1;
	build(m, in_terminal, &a);
	if (g_token[0]) {
		snprintf(env, sizeof(env), "XDG_ACTIVATION_TOKEN=%s", g_token);
		envv[nenv++] = env;
	}
	g_token[0] = '\0';

	if (!watched(m, in_terminal)) {
		if (nenv)
			setenv("XDG_ACTIVATION_TOKEN", env + 21, 1);
		kb_run_detach(&a);
		unsetenv("XDG_ACTIVATION_TOKEN");
		return 0;
	}

	/* Fork twice so the watcher belongs to init and the runner never has
	 * a child to reap. */
	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		pid_t w = fork();

		if (w == 0) {
			char err[4096];
			int st;

			setsid();
			kb_child_reset_signals();
			st = kb_run_feed_env(&a, envv, nenv, NULL, 0, err,
					     sizeof(err));
			if (st != 0)
				report(m, st, err);
			_exit(0);
		}
		_exit(w < 0);
	}
	while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
		;
	return 0;
}

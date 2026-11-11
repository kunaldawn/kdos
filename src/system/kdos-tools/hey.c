/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos hey — the window manager, from a shell prompt
 *
 *   kdos hey list [--json]          every window: its app id, its box, its title
 *   kdos hey boxes                  the boxes with a window open
 *   kdos hey run <action> <id>      close, activate, minimize, maximize… one
 *   kdos hey outputs [--json]       the screens
 *
 * Haiku's `hey` is the ancestor: the window manager answers questions from the
 * command line, so a window is something a script can find and act on. KWin
 * answers over its window-list protocol (windows.c); the screens are
 * kscreen's.
 *
 * WHICH BOX A WINDOW IS IN, by its process: a box's cgroup scope names it
 * (app-kdos-<box>-<n>.scope, with each `-` of the box written `\x2d`), where
 * the box has one; a process in a mount namespace other than this program's
 * whose environment carries KDOS_BOX=<box>, which kdos-appbox puts in every
 * box's container and which podman passes to every exec, is that box's; and
 * failing both the window's app id is a running container's name or begins
 * with it and a dot — the launcher ids a box's windows carry. The longest
 * such name wins, so `app.x` does not claim `app.xyz`'s windows. A throwaway
 * box is found by its scope or its KDOS_BOX (the first two rules) alone,
 * because its windows carry its application's launcher id, not the box's.
 *
 * `outputs` is kscreen-doctor's, which colours its text whether or not it
 * writes to a terminal; into anything else it is captured and written with
 * every CSI escape taken out, so a pipe gets plain text.
 *
 * `boxes` is what `kdos-box gc` reads to leave a box with a window running:
 * exit 0 with an answer, an empty one included, and 1 when there is no session
 * to ask — which gc takes as "cannot tell" and stops nothing.
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kproc.h"

/* ── which box ─────────────────────────────────────────────────────────── */

/* KDOS_BOX=<box> from the process's environment, for a process in a mount
 * namespace other than this program's. The namespace test keeps a host
 * process that merely inherited the variable — a shell `kdos-box enter`
 * left, a program started from one — from being taken for the box's. */
static int box_of_environ(long pid, char *out, size_t n)
{
	static const char key[] = "KDOS_BOX=";
	char path[64];
	struct stat self, st;
	size_t len = 0;
	char *env;
	int rc = -1;

	snprintf(path, sizeof(path), "/proc/%ld/ns/mnt", pid);
	if (stat("/proc/self/ns/mnt", &self) != 0 || stat(path, &st) != 0 ||
	    (st.st_ino == self.st_ino && st.st_dev == self.st_dev))
		return -1;
	snprintf(path, sizeof(path), "/proc/%ld/environ", pid);
	env = kb_read_all(path, &len);
	for (size_t o = 0; env && o < len; o += strlen(env + o) + 1) {
		const char *v = env + o;

		if (strncmp(v, key, sizeof(key) - 1))
			continue;
		v += sizeof(key) - 1;
		if (*v && strlen(v) < n && !strchr(v, '/')) {
			strcpy(out, v);
			rc = 0;
		}
		break;
	}
	free(env);
	return rc;
}

int kdt_box_of_pid(long pid, char *out, size_t n)
{
	char path[64], *cg;
	static const char pfx[] = "app-kdos-";

	if (pid <= 0 || !n)
		return -1;
	snprintf(path, sizeof(path), "/proc/%ld/cgroup", pid);
	cg = kb_read_all(path, NULL);
	for (char *c = cg; c && (c = strstr(c, pfx));) {
		char *name = c + sizeof(pfx) - 1;
		char *end = strstr(name, ".scope");
		char *dash;
		size_t o = 0;

		c = name;
		if (!end)
			continue;
		/* The last `-` before `.scope` begins the instance number. */
		dash = end;
		while (dash > name && dash[-1] != '-')
			dash--;
		if (dash == name || dash == end ||
		    strspn(dash, "0123456789") != (size_t)(end - dash))
			continue;
		for (char *p = name; p < dash - 1 && o + 1 < n; p++) {
			if (!strncmp(p, "\\x2d", 4)) {
				out[o++] = '-';
				p += 3;
			} else {
				out[o++] = *p;
			}
		}
		out[o] = '\0';
		free(cg);
		if (o)
			return 0;
		break;
	}
	free(cg);
	return box_of_environ(pid, out, n);
}

/* The running containers, one name a line, for the app-id rule. NULL when
 * podman is not here or answered nothing. */
static char *running_boxes(void)
{
	KbArgv a = { 0 };
	KbBuf b = { 0 };

	if (!kb_have_prog("podman"))
		return NULL;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--filter");
	kb_argv_add(&a, "status=running");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &b) != 0) {
		kb_buf_free(&b);
		return NULL;
	}
	return b.p;
}

static void box_of_window(const KdtWindow *w, const char *boxes, char *out,
			  size_t n)
{
	size_t best = 0;

	out[0] = '\0';
	if (kdt_box_of_pid(w->pid, out, n) == 0)
		return;
	for (const char *l = boxes; l && *l;) {
		size_t len = strcspn(l, "\n");

		if (len && len > best && len < n &&
		    !strncmp(w->app_id, l, len) &&
		    (w->app_id[len] == '\0' || w->app_id[len] == '.')) {
			memcpy(out, l, len);
			out[len] = '\0';
			best = len;
		}
		l += len;
		if (*l)
			l++;
	}
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

static void state_letters(unsigned long st, char out[8])
{
	int o = 0;

	if (st & 0x1)
		out[o++] = 'F';
	if (st & 0x2)
		out[o++] = 'm';
	if (st & 0x4)
		out[o++] = 'M';
	if (st & 0x8)
		out[o++] = 'X';
	if (st & 0x10)
		out[o++] = 'a';
	if (!o)
		out[o++] = '-';
	out[o] = '\0';
}

static int no_session(void)
{
	fprintf(stderr, "kdos hey: there is no Plasma session to ask\n");
	return 1;
}

static int cmd_list(int json)
{
	KdtWindow *w;
	int n = kdt_windows(&w);
	char *boxes;

	if (n < 0)
		return no_session();
	boxes = running_boxes();
	if (json) {
		KbBuf o = { 0 };

		kb_buf_str(&o, "[");
		for (int i = 0; i < n; i++) {
			char box[128];

			box_of_window(&w[i], boxes, box, sizeof(box));
			kb_buf_printf(&o, "%s{\"id\": %d, \"uuid\": ",
				      i ? ", " : "", i + 1);
			kb_json_str(&o, w[i].uuid);
			kb_buf_printf(&o, ", \"pid\": %ld, \"app_id\": ",
				      w[i].pid);
			kb_json_str(&o, w[i].app_id);
			kb_buf_str(&o, ", \"resource\": ");
			kb_json_str(&o, w[i].resource);
			kb_buf_str(&o, ", \"box\": ");
			if (*box)
				kb_json_str(&o, box);
			else
				kb_buf_str(&o, "null");
			kb_buf_str(&o, ", \"title\": ");
			kb_json_str(&o, w[i].title);
			kb_buf_printf(&o, ", \"active\": %s, \"minimized\": %s, "
					  "\"maximized\": %s, "
					  "\"fullscreen\": %s}",
				      w[i].state & 1 ? "true" : "false",
				      w[i].state & 2 ? "true" : "false",
				      w[i].state & 4 ? "true" : "false",
				      w[i].state & 8 ? "true" : "false");
		}
		kb_buf_str(&o, "]\n");
		fwrite(o.p, 1, o.n, stdout);
		kb_buf_free(&o);
	} else {
		/* THE BOX COLUMN IS NOT TRUNCATED, and every column holds a
		 * word: a script matches on the box by column, and a cut or
		 * empty field moves every column after it. */
		/* `ws` stays a column though it is always `-`: KWin names a
		 * virtual desktop by an id no person types, and the column
		 * order is what scripts read. */
		printf("  %3s %3s %-5s %-30s %-20s %s\n", "id", "ws", "state",
		       "app_id", "box", "title");
		for (int i = 0; i < n; i++) {
			char box[128], st[8];

			box_of_window(&w[i], boxes, box, sizeof(box));
			state_letters(w[i].state, st);
			printf("  %3d %3s %-5s %-30s %-20s %.60s\n", i + 1, "-",
			       st,
			       *w[i].app_id ? w[i].app_id : "-",
			       *box ? box : "-", w[i].title);
		}
		if (!n)
			printf("  (no windows)\n");
	}
	free(boxes);
	kdt_windows_free(w, n);
	return 0;
}

static int cmd_boxes(void)
{
	KdtWindow *w;
	int n = kdt_windows(&w);
	char *boxes, **seen;
	int ns = 0;

	if (n < 0)
		return no_session();
	boxes = running_boxes();
	seen = kb_calloc((size_t)n + 1, sizeof(*seen));
	for (int i = 0; i < n; i++) {
		char box[128];
		int dup = 0;

		box_of_window(&w[i], boxes, box, sizeof(box));
		if (!*box)
			continue;
		for (int k = 0; k < ns; k++)
			if (!strcmp(seen[k], box))
				dup = 1;
		if (dup)
			continue;
		seen[ns++] = kb_strdup(box);
		printf("  %s\n", box);
	}
	for (int k = 0; k < ns; k++)
		free(seen[k]);
	free(seen);
	free(boxes);
	kdt_windows_free(w, n);
	return 0;
}

/* `<id>` is the row number `list` printed, or the window's uuid. */
static int cmd_run(const char *action, const char *id)
{
	KdtWindow *w;
	int n = kdt_windows(&w), rc;
	char *uuid = NULL;

	if (n < 0)
		return no_session();
	if (strspn(id, "0123456789") == strlen(id) && *id) {
		int k = atoi(id);

		if (k >= 1 && k <= n)
			uuid = kb_strdup(w[k - 1].uuid);
	} else {
		for (int i = 0; i < n && !uuid; i++)
			if (!strcmp(w[i].uuid, id))
				uuid = kb_strdup(w[i].uuid);
	}
	kdt_windows_free(w, n);
	if (!uuid) {
		fprintf(stderr, "kdos hey: no window %s — try: kdos hey list\n",
			id);
		return 1;
	}
	rc = kdt_window_act(uuid, action);
	free(uuid);
	if (rc == 2) {
		fprintf(stderr, "kdos hey: no action %s\n", action);
		return 2;
	}
	if (rc < 0)
		return no_session();
	if (rc == 1)
		fprintf(stderr, "kdos hey: the window closed before it was "
				"reached\n");
	return rc;
}

/* buf without its CSI escapes: ESC [, parameter and intermediate bytes,
 * one final byte. */
static void write_plain(const char *p, size_t n)
{
	size_t i = 0, from = 0;

	while (i < n) {
		if (p[i] != '\033' || i + 1 >= n || p[i + 1] != '[') {
			i++;
			continue;
		}
		fwrite(p + from, 1, i - from, stdout);
		i += 2;
		while (i < n && (unsigned char)p[i] >= 0x20 &&
		       (unsigned char)p[i] <= 0x3f)
			i++;
		if (i < n)
			i++;
		from = i;
	}
	fwrite(p + from, 1, n - from, stdout);
}

static int cmd_outputs(int json)
{
	KbArgv a = { 0 };
	KbBuf out = { 0 };
	int rc;

	if (!kb_have_prog("kscreen-doctor")) {
		fprintf(stderr, "kdos hey: kscreen-doctor is not installed\n");
		return 1;
	}
	kb_argv_add(&a, "kscreen-doctor");
	kb_argv_add(&a, json ? "-j" : "-o");
	kb_argv_end(&a);
	if (json || isatty(STDOUT_FILENO))
		return kb_run_tty(&a) == 0 ? 0 : 1;
	rc = kb_run_capture_buf(&a, &out);
	if (out.p)
		write_plain(out.p, out.n);
	kb_buf_free(&out);
	return rc == 0 ? 0 : 1;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos hey list [--json]\n"
		"       kdos hey boxes\n"
		"       kdos hey run <action> <id>\n"
		"       kdos hey outputs [--json]\n"
		"\nactions: close activate minimize unminimize maximize "
		"unmaximize fullscreen\n         unfullscreen keep-above "
		"no-keep-above\n");
	return 2;
}

int hey_main(int argc, char **argv)
{
	const char *what = argc > 1 ? argv[1] : "list";
	int json = 0;

	for (int i = 2; i < argc; i++)
		if (!strcmp(argv[i], "--json"))
			json = 1;
	if (!strcmp(what, "list") && argc <= 3 && (argc < 3 || json))
		return cmd_list(json);
	if (!strcmp(what, "boxes") && argc == 2)
		return cmd_boxes();
	if (!strcmp(what, "outputs") && argc <= 3 && (argc < 3 || json))
		return cmd_outputs(json);
	if (!strcmp(what, "run") && argc == 4)
		return cmd_run(argv[2], argv[3]);
	return usage();
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-run — Alt+F2
 *
 *   ┌ Run ─────────────────────────────────┐
 *   │ > mc /mnt/disk                       │
 *   │                                      │
 *   │ Enter run    Ctrl+Enter in a terminal│
 *   └──────────────────────────────────────┘
 *
 * The other half of the launcher. kdos-launcher searches installed
 * applications by name; this runs a COMMAND, which is a different question and
 * the one you have when the thing you want has no `.desktop` file — a script, a
 * binary in ~/bin, something with arguments.
 *
 * Ctrl+Enter runs it inside foot, because the commands people type into a run
 * box are usually the ones that print something.
 *
 * argv, never `/bin/sh -c`. That is the rule everywhere in this tree, and it is
 * NOT redundant here just because the user typed the string themselves: a shell
 * would also expand `$(...)` out of a paste, and the cost of not having one is
 * that an argument cannot contain a space.
 * ---------------------------------
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define MAX_CMD 512
#define MAX_HIST 50

/*
 * The history, which is what makes a run box worth opening twice.
 *
 * `$XDG_DATA_HOME/kdos/run-history`, one command per line, newest last. A
 * plain file rather than anything cleverer: it is readable, it is editable,
 * and `kdos-run` is not important enough to own a format. Fifty lines, because
 * the fifty-first is never what anybody was reaching for.
 */
static char hist[MAX_HIST][MAX_CMD];
static int nhist;

static int hist_path(char *buf, size_t n)
{
	const char *data = getenv("XDG_DATA_HOME");
	const char *home = getenv("HOME");

	if (data && *data)
		snprintf(buf, n, "%s/kdos", data);
	else if (home && *home)
		snprintf(buf, n, "%s/.local/share/kdos", home);
	else
		return -1;
	mkdir(buf, 0700);
	if (data && *data)
		snprintf(buf, n, "%s/kdos/run-history", data);
	else
		snprintf(buf, n, "%s/.local/share/kdos/run-history", home);
	return 0;
}

static void hist_load(void)
{
	char path[512], line[MAX_CMD];
	FILE *f;

	if (hist_path(path, sizeof(path)) != 0)
		return;
	f = fopen(path, "r");
	if (!f)
		return;
	while (nhist < MAX_HIST && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		if (*line)
			snprintf(hist[nhist++], MAX_CMD, "%s", line);
	}
	fclose(f);
}

/* Append, unless it is what was run last — a history whose top five entries
 * are the same command is a history with four wasted rows. */
static void hist_add(const char *cmd)
{
	char path[512];
	FILE *f;

	if (!*cmd || (nhist && !strcmp(hist[nhist - 1], cmd)))
		return;
	if (hist_path(path, sizeof(path)) != 0)
		return;

	if (nhist == MAX_HIST) {
		memmove(hist[0], hist[1], (MAX_HIST - 1) * MAX_CMD);
		nhist--;
	}
	snprintf(hist[nhist++], MAX_CMD, "%s", cmd);

	/* Rewritten whole rather than appended: the trim above has to reach
	 * the file too, and fifty short lines is nothing to write. */
	f = fopen(path, "w");
	if (!f)
		return;
	for (int i = 0; i < nhist; i++)
		fprintf(f, "%s\n", hist[i]);
	fclose(f);
}

static void launch(const char *cmd, bool in_term)
{
	char buf[MAX_CMD + 32];
	char *argv[32];
	int n = 0;

	if (!*cmd)
		return;
	if (in_term)
		snprintf(buf, sizeof(buf), "foot -e %s", cmd);
	else
		snprintf(buf, sizeof(buf), "%s", cmd);

	for (char *p = strtok(buf, " \t"); p && n < 31; p = strtok(NULL, " \t"))
		argv[n++] = p;
	argv[n] = NULL;
	if (!n)
		return;

	/* Double fork: the run box is about to exit, and a single fork would
	 * take the program with it. */
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], argv);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

int run_main(int argc, char **argv)
{
	const char *font = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-run [--font NAME]\n");
			return 2;
		}
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 52,
		.rows = 5,
		.app_id = "kdos-run",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-run: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	char cmd[MAX_CMD] = { 0 };
	size_t len = 0;
	int rc = 1;

	hist_load();
	/* nhist means "the line being typed"; walking up from it is walking
	 * back through what was run before. */
	int hpos = nhist;

	while (!kwl_should_close()) {
		int w = ktui_w, h = ktui_h;
		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
		ktui_draw_box(krect(0, 0, w, h), "Run", KT_ACCENT, KT_SURFACE, 0);

		ktui_draw_text(2, 1, 1, ">", KT_ACCENT, KT_SURFACE, KT_A_NONE);
		/* The tail, not the head: a long command that scrolled off the
		 * left is still being typed at its right-hand end, and showing
		 * the beginning would hide the cursor. */
		int room = w - 6;
		const char *shown = cmd;
		if ((int)len > room)
			shown = cmd + len - room;
		ktui_draw_text(4, 1, room, shown, KT_TEXT, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(4 + (int)(len > (size_t)room ? (size_t)room : len),
			       1, 1, "_", KT_ACCENT, KT_SURFACE, KT_A_NONE);
		ktui_draw_text(2, h - 2, w - 4,
			       "Enter run    Ctrl+Enter in a terminal    Esc cancel",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;
		/*
		 * A right press closes it, the same as Escape. There is nothing
		 * else here to click — a run box is a text field — but a
		 * dialog that ignores the mouse entirely is a dialog people
		 * poke at before they find the keyboard. (Clicking AWAY closes
		 * it too — `dismiss_on_unfocus` above.)
		 */
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_PRESS && ev.btn == KT_MB_RIGHT)
				break;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_ENTER) {
			hist_add(cmd);
			launch(cmd, (ev.mods & KT_MOD_CTRL) != 0);
			rc = 0;
			break;
		}
		if (ev.key == KT_K_BACKSPACE) {
			if (len)
				cmd[--len] = '\0';
			continue;
		}
		if (ev.key == KT_K_UP || ev.key == KT_K_DOWN) {
			if (ev.key == KT_K_UP && hpos > 0)
				hpos--;
			else if (ev.key == KT_K_DOWN && hpos < nhist)
				hpos++;
			if (hpos >= nhist)
				cmd[0] = '\0';	/* back to a fresh line */
			else
				snprintf(cmd, sizeof(cmd), "%s", hist[hpos]);
			len = strlen(cmd);
			continue;
		}
		if (ev.key == 21) {		/* Ctrl+U: clear the line */
			cmd[0] = '\0';
			len = 0;
			continue;
		}
		if (ev.key == 23) {		/* Ctrl+W: the last word */
			while (len && cmd[len - 1] == ' ')
				cmd[--len] = '\0';
			while (len && cmd[len - 1] != ' ')
				cmd[--len] = '\0';
			continue;
		}
		/* Printable ASCII only: the command is split into argv by
		 * bytes, and accepting multi-byte input here would let half a
		 * codepoint end an argument. */
		if (ev.key >= 0x20 && ev.key < 0x7f && len + 1 < sizeof(cmd)) {
			cmd[len++] = (char)ev.key;
			cmd[len] = '\0';
		}
	}

	kwl_shutdown();
	return rc;
}

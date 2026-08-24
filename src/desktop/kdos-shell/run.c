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

#include <dirent.h>
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

/* Append — but a command already ANYWHERE in the list moves to the end
 * instead of appearing twice, so the up-arrow order is recency and a
 * re-run of an old command does not push a fresh one off the top. */
static void hist_add(const char *cmd)
{
	char path[512], keep[MAX_CMD];
	FILE *f;
	int i;

	if (!*cmd)
		return;
	for (i = 0; i < nhist; i++) {
		if (strcmp(hist[i], cmd))
			continue;
		memcpy(keep, hist[i], MAX_CMD);
		/*
		 * Moved by ROW, not by byte: `hist + i` points at a whole
		 * MAX_CMD row inside the 2-D array, so the extent the compiler
		 * checks the move against is the array. Writing the same move
		 * as `hist[i]` hands it a pointer to one row and a length of
		 * many, which a fortified libc rejects as reading past that
		 * row's end.
		 */
		memmove(hist + i, hist + i + 1,
			(size_t)(nhist - 1 - i) * sizeof(hist[0]));
		memcpy(hist[nhist - 1], keep, MAX_CMD);
		goto write;
	}
	if (nhist == MAX_HIST) {
		memmove(hist, hist + 1, (MAX_HIST - 1) * sizeof(hist[0]));
		nhist--;
	}
	snprintf(hist[nhist++], MAX_CMD, "%s", cmd);

write:
	if (hist_path(path, sizeof(path)) != 0)
		return;
	/* Rewritten whole rather than appended: the trim above has to reach
	 * the file too, and fifty short lines is nothing to write. */
	f = fopen(path, "w");
	if (!f)
		return;
	for (i = 0; i < nhist; i++)
		fprintf(f, "%s\n", hist[i]);
	fclose(f);
}

/* ── $PATH completion ──────────────────────────────────────────────────── */

/*
 * Tab completes the FIRST token against the executables on $PATH — readdir
 * and a prefix match, no shell anywhere. The matches are gathered once per
 * cycle (the prefix as it stood at the first Tab) and repeated Tabs walk
 * them; any other key ends the cycle.
 */
#define MAX_COMP 64

static char comp[MAX_COMP][128];
static int ncomp = -1;			/* -1: no cycle in progress */
static int comp_i;

static int cmp_str(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

static void complete_scan(const char *prefix)
{
	const char *path = getenv("PATH");
	size_t plen = strlen(prefix);
	char dir[512];

	ncomp = 0;
	if (!path || !plen)
		return;
	for (const char *p = path; *p;) {
		const char *sep = strchr(p, ':');
		size_t dl = sep ? (size_t)(sep - p) : strlen(p);
		if (dl && dl < sizeof(dir)) {
			memcpy(dir, p, dl);
			dir[dl] = '\0';
			DIR *d = opendir(dir);
			struct dirent *e;
			while (d && (e = readdir(d))) {
				if (strncmp(e->d_name, prefix, plen))
					continue;
				char full[768];
				struct stat st;
				snprintf(full, sizeof(full), "%s/%s", dir,
					 e->d_name);
				/* A regular executable, not "." matching a
				 * directory access() would also say yes to. */
				if (stat(full, &st) || !S_ISREG(st.st_mode) ||
				    access(full, X_OK))
					continue;
				int dup = 0;
				for (int i = 0; i < ncomp; i++)
					if (!strcmp(comp[i], e->d_name)) {
						dup = 1;
						break;
					}
				if (dup)
					continue;
				if (ncomp < MAX_COMP) {
					snprintf(comp[ncomp++], sizeof(comp[0]),
						 "%.*s", (int)sizeof(comp[0]) - 1,
						 e->d_name);
					continue;
				}
				/*
				 * Full: keep the lexicographically first
				 * MAX_COMP, which is the head of the list the
				 * qsort below is about to produce. The cap used
				 * to be applied in readdir order with the sort
				 * AFTER it, so which matches survived was
				 * filesystem order biased to the earliest $PATH
				 * entry — with ~90 alien-app shims in
				 * /usr/local/bin, `k` then Tab could simply not
				 * contain `kpkg`, and nothing said so.
				 */
				int worst = 0;
				for (int i = 1; i < ncomp; i++)
					if (strcmp(comp[i], comp[worst]) > 0)
						worst = i;
				if (strcmp(e->d_name, comp[worst]) < 0)
					snprintf(comp[worst], sizeof(comp[0]),
						 "%.*s", (int)sizeof(comp[0]) - 1,
						 e->d_name);
			}
			if (d)
				closedir(d);
		}
		if (!sep)
			break;
		p = sep + 1;
	}
	qsort(comp, (size_t)ncomp, sizeof(comp[0]), cmp_str);
}

/* Display columns in the first `nbytes` bytes — the caret's cell. */
static int col_of(const char *s, size_t nbytes)
{
	const char *p = s, *end = s + nbytes;
	int c = 0;

	while (p < end && *p) {
		uint32_t cp;
		p = ktui_utf8_next(p, &cp);
		c++;
	}
	return c;
}

/* Most useful first: the shared bar drops from the RIGHT. */
enum { RB_RUN, RB_TERM, RB_CANCEL, RB_N };

/* The inverse: the byte offset of a column, clamped to the end of the line.
 * A text field that cannot be clicked into is one you have to arrow across. */
static size_t cur_at_col(const char *s, int want)
{
	const char *p = s;
	int c = 0;

	while (*p && c < want) {
		uint32_t cp;
		p = ktui_utf8_next(p, &cp);
		c++;
	}
	return (size_t)(p - s);
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
		/* Fifty-six, not fifty-two: the three buttons come to
		 * thirty-five columns and the hint beside them is drawn whole
		 * or not at all, so four more cells are the difference between
		 * a row that explains itself and a row of buttons alone. */
		.cols = 56,
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
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	char cmd[MAX_CMD] = { 0 };
	size_t len = 0, cur = 0;	/* bytes; cur is the caret */
	int off = 0;			/* leftmost shown column */
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
		/* The window follows the CARET, not the tail: a long command
		 * scrolls under a caret that stays inside the box, so editing
		 * in the middle is visible wherever the middle is. */
		int room = w - 6;
		if (room < 1)
			room = 1;
		int pw = col_of(cmd, cur);
		if (pw < off)
			off = pw;
		if (pw - off >= room)
			off = pw - room + 1;
		const char *shown = cmd;
		for (int c = 0; c < off && *shown; c++) {
			uint32_t cp;
			shown = ktui_utf8_next(shown, &cp);
		}
		ktui_draw_text(4, 1, room, shown, KT_TEXT, KT_SURFACE, KT_A_NONE);
		/* The caret: the cell under it with the colours swapped, or a
		 * bare underscore when it sits past the end of the line. */
		int cx = 4 + (pw - off);
		if (cur < len) {
			uint32_t cp;
			const char *nx = ktui_utf8_next(cmd + cur, &cp);
			char ch[8];
			snprintf(ch, sizeof(ch), "%.*s",
				 (int)(nx - (cmd + cur)), cmd + cur);
			ktui_draw_fill(krect(cx, 1, 1, 1), KT_ACCENT);
			ktui_draw_text(cx, 1, 1, ch, KT_SURFACE, KT_ACCENT,
				       KT_A_NONE);
		} else {
			ktui_draw_text(cx, 1, 1, "_", KT_ACCENT, KT_SURFACE,
				       KT_A_NONE);
		}
		/*
		 * THE VERBS AS BUTTONS. The row said `Enter run    Ctrl+Enter
		 * in a terminal    Esc cancel`, which is a legend: the one
		 * feature this box has beyond a prompt — running something in
		 * a terminal — was a modifier nobody is told about, and a
		 * pointer could do nothing here at all. Three buttons say the
		 * same three things and can be pressed. Most useful first, so
		 * the shared bar drops Cancel before it drops Run.
		 */
		struct kch_button rb[RB_N];
		rb[RB_RUN] = (struct kch_button){ "Run", cmd[0] != '\0' };
		rb[RB_TERM] = (struct kch_button){ "In Terminal",
						  cmd[0] != '\0' };
		rb[RB_CANCEL] = (struct kch_button){ "Cancel", 1 };
		int bx = kch_buttons(w, h - 2, rb, RB_N, -1);
		const char *hint = "type a command";
		if (bx - 3 >= (int)ktui_utf8_width(hint))
			ktui_draw_text(2, h - 2, bx - 3, hint, KT_DIM,
				       KT_SURFACE, KT_A_NONE);
		ktui_draw_flush();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			/* The grid follows a configure only when the loop that
			 * owns the surface applies it — the caret window above
			 * is computed from a width that no longer exists
			 * otherwise. */
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		/*
		 * A right press closes it, the same as Escape. There is nothing
		 * else here to click — a run box is a text field — but a
		 * dialog that ignores the mouse entirely is a dialog people
		 * poke at before they find the keyboard. (Clicking AWAY closes
		 * it too — `dismiss_on_unfocus` above.)
		 */
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press == KT_MP_PRESS && ev.btn == KT_MB_RIGHT)
				break;
			if (ev.press == KT_MP_PRESS && ev.btn == KT_MB_LEFT) {
				int bi = kch_button_at(ev.mx, ev.my);

				if (bi == RB_CANCEL)
					break;
				if ((bi == RB_RUN || bi == RB_TERM) && cmd[0]) {
					hist_add(cmd);
					launch(cmd, bi == RB_TERM);
					rc = 0;
					break;
				}
				/* A CLICK PLACES THE CARET. It is the one
				 * thing a pointer owes a text field, and the
				 * only mouse gesture this box could not
				 * answer: editing the middle of a long command
				 * meant arrowing to it. */
				if (bi < 0 && ev.my == 1 && ev.mx >= 4)
					cur = cur_at_col(cmd, off + ev.mx - 4);
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		/* Any key but Tab ends a completion cycle — the next Tab
		 * gathers matches for whatever the line says by then. */
		if (ev.key != KT_K_TAB)
			ncomp = -1;

		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_ENTER) {
			hist_add(cmd);
			launch(cmd, (ev.mods & KT_MOD_CTRL) != 0);
			rc = 0;
			break;
		}
		if (ev.key == KT_K_TAB) {
			/* First token only: a run box completes the COMMAND;
			 * an argument is a path question it does not answer. */
			size_t tok = strcspn(cmd, " \t");
			if (cur > tok)
				continue;
			if (ncomp < 0) {
				char pre[MAX_CMD];
				snprintf(pre, sizeof(pre), "%.*s", (int)tok,
					 cmd);
				complete_scan(pre);
				comp_i = 0;
			}
			if (ncomp > 0) {
				const char *m = comp[comp_i++ % ncomp];
				size_t ml = strlen(m);
				if (ml + len - tok < sizeof(cmd)) {
					memmove(cmd + ml, cmd + tok,
						len - tok + 1);
					memcpy(cmd, m, ml);
					len += ml - tok;
					cur = ml;
				}
			}
			continue;
		}
		if (ev.key == KT_K_BACKSPACE) {
			if (cur) {
				size_t p = cur - 1;
				while (p > 0 && (cmd[p] & 0xc0) == 0x80)
					p--;
				memmove(cmd + p, cmd + cur, len - cur + 1);
				len -= cur - p;
				cur = p;
			}
			continue;
		}
		if (ev.key == KT_K_DEL) {
			if (cur < len) {
				uint32_t cp;
				size_t nx = (size_t)(ktui_utf8_next(cmd + cur,
								    &cp) - cmd);
				memmove(cmd + cur, cmd + nx, len - nx + 1);
				len -= nx - cur;
			}
			continue;
		}
		if (ev.key == KT_K_LEFT) {
			while (cur > 0) {
				cur--;
				if ((cmd[cur] & 0xc0) != 0x80)
					break;
			}
			continue;
		}
		if (ev.key == KT_K_RIGHT) {
			if (cur < len) {
				uint32_t cp;
				cur = (size_t)(ktui_utf8_next(cmd + cur, &cp) -
					       cmd);
			}
			continue;
		}
		if (ev.key == KT_K_HOME) {
			cur = 0;
			continue;
		}
		if (ev.key == KT_K_END) {
			cur = len;
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
			cur = len;
			continue;
		}
		if (ev.key == 21) {		/* Ctrl+U: clear the line */
			cmd[0] = '\0';
			len = 0;
			cur = 0;
			continue;
		}
		if (ev.key == 23) {		/* Ctrl+W: the word before the caret */
			size_t p = cur;
			while (p && cmd[p - 1] == ' ')
				p--;
			while (p && cmd[p - 1] != ' ')
				p--;
			memmove(cmd + p, cmd + cur, len - cur + 1);
			len -= cur - p;
			cur = p;
			continue;
		}
		/* Printable ASCII only: the command is split into argv by
		 * bytes, and accepting multi-byte input here would let half a
		 * codepoint end an argument. */
		if (ev.key >= 0x20 && ev.key < 0x7f && len + 1 < sizeof(cmd)) {
			memmove(cmd + cur + 1, cmd + cur, len - cur + 1);
			cmd[cur++] = (char)ev.key;
			len++;
		}
	}

	kwl_shutdown();
	return rc;
}

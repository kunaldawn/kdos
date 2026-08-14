/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-pick — the file chooser
 *
 *   ┌ Open Image ──────────────────────────────┐
 *   │ /home/kdos/Pictures                      │
 *   │ ░ ..                                     │
 *   │ ▓ Screenshots/                           │
 *   │ ▒ kdos.png                        412 K  │
 *   │▸▒ wallpaper.png                   1.2 M  │
 *   │ Filter: Images (*.png *.jpg)             │
 *   │ [ Open ]  [ Cancel ]                     │
 *   └──────────────────────────────────────────┘
 *
 * mc is the file MANAGER on this desktop, and it cannot be the file CHOOSER:
 * `--printwd` hands back a directory and there is no selection-and-exit
 * protocol for a file, a filter, a multi-selection or a save name. So this is
 * ours, and it is the smaller half of what a from-scratch file manager would
 * have been.
 *
 * IT PRINTS URIs ON STDOUT AND NOTHING ELSE. That is the whole interface, which
 * is what lets xdg-desktop-portal-kdos drive it without linking it, and what
 * lets a script use it. Anything it wants to say to a human goes on stderr.
 *
 * Exit 0 with at least one line means a choice; exit 1 means cancelled. A
 * chooser that exited 0 having chosen nothing would make every caller guess.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

#define MAX_ROWS 4096
#define MAX_SEL 64
#define MAX_PATTERNS 16

struct row {
	char name[256];
	bool dir;
	bool selected;
	long long size;
};

static struct row rows[MAX_ROWS];
static int nrows, sel, top;
static char cwd[1024];

/* The filter, as a list of glob patterns. NULL means everything. */
static char patterns[MAX_PATTERNS][64];
static int npatterns;
static char filter_label[256];

static bool save_mode, dir_mode, multi_mode;
static char save_name[256];

/* ── matching ──────────────────────────────────────────────────────────── */

/*
 * `*` and `?` only.
 *
 * A .desktop MimeType or a portal filter is `*.png`, `*.[tT][xX][tT]` at worst,
 * and fnmatch() would pull in a locale-aware matcher for a job this does in
 * fifteen lines. Character classes are deliberately NOT supported, and a
 * pattern using one simply fails to match rather than matching wrongly.
 */
/*
 * ITERATIVE, with a single backtrack point — not the obvious recursion.
 *
 * The obvious version recurses on every `*` and backtracks exponentially:
 * `*a*a*a*a*a*b` against forty `a`s does not finish this century. That matters
 * here and not in a shell, because the pattern is NOT the user's — it arrives
 * from another application through the FileChooser portal's `filters` option,
 * so a pattern is untrusted input and a chooser that wedges takes the portal's
 * bus loop with it.
 *
 * The standard linear algorithm: remember where the last `*` was and where it
 * had matched to; on a mismatch, advance that and carry on. Linear in the
 * length of the name, no stack at all.
 */
static bool glob_match(const char *pat, const char *s)
{
	const char *star = NULL, *retry = NULL;

	while (*s) {
		if (*pat == '?' || *pat == *s) {
			pat++;
			s++;
		} else if (*pat == '*') {
			star = pat++;
			retry = s;
		} else if (star) {
			pat = star + 1;
			s = ++retry;
		} else {
			return false;
		}
	}
	while (*pat == '*')
		pat++;
	return !*pat;
}

static bool passes_filter(const char *name)
{
	if (!npatterns)
		return true;
	for (int i = 0; i < npatterns; i++)
		if (glob_match(patterns[i], name))
			return true;
	return false;
}

/* ── reading a directory ───────────────────────────────────────────────── */

static int cmp_row(const void *a, const void *b)
{
	const struct row *x = a, *y = b;
	if (x->dir != y->dir)
		return x->dir ? -1 : 1;
	return strcasecmp(x->name, y->name);
}

static void load_dir(void)
{
	nrows = 0;
	sel = 0;
	top = 0;

	DIR *d = opendir(cwd);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && nrows < MAX_ROWS - 1) {
		if (!strcmp(e->d_name, ".") || e->d_name[0] == '.')
			continue;

		char path[2048];
		snprintf(path, sizeof(path), "%s/%s", cwd, e->d_name);
		struct stat st;
		bool isdir = stat(path, &st) == 0 && S_ISDIR(st.st_mode);

		/*
		 * Directories are never filtered out. A filter says which files
		 * you want, not where you are allowed to look — hiding a folder
		 * because it is not a PNG makes the picture inside it
		 * unreachable.
		 */
		if (!isdir && (dir_mode || !passes_filter(e->d_name)))
			continue;

		struct row *r = &rows[nrows++];
		memset(r, 0, sizeof(*r));
		snprintf(r->name, sizeof(r->name), "%s", e->d_name);
		r->dir = isdir;
		r->size = isdir ? 0 : (long long)st.st_size;
	}
	closedir(d);
	qsort(rows, (size_t)nrows, sizeof(rows[0]), cmp_row);

	/* `..` first and always, even in a directory that is otherwise empty —
	 * a chooser you cannot leave is a chooser you have to kill. */
	if (nrows < MAX_ROWS && strcmp(cwd, "/")) {
		memmove(&rows[1], &rows[0], (size_t)nrows * sizeof(rows[0]));
		memset(&rows[0], 0, sizeof(rows[0]));
		snprintf(rows[0].name, sizeof(rows[0].name), "..");
		rows[0].dir = true;
		nrows++;
	}
}

static void enter_dir(const char *name)
{
	char next[1400];
	if (!strcmp(name, "..")) {
		char *slash = strrchr(cwd, '/');
		if (slash && slash != cwd)
			*slash = '\0';
		else
			snprintf(cwd, sizeof(cwd), "/");
	} else {
		snprintf(next, sizeof(next), "%s%s%s", cwd,
			 strcmp(cwd, "/") ? "/" : "", name);
		/* Refuse rather than truncate. A silently shortened path is a
		 * chooser that opens the wrong directory, which is worse than
		 * one that declines to descend into a pathological name. */
		size_t n = strlen(next);
		if (n < sizeof(cwd))
			memcpy(cwd, next, n + 1);
	}
	load_dir();
}

/* ── output ────────────────────────────────────────────────────────────── */

/*
 * A file:// URI, percent-encoded.
 *
 * The portal's return value is a URI array and a client will feed it straight
 * to a URI parser, so a path with a space or a `#` in it has to be escaped or
 * the name is silently truncated at the first offending byte.
 */
static void print_uri(const char *path)
{
	static const char *hex = "0123456789ABCDEF";
	fputs("file://", stdout);
	for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || strchr("/-_.~", *p))
			putchar(*p);
		else
			printf("%%%c%c", hex[*p >> 4], hex[*p & 0xf]);
	}
	putchar('\n');
}

static void emit_path(const char *name)
{
	char full[2048];
	snprintf(full, sizeof(full), "%s%s%s", cwd, strcmp(cwd, "/") ? "/" : "",
		 name);
	print_uri(full);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void human(long long n, char *out, size_t len)
{
	if (n < 1024)
		snprintf(out, len, "%lld", n);
	else if (n < 1024 * 1024)
		snprintf(out, len, "%lld K", n / 1024);
	else if (n < 1024LL * 1024 * 1024)
		snprintf(out, len, "%lld M", n / (1024 * 1024));
	else
		snprintf(out, len, "%lld G", n / (1024LL * 1024 * 1024));
}

/* Where the last frame put the two buttons, so a click maps back to what was
 * drawn rather than to what the layout intended. */
static int btn_ok_x, btn_ok_end, btn_cancel_x, btn_cancel_end, btn_row;

static void draw(const char *title)
{
	int w = ktui_w, h = ktui_h;
	/* Two rows of chrome at the top (border, path) and three at the bottom
	 * (name or filter, buttons, border). */
	int list_top = 2;
	int list_rows = h - list_top - 3;
	if (list_rows < 1)
		list_rows = 1;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 0);

	/* The path, elided from the LEFT: the end of a path is the part that
	 * identifies it, and truncating the tail is how a chooser shows you
	 * three identical rows of "/home/kdos/Documents/…". */
	/* Clamped: on a surface narrower than the chrome, `pw` goes negative and
	 * the elide below would index PAST the end of the string. */
	int pw = w - 4;
	if (pw < 1)
		pw = 1;
	const char *shown = cwd;
	if ((int)strlen(cwd) > pw)
		shown = cwd + strlen(cwd) - pw;
	ktui_draw_text(2, 1, pw, shown, KT_MID, KT_SURFACE, KT_A_NONE);

	for (int i = 0; i < list_rows; i++) {
		int idx = top + i;
		if (idx >= nrows)
			break;
		const struct row *r = &rows[idx];
		bool on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_SURFACE;

		ktui_draw_fill(krect(1, list_top + i, w - 2, 1), bg);
		if (multi_mode)
			ktui_draw_text(1, list_top + i, 1,
				       r->selected ? ktui_glyph[KT_G_SQUARE] : " ",
				       fg, bg, KT_A_NONE);
		ktui_draw_text(2, list_top + i, 2,
			       r->dir ? ktui_glyph[KT_G_FULL]
				      : ktui_glyph[KT_G_DOT],
			       on ? KT_SURFACE : (r->dir ? KT_ACCENT : KT_MID),
			       bg, KT_A_NONE);

		char label[300];
		snprintf(label, sizeof(label), "%s%s", r->name, r->dir ? "/" : "");
		ktui_draw_text(4, list_top + i, w - 18, label, fg, bg, KT_A_NONE);

		if (!r->dir) {
			char sz[32];
			human(r->size, sz, sizeof(sz));
			ktui_draw_text_right(0, list_top + i, w - 2, sz,
					     on ? KT_SURFACE : KT_DIM, bg,
					     KT_A_NONE);
		}
	}

	int y = h - 3;
	if (save_mode) {
		char line[320];
		snprintf(line, sizeof(line), "Name: %s", save_name);
		ktui_draw_text(2, y, w - 4, line, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (filter_label[0]) {
		char line[320];
		snprintf(line, sizeof(line), "Filter: %s", filter_label);
		ktui_draw_text(2, y, w - 4, line, KT_DIM, KT_SURFACE, KT_A_NONE);
	}

	ktui_draw_text(2, h - 2, w - 4,
		       save_mode ? "Enter save   Esc cancel"
		       : multi_mode ? "Space mark   Enter open   Esc cancel"
				    : "Enter open   Esc cancel",
		       KT_DIM, KT_SURFACE, KT_A_NONE);

	/*
	 * TWO REAL BUTTONS, right-aligned on the hint row.
	 *
	 * This dialog is what every boxed application's Open and Save reaches
	 * through the portal — GIMP, LibreOffice, Firefox — and a person who
	 * got here by clicking File ▸ Open is holding a mouse. Shipping it
	 * keyboard-only meant the one dialog on the system that other people's
	 * software puts in front of you was the one you could not click.
	 */
	const char *ok = save_mode ? " Save " : dir_mode ? " Choose " : " Open ";
	int okw = ktui_utf8_width(ok) + 2, cw = 10;
	btn_ok_x = w - 2 - okw;
	btn_ok_end = btn_ok_x + okw;
	btn_cancel_x = btn_ok_x - 1 - cw;
	btn_cancel_end = btn_cancel_x + cw;
	btn_row = h - 2;
	if (btn_cancel_x > 26) {
		ktui_draw_text(btn_cancel_x, btn_row, cw, "[ Cancel ]", KT_TEXT,
			       KT_SURFACE, KT_A_NONE);
		ktui_draw_text(btn_ok_x, btn_row, 1, "[", KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(btn_ok_x + 1, btn_row, okw - 2, ok, KT_SURFACE,
			       KT_ACCENT, KT_A_NONE);
		ktui_draw_text(btn_ok_end - 1, btn_row, 1, "]", KT_DIM,
			       KT_SURFACE, KT_A_NONE);
	} else {
		/* No room: the buttons are the first thing to go, because the
		 * keyboard path still works and a button drawn over the hint
		 * text is worse than no button. */
		btn_ok_x = btn_ok_end = btn_cancel_x = btn_cancel_end = 0;
	}
	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

static void add_pattern(const char *p)
{
	if (npatterns < MAX_PATTERNS)
		snprintf(patterns[npatterns++], sizeof(patterns[0]), "%s", p);
}

/*
 * What Enter — and the Open button, and a click on an already-selected row —
 * all do. One function, because three entry points into a chooser that has
 * four modes is three chances for them to disagree about what a directory
 * means in save mode.
 *
 * Returns 1 when the picker has an answer and should close, 0 to stay open
 * (a directory was entered, or there was nothing to accept).
 */
static int activate(void)
{
	if (save_mode) {
		if (sel >= 0 && sel < nrows && rows[sel].dir) {
			enter_dir(rows[sel].name);
			return 0;
		}
		if (!save_name[0])
			return 0;
		emit_path(save_name);
		return 1;
	}
	if (dir_mode) {
		/*
		 * In directory mode Enter on a folder CHOOSES it rather than
		 * entering it — except `..`, which is the only way back up and
		 * would otherwise be unusable.
		 */
		if (sel >= 0 && sel < nrows && !strcmp(rows[sel].name, "..")) {
			enter_dir("..");
			return 0;
		}
		if (sel >= 0 && sel < nrows)
			emit_path(rows[sel].name);
		else
			print_uri(cwd);
		return 1;
	}
	if (sel >= 0 && sel < nrows && rows[sel].dir) {
		enter_dir(rows[sel].name);
		return 0;
	}
	if (multi_mode) {
		int n = 0;
		for (int i = 0; i < nrows; i++)
			if (rows[i].selected) {
				emit_path(rows[i].name);
				n++;
			}
		if (!n && sel >= 0 && sel < nrows) {
			emit_path(rows[sel].name);
			n++;
		}
		return n ? 1 : 0;
	}
	if (sel >= 0 && sel < nrows) {
		emit_path(rows[sel].name);
		return 1;
	}
	return 0;
}

int pick_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *title = "Open File";
	const char *start = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--title") && i + 1 < argc)
			title = argv[++i];
		else if (!strcmp(argv[i], "--save"))
			save_mode = true;
		else if (!strcmp(argv[i], "--directory"))
			dir_mode = true;
		else if (!strcmp(argv[i], "--multiple"))
			multi_mode = true;
		else if (!strcmp(argv[i], "--name") && i + 1 < argc)
			snprintf(save_name, sizeof(save_name), "%s", argv[++i]);
		else if (!strcmp(argv[i], "--dir") && i + 1 < argc)
			start = argv[++i];
		else if (!strcmp(argv[i], "--filter") && i + 1 < argc) {
			/* "Label:pat pat pat" — the label is what the user
			 * reads and the patterns are what the list obeys. */
			char buf[256];
			snprintf(buf, sizeof(buf), "%s", argv[++i]);
			char *colon = strchr(buf, ':');
			char *pats = buf;
			if (colon) {
				*colon = '\0';
				snprintf(filter_label, sizeof(filter_label),
					 "%s", buf);
				pats = colon + 1;
			}
			for (char *p = strtok(pats, " ,;"); p;
			     p = strtok(NULL, " ,;"))
				add_pattern(p);
			if (!filter_label[0])
				snprintf(filter_label, sizeof(filter_label),
					 "%s", pats);
		} else {
			fprintf(stderr,
				"usage: kdos-pick [--save] [--directory] "
				"[--multiple] [--title T] [--name N]\n"
				"                 [--dir D] "
				"[--filter 'Label:*.png *.jpg'] [--font F]\n");
			return 2;
		}
	}

	const char *home = getenv("HOME");
	snprintf(cwd, sizeof(cwd), "%s", start ? start : (home ? home : "/"));
	load_dir();

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 64,
		.rows = 22,
		.app_id = "kdos-pick",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-pick: no compositor or no layer-shell\n");
		return 2;
	}
	ktui_draw_init();

	int rc = 1;
	while (!kwl_should_close()) {
		int list_rows = ktui_h - 5;
		if (list_rows < 1)
			list_rows = 1;
		if (sel < top)
			top = sel;
		if (sel >= top + list_rows)
			top = sel - list_rows + 1;
		draw(title);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;

		/*
		 * The mouse, on the same contract as the menu and the
		 * launcher: hover selects, the wheel scrolls, a click on a row
		 * selects it and a click on an already-selected row opens it —
		 * the spatial model kdos-desk uses, and the one that does not
		 * descend into a directory every time the pointer crosses it.
		 * A right press goes UP, which is where a right click in a
		 * file list has gone since Norton Commander.
		 */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 2 + top;
			int on_row = ev.my >= 2 && ev.my < ktui_h - 3 &&
				     row >= 0 && row < nrows;
			if (ev.press == KT_MP_DRAG) {
				if (on_row)
					sel = row;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel--;
			} else if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel++;
			} else if (ev.btn == KT_MB_RIGHT) {
				enter_dir("..");
			} else if (ev.btn == KT_MB_MIDDLE && multi_mode &&
				   on_row && !rows[row].dir) {
				sel = row;
				rows[row].selected = !rows[row].selected;
			} else if (ev.btn == KT_MB_LEFT) {
				if (btn_ok_end > btn_ok_x && ev.my == btn_row &&
				    ev.mx >= btn_ok_x && ev.mx < btn_ok_end) {
					if (activate()) {
						rc = 0;
						break;
					}
				} else if (btn_cancel_end > btn_cancel_x &&
					   ev.my == btn_row &&
					   ev.mx >= btn_cancel_x &&
					   ev.mx < btn_cancel_end) {
					break;
				} else if (on_row) {
					if (row == sel && activate()) {
						rc = 0;
						break;
					}
					sel = row;
				}
			}
			if (sel < 0)
				sel = 0;
			if (sel >= nrows)
				sel = nrows ? nrows - 1 : 0;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_UP) {
			sel--;
		} else if (ev.key == KT_K_DOWN) {
			sel++;
		} else if (ev.key == KT_K_LEFT) {
			enter_dir("..");
		} else if (ev.key == ' ' && multi_mode && !save_mode) {
			if (sel >= 0 && sel < nrows && !rows[sel].dir)
				rows[sel].selected = !rows[sel].selected;
			sel++;
		} else if (ev.key == KT_K_ENTER) {
			if (activate()) {
				rc = 0;
				break;
			}
			continue;
		} else if (save_mode) {
			size_t n = strlen(save_name);
			if (ev.key == KT_K_BACKSPACE) {
				if (n)
					save_name[n - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   n + 1 < sizeof(save_name)) {
				save_name[n] = (char)ev.key;
				save_name[n + 1] = '\0';
			}
		}

		if (sel < 0)
			sel = 0;
		if (sel >= nrows)
			sel = nrows ? nrows - 1 : 0;
	}

	kwl_shutdown();
	fflush(stdout);
	return rc;
}

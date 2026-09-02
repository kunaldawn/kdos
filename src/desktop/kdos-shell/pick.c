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
#include <errno.h>
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
/* The viewport follows the SELECTION only when the selection is what moved —
 * see kch_list_wheel. A clamp that followed unconditionally would undo a page
 * scroll on the next frame. */
static int sel_follow = 1;
static char cwd[1024];

/* The filter, as a list of glob patterns. NULL means everything. */
static char patterns[MAX_PATTERNS][64];
static int npatterns;
static char filter_label[256];

static bool save_mode, dir_mode, multi_mode;
/*
 * BROWSE: the same list, opening things instead of answering with them.
 *
 * The Start menu's Places rows have always run `kdos-pick --browse <dir>` and
 * this program had no such flag, so every one of them — Home, Documents,
 * Downloads, Pictures — hit the unknown-argument branch, printed a usage line
 * to a stderr nobody was reading, and exited before a surface existed. Four
 * dead rows in the most-used menu on the desktop.
 *
 * It is this program rather than a second one because a file manager and a
 * file chooser are the same list with a different verb: here Enter on a file
 * hands it to `kdos-appbox open`, which is already what "open this on this
 * machine" means, and the dialog STAYS UP — a browser that closed after one
 * file would be a chooser wearing the wrong name.
 */
static bool browse_mode;
static char save_name[256];
/*
 * Dotfiles, off by default and reachable — Ctrl+H, which is the same key every
 * file manager on the machine uses, mc included. A chooser that can never show
 * a hidden file cannot open one, and `~/.config` is a real place people keep
 * things they want to edit.
 */
static bool show_hidden;
/* Save mode only: Enter on an existing name asks once before it answers. */
static bool overwrite_ok;
static char note[128];

/*
 * The one line editor, shared by Create Folder (F7/Ctrl+N, save mode) and the
 * Ctrl+L path entry — the same editing path the save name already has, worn
 * by two more questions.
 */
enum { ED_NONE = 0, ED_NEWDIR, ED_PATH };
static int edit_mode;
static char edit_buf[1024];

/* Ctrl+F: the portal's filter, or everything. The patterns another
 * application handed the portal are which files IT wants; '*' is which files
 * the user can see anyway, and a chooser that cannot show them cannot open
 * them. */
static bool filter_off;

/* Where the last frame ended the list — the preview pane starts there, and a
 * click under the pane must not select the row it covers. */
static int list_w = 1 << 20;

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
	if (!npatterns || filter_off)
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

	/*
	 * A directory that cannot be READ still gets its `..` below — Ctrl+L
	 * into somebody else's home, or a cwd deleted under us, used to leave a
	 * chooser with no rows at all and therefore no way out that is drawn
	 * anywhere. The reason goes in the note, or an empty list explains
	 * nothing.
	 */
	DIR *d = opendir(cwd);
	if (!d)
		snprintf(note, sizeof(note), "%.60s: %s", cwd, strerror(errno));
	struct dirent *e;
	while (d && (e = readdir(d)) && nrows < MAX_ROWS - 1) {
		/* `..` is added below, unconditionally and first; `.` is never
		 * useful, and everything else beginning with a dot is a
		 * decision the user makes with Ctrl+H. */
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		if (!show_hidden && e->d_name[0] == '.')
			continue;

		char path[2048];
		snprintf(path, sizeof(path), "%s/%s", cwd, e->d_name);
		/* Zeroed: stat fails on a dangling symlink and on a file deleted
		 * between the readdir and here, and st_size is read below. */
		struct stat st = {0};
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
	if (d)
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

/*
 * Reload, keeping the multi-select marks — by NAME, because the row indexes
 * are the one thing a reload does not preserve. Toggling hidden files to
 * find one more attachment used to throw away every mark already made.
 */
static void reload_keep_marks(void)
{
	static char keep[MAX_SEL][256];
	int nkeep = 0;

	for (int i = 0; i < nrows && nkeep < MAX_SEL; i++)
		if (rows[i].selected)
			snprintf(keep[nkeep++], sizeof(keep[0]), "%.*s",
				 (int)sizeof(keep[0]) - 1, rows[i].name);
	load_dir();
	for (int k = 0; k < nkeep; k++)
		for (int i = 0; i < nrows; i++)
			if (!strcmp(rows[i].name, keep[k])) {
				rows[i].selected = true;
				break;
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

/* ── image preview (D7.9) ──────────────────────────────────────────────────
 *
 * P6 only. The parser is asciicmd.c's, copied rather than shared: the
 * offscreen --dump build links this file WITHOUT libkcell (see
 * testing/selftest.sh's dumpcheck line), so the preview cannot reference the
 * glyph matcher — and grim writes `-t ppm`, which makes P6 the format
 * screenshots actually arrive in. PNG and JPG say "no preview"; decoding
 * them is a future piece of work, not a missing include.
 *
 * The glyphs are ktui's own vt-tier ramp — ░ ▒ █, which the console font and
 * Terminus both carry — not the rich tier's eighth blocks, which are BAR
 * glyphs: partial-height cells turn a photograph into stripes.
 */
#define PV_COLS 20
#define PV_ROWS 17
#define PV_MAX_BYTES (20 * 1024 * 1024)

static char pv_want[2048];	/* what the selection asks for; "" = nothing */
static char pv_path[2048];	/* what pv_state describes                   */
static int pv_state;		/* 2 ready, 3 unreadable/over budget         */
static unsigned char pv_lum[PV_COLS * PV_ROWS];
static int pv_cols, pv_rows;

/* 1 = previewable here, 2 = an image this cannot decode, 0 = not an image. */
static int pv_kind(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	if (!strcasecmp(dot, ".ppm") || !strcasecmp(dot, ".pnm"))
		return 1;
	if (!strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") ||
	    !strcasecmp(dot, ".jpeg"))
		return 2;
	return 0;
}

static const char *pv_glyph(double f)
{
	static const char *const SHADE[] = { " ", "\xe2\x96\x91",
					     "\xe2\x96\x92", "\xe2\x96\x88" };
	static const char *const ASCII[] = { " ", ".", ":", "#" };
	const char *const *t = (ktui_caps & KT_CAP_UTF8) ? SHADE : ASCII;
	int i = (int)(f * 4.0);

	if (i > 3)
		i = 3;
	if (i < 0)
		i = 0;
	return t[i];
}

/* Whitespace and `#` comments, anywhere in the header — netpbm allows them. */
static int pv_next_int(FILE *f, int *out)
{
	int c, v = 0;
	bool got = false;

	for (;;) {
		c = fgetc(f);
		if (c == EOF)
			return -1;
		if (c == '#') {
			while (c != '\n' && c != EOF)
				c = fgetc(f);
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (got)
				break;
			continue;
		}
		if (c < '0' || c > '9')
			return -1;
		v = v * 10 + (c - '0');
		got = true;
	}
	if (!got)
		return -1;
	*out = v;
	return 0;
}

static void pv_decode(const char *path)
{
	snprintf(pv_path, sizeof(pv_path), "%s", path);
	pv_state = 3;
	pv_cols = pv_rows = 0;

	/* Over budget is "no preview", never a stall: this runs in the poll
	 * loop's idle slot and a 2 GB file must not own the dialog. */
	struct stat st;
	if (stat(path, &st) != 0 || st.st_size > PV_MAX_BYTES)
		return;
	FILE *f = fopen(path, "rb");
	if (!f)
		return;
	int w = 0, h = 0, maxv = 0;
	if (fgetc(f) != 'P' || fgetc(f) != '6' || pv_next_int(f, &w) ||
	    pv_next_int(f, &h) || pv_next_int(f, &maxv) || w <= 0 || h <= 0 ||
	    maxv != 255 || (long long)w * h * 3 > (long long)PV_MAX_BYTES) {
		fclose(f);
		return;
	}
	unsigned char *rgb = malloc((size_t)w * h * 3);
	if (!rgb || fread(rgb, 3, (size_t)w * h, f) != (size_t)w * h) {
		free(rgb);
		fclose(f);
		return;
	}
	fclose(f);

	/* Fit into the pane. A cell is twice as tall as wide, so the vertical
	 * scale carries a factor of two or the picture stretches — the same
	 * constraint genlogo.py records for the banner. */
	int gw = PV_COLS;
	int gh = (int)(((long long)h * PV_COLS) / ((long long)w * 2));
	if (gh > PV_ROWS) {
		gh = PV_ROWS;
		gw = (int)(((long long)w * PV_ROWS * 2) / h);
		if (gw > PV_COLS)
			gw = PV_COLS;
	}
	if (gw < 1)
		gw = 1;
	if (gh < 1)
		gh = 1;

	for (int cy = 0; cy < gh; cy++) {
		int y0 = (int)((long long)cy * h / gh);
		int y1 = (int)((long long)(cy + 1) * h / gh);
		if (y1 <= y0)
			y1 = y0 + 1;
		for (int cx = 0; cx < gw; cx++) {
			int x0 = (int)((long long)cx * w / gw);
			int x1 = (int)((long long)(cx + 1) * w / gw);
			if (x1 <= x0)
				x1 = x0 + 1;
			long long sum = 0;
			for (int y = y0; y < y1; y++) {
				const unsigned char *p =
					rgb + ((size_t)y * w + x0) * 3;
				for (int x = x0; x < x1; x++, p += 3)
					sum += 299 * p[0] + 587 * p[1] +
					       114 * p[2];
			}
			long long npix = (long long)(y1 - y0) * (x1 - x0);
			pv_lum[cy * PV_COLS + cx] =
				(unsigned char)(sum / (npix * 1000));
		}
	}
	free(rgb);
	pv_cols = gw;
	pv_rows = gh;
	pv_state = 2;
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
/* Which button the pointer is on, or -1. A control that looks the same under
 * the hand as away from it is one people click to find out what it is. */
static int hover_btn = -1;

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
	sh_frame(w, h, title, KT_ACCENT, KT_SURFACE, 0);

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

	/*
	 * The preview pane is CARVED from the list when the selected row is an
	 * image: only the list's WIDTH changes, so the row hit math the mouse
	 * uses stays exactly the accumulation it always was.
	 */
	int kind = 0;
	if (sel >= 0 && sel < nrows && !rows[sel].dir)
		kind = pv_kind(rows[sel].name);
	int pane_x = w;
	if (kind && w >= 46)
		pane_x = w - 1 - w / 3;
	list_w = pane_x < w ? pane_x : 1 << 20;
	int lw = pane_x < w ? pane_x : w;

	for (int i = 0; i < list_rows; i++) {
		int idx = top + i;
		if (idx >= nrows)
			break;
		const struct row *r = &rows[idx];
		bool on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_SURFACE;

		ktui_draw_fill(krect(1, list_top + i, lw - 2, 1), bg);
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
		ktui_draw_text(4, list_top + i, lw - 18, label, fg, bg, KT_A_NONE);

		if (!r->dir) {
			char sz[32];
			human(r->size, sz, sizeof(sz));
			ktui_draw_text_right(0, list_top + i, lw - 2, sz,
					     on ? KT_SURFACE : KT_DIM, bg,
					     KT_A_NONE);
		}
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE — see kch_scrollbar. It goes
	 * on the list pane's own right edge, which is the divider before the
	 * preview when there is one and the window's border when there is
	 * not. A directory of forty files gave no sign that thirty of them
	 * were below the fold.
	 */
	kch_scrollbar(0, lw - 1, list_top, list_rows, nrows, top,
		      KT_SURFACE);

	if (pane_x < w) {
		ktui_draw_vline(pane_x - 1, list_top, list_rows, KT_G_VL,
				KT_DIM, KT_SURFACE);
		int pw2 = w - 1 - pane_x;
		int have = pv_want[0] && !strcmp(pv_want, pv_path);
		if (kind == 2 || (have && pv_state == 3)) {
			ktui_draw_text(pane_x + (pw2 > 10 ? (pw2 - 10) / 2 : 0),
				       list_top + list_rows / 2, pw2,
				       "no preview", KT_MID, KT_SURFACE,
				       KT_A_NONE);
		} else if (!have) {
			/* Still pending: the decode runs in the idle slot and
			 * may render a frame late, never block a click. */
			ktui_draw_text(pane_x + pw2 / 2,
				       list_top + list_rows / 2, pw2,
				       ktui_glyph[KT_G_ELLIPSIS], KT_MID,
				       KT_SURFACE, KT_A_NONE);
		} else {
			int x0 = pane_x + (pw2 - pv_cols) / 2;
			int y0 = list_top + (list_rows - pv_rows) / 2;
			if (x0 < pane_x)
				x0 = pane_x;
			if (y0 < list_top)
				y0 = list_top;
			for (int py = 0;
			     py < pv_rows && y0 + py < list_top + list_rows;
			     py++)
				for (int qx = 0;
				     qx < pv_cols && x0 + qx < w - 1; qx++)
					ktui_draw_text(x0 + qx, y0 + py, 1,
						       pv_glyph(pv_lum[py * PV_COLS + qx] / 255.0),
						       KT_TEXT, KT_SURFACE,
						       KT_A_NONE);
		}
	}

	int y = h - 3;
	if (edit_mode) {
		/* The editor takes the name/filter row; elided from the LEFT
		 * like the path above it — the tail is where the caret is. */
		char line[1100];
		snprintf(line, sizeof(line), "%s: %s",
			 edit_mode == ED_NEWDIR ? "New folder" : "Path",
			 edit_buf);
		/* `pw`, not `w - 4`: the same clamp the path row above needs,
		 * and for the same reason — a negative width sends the elide
		 * past the end of the string. */
		const char *el = line;
		if ((int)strlen(line) > pw)
			el = line + strlen(line) - pw;
		ktui_draw_text(2, y, pw, el, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (save_mode) {
		char line[320];
		snprintf(line, sizeof(line), "Name: %s", save_name);
		ktui_draw_text(2, y, w - 4, line, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (filter_label[0]) {
		char line[320];
		snprintf(line, sizeof(line), "Filter: %s",
			 filter_off ? "*" : filter_label);
		ktui_draw_text(2, y, w - 4, line, KT_MID, KT_SURFACE, KT_A_NONE);
	}

	/* The note takes the hint row when there is one: "that file exists" is
	 * worth more than a reminder of what Escape does, and it is the only
	 * row a dialog this size can spare. */
	if (note[0])
		ktui_draw_text(2, h - 2, w - 26, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	else
		ktui_draw_text(2, h - 2, w - 26,
			       edit_mode ? "Enter ok   Esc cancel"
			       : save_mode ? "Enter save   ^N folder   Esc cancel"
			       : browse_mode
				       ? "Enter open   ^H hidden   Esc close"
			       : multi_mode
				       ? "Space mark   ^H hidden   Esc cancel"
				       : "Enter open   ^H hidden   Esc cancel",
			       KT_MID, KT_SURFACE, KT_A_NONE);

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
	/* Browsing has nothing to cancel: the second button says Close, and
	 * both do the same thing a chooser's Cancel does. */
	const char *cancel = browse_mode ? "[ Close ]" : "[ Cancel ]";
	int okw = ktui_utf8_width(ok) + 2, cw = ktui_utf8_width(cancel);
	btn_ok_x = w - 2 - okw;
	btn_ok_end = btn_ok_x + okw;
	btn_cancel_x = btn_ok_x - 1 - cw;
	btn_cancel_end = btn_cancel_x + cw;
	btn_row = h - 2;
	if (btn_cancel_x > 26) {
		int cbg = hover_btn == 1 ? KT_DIM : KT_SURFACE;
		if (cbg != KT_SURFACE)
			ktui_draw_fill(krect(btn_cancel_x, btn_row, cw, 1), cbg);
		ktui_draw_text(btn_cancel_x, btn_row, cw, cancel, KT_TEXT, cbg,
			       KT_A_NONE);
		ktui_draw_text(btn_ok_x, btn_row, 1, "[",
			       hover_btn == 0 ? KT_TEXT : KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		/* The default action is already accent-filled, so its hover is
		 * the WARN fill — the same "brighter than rest, different from
		 * focus" rule the shared button bar keeps. */
		ktui_draw_text(btn_ok_x + 1, btn_row, okw - 2, ok, KT_SURFACE,
			       hover_btn == 0 ? KT_WARN : KT_ACCENT, KT_A_NONE);
		ktui_draw_text(btn_ok_end - 1, btn_row, 1, "]",
			       hover_btn == 0 ? KT_TEXT : KT_DIM, KT_SURFACE,
			       KT_A_NONE);
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
		/*
		 * An existing file's row ADOPTS its name first — the universal
		 * "click the file, hit Save" overwrite flow. Only when the
		 * name differs: the same name falls through to the ask-once
		 * below, so the second Enter is the confirmation, not another
		 * adoption.
		 */
		if (sel >= 0 && sel < nrows && !rows[sel].dir &&
		    strcmp(rows[sel].name, save_name) != 0) {
			snprintf(save_name, sizeof(save_name), "%s",
				 rows[sel].name);
			overwrite_ok = false;
			note[0] = '\0';
			return 0;
		}
		if (!save_name[0])
			return 0;
		/*
		 * ASK ONCE before naming a file that already exists. The
		 * application does the writing and most of them will not ask
		 * again — a Save dialog that silently answers with the name of
		 * somebody's existing document is how work is lost. A second
		 * Enter is the confirmation, which is why the note says so.
		 */
		char full[2048];
		snprintf(full, sizeof(full), "%s%s%s", cwd,
			 strcmp(cwd, "/") ? "/" : "", save_name);
		if (!overwrite_ok && access(full, F_OK) == 0) {
			overwrite_ok = true;
			snprintf(note, sizeof(note),
				 "%.60s exists — Enter again to replace it",
				 save_name);
			return 0;
		}
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
	if (browse_mode) {
		/* `kdos-appbox open` IS the answer to "what opens this here" —
		 * the same resolution the portal's OpenURI and a double-click
		 * on the desktop go through, so a boxed app is found by
		 * exactly the lookup a host one is. Nothing is printed: a
		 * browser has no caller waiting on stdout. */
		if (sel >= 0 && sel < nrows) {
			char full[2048];
			snprintf(full, sizeof(full), "%s%s%s", cwd,
				 strcmp(cwd, "/") ? "/" : "", rows[sel].name);
			const char *argv[] = { "kdos-appbox", "open", full,
					       NULL };
			sh_spawn(argv);
		}
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
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		/* One frame, offscreen, as text: see kdos-launcher --dump. This
		 * is the dialog every boxed application's Open goes through, so
		 * it is the one whose layout is worth checking without a
		 * compositor to hand. */
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--title") && i + 1 < argc)
			title = argv[++i];
		else if (!strcmp(argv[i], "--save"))
			save_mode = true;
		else if (!strcmp(argv[i], "--directory"))
			dir_mode = true;
		else if (!strcmp(argv[i], "--multiple"))
			multi_mode = true;
		/* `--browse [DIR]`: the directory is optional and positional
		 * so the Start menu's rows read as `kdos-pick --browse $HOME`
		 * rather than as a chooser invocation. */
		else if (!strcmp(argv[i], "--browse")) {
			browse_mode = true;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				start = argv[++i];
		}
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
				"[--multiple] [--browse [DIR]]\n"
				"                 [--title T] [--name N] "
				"[--dir D] "
				"[--filter 'Label:*.png *.jpg']\n"
				"                 [--dump] [--font F]\n");
			return 2;
		}
	}

	const char *home = getenv("HOME");
	snprintf(cwd, sizeof(cwd), "%s", start ? start : (home ? home : "/"));
	/* The window is named after the place it is showing, not after the
	 * verb: a browser called "Open File" reads as a dialog somebody is
	 * waiting on. Set only when --title did not, so a caller can still
	 * name it. */
	static char btitle[256];
	if (browse_mode && !strcmp(title, "Open File")) {
		const char *base = strrchr(cwd, '/');
		/* An explicit precision, not a bare %s: cwd is 1024 bytes and
		 * this buffer is 256, and gcc's format-truncation warning is
		 * an error in the gate. */
		snprintf(btitle, sizeof(btitle), "%.*s",
			 (int)sizeof(btitle) - 1,
			 base && base[1] ? base + 1 : cwd);
		title = btitle;
	}
	load_dir();

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(64, 22);
		draw(title);
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = 64,
		.rows = 22,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Files",
		.app_id = "kdos-pick",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-pick: no compositor or no layer-shell\n");
		return 2;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	int rc = 1;
	while (!kdisp_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). The
		 * file chooser is the dialog most likely to be open when one
		 * happens — every boxed app's Open reaches it through the
		 * portal. */
		sh_theme_poll();
		int list_rows = ktui_h - 5;
		if (list_rows < 1)
			list_rows = 1;
		kch_list_clamp(&top, sel, nrows, list_rows, sel_follow);
		sel_follow = 0;

		/* What the preview pane wants decoded, from the selection. */
		pv_want[0] = '\0';
		if (sel >= 0 && sel < nrows && !rows[sel].dir &&
		    pv_kind(rows[sel].name) == 1)
			snprintf(pv_want, sizeof(pv_want), "%s%s%s", cwd,
				 strcmp(cwd, "/") ? "/" : "", rows[sel].name);
		int pv_pending = pv_want[0] && strcmp(pv_want, pv_path) != 0;

		/* Browsing, the title FOLLOWS the folder — the window says
		 * where you are, and a browser whose name froze on the folder
		 * it opened in is a browser that lies after the first click. */
		if (browse_mode && title == btitle) {
			const char *base = strrchr(cwd, '/');
			snprintf(btitle, sizeof(btitle), "%.*s",
				 (int)sizeof(btitle) - 1,
				 base && base[1] ? base + 1 : cwd);
		}
		draw(title);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, pv_pending ? 15 : 1000)) {
			/* The idle slot: a decode never sits between an input
			 * event and its answer — a slow file renders late
			 * rather than making the dialog miss a click. */
			if (pv_pending)
				pv_decode(pv_want);
			/* The grid follows a configure only when the loop that
			 * owns the surface applies it: without this the buttons
			 * are placed off the visible surface while the hit test
			 * goes on accepting those columns. */
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

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
			/* The editor is keyboard-owned; a click mid-edit must
			 * not change the directory under a half-typed path. */
			if (edit_mode)
				continue;
			int row = ev.my - 2 + top;
			int on_row = ev.my >= 2 && ev.my < ktui_h - 3 &&
				     row >= 0 && row < nrows &&
				     ev.mx < list_w;
			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt;

				bt = kch_scrollbar_drag(ev.my);
				if (bt >= 0 && kch_scrollbar_grabbed() == 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				if (on_row) {
					sel = row;
					sel_follow = 1;
				}
				hover_btn = -1;
				if (ev.my == btn_row) {
					if (btn_ok_end > btn_ok_x &&
					    ev.mx >= btn_ok_x &&
					    ev.mx < btn_ok_end)
						hover_btn = 0;
					else if (btn_cancel_end > btn_cancel_x &&
						 ev.mx >= btn_cancel_x &&
						 ev.mx < btn_cancel_end)
						hover_btn = 1;
				}
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt;

				bt = kch_scrollbar_press(0, ev.mx, ev.my);
				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;
				int lr = ktui_h - 5 > 0 ? ktui_h - 5 : 1;
				if (!kch_list_wheel(up, &top, nrows, lr)) {
					sel += up ? -1 : 1;
					sel_follow = 1;
				}
			} else if (ev.btn == KT_MB_RIGHT) {
				enter_dir("..");
			} else if (ev.btn == KT_MB_MIDDLE && multi_mode &&
				   on_row && !rows[row].dir) {
				sel = row;
				sel_follow = 1;
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
					/* F6: one click on an existing file
					 * adopts its name; the second click is
					 * activate(), which then asks about
					 * replacing exactly that file. */
					if (save_mode && !rows[row].dir &&
					    strcmp(rows[row].name, save_name)) {
						snprintf(save_name,
							 sizeof(save_name), "%s",
							 rows[row].name);
						overwrite_ok = false;
						note[0] = '\0';
					}
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

		/* A key either moves the cursor or refills the list; both want
		 * the viewport to follow. */
		sel_follow = 1;
		int page = ktui_h - 5;
		if (page < 1)
			page = 1;

		if (edit_mode) {
			size_t n = strlen(edit_buf);
			if (ev.key == KT_K_ESC) {
				edit_mode = ED_NONE;
				note[0] = '\0';
			} else if (ev.key == KT_K_ENTER) {
				if (edit_mode == ED_NEWDIR) {
					if (!edit_buf[0] ||
					    strchr(edit_buf, '/')) {
						snprintf(note, sizeof(note),
							 "not a usable name");
					} else {
						char p[2048];
						snprintf(p, sizeof(p),
							 "%s%s%s", cwd,
							 strcmp(cwd, "/") ? "/" : "",
							 edit_buf);
						if (mkdir(p, 0755) == 0) {
							edit_mode = ED_NONE;
							load_dir();
							for (int i = 0; i < nrows; i++)
								if (!strcmp(rows[i].name, edit_buf)) {
									sel = i;
									break;
								}
						} else {
							snprintf(note, sizeof(note),
								 "cannot create %.60s: %s",
								 edit_buf,
								 strerror(errno));
						}
					}
				} else {
					/* Ctrl+L: chdir to what was typed, if
					 * it is a directory; say so if not. */
					char p[1024];
					if (edit_buf[0] == '~' &&
					    (edit_buf[1] == '/' || !edit_buf[1]))
						snprintf(p, sizeof(p), "%s%s",
							 home ? home : "",
							 edit_buf + 1);
					else
						snprintf(p, sizeof(p), "%s",
							 edit_buf);
					size_t pl = strlen(p);
					while (pl > 1 && p[pl - 1] == '/')
						p[--pl] = '\0';
					struct stat st;
					if (p[0] == '/' && stat(p, &st) == 0 &&
					    S_ISDIR(st.st_mode)) {
						snprintf(cwd, sizeof(cwd), "%s", p);
						edit_mode = ED_NONE;
						load_dir();
					} else {
						snprintf(note, sizeof(note),
							 "not a directory: %.80s",
							 edit_buf);
					}
				}
			} else if (ev.key == KT_K_BACKSPACE) {
				if (n)
					edit_buf[n - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   n + 1 < sizeof(edit_buf)) {
				edit_buf[n] = (char)ev.key;
				edit_buf[n + 1] = '\0';
			}
			continue;
		}

		if (ev.key == KT_K_ESC)
			break;
		if (ev.key == KT_K_UP) {
			sel--;
		} else if (ev.key == KT_K_DOWN) {
			sel++;
		} else if (ev.key == KT_K_PGUP) {
			sel -= page;
		} else if (ev.key == KT_K_PGDN) {
			sel += page;
		} else if (ev.key == KT_K_HOME) {
			sel = 0;
		} else if (ev.key == KT_K_END) {
			sel = nrows - 1;
		} else if (ev.key == 8) {
			/* Ctrl+H: xkb folds the modifier into the control code,
			 * so this is the character, not a chord to decode.
			 * BACKSPACE is 127 and is the save name's, not this. */
			show_hidden = !show_hidden;
			reload_keep_marks();
		} else if (ev.key == KT_K_F7 || ev.key == 14) {
			/* Ctrl+N / F7: Create Folder, in save mode — the one
			 * mode where the directory being missing is the
			 * reason the dialog is open. */
			if (save_mode) {
				edit_mode = ED_NEWDIR;
				edit_buf[0] = '\0';
				note[0] = '\0';
			}
		} else if (ev.key == 12) {
			/* Ctrl+L: type the path instead of walking it. */
			edit_mode = ED_PATH;
			snprintf(edit_buf, sizeof(edit_buf), "%s", cwd);
			note[0] = '\0';
		} else if (ev.key == 6) {
			/* Ctrl+F: the portal's filter, or everything. */
			if (npatterns) {
				filter_off = !filter_off;
				reload_keep_marks();
			}
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
			/* The name changed, so the answer to "may I replace
			 * that" is no longer about this file. */
			overwrite_ok = false;
			note[0] = '\0';
		} else if (ev.key >= 0x20 && ev.key < 0x7f) {
			/*
			 * Type-ahead. A directory can hold four thousand
			 * entries and this is the dialog every boxed
			 * application's Open goes through; arrowing to `zlib`
			 * is not a way to choose a file. Wraps from where the
			 * cursor is, so the same letter cycles.
			 */
			int lc = ev.key >= 'A' && ev.key <= 'Z' ? ev.key + 32
								: ev.key;
			for (int k = 1; k <= nrows; k++) {
				int i = (sel + k) % nrows;
				int f = rows[i].name[0];
				if (f >= 'A' && f <= 'Z')
					f += 32;
				if (f == lc) {
					sel = i;
					break;
				}
			}
		}

		if (sel < 0)
			sel = 0;
		if (sel >= nrows)
			sel = nrows ? nrows - 1 : 0;
	}

	kdisp_shutdown();
	fflush(stdout);
	return rc;
}

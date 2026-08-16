/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-settings — the knobs, on the grid
 *
 *   ╔═ Settings ═══════════════════════════════════════════════╗
 *   ║ Appearance │ accent           phosphor            live   ║
 *   ║ Session    │▸crt              55                  live   ║
 *   ║ Input      │ crt_scanlines    60                  live   ║
 *   ║ Apps       │ chrome_font      Terminus:pixel…     login  ║
 *   ╟────────────┴─────────────────────────────────────────────╢
 *   ║ the phosphor shader's strength; 0 is an honest off        ║
 *   ║ ←→ change  Enter edit  a apply  Esc close      [ Apply ]  ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * Every knob on this desktop was a text file and nothing else — which is the
 * right storage and the wrong interface for somebody who does not already know
 * the file exists. This is the OS/2-setup lineage: a category list, a form, and
 * no mode the mouse cannot reach.
 *
 * IT WRITES THE SAME TEXT FILES, AND PRESERVES THEM. comp.conf ships as a
 * commented essay about each key; a settings app that rewrote it would delete
 * the documentation the moment anybody touched a slider. Every write is a
 * line-by-line pass that replaces only the keys it owns and appends the ones
 * the file has never carried — comments, blank lines and unknown keys survive
 * verbatim — and it lands through temp + fsync + rename, because a half-written
 * comp.conf is a session that comes up wrong at the next login.
 *
 * AND EVERY FIELD SAYS WHEN IT TAKES EFFECT. `live` means the SIGHUP this
 * program sends is enough (kdos_conf_reload re-reads it); `login` means the key
 * is a child's argv or is read once at startup and the running session keeps
 * what it has. A settings surface that lied about that would be worse than
 * none: the user would change a thing, see nothing, and change it again.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

enum { CAT_APPEARANCE = 0, CAT_SESSION, CAT_INPUT, CAT_APPS, NCAT };

static const char *const CAT_NAMES[NCAT] = {
	"Appearance", "Session", "Input", "Apps"
};

/* Where a row's value is stored. ST_THEME is the accent, which is not a file
 * this program writes at all: `kdos theme` owns the whole palette pipeline
 * (icons, cursors, gtk.css, foot, btop, starship) and a second writer of the
 * accent would be a second thing to keep in agreement with it. */
enum { ST_NONE = 0, ST_COMP, ST_THEME };

/* When a change takes effect. */
enum { SC_NONE = 0, SC_LIVE, SC_LOGIN };

enum { FT_CHOICE = 0, FT_INT, FT_TEXT, FT_NOTE, FT_APP };

struct row {
	int cat;
	int type;
	int store;
	int scope;
	const char *key;
	const char *label;
	const char *const *choices;
	int nchoices;
	int min, max, step;
	const char *help;
	char val[192];
	char orig[192];
};

static const char *const YESNO[] = { "yes", "no" };
static const char *const ONOFF[] = { "on", "off" };
static const char *const LIDS[] = { "off", "lock", "suspend" };

/* The accent names, filled from libktui's own table at startup — the palette
 * lives in libkcolor and every consumer expands the same one. */
static const char *accents[8];
static int naccents;

static struct row rows[] = {
	/* ── Appearance ─────────────────────────────────────────────── */
	{ CAT_APPEARANCE, FT_CHOICE, ST_THEME, SC_LIVE, "accent", "accent",
	  NULL, 0, 0, 0, 0,
	  "runs `kdos theme <accent>`: host, box and window frames together",
	  "phosphor", "phosphor" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt", "crt",
	  NULL, 0, 0, 100, 5,
	  "the phosphor shader's strength; 0 is an honest off and gives the "
	  "fill rate back",
	  "55", "55" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt_scanlines",
	  "crt_scanlines", NULL, 0, 0, 100, 5,
	  "the scanlines on their own — the part people either love or want gone",
	  "60", "60" },
	{ CAT_APPEARANCE, FT_INT, ST_COMP, SC_LIVE, "crt_curve", "crt_curve",
	  NULL, 0, 0, 100, 5,
	  "tube curvature; nothing is cropped at any value, but it argues with "
	  "a character grid",
	  "0", "0" },
	{ CAT_APPEARANCE, FT_CHOICE, ST_COMP, SC_LIVE, "crt_fullscreen",
	  "crt_fullscreen", ONOFF, 2, 0, 0, 0,
	  "off skips the pass while a window is fullscreen — a film gets direct "
	  "scanout back",
	  "on", "on" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LIVE, "wallpaper", "wallpaper",
	  NULL, 0, 0, 0, 0,
	  "a PNG, scaled to cover and centred; the word `none` is an honest off",
	  "/usr/share/backgrounds/kdos/default-wallpaper.png",
	  "/usr/share/backgrounds/kdos/default-wallpaper.png" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LOGIN, "chrome_font",
	  "chrome_font", NULL, 0, 0, 0, 0,
	  "the chrome's font, as a PIXEL size — Terminus:pixelsize=64 is the "
	  "4K answer",
	  "Terminus:pixelsize=32", "Terminus:pixelsize=32" },
	{ CAT_APPEARANCE, FT_TEXT, ST_COMP, SC_LOGIN, "clock_format",
	  "clock_format", NULL, 0, 0, 0, 0,
	  "the panel clock as a strftime format; it reaches the panel on its "
	  "command line",
	  "%H:%M", "%H:%M" },

	/* ── Session ────────────────────────────────────────────────── */
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_dim", "idle_dim",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the dim; 0 never. In a VM all three default to 0 — "
	  "writing one here means it",
	  "300", "300" },
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_lock", "idle_lock",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the lock, measured from the last activity, not from the dim",
	  "600", "600" },
	{ CAT_SESSION, FT_INT, ST_COMP, SC_LIVE, "idle_off", "idle_off",
	  NULL, 0, 0, 86400, 60,
	  "seconds to the outputs going off; activity powers them back on and "
	  "never unlocks",
	  "900", "900" },
	{ CAT_SESSION, FT_CHOICE, ST_COMP, SC_LIVE, "lid_close", "lid_close",
	  LIDS, 3, 0, 0, 0,
	  "what closing the laptop lid does; in a VM the default is off",
	  "suspend", "suspend" },
	{ CAT_SESSION, FT_CHOICE, ST_COMP, SC_LOGIN, "panel_bottom",
	  "panel_bottom", YESNO, 2, 0, 0, 0,
	  "the second panel: window list, pager and show-desktop. A second "
	  "process, started at login",
	  "yes", "yes" },
	{ CAT_SESSION, FT_CHOICE, ST_COMP, SC_LOGIN, "desktop_icons",
	  "desktop_icons", YESNO, 2, 0, 0, 0,
	  "~/Desktop drawn on the background layer, with Home and Trash pinned "
	  "last",
	  "yes", "yes" },
	{ CAT_SESSION, FT_CHOICE, ST_COMP, SC_LOGIN, "panel_autohide",
	  "panel_autohide", YESNO, 2, 0, 0, 0,
	  "the panels retreat to an edge strip until the pointer reaches them",
	  "no", "no" },

	/* ── Input ──────────────────────────────────────────────────────
	 *
	 * NOTES, and no fields. Nothing here writes the keyboard's settings,
	 * because nothing on this system would read what it wrote: the layout
	 * comes from /etc/keymap by way of kdos-desktop, and every keyboard knob
	 * the compositor has is rc.xml's. A row that wrote a file no program
	 * opens is exactly the "change a thing, see nothing" this surface
	 * promised not to be, so the category says where the knobs really are. */
	{ CAT_INPUT, FT_NOTE, ST_NONE, SC_NONE, NULL, "where layout comes from",
	  NULL, 0, 0, 0, 0,
	  "/etc/keymap is the console map; kdos-desktop maps it to "
	  "XKB_DEFAULT_LAYOUT at login, and rc.xml's <keyboard><layout> "
	  "overrides both inside the compositor",
	  "", "" },
	{ CAT_INPUT, FT_NOTE, ST_NONE, SC_NONE, NULL, "where key repeat comes from",
	  NULL, 0, 0, 0, 0,
	  "rc.xml's <keyboard><repeatRate> and <repeatDelay>; kdos-comp reads "
	  "them at startup and on the Reconfigure a SIGHUP is",
	  "", "" },
};

#define NROWS ((int)(sizeof(rows) / sizeof(rows[0])))

/* The Apps category: what ~/.config/mimeapps.list currently says, one row per
 * declared default. Re-read on the idle tick, because the chooser that changes
 * one of them is a SEPARATE process and cannot tell us it wrote. */
#define MAX_APPS 64

struct app_row {
	char mime[128];
	char id[160];
};

static struct app_row apps[MAX_APPS];
static int napps;

static int cat, sel, top, pane;		/* pane 0 = categories, 1 = fields */
static char note[192];
static int editing, quit_armed;
static char edit_buf[256];

/* ── the files ─────────────────────────────────────────────────────────── */

static void cfg_path(const char *leaf, char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%.400s/kdos/%.60s", cfg, leaf);
	else
		snprintf(out, n, "%.400s/.config/kdos/%.60s", kb_home_dir(),
			 leaf);
}

/*
 * `key = value`, the shape comp.conf uses. A commented line is a comment:
 * comp.conf documents every key as `#crt = 55`, and reading those as settings
 * would show a fresh install a screen of values nothing is using.
 */
static void load_kv(const char *path, int store)
{
	char *data = kb_read_all(path, NULL);

	if (!data)
		return;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';

		char *k = line;
		while (*k == ' ' || *k == '\t')
			k++;
		if (!*k || *k == '#')
			continue;
		char *eq = strchr(k, '=');
		if (!eq)
			continue;
		char *v = eq + 1;
		while (eq > k && (eq[-1] == ' ' || eq[-1] == '\t'))
			eq--;
		*eq = '\0';
		while (*v == ' ' || *v == '\t')
			v++;
		char *e = v + strlen(v);
		while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
			*--e = '\0';

		for (int i = 0; i < NROWS; i++) {
			if (rows[i].store != store || !rows[i].key ||
			    strcmp(rows[i].key, k))
				continue;
			kb_strlcpy(rows[i].val, v, sizeof(rows[i].val));
			kb_strlcpy(rows[i].orig, v, sizeof(rows[i].orig));
		}
	}
	free(data);
}

static void load_apps(void)
{
	char path[700];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char *data;
	int in_sec = 0;

	napps = 0;
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfg);
	else
		snprintf(path, sizeof(path), "%.500s/.config/mimeapps.list",
			 kb_home_dir());
	data = kb_read_all(path, NULL);
	if (!data)
		return;

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		if (nl)
			*nl = '\0';
		if (line[0] == '[') {
			in_sec = !strncmp(line, "[Default Applications]", 22);
			continue;
		}
		if (!in_sec || !line[0] || line[0] == '#')
			continue;
		char *eq = strchr(line, '=');
		if (!eq || napps >= MAX_APPS)
			continue;
		*eq = '\0';
		kb_strlcpy(apps[napps].mime, line, sizeof(apps[0].mime));
		kb_strlcpy(apps[napps].id, eq + 1, sizeof(apps[0].id));
		napps++;
	}
	free(data);
}

static void load_all(void)
{
	char path[700];

	for (int i = 0; i < ktui_ntheme && naccents < 8; i++)
		accents[naccents++] = ktui_themes[i].name;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].store == ST_THEME) {
			rows[i].choices = accents;
			rows[i].nchoices = naccents;
		}

	/* The accent in force is the one-word state file every other surface
	 * reads, not a comp.conf key. */
	char accent[64] = "";
	const char *cache = getenv("XDG_CACHE_HOME");
	if (cache && *cache)
		snprintf(path, sizeof(path), "%.500s/kdos/theme", cache);
	else
		snprintf(path, sizeof(path), "%.500s/.cache/kdos/theme",
			 kb_home_dir());
	if (kb_read_line_file(path, accent, sizeof(accent)) > 0 && accent[0])
		for (int i = 0; i < NROWS; i++)
			if (rows[i].store == ST_THEME) {
				kb_strlcpy(rows[i].val, accent,
					   sizeof(rows[i].val));
				kb_strlcpy(rows[i].orig, accent,
					   sizeof(rows[i].orig));
			}

	cfg_path("comp.conf", path, sizeof(path));
	load_kv(path, ST_COMP);
	load_apps();
}

/* ── writing ───────────────────────────────────────────────────────────── */

/*
 * Rewrite `path`, replacing every key this store owns that has changed and
 * appending the ones the file never carried. Everything else — comments, blank
 * lines, keys nothing here knows about — is copied byte for byte.
 *
 * A commented `#crt = 55` is left alone deliberately: it is the file's
 * documentation of the default, the appended `crt = 40` below it wins, and the
 * two together read as "the default was this, I chose that".
 */
static int write_kv(const char *path, int store)
{
	char tmp[720], dir[700];
	char *old = kb_read_all(path, NULL);
	KbBuf out = {0};
	int done[NROWS];
	int rc = -1;

	/* A file that EXISTS and did not read is not an empty one. kb_read_all
	 * answers NULL to ENOENT and to EACCES alike, and taking the second for
	 * "no file yet" would replace a comp.conf somebody root-owns by hand
	 * with the two lines this program changed. */
	if (!old && access(path, F_OK) == 0)
		return -1;

	for (int i = 0; i < NROWS; i++)
		done[i] = 0;

	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}

	for (char *line = old, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : line + strlen(line);
		size_t len = (size_t)(next - line);

		const char *t = line;
		while (*t == ' ' || *t == '\t')
			t++;
		int hit = -1;
		for (int i = 0; i < NROWS && hit < 0; i++) {
			if (rows[i].store != store || !rows[i].key ||
			    !strcmp(rows[i].val, rows[i].orig))
				continue;
			size_t kl = strlen(rows[i].key);
			if (!strncmp(t, rows[i].key, kl) &&
			    (t[kl] == ' ' || t[kl] == '\t' || t[kl] == '='))
				hit = i;
		}
		if (hit >= 0) {
			kb_buf_printf(&out, "%s = %s\n", rows[hit].key,
				      rows[hit].val);
			done[hit] = 1;
		} else {
			kb_buf_add(&out, line, len);
		}
	}

	for (int i = 0; i < NROWS; i++) {
		if (rows[i].store != store || !rows[i].key || done[i] ||
		    !strcmp(rows[i].val, rows[i].orig))
			continue;
		if (out.n && out.p[out.n - 1] != '\n')
			kb_buf_add(&out, "\n", 1);
		kb_buf_printf(&out, "%s = %s\n", rows[i].key, rows[i].val);
	}

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *f = fopen(tmp, "w");
	if (f) {
		int ok = out.n == 0 || fwrite(out.p, 1, out.n, f) == out.n;
		if (ok && fflush(f) == 0 && fsync(fileno(f)) == 0 &&
		    fclose(f) == 0) {
			f = NULL;
			if (rename(tmp, path) == 0) {
				rc = 0;
				/* The directory entry is what the rename
				 * created, and it has to reach the disk too —
				 * the fsync people leave out. */
				if (slash) {
					int d = open(dir, O_RDONLY);
					if (d >= 0) {
						fsync(d);
						close(d);
					}
				}
			}
		}
		if (f)
			fclose(f);
		if (rc != 0)
			remove(tmp);
	}
	kb_buf_free(&out);
	free(old);
	return rc;
}

/*
 * The signal `kdos theme` already sends, for the same reason and by the same
 * route: pkill -x, exact, because `kdos-comp` must not also match
 * `kdos-desktop-start` — an unhandled SIGHUP kills a shell, and those two
 * /bin/sh scripts own the session.
 */
static void sighup_comp(void)
{
	KbArgv a = {0};

	if (!kb_have_prog("pkill"))
		return;
	kb_argv_add(&a, "pkill");
	kb_argv_add(&a, "-x");
	kb_argv_add(&a, "-HUP");
	kb_argv_add(&a, "kdos-comp");
	kb_argv_end(&a);
	kb_run(&a);		/* no session running is not an error */
}

static int dirty_count(int store)
{
	int n = 0;

	for (int i = 0; i < NROWS; i++)
		if (rows[i].store == store && strcmp(rows[i].val, rows[i].orig))
			n++;
	return n;
}

static void apply(void)
{
	char path[700];
	int comp = dirty_count(ST_COMP), theme = dirty_count(ST_THEME);
	int live = 0, login = 0, failed = 0;

	for (int i = 0; i < NROWS; i++) {
		if (!strcmp(rows[i].val, rows[i].orig))
			continue;
		if (rows[i].scope == SC_LIVE)
			live++;
		else if (rows[i].scope == SC_LOGIN)
			login++;
	}

	if (comp) {
		cfg_path("comp.conf", path, sizeof(path));
		if (write_kv(path, ST_COMP) != 0)
			failed++;
		else
			sighup_comp();
	}
	if (theme) {
		for (int i = 0; i < NROWS; i++) {
			if (rows[i].store != ST_THEME ||
			    !strcmp(rows[i].val, rows[i].orig))
				continue;
			/*
			 * Detached, not waited for: `kdos theme` regenerates
			 * ~10 000 icons and every cursor, which is seconds of
			 * work, and it sends its own SIGHUP to the whole
			 * desktop when it is done. A settings window frozen for
			 * the duration would look like the crash it is not.
			 */
			KbArgv a = {0};
			kb_argv_add(&a, "kdos");
			kb_argv_add(&a, "theme");
			kb_argv_add(&a, rows[i].val);
			kb_argv_end(&a);
			kb_run_detach(&a);
		}
	}

	if (failed) {
		snprintf(note, sizeof(note),
			 "could not write %d file(s) — nothing else changed",
			 failed);
		return;
	}
	for (int i = 0; i < NROWS; i++)
		kb_strlcpy(rows[i].orig, rows[i].val, sizeof(rows[i].orig));

	if (!live && !login && !theme)
		snprintf(note, sizeof(note), "nothing to apply");
	else if (login)
		snprintf(note, sizeof(note),
			 "applied: %d now, %d at the next login", live + theme,
			 login);
	else
		snprintf(note, sizeof(note), "applied: %d now", live + theme);
	quit_armed = 0;
}

/*
 * Closing with edits that were never applied asks once — Escape, the right
 * button and the compositor's own dismissal all come through here. The staging
 * IS the safety in this program (nothing has been written yet), so the one
 * thing it must not do is discard silently; the second press does close.
 */
static int try_quit(void)
{
	if (!dirty_count(ST_COMP) && !dirty_count(ST_THEME))
		return 1;
	if (quit_armed)
		return 1;
	quit_armed = 1;
	snprintf(note, sizeof(note),
		 "unapplied changes — again to discard, a to apply");
	return 0;
}

/* ── the rows of the selected category ─────────────────────────────────── */

static int cat_rows(void)
{
	int n = 0;

	if (cat == CAT_APPS)
		return napps ? napps : 1;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].cat == cat)
			n++;
	return n;
}

/* Index into rows[] of the n'th field of the current category, or -1. */
static int cat_row(int n)
{
	int k = 0;

	if (cat == CAT_APPS)
		return -1;
	for (int i = 0; i < NROWS; i++)
		if (rows[i].cat == cat && k++ == n)
			return i;
	return -1;
}

static void cycle(struct row *r, int dir)
{
	if (r->type == FT_CHOICE && r->nchoices) {
		int at = 0;
		for (int i = 0; i < r->nchoices; i++)
			if (!strcmp(r->val, r->choices[i]))
				at = i;
		at = (at + dir + r->nchoices) % r->nchoices;
		kb_strlcpy(r->val, r->choices[at], sizeof(r->val));
	} else if (r->type == FT_INT) {
		int v = atoi(r->val) + dir * (r->step ? r->step : 1);
		if (v < r->min)
			v = r->min;
		if (v > r->max)
			v = r->max;
		snprintf(r->val, sizeof(r->val), "%d", v);
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

#define CATW 13

/*
 * A value too wide for its column, marked as such — libktui clips silently,
 * and a clipped value is indistinguishable from the real one. Measured on a
 * booted ISO: `wallpaper` read `/usr/share/backgrounds/kdos/`, which is a
 * directory that exists, so nothing about it looked wrong.
 *
 * A PATH keeps its TAIL, because the file name is the part being identified
 * and every wallpaper on this machine shares the first three components.
 * Anything else keeps its head. The glyph is one cell of the reserved width,
 * never an extra one.
 */
static const char *elide(const char *s, int width, char *buf, size_t cap)
{
	if (width < 2 || ktui_utf8_width(s) <= width)
		return s;

	/*
	 * Cut at a SEPARATOR, not at whatever character the width lands on:
	 * the first cut that fit produced `…kgrounds/kdos/default-wallpaper.png`,
	 * which invents a directory. Left to right, the first `/` whose tail
	 * fits is the longest one that does. A single component too long for
	 * the column falls through to the plain head cut below.
	 */
	for (const char *p = strchr(s, '/'); p; p = strchr(p + 1, '/')) {
		if (ktui_utf8_width(p) <= width - 1) {
			snprintf(buf, cap, "…%s", p);
			return buf;
		}
	}

	size_t n = 0;
	int cells = 0;
	while (s[n] && cells < width - 1) {
		/* Step whole UTF-8 sequences: a half-copied one is a broken
		 * glyph, and this is the same rule ktui_utf8_width counts by. */
		size_t len = 1;
		while ((s[n + len] & 0xc0) == 0x80)
			len++;
		char one[8];
		snprintf(one, sizeof(one), "%.*s", (int)len, s + n);
		cells += ktui_utf8_width(one);
		if (cells > width - 1)
			break;
		n += len;
	}
	snprintf(buf, cap, "%.*s…", (int)n, s);
	return buf;
}

static int btn_x, btn_end, btn_row;

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int pane_rows = h - 5;
	int n = cat_rows();

	if (pane_rows < 1)
		pane_rows = 1;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " Settings ", KT_ACCENT, KT_SURFACE, 1);
	ktui_draw_vline(CATW + 1, 1, pane_rows, KT_G_VL, KT_DIM, KT_SURFACE);

	for (int i = 0; i < NCAT && i < pane_rows; i++) {
		int on = i == cat;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? (pane == 0 ? KT_ACCENT : KT_DIM) : KT_SURFACE;
		if (on)
			ktui_draw_fill(krect(1, 1 + i, CATW, 1), bg);
		ktui_draw_text(2, 1 + i, CATW - 1, CAT_NAMES[i],
			       on ? fg : KT_TEXT, bg, KT_A_NONE);
	}

	int fx = CATW + 3;
	int fw = w - fx - 1;
	if (fw < 8)
		fw = 8;

	for (int i = 0; i < pane_rows; i++) {
		int idx = top + i;
		if (idx >= n)
			break;
		int y = 1 + i;
		int on = idx == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? (pane == 1 ? KT_ACCENT : KT_DIM) : KT_SURFACE;

		if (on)
			ktui_draw_fill(krect(fx - 1, y, w - fx, 1), bg);

		if (cat == CAT_APPS) {
			if (!napps) {
				ktui_draw_text(fx, y, fw,
					       "no default handlers are "
					       "recorded yet",
					       KT_DIM, KT_SURFACE, KT_A_NONE);
				break;
			}
			ktui_draw_text(fx, y, fw / 2, apps[idx].mime, fg, bg,
				       KT_A_NONE);
			ktui_draw_text(fx + fw / 2, y, fw - fw / 2 - 1,
				       apps[idx].id, on ? fg : KT_MID, bg,
				       KT_A_NONE);
			continue;
		}

		int ri = cat_row(idx);
		if (ri < 0)
			continue;
		const struct row *r = &rows[ri];
		if (r->type == FT_NOTE) {
			ktui_draw_text(fx, y, fw, r->label, on ? fg : KT_MID, bg,
				       KT_A_NONE);
			continue;
		}

		char label[64];
		snprintf(label, sizeof(label), "%s%s", r->label,
			 strcmp(r->val, r->orig) ? " *" : "");
		ktui_draw_text(fx, y, 18, label, fg, bg, KT_A_NONE);

		/* The value, then the scope tag hard against the right edge:
		 * `live` and `login` are the two answers to "did that do
		 * anything", and they belong on the row that raises it. */
		const char *tag = r->scope == SC_LIVE ? "live" : "login";
		int vw = fw - 20 - 7;
		if (vw < 4)
			vw = 4;
		char shown[256];
		ktui_draw_text(fx + 19, y, vw,
			       elide(r->val[0] ? r->val : "(unset)", vw,
				     shown, sizeof(shown)),
			       on ? fg : (r->val[0] ? KT_MID : KT_DIM), bg,
			       KT_A_NONE);
		ktui_draw_text_right(0, y, w - 2, tag,
				     on ? fg : (r->scope == SC_LIVE ? KT_MID
							            : KT_DIM),
				     bg, KT_A_NONE);
	}

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	/* The junction where the category divider meets the rule under it —
	 * without it the two panes read as one column of text with a line
	 * through the middle of it. */
	ktui_draw_text(CATW + 1, h - 4, 1, ktui_glyph[KT_G_TEE_B], KT_DIM,
		       KT_SURFACE, KT_A_NONE);

	/* The help row: what the selected field is, and what it costs. */
	if (editing) {
		char line[320];
		snprintf(line, sizeof(line), "%s: %s",
			 cat_row(sel) >= 0 ? rows[cat_row(sel)].label : "value",
			 edit_buf);
		const char *el = line;
		if ((int)strlen(line) > w - 4)
			el = line + strlen(line) - (size_t)(w - 4);
		ktui_draw_text(2, h - 3, w - 4, el, KT_TEXT, KT_SURFACE,
			       KT_A_UNDERLINE);
	} else if (note[0]) {
		ktui_draw_text(2, h - 3, w - 4, note, KT_WARN, KT_SURFACE,
			       KT_A_NONE);
	} else if (cat == CAT_APPS) {
		ktui_draw_text(2, h - 3, w - 4,
			       "Enter opens the chooser for this type — it "
			       "writes the default itself",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
	} else {
		int ri = cat_row(sel);
		ktui_draw_text(2, h - 3, w - 4,
			       ri >= 0 && rows[ri].help ? rows[ri].help : "",
			       KT_DIM, KT_SURFACE, KT_A_NONE);
	}

	int pending = dirty_count(ST_COMP) + dirty_count(ST_THEME);
	/* The arrows come from the glyph table: the console font has ◀ ▶ and
	 * has no ← →, which is why ktui_glyph carries that pair. */
	char hint[96];
	if (editing)
		snprintf(hint, sizeof(hint), "Enter ok   Esc cancel");
	else if (cat == CAT_APPS)
		snprintf(hint, sizeof(hint),
			 "Enter chooser   Tab panes   Esc close");
	else
		snprintf(hint, sizeof(hint),
			 "%s%s change   Enter edit   a apply   Esc close",
			 ktui_glyph[KT_G_LEFT], ktui_glyph[KT_G_RIGHT]);
	ktui_draw_text(2, h - 2, w - 16, hint, KT_DIM, KT_SURFACE, KT_A_NONE);

	/* One button, and it is the one irreversible thing on the screen. */
	char blabel[24];
	snprintf(blabel, sizeof(blabel), " Apply%s ", pending ? " *" : "");
	int bw = ktui_utf8_width(blabel) + 2;
	btn_x = w - 2 - bw;
	btn_end = btn_x + bw;
	btn_row = h - 2;
	if (btn_x > 24) {
		ktui_draw_text(btn_x, btn_row, 1, "[", KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(btn_x + 1, btn_row, bw - 2, blabel,
			       pending ? KT_SURFACE : KT_DIM,
			       pending ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		ktui_draw_text(btn_end - 1, btn_row, 1, "]", KT_DIM, KT_SURFACE,
			       KT_A_NONE);
	} else {
		btn_x = btn_end = 0;
	}
	ktui_draw_flush();
}

/*
 * `--dump-cells`: one line per painted cell, codepoint and colour SLOT, which
 * is what a golden frame has to compare — a plain text dump proves the layout
 * and says nothing about whether the surface is wearing the palette.
 *
 * Through the backend vtable rather than ktui_offscreen_init(), because the
 * cell buffer is private to libktui and offscreen mode short-circuits the
 * flush entirely. Same seam as keys.c, per front end.
 */
static int cap_w = 72, cap_h = 20;

static void cap_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)prev;
	(void)ff;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const KtuiCell *c = &cur[y * w + x];
			if (!c->ch || c->ch == ' ' || c->ch == KTUI_WIDE_CONT)
				continue;
			printf("%d %d U+%04X %d %d %d\n", y, x, c->ch, c->fg,
			       c->bg, c->attr);
		}
}

static int cap_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static void cap_size(int *w, int *h)
{
	*w = cap_w;
	*h = cap_h;
}

static int cap_caps(void)
{
	return KT_CAP_UTF8 | KT_CAP_TRUECOLOR;
}

static const KtuiBackend cap_backend = {
	"dump-cells", cap_flush, cap_poll, cap_size, cap_caps
};

/* ── main ──────────────────────────────────────────────────────────────── */

/* Enter on a row: a choice cycles, an int or a text opens the editor, an Apps
 * row hands the type to the chooser that owns default handlers. */
static void activate(void)
{
	if (cat == CAT_APPS) {
		if (!napps || sel < 0 || sel >= napps)
			return;
		const char *argv[4] = { "kdos-openwith", "--mime",
					apps[sel].mime, NULL };
		pid_t p = fork();
		if (p == 0) {
			if (fork() == 0) {
				setsid();
				execvp(argv[0], (char *const *)argv);
				_exit(127);
			}
			_exit(0);
		}
		if (p > 0) {
			int st;
			waitpid(p, &st, 0);
		}
		snprintf(note, sizeof(note),
			 "chooser opened for %.60s — this list re-reads itself",
			 apps[sel].mime);
		return;
	}

	int ri = cat_row(sel);
	if (ri < 0 || rows[ri].type == FT_NOTE)
		return;
	if (rows[ri].type == FT_CHOICE) {
		cycle(&rows[ri], 1);
		return;
	}
	editing = 1;
	kb_strlcpy(edit_buf, rows[ri].val, sizeof(edit_buf));
	note[0] = '\0';
}

int settings_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0, cells = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-cells"))
			cells = 1;
		/* A golden frame is named for its size, so the harness has to
		 * be able to ask for one. */
		else if (!strcmp(argv[i], "--dump-size") && i + 1 < argc) {
			int dw, dh;
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) == 2 &&
			    dw > 23 && dh > 5) {
				cap_w = dw;
				cap_h = dh;
			}
		} else {
			fprintf(stderr, "usage: kdos-settings "
					"[--dump|--dump-cells] [--dump-size WxH]\n"
					"                     [--font NAME]\n");
			return 2;
		}
	}

	load_all();
	pane = 1;

	if (dump || cells) {
		sh_theme_from_cache();
		if (cells) {
			ktui_backend_set(&cap_backend);
			ktui_draw_init();
			draw();
			return 0;
		}
		ktui_offscreen_init(cap_w, cap_h);
		draw();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 72,
		.rows = 20,
		.app_id = "kdos-settings",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-settings: no compositor or no "
				"layer-shell\n");
		return 2;
	}
	ktui_draw_init();

	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int pane_rows = ktui_h - 5;
		int n = cat_rows();

		if (pane_rows < 1)
			pane_rows = 1;
		if (sel >= n)
			sel = n ? n - 1 : 0;
		if (sel < 0)
			sel = 0;
		if (sel < top)
			top = sel;
		if (sel >= top + pane_rows)
			top = sel - pane_rows + 1;

		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			/* The chooser is another process; the only way to know
			 * it wrote is to look again. */
			if (cat == CAT_APPS && !editing)
				load_apps();
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + top;
			int in_cats = ev.mx <= CATW && ev.my >= 1 &&
				      ev.my - 1 < NCAT;
			int in_fields = ev.mx > CATW + 1 && ev.my >= 1 &&
					ev.my < ktui_h - 4 && row >= 0 &&
					row < n;
			if (ev.press == KT_MP_DRAG) {
				if (in_fields && !editing) {
					pane = 1;
					sel = row;
				}
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel--;
			} else if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel++;
			} else if (ev.btn == KT_MB_RIGHT) {
				if (try_quit())
					break;
			} else if (ev.btn == KT_MB_LEFT) {
				if (btn_end > btn_x && ev.my == btn_row &&
				    ev.mx >= btn_x && ev.mx < btn_end) {
					apply();
				} else if (in_cats && !editing) {
					if (ev.my - 1 != cat) {
						cat = ev.my - 1;
						sel = 0;
						top = 0;
						if (cat == CAT_APPS)
							load_apps();
					}
					pane = 0;
				} else if (in_fields && !editing) {
					/* A click on the row that is already
					 * selected activates it — pick.c's
					 * rule, so one hand learns one thing. */
					if (row == sel && pane == 1)
						activate();
					pane = 1;
					sel = row;
				}
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		if (editing) {
			int ri = cat_row(sel);
			size_t len = strlen(edit_buf);
			if (ev.key == KT_K_ESC) {
				editing = 0;
			} else if (ev.key == KT_K_ENTER) {
				if (ri >= 0) {
					if (rows[ri].type == FT_INT) {
						int v = atoi(edit_buf);
						if (v < rows[ri].min)
							v = rows[ri].min;
						if (v > rows[ri].max)
							v = rows[ri].max;
						snprintf(rows[ri].val,
							 sizeof(rows[ri].val),
							 "%d", v);
					} else {
						kb_strlcpy(rows[ri].val,
							   edit_buf,
							   sizeof(rows[ri].val));
					}
				}
				editing = 0;
			} else if (ev.key == KT_K_BACKSPACE) {
				if (len)
					edit_buf[len - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   len + 1 < sizeof(edit_buf)) {
				edit_buf[len] = (char)ev.key;
				edit_buf[len + 1] = '\0';
			}
			continue;
		}

		if (ev.key == KT_K_ESC) {
			if (!try_quit())
				continue;
			break;
		}
		quit_armed = 0;

		int ri = cat_row(sel);
		switch (ev.key) {
		case KT_K_TAB:
			pane = !pane;
			break;
		case KT_K_UP:
			if (pane == 0) {
				if (cat > 0) {
					cat--;
					sel = top = 0;
					if (cat == CAT_APPS)
						load_apps();
				}
			} else {
				sel--;
			}
			break;
		case KT_K_DOWN:
			if (pane == 0) {
				if (cat < NCAT - 1) {
					cat++;
					sel = top = 0;
					if (cat == CAT_APPS)
						load_apps();
				}
			} else {
				sel++;
			}
			break;
		case KT_K_LEFT:
			if (pane == 1 && ri >= 0)
				cycle(&rows[ri], -1);
			else
				pane = 0;
			break;
		case KT_K_RIGHT:
			if (pane == 0)
				pane = 1;
			else if (ri >= 0)
				cycle(&rows[ri], 1);
			break;
		case KT_K_PGUP:
			sel -= pane_rows;
			break;
		case KT_K_PGDN:
			sel += pane_rows;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
			sel = n - 1;
			break;
		case KT_K_ENTER:
			if (pane == 0)
				pane = 1;
			else
				activate();
			break;
		case 'a':
			apply();
			break;
		default:
			break;
		}
	}

	kwl_shutdown();
	return 0;
}

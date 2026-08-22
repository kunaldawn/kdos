/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-start — the Start menu
 *
 *   ┌ kdos@kdos ──────────────────────┬────────────────────────┐
 *   │▸▪ Firefox                       │ ▪ Home                 │
 *   │ ▪ Files                         │ ▪ Documents            │
 *   │ ────────────────────────────────│ ▪ Downloads            │
 *   │ ▪ GIMP                          │ ────────────────────── │
 *   │ ▪ foot                          │ ▪ Settings             │
 *   │                                 │ ▪ Displays             │
 *   │ ▸ All Programs                ▶ │ ▪ Network              │
 *   ├─────────────────────────────────┴────────────────────────┤
 *   │ gim_                        [ Log Off ] [ Shut Down ]    │
 *   └──────────────────────────────────────────────────────────┘
 *
 * THERE WERE THREE FRONT DOORS AND NO DOOR. `kdos-menu` (three cascading
 * columns off a menu bar), `kdos-launcher` (a full-screen search) and the root
 * menu's Applications entry are three answers to "start a program", and none
 * of them was the one a person aims at. This is the one they aim at, and the
 * other two keep the jobs they are actually good at: the launcher is the
 * keyboard's route (`W-d`) and kdos-menu is the window manager's and the
 * scriptable one.
 *
 * THE LEFT COLUMN IS WHAT YOU USE, NOT WHAT IS INSTALLED. Pinned entries above
 * the rule and most-frequent below it, from the usage count apps.c records —
 * which nothing on this desktop had before, and without which this column is
 * just an alphabetical list with fewer entries. All Programs opens the
 * category list IN PLACE rather than as a cascade: a cascade needs a surface
 * per level and a pointer-tracking policy to decide when a level goes away,
 * and on a character grid it buys nothing.
 *
 * TYPING SEARCHES, and it searches the same index the launcher does. There is
 * no mode to enter and no field to click into — the first printable character
 * turns the left column into results, Escape empties it, and a second Escape
 * closes the menu. That is the one thing this shape got right in 2001 and
 * every desktop since has kept.
 *
 * IT IS ANCHORED TO THE BOTTOM-LEFT CORNER, which is a libkwl addition
 * (KWL_CORNER_BOTTOM_LEFT): a menu belonging to a bar on the bottom edge has
 * to grow upwards from it, and a client cannot express that by anchoring TOP
 * with a computed margin because it does not know the output's pixel height.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kicon.h"
#include "kwl.h"
#include "shell.h"

#define ST_COLS 66
#define ST_ROWS 22
#define ST_LEFT_W 38
#define ST_MAX_ROWS 128

/* What the left column is showing. */
enum {
	ST_MAIN = 0,	/* pinned, frequent, All Programs */
	ST_CATS,	/* the category list              */
	ST_CAT,		/* one category's applications    */
	ST_SEARCH,	/* results for what was typed     */
};

/*
 * One row of either column. `app` is set for an application; everything else
 * is a fixed entry whose action is an argv, and the two are deliberately the
 * same struct — a Start menu whose Places rows went through different code
 * from its application rows is a Start menu with two hover behaviours.
 */
struct row {
	char label[64];
	const char *icon;		/* an icon NAME, or NULL          */
	const struct sh_app *app;	/* set for an application         */
	const char *argv[6];		/* set for a fixed entry          */
	int rule;			/* a separator, not a row         */
	int submenu;			/* opens the categories, or a cat */
	int back;			/* returns to the level above     */
	int pinned;			/* on the quick-launch row        */
	/* What this row answers to besides its label, so a search for `wifi`
	 * finds Network. Resolved at BUILD time — a lookup per row per frame
	 * would be a file open per row per frame. */
	const char *keys;
	const char *confirm;		/* ask first — the three that end
					 * the session or the machine     */
};

static struct row left[ST_MAX_ROWS];
static int nleft;
static struct row right[ST_MAX_ROWS];
static int nright;

static int mode = ST_MAIN;
/* The next draw should pull the selection into view: set by everything that
 * MOVES the selection and by nothing that scrolls the page. */
static int sel_follow = 1;
static int cat;			/* which category ST_CAT is showing */
static int sel;			/* selected row in the focused column */
static int top;			/* first visible row, left column     */
static int focus_right;		/* which column has the selection     */
static int rsel, rtop;
static char query[64];
static int icons_on = 1;

/* Which footer button the pointer is on, or -1 — the same three-state rule the
 * shared button bar keeps: focus is one thing, hover is another. */
static int hover_btn = -1;
/* Where the search field and its clear mark were drawn, and whether the field
 * is ACTIVE — clicked, or holding a query. A field that looks the same before
 * and after a click is one people click again. */
static int search_x, search_end, clear_x, clear_end, search_lit;
/* How wide the left column's rows were DRAWN — one narrower when the
 * scrollbar is there, which moves the pin with it. */
static int left_row_w;

static int in_span(int v, int a, int b)
{
	return b > a && v >= a && v < b;
}

/* ── building the columns ──────────────────────────────────────────────── */

static struct row *push(struct row *v, int *n, const char *label)
{
	if (*n >= ST_MAX_ROWS)
		return NULL;
	struct row *r = &v[(*n)++];
	memset(r, 0, sizeof(*r));
	snprintf(r->label, sizeof(r->label), "%s", label);
	return r;
}

static void rule(struct row *v, int *n)
{
	struct row *r = push(v, n, "");
	if (r)
		r->rule = 1;
}

/*
 * ~/.config/kdos/favorites is the pinned list — the SAME file the taskbar's
 * quick-launch row reads. Two lists of favourites is two things to keep in
 * agreement and one of them always loses.
 */
static int load_pinned(void)
{
	char path[512], line[256];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	FILE *f;
	int n = 0;

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/favorites", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/favorites", home);
	else
		return 0;
	f = fopen(path, "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f) && n < 8) {
		line[strcspn(line, "\n")] = '\0';
		char *s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		if (!*s || *s == '#')
			continue;
		const struct sh_app *a = sh_apps_find(s);
		if (!a)
			continue;	/* an id with no entry launches nothing */
		struct row *r = push(left, &nleft, a->name);
		if (!r)
			break;
		r->app = a;
		r->icon = a->icon[0] ? a->icon : a->id;
		r->pinned = 1;
		n++;
	}
	fclose(f);
	return n;
}

/*
 * WHICH CATEGORY WAS LAST OPENED, remembered across menus.
 *
 * This is a short-lived process — the menu is spawned, used and gone — so
 * "remember" has to mean a file, and it is the same one-line, fsync-rename
 * shape every other state file on this desktop uses. It does not open the
 * category; it SELECTS it in the list, so All Programs lands on Graphics for
 * somebody who lives in Graphics and costs nobody a keystroke.
 */
static int lastcat_path(char *out, size_t n)
{
	const char *state = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");

	if (state && *state)
		return snprintf(out, n, "%s/kdos/startcat", state) < (int)n;
	if (home && *home)
		return snprintf(out, n, "%s/.local/state/kdos/startcat",
				home) < (int)n;
	return 0;
}

static void lastcat_save(const char *name)
{
	char path[512], tmp[544];
	FILE *f;

	if (!name || !*name || !lastcat_path(path, sizeof(path)))
		return;
	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return;		/* no state dir yet is not an error worth a word */
	fprintf(f, "%s\n", name);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0)
		unlink(tmp);
}

static const char *lastcat_load(void)
{
	static char buf[64];
	char path[512];
	FILE *f;

	if (!lastcat_path(path, sizeof(path)))
		return NULL;
	f = fopen(path, "r");
	if (!f)
		return NULL;
	if (!fgets(buf, sizeof(buf), f))
		buf[0] = '\0';
	fclose(f);
	buf[strcspn(buf, "\n")] = '\0';
	return buf[0] ? buf : NULL;
}

/*
 * THE WAY BACK, AS A ROW.
 *
 * Escape and the right button have always stepped up a level, and neither is
 * discoverable: All Programs opened a list of categories, a category opened a
 * list of applications, and from there the only ways out were a key nobody was
 * told about and a button whose other meaning on this desktop is "menu". A
 * pointer-only user — which is every first-time user of a Start menu — was
 * stuck two levels down with the mouse in their hand.
 *
 * It is the first row and it carries the same left-pointing mark the title
 * does, so the eye finds it in the place every file manager and every settings
 * app puts it.
 */
static void push_back(const char *what)
{
	struct row *r = push(left, &nleft, what);

	if (r) {
		r->back = 1;
		r->icon = "go-previous";
	}
}

/*
 * Does this fixed row answer to what was typed? Its label, then the synonyms
 * it carries — case-insensitive substring both ways, which is what the
 * application matcher does and is all a nine-row list needs.
 */
static int fixed_match(const struct row *r, const char *q)
{
	if (!q || !*q)
		return 0;
	if (strcasestr(r->label, q))
		return 1;
	return r->keys && strcasestr(r->keys, q) != NULL;
}

static void build_left(void)
{
	nleft = 0;

	if (mode == ST_SEARCH) {
		const struct sh_app *hits[64];
		int n = sh_apps_match(query, hits, 64);

		/* A search has a way out too, and clearing the query is the
		 * one thing a pointer could not do at all: the field takes
		 * keystrokes and there was no backspace to click. */
		push_back("Clear search");
		for (int i = 0; i < n; i++) {
			struct row *r = push(left, &nleft, hits[i]->name);
			if (!r)
				break;
			r->app = hits[i];
			r->icon = hits[i]->icon[0] ? hits[i]->icon
						   : hits[i]->id;
			r->pinned = sh_fav_has(hits[i]->id);
		}
		/*
		 * AND THE PLACES AND THE SETTINGS, which a search over the
		 * application index alone could never find. Typing `wifi`
		 * looked like a machine with no network tool on it — the one
		 * is in the right column, three rows from the top, and a
		 * person who is typing has stopped looking over there. Every
		 * fixed row carries its own synonyms; see build_right.
		 */
		int extra = 0;
		for (int i = 0; i < nright && nleft < ST_MAX_ROWS; i++) {
			if (right[i].rule || !fixed_match(&right[i], query))
				continue;
			if (!extra++)
				rule(left, &nleft);
			struct row *r = push(left, &nleft, right[i].label);
			if (!r)
				break;
			*r = right[i];
		}
		if (!n && !extra)
			push(left, &nleft, "no match");
		return;
	}

	if (mode == ST_CATS) {
		const char *last = lastcat_load();

		push_back("Back");
		for (int g = 0; g < sh_app_ngroups(); g++) {
			const struct sh_app *tmp[1];
			if (!sh_apps_in_group(g, tmp, 1))
				continue;	/* an empty category is noise */
			struct row *r = push(left, &nleft,
					     sh_app_group_name(g));
			if (!r)
				break;
			r->submenu = g + 1;
			r->icon = "folder";
			if (last && !strcmp(last, r->label)) {
				sel = nleft - 1;
				sel_follow = 1;
			}
		}
		return;
	}

	if (mode == ST_CAT) {
		const struct sh_app *tmp[ST_MAX_ROWS];
		int n = sh_apps_in_group(cat, tmp, ST_MAX_ROWS);

		push_back("All Programs");
		for (int i = 0; i < n; i++) {
			struct row *r = push(left, &nleft, tmp[i]->name);
			if (!r)
				break;
			r->app = tmp[i];
			r->icon = tmp[i]->icon[0] ? tmp[i]->icon : tmp[i]->id;
			r->pinned = sh_fav_has(tmp[i]->id);
		}
		return;
	}

	/* ST_MAIN */
	int pinned = load_pinned();
	const struct sh_app *freq[10];
	int nf = sh_apps_frequent(freq, 10);

	if (pinned && nf)
		rule(left, &nleft);
	for (int i = 0; i < nf; i++) {
		/* Something already pinned must not appear twice: the column
		 * is short and a duplicate costs a row that a person would
		 * have used. */
		int dup = 0;
		for (int j = 0; j < pinned; j++)
			if (left[j].app == freq[i])
				dup = 1;
		if (dup)
			continue;
		struct row *r = push(left, &nleft, freq[i]->name);
		if (!r)
			break;
		r->app = freq[i];
		r->icon = freq[i]->icon[0] ? freq[i]->icon : freq[i]->id;
	}
	/* No leading rule on a machine with nothing above it: a separator that
	 * separates one thing from nothing reads as a rendering fault. */
	if (nleft)
		rule(left, &nleft);
	struct row *r = push(left, &nleft, "All Programs");
	if (r) {
		r->submenu = -1;
		r->icon = "folder";
	}
}

/*
 * The right column: Places, then System. Every row is an argv and there is no
 * shell anywhere in it — these are the entries that end a session or a
 * machine, and a string that got interpolated into a command line here would
 * be the worst possible place for it.
 */
static void build_right(void)
{
	const char *home = getenv("HOME");
	static char docs[512], dl[512], pics[512];
	struct row *r;

	nright = 0;

	if (home && *home) {
		snprintf(docs, sizeof(docs), "%s/Documents", home);
		snprintf(dl, sizeof(dl), "%s/Downloads", home);
		snprintf(pics, sizeof(pics), "%s/Pictures", home);

		r = push(right, &nright, "Home");
		if (r) {
			r->keys = "files folder browse";
			r->icon = "user-home";
			r->argv[0] = "kdos-pick";
			r->argv[1] = "--browse";
			r->argv[2] = home;
		}
		r = push(right, &nright, "Documents");
		if (r) {
			r->keys = "docs files";
			r->icon = "folder-documents";
			r->argv[0] = "kdos-pick";
			r->argv[1] = "--browse";
			r->argv[2] = docs;
		}
		r = push(right, &nright, "Downloads");
		if (r) {
			r->keys = "files";
			r->icon = "folder-download";
			r->argv[0] = "kdos-pick";
			r->argv[1] = "--browse";
			r->argv[2] = dl;
		}
		r = push(right, &nright, "Pictures");
		if (r) {
			r->keys = "photos images files";
			r->icon = "folder-pictures";
			r->argv[0] = "kdos-pick";
			r->argv[1] = "--browse";
			r->argv[2] = pics;
		}
	}
	r = push(right, &nright, "Files");
	if (r) {
		r->keys = "manager mc browse";
		r->icon = "folder-open";
		r->argv[0] = "foot";
		r->argv[1] = "-e";
		r->argv[2] = "mc";
	}

	rule(right, &nright);

	/*
	 * The names below are the ones the SHIPPED ATLAS carries, checked
	 * against it rather than taken from the freedesktop naming spec:
	 * Papirus has no `preferences-system`, no `utilities-terminal` and no
	 * `help-browser`, and a row whose icon silently resolves to nothing is
	 * a row that quietly loses its picture while every other row keeps
	 * one. A name that misses still draws — the glyph tier is the
	 * fallback — so this is a polish decision, not a correctness one.
	 */
	r = push(right, &nright, "Settings");
	if (r) {
		r->keys = "preferences control panel config options";
		r->icon = "settings";
		r->argv[0] = "kdos-settings";
	}
	r = push(right, &nright, "Network");
	if (r) {
		r->keys = "wifi wireless internet ethernet vpn connect";
		r->icon = "network-wireless";
		r->argv[0] = "kdos-net";
	}
	r = push(right, &nright, "Bluetooth");
	if (r) {
		r->keys = "bt pair headset";
		r->icon = "bluetooth";
		r->argv[0] = "kdos-bt";
	}
	r = push(right, &nright, "Sound");
	if (r) {
		r->keys = "audio volume speaker microphone mixer";
		r->icon = "audio-speakers";
		r->argv[0] = "kdos-audio";
	}
	r = push(right, &nright, "Notifications");
	if (r) {
		r->keys = "alerts toasts history missed notify";
		r->icon = "dialog-information";
		r->argv[0] = "kdos-notify";
	}
	r = push(right, &nright, "Devices");
	if (r) {
		r->keys = "camera webcam usb stick disk mount eject";
		r->icon = "camera-web";
		r->argv[0] = "kdos-devices";
	}
	r = push(right, &nright, "Displays");
	if (r) {
		r->keys = "monitor screen resolution scale rotate";
		r->icon = "video-display";
		r->argv[0] = "kdos-display";
	}
	r = push(right, &nright, "Terminal");
	if (r) {
		r->keys = "shell console foot prompt";
		r->icon = "system-run";
		r->argv[0] = "foot";
	}
	r = push(right, &nright, "Help");
	if (r) {
		r->keys = "docs manual guide keys";
		r->icon = "help";
		r->argv[0] = "kdos-doc";
	}
	r = push(right, &nright, "Lock Screen");
	if (r) {
		r->keys = "lock away";
		r->icon = "system-lock-screen";
		r->argv[0] = "kdos-lock";
	}
	/*
	 * SUSPEND AND RESTART WERE IN NO MENU A POINTER COULD REACH. The
	 * footer carries Log Off and Shut Down, and the other two verbs lived
	 * only in the compositor's own System menu — a RIGHT click on the
	 * Start button, which is not a gesture anybody guesses. Suspend asks
	 * nothing, because it is the one power action that undoes itself;
	 * Restart asks, because it does not.
	 */
	r = push(right, &nright, "Suspend");
	if (r) {
		r->keys = "sleep power";
		r->icon = "system-suspend";
		r->argv[0] = "kdos-power";
		r->argv[1] = "suspend";
	}
	r = push(right, &nright, "Restart");
	if (r) {
		r->keys = "reboot power";
		r->icon = "system-reboot";
		r->argv[0] = "kdos-power";
		r->argv[1] = "reboot";
		r->confirm = "Restart this machine?";
	}
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * PINNING, WHERE THE THING BEING PINNED IS.
 *
 * `~/.config/kdos/favorites` is what the taskbar's quick-launch row draws, and
 * the only way to write it was the WINDOW menu — a right click on a running
 * window's button. So an application had to be started before it could be
 * pinned, and the menu that lists every application on the machine could not
 * pin any of them. The mark is at the right edge of the row: filled for a
 * pinned entry always, hollow on the row under the cursor, and absent
 * everywhere else, which is the affordance every dock of this shape uses.
 */
#define PIN_W 2

static int pin_col(int x, int w)
{
	return x + w - PIN_W - 1;
}

static void draw_row(const struct row *r, int x, int y, int w, int selected)
{
	int fg = selected ? KT_SURFACE : KT_TEXT;
	int bg = selected ? KT_ACCENT : KT_BG;

	if (r->rule) {
		ktui_draw_hline(x, y, w, KT_G_HL, KT_DIM, KT_BG);
		return;
	}
	/* FILL, then draw with the slots swapped — not KT_A_REVERSE over the
	 * label, which inverts only the cells the text covers and turns a
	 * two-word entry into two lit blocks. */
	ktui_draw_fill(krect(x, y, w, 1), bg);

	/*
	 * THE LABEL STARTS IN THE SAME COLUMN WHETHER OR NOT THERE IS A
	 * PICTURE. Not every alien app installs an icon into the hicolor tree,
	 * so a category list is a mix of both — and with the glyph fallback
	 * drawn one cell wide and its label two cells further left, the
	 * Graphics list came out with `Blender` and `gThumb` hanging two
	 * columns into the margin while everything with artwork lined up.
	 * Photographed. The glyph sits in the middle of the picture's own slot
	 * instead.
	 */
	int tx = x + 3;
	int icon = icons_on && r->icon ? kicon_slot(r->icon, 2, 1) : -1;
	if (icon >= 0) {
		ktui_draw_sprite(krect(x, y, 2, 1), icon, fg, bg);
	} else {
		ktui_draw_text(x + 1, y, 1,
			       r->back  ? ktui_glyph[KT_G_LEFT]
			       : r->app ? ktui_glyph[KT_G_SQUARE]
					: ktui_glyph[KT_G_DOT],
			       selected ? KT_SURFACE : KT_DIM, bg, KT_A_NONE);
	}
	/*
	 * `[box]`, WHICH ONLY THIS DISTRO HAS TO SAY. Every fat application
	 * here lives in a container, and the first launch of one costs a
	 * container start — eighteen seconds cold, measured. A menu that
	 * showed no difference between that and a host program was hiding the
	 * one thing a person is entitled to know before they click. The
	 * launcher has said it for a release; the shared index knows it now,
	 * so the two cannot disagree.
	 */
	int tagw = r->app && r->app->alien ? 6 : 0;
	int room = pin_col(x, w) - tx - (r->app ? 1 : 0) - tagw;

	if (room < 1) {
		room = 1;
		tagw = 0;	/* the NAME is what the row is for */
	}
	ktui_draw_text(tx, y, room, r->label, fg, bg, KT_A_NONE);
	if (tagw)
		ktui_draw_text(pin_col(x, w) - tagw, y, tagw - 1, "[box]",
			       selected ? fg : KT_DIM, bg, KT_A_NONE);
	if (r->submenu)
		ktui_draw_text(x + w - 2, y, 1, ktui_glyph[KT_G_RIGHT], fg, bg,
			       KT_A_NONE);
	else if (r->app && (r->pinned || selected)) {
		int px = pin_col(x, w);
		int pin = icons_on ? kicon_slot(r->pinned ? "starred"
							  : "non-starred",
						PIN_W, 1)
				   : -1;
		if (pin >= 0)
			ktui_draw_sprite(krect(px, y, PIN_W, 1), pin,
					 r->pinned ? (selected ? KT_SURFACE
							       : KT_WARN)
						   : fg,
					 bg);
		else
			/* The console font has no star; a filled block against
			 * a hollow one is the same two states in the vt tier. */
			ktui_draw_text(px + 1, y, 1,
				       ktui_glyph[r->pinned ? KT_G_FULL
							    : KT_G_SHADE],
				       r->pinned ? (selected ? KT_SURFACE
							     : KT_WARN)
						 : (selected ? KT_SURFACE
							     : KT_DIM),
				       bg, KT_A_NONE);
	}
}

static int dumping;

/*
 * The title is who you are and where — the row every menu of this shape has
 * put the user in since 2001.
 *
 * A DUMP says `kdos` instead, and that is not cosmetic: a golden frame has to
 * be byte-identical on any machine, and the login name and the hostname are
 * the two things in this window that are guaranteed not to be.
 */
static const char *title_text(void)
{
	static char buf[64];
	const char *user = getenv("USER");
	char host[64] = "";

	if (mode == ST_CAT)
		return sh_app_group_name(cat);
	if (mode == ST_CATS)
		return "All Programs";
	if (dumping)
		return "kdos";
	if (gethostname(host, sizeof(host) - 1) != 0)
		host[0] = '\0';
	snprintf(buf, sizeof(buf), "%s%s%s", user ? user : "kdos",
		 host[0] ? "@" : "", host);
	return buf;
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	int lw = ST_LEFT_W < w - 12 ? ST_LEFT_W : w / 2;
	/*
	 * ROWS 1 .. h-4. The title takes row 0, the rule is at h-3, the footer
	 * at h-2 and the border at h-1 — so `h - 3` was one too many and the
	 * last row of a full column was drawn straight over the rule. Invisible
	 * for as long as neither column was long enough to reach it, which on
	 * the shipped 22-row menu is twenty rows; the right column got there
	 * first when Suspend and Restart landed. Found with `--dump` at a size
	 * that forces it, which is what that harness is for.
	 */
	int body = h - 4;

	if (w < 24 || h < 6)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), title_text(), KT_ACCENT, KT_BG, 1);

	/* The divider between the columns, and the one above the footer. */
	ktui_draw_vline(lw, 1, body, KT_G_VL, KT_DIM, KT_BG);
	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_DIM, KT_BG);

	/*
	 * ── the left column ──
	 *
	 * ONE flag, TWO columns: the left clamp used to consume `sel_follow`
	 * and the right one always saw zero, so a selection driven into the
	 * right column could never scroll it into view. Each column follows
	 * only while it holds the selection.
	 */
	int follow = sel_follow;

	sel_follow = 0;
	kch_list_clamp(&top, sel, nleft, body, follow && !focus_right);
	/* The scrollbar takes a column OF ITS OWN rather than overdrawing the
	 * rows' last one: a selected row is an accent fill, and a bar drawn on
	 * top of it puts a notch in the highlight. */
	left_row_w = nleft > body ? lw - 2 : lw - 1;
	for (int i = 0; i < body; i++) {
		int idx = top + i;
		if (idx >= nleft)
			break;
		draw_row(&left[idx], 1, 1 + i, left_row_w,
			 !focus_right && idx == sel);
	}
	/* The column that says there is more. It matters more here than
	 * anywhere: the wheel moves the PAGE once a list is longer than the
	 * window, so without it the content moves for no visible reason. */
	kch_scrollbar(0, lw - 1, 1, body, nleft, top, KT_BG);

	/*
	 * ── the right column ──
	 *
	 * It scrolls too, and it did not: seventeen fixed rows against a body
	 * of eighteen fitted by one, so the next entry anybody adds would have
	 * been silently invisible — and the wheel already moved a selection
	 * that the draw could not follow. Same clamp, same bar, same rule.
	 */
	kch_list_clamp(&rtop, rsel, nright, body, follow && focus_right);
	int rw = nright > body ? w - lw - 4 : w - lw - 3;
	for (int i = 0; i < body && rtop + i < nright; i++)
		draw_row(&right[rtop + i], lw + 2, 1 + i, rw,
			 focus_right && rtop + i == rsel);
	kch_scrollbar(1, w - 2, 1, body, nright, rtop, KT_BG);

	/*
	 * ── the footer: a search FIELD, and the two ways out ──
	 *
	 * The buttons are the shared bar, so they hover, they drop from the
	 * right when the menu is narrow, and they hand back the column the
	 * field has to stop at — three behaviours this footer used to
	 * reimplement badly. Most useful first: the bar drops Shut Down before
	 * Log Off, and both are also rows in the right column now.
	 */
	struct kch_button fb[2] = {
		{ "Log Off", 1 },
		{ "Shut Down", 1 },
	};
	int bx = kch_buttons(w, h - 2, fb, 2, -1);

	/*
	 * A FIELD THAT LOOKS LIKE ONE, AND LOOKS DIFFERENT WHEN IT IS ACTIVE.
	 *
	 * `Search` in dim text with nothing round it is a caption. This takes
	 * typing, so it wears the magnifier every filter on this desktop uses,
	 * it is SUNKEN (its own fill, one step off the page), it lights under
	 * the pointer, and it goes to the accent the moment it is active — a
	 * click on it or the first character typed. A control that looks
	 * identical before and after it has been clicked is one people click
	 * again to find out whether it worked.
	 *
	 * `✕` is what clears it, and it is drawn only when there is something
	 * to clear: a permanent one is a button that does nothing most of the
	 * time. Escape and the `Clear search` row do the same thing.
	 */
	/*
	 * LIT, NOT A SLAB. The first version filled the whole field with the
	 * accent, and forty columns of it beside a dim list was the loudest
	 * thing on the screen — photographed. A text field is a SUNKEN box:
	 * one step off the page at rest, the fill when the pointer is on it or
	 * it is active, and the ACCENT saved for the caret and the magnifier,
	 * which is where the eye goes anyway.
	 */
	int lit = search_lit || query[0];
	int fbg = lit || hover_btn == 2 ? KT_DIM : KT_SURFACE;
	int ffg = query[0] ? KT_TEXT : KT_MID;
	int fmark = lit ? KT_ACCENT : KT_MID;

	search_x = 2;
	search_end = bx - 2 < w / 2 + 8 ? bx - 2 : w / 2 + 8;
	clear_x = clear_end = 0;
	if (search_end > search_x + 6) {
		int fw = search_end - search_x;
		int tx = search_x;

		ktui_draw_fill(krect(search_x, h - 2, fw, 1), fbg);
		int mag = icons_on ? kicon_slot("edit-find", 2, 1) : -1;
		if (mag >= 0) {
			ktui_draw_sprite(krect(search_x, h - 2, 2, 1), mag,
					 fmark, fbg);
			tx = search_x + 3;
		} else {
			/* No artwork: the prompt every filter box has had since
			 * the eighties. */
			ktui_draw_text(search_x, h - 2, 1, ">", fmark, fbg,
				       KT_A_NONE);
			tx = search_x + 2;
		}

		/* The clear mark first, so the text knows where to stop. */
		int room = search_end - tx;
		if (query[0]) {
			clear_end = search_end;
			clear_x = search_end - 2;
			int cx = icons_on ? kicon_slot("edit-clear", 2, 1) : -1;
			if (cx >= 0)
				ktui_draw_sprite(krect(clear_x, h - 2, 2, 1),
						 cx, ffg, fbg);
			else
				ktui_draw_text(clear_x + 1, h - 2, 1, "x", ffg,
					       fbg, KT_A_NONE);
			room = clear_x - tx - 1;
		}
		if (room < 1)
			room = 1;

		/* Wide enough for a desktop entry's whole Comment: the draw
		 * clips to the field, and a truncation here would be the one
		 * the compiler warns about rather than the one that matters. */
		char field[192];
		if (query[0]) {
			snprintf(field, sizeof(field), "%s", query);
		} else {
			/*
			 * AND WHEN THERE IS NOTHING TYPED, THE ROW EXPLAINS THE
			 * SELECTION. Every desktop entry carries a Comment and
			 * this menu threw it away, so `Meld` and `Gwenview`
			 * were words with pictures beside them. The placeholder
			 * comes back the moment there is nothing to describe,
			 * so the field never stops saying what it is for.
			 */
			const struct row *cur = focus_right
							? (rsel < nright
								   ? &right[rsel]
								   : NULL)
							: (sel < nleft ? &left[sel]
								       : NULL);
			const char *why = cur && cur->app && cur->app->comment[0]
						  ? cur->app->comment
						  : NULL;
			snprintf(field, sizeof(field), "%s",
				 why ? why : "Type to search");
		}
		ktui_draw_text(tx, h - 2, room, field, ffg, fbg, KT_A_NONE);
		/* The caret is its own draw so it can be the accent — a block,
		 * the same one the launcher's query line uses, because a cell
		 * grid has no hardware cursor to place and `_` is a character a
		 * name can contain. */
		if (query[0]) {
			int qw = ktui_utf8_width(query);

			if (qw < room)
				ktui_draw_text(tx + qw, h - 2, 1,
					       ktui_glyph[KT_G_FULL], KT_ACCENT,
					       fbg, KT_A_NONE);
		}
	}
	ktui_draw_flush();
}

/* ── acting ────────────────────────────────────────────────────────────── */

static int confirm(const char *msg)
{
	const char *argv[] = { "kdos-prompt", msg, NULL };
	pid_t pid = fork();
	int st = 0;

	if (pid == 0) {
		execvp(argv[0], (char *const *)argv);
		_exit(2);
	}
	if (pid < 0)
		return 0;
	while (waitpid(pid, &st, 0) < 0)
		;
	/* kdos-prompt answers 0 yes, 1 no, 254 cancelled — the same codes
	 * kdos-comp's If/prompt action dispatches on. */
	return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/*
 * Log Out is `pkill -TERM -x kdos-comp`, which looks blunt and is the
 * accurate description of what logging out means here: the compositor IS the
 * session, labwc handles SIGTERM through its own event loop and tears down in
 * order, and kdos-desktop returns to the tty. The `-x` is load-bearing —
 * `kdos-comp` is a prefix of nothing today and `kdos-desk` was a substring of
 * `kdos-desktop` yesterday, which is exactly how a signal ends the wrong
 * process.
 */
static void log_out(void)
{
	const char *argv[] = { "pkill", "-TERM", "-x", "kdos-comp", NULL };

	if (!confirm("Log out of this session?"))
		return;
	sh_spawn(argv);
}

static void power(const char *what, const char *question)
{
	if (!confirm(question))
		return;
	const char *argv[] = { "kdos-power", what, NULL };
	sh_spawn(argv);
}

static int back(void);

/* Returns 1 when the menu should close. */
static int activate(struct row *r)
{
	if (!r || r->rule)
		return 0;

	if (r->back) {
		/* Never closes: the row is the way UP, and the way out is
		 * Escape, the right button or a click on the desktop. A Back
		 * that dismissed the menu from the top level would be a
		 * different control wearing the same word. */
		back();
		return 0;
	}
	if (r->submenu == -1) {
		mode = ST_CATS;
		sel = top = 0;
		sel_follow = 1;
		build_left();
		return 0;
	}
	if (r->submenu > 0) {
		cat = r->submenu - 1;
		lastcat_save(sh_app_group_name(cat));
		mode = ST_CAT;
		sel = top = 0;
		sel_follow = 1;
		build_left();
		return 0;
	}
	if (r->app) {
		sh_apps_launch(r->app);
		return 1;
	}
	if (r->argv[0]) {
		/* The `confirm` field was declared with the rest of the row
		 * and read by nothing, so the two rows that end a session or a
		 * machine had to be special cases in the footer. It is a
		 * property of the ROW now, which is what let Restart and
		 * Suspend become ordinary entries. */
		if (r->confirm && !confirm(r->confirm))
			return 0;
		sh_spawn(r->argv);
		return 1;
	}
	return 0;
}

/* Escape and the right button back out one level, then close — the contract
 * every other surface in this shell keeps. */
static int back(void)
{
	if (query[0]) {
		query[0] = '\0';
		mode = ST_MAIN;
		sel = top = 0;
		sel_follow = 1;
		build_left();
		return 0;
	}
	if (mode == ST_CAT) {
		mode = ST_CATS;
		sel = top = 0;
		sel_follow = 1;
		build_left();
		return 0;
	}
	if (mode == ST_CATS) {
		mode = ST_MAIN;
		sel = top = 0;
		sel_follow = 1;
		build_left();
		return 0;
	}
	return 1;
}

static void step(int d)
{
	int n = focus_right ? nright : nleft;
	int *s = focus_right ? &rsel : &sel;

	if (n <= 0)
		return;
	/* The CURSOR moved, so the viewport follows it. A wheel that scrolled
	 * the viewport instead deliberately does not set this — see
	 * kch_list_wheel and draw_frame. */
	sel_follow = 1;
	for (int i = 0; i < n; i++) {
		*s += d;
		if (*s < 0)
			*s = n - 1;
		if (*s >= n)
			*s = 0;
		if (!(focus_right ? right[*s] : left[*s]).rule)
			return;
	}
}

static void typeahead(int ch)
{
	size_t n = strlen(query);

	if (ch == '\b' || ch == 127) {
		if (n)
			query[n - 1] = '\0';
	} else if (ch >= 0x20 && ch < 0x7f && n + 1 < sizeof(query)) {
		query[n] = (char)ch;
		query[n + 1] = '\0';
	} else {
		return;
	}
	mode = query[0] ? ST_SEARCH : ST_MAIN;
	sel = top = 0;
	sel_follow = 1;
	focus_right = 0;
	build_left();
}

int start_main(int argc, char **argv)
{
	const char *font = NULL;
	int at_x = -1, at_y = 0, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[i + 1]);
			at_y = atoi(argv[i + 2]);
			i += 2;
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--no-icons")) {
			icons_on = 0;
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			fprintf(stderr, "usage: kdos-start [--at-bottom X Y] "
					"[--font NAME] [--no-icons] [--dump]\n");
			return 2;
		}
	}

	sh_apps_load();
	/* The fixed rows first: a search reads them, and the one at startup
	 * would otherwise run against an empty right column. */
	build_right();
	build_left();

	if (dump) {
		sh_theme_from_cache();
		dumping = 1;
		icons_on = 0;
		ktui_offscreen_init(ST_COLS, ST_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = ST_COLS,
		.rows = ST_ROWS,
		/*
		 * Above the taskbar, in its corner. Anchoring TOP with a
		 * computed margin cannot express this: the client does not
		 * know the output's pixel height.
		 */
		.corner = at_x >= 0 ? KWL_CORNER_BOTTOM_LEFT : KWL_CORNER_CENTER,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-start",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-start: no compositor or no layer-shell\n");
		return 1;
	}
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	ktui_draw_init();

	while (!kwl_should_close()) {
		sh_theme_poll();
		draw_frame();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			/* A configure is not applied until the loop that owns
			 * the surface applies it. */
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int lw = ST_LEFT_W < ktui_w - 12 ? ST_LEFT_W
							 : ktui_w / 2;
			int body = ktui_h - 4;	/* see draw_frame */
			int on_left = ev.mx >= 1 && ev.mx < lw;
			int idx = ev.my - 1;

			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					if (kch_scrollbar_grabbed() == 0)
						top = bt;
					else
						rtop = bt;
					sel_follow = 0;
					continue;
				}
				/*
				 * A MOTION IS A REAL MOVE. libkwl reports one
				 * only when the pointer changed cell, which is
				 * what lets the wheel own the highlight while
				 * the hand is still: without it the position
				 * that rides along with an axis event put the
				 * selection straight back under the pointer.
				 */
				/* The buttons light through the shared bar; the
				 * field is this file's own. */
				kch_hover(ev.mx, ev.my);
				hover_btn = ev.my == ktui_h - 2 &&
						    in_span(ev.mx, search_x, search_end)
					    ? 2
					    : -1;
				if (ev.my >= 1 && ev.my - 1 < body) {
					if (on_left && top + idx < nleft &&
					    !left[top + idx].rule) {
						focus_right = 0;
						sel = top + idx;
					} else if (!on_left && ev.mx > lw &&
						   rtop + idx < nright &&
						   !right[rtop + idx].rule) {
						focus_right = 1;
						rsel = rtop + idx;
						sel_follow = 1;
					}
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
				bt = kch_scrollbar_press(1, ev.mx, ev.my);
				if (bt >= 0) {
					rtop = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;
				/*
				 * THE COLUMN UNDER THE POINTER, not the one
				 * with the keyboard focus — and the focus
				 * follows, or the highlight would move in a
				 * column the eye is not on. Only the left
				 * column can be longer than the window; the
				 * right one is a fixed list of places.
				 */
				focus_right = !on_left && ev.mx > lw;
				if (!kch_list_wheel(up,
						   focus_right ? &rtop : &top,
						   focus_right ? nright : nleft,
						   body))
					step(up ? -1 : 1);
				continue;
			}
			if (ev.btn == KT_MB_RIGHT) {
				if (back())
					break;
				continue;
			}
			if (ev.btn != KT_MB_LEFT)
				continue;
			/* The footer: the shared bar's own hit map, then the
			 * field. */
			if (ev.my == ktui_h - 2) {
				int fb = kch_button_at(ev.mx, ev.my);

				if (fb == 0) {
					log_out();
					break;
				}
				if (fb == 1) {
					power("poweroff",
					      "Shut down this machine?");
					break;
				}
				/* `x` empties it — the pointer's backspace.
				 * Anywhere else in the field makes it ACTIVE,
				 * which is what a click on a text field means. */
				if (query[0] &&
				    in_span(ev.mx, clear_x, clear_end))
					back();
				else if (in_span(ev.mx, search_x, search_end))
					search_lit = 1;
				continue;
			}
			/* A click anywhere else takes the highlight off the
			 * field, the way every other text field behaves. */
			search_lit = 0;
			if (ev.my >= 1 && ev.my - 1 < body) {
				if (on_left && top + idx < nleft) {
					struct row *rr = &left[top + idx];

					/* THE PIN IS ITS OWN TARGET, checked before
					 * the row's action: a click there must pin
					 * the application, not launch it and close
					 * the menu under the hand. */
					if (rr->app &&
					    in_span(ev.mx, pin_col(1, left_row_w),
						    pin_col(1, left_row_w) + PIN_W)) {
						sh_fav_set(rr->app->id, !rr->pinned);
						build_left();
						/* Unpinning takes a row out of
						 * the list under the cursor —
						 * the pinned block shrinks by
						 * one — so the selection has
						 * to be pulled back inside it
						 * or the highlight lands on
						 * nothing. */
						if (sel >= nleft)
							sel = nleft ? nleft - 1 : 0;
						sel_follow = 1;
						continue;
					}
					if (activate(rr))
						break;
				} else if (!on_left && ev.mx > lw &&
					   rtop + idx < nright) {
					if (activate(&right[rtop + idx]))
						break;
				}
			}
			continue;
		}

		if (ev.type != KT_EVT_KEY)
			continue;
		switch (ev.key) {
		case KT_K_ESC:
			if (back())
				goto done;
			break;
		case KT_K_UP:
			step(-1);
			break;
		case KT_K_DOWN:
			step(1);
			break;
		case KT_K_LEFT:
			if (focus_right)
				focus_right = 0;
			else if (back())
				goto done;
			break;
		case KT_K_RIGHT:
			if (!focus_right && sel < nleft && left[sel].submenu) {
				if (activate(&left[sel]))
					goto done;
			} else {
				focus_right = 1;
			}
			break;
		case KT_K_TAB:
			focus_right = !focus_right;
			break;
		case KT_K_ENTER:
			if (focus_right) {
				if (rsel < nright && activate(&right[rsel]))
					goto done;
			} else if (sel < nleft && activate(&left[sel])) {
				goto done;
			}
			break;
		default:
			typeahead(ev.key);
			break;
		}
	}
done:
	kicon_finish();
	kwl_shutdown();
	return 0;
}

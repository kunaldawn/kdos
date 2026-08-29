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
#include "kcell.h"
#include "kwl.h"
#include "shell.h"

/* 56, not 66. At the chrome cell that is 896 pixels — seventy per cent of a
 * 1280 screen rather than eighty-two, which is a menu and not a takeover. */
#define ST_COLS 56
#define ST_ROWS 20
/*
 * THE TWO COLUMNS SHARE THE WIDTH, so narrowing the menu has to narrow this
 * too. Left at 38 in a 56-column menu leaves the right column fifteen columns
 * — three for the icon and twelve for the label — and `Notifications` and
 * `Lock Screen` came out as `Notificat` and `Lock Scre`. At 32 the right
 * column has eighteen columns of label, which is the longest entry it carries,
 * and the left still fits `GNU Image Manipulation P`.
 */
#define ST_LEFT_W 32
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
	/* NUL-terminated for sh_spawn, so the last slot is never filled: the
	 * longest row here is `foot -e kdos app install <id>`, six words. */
	const char *argv[8];		/* set for a fixed entry          */
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

/* The power row — see build_power(). Declared here because the SEARCH walks
 * it, and the search is built long before the footer is drawn. */
static struct row power_row[8];
static int npower;
static int hover_power = -1;
static int pow_x;		/* where the power row was DRAWN, for the hit test */
/* Three cells each: a 32-pixel square in a 48-pixel box, the same odd width
 * the panel's applet tiles use and for the same reason — an even one leaves
 * the picture half a cell off centre. */
#define POW_W 3
/* A cell of air between them. The pictures do not need it — a sprite has its
 * own bounds — but the no-artwork tags do: `LckSlpRstOutOff` is one word. */
#define POW_STRIDE (POW_W + 1)
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

/*
 * A separator, optionally CAPTIONED.
 *
 * The right column was fifteen ungrouped rows mixing three kinds of thing —
 * places, settings and power. A caption on the rule that already sits between
 * them groups the list for NO extra row, which matters here: the menu's body
 * is exactly as tall as its content, so a heading drawn as its own row would
 * push the last entry below the fold, and growing the menu to fit fights the
 * whole point of shrinking it.
 */
static void rule_named(struct row *v, int *n, const char *caption)
{
	struct row *r = push(v, n, caption ? caption : "");

	if (r)
		r->rule = 1;
}

static void rule(struct row *v, int *n)
{
	rule_named(v, n, NULL);
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

/*
 * THE PACKS ON THE MEDIUM ARE IN THE MENU. On a distro whose medium IS the
 * software library, a Start menu that listed only what was installed showed
 * the eleven recommended applications on a live stick carrying a hundred and
 * fifty — and the rest were reachable only by typing a name into the search
 * and knowing there was something to find. So every category lists, under
 * an `ON THE MEDIUM` rule, the application packs it would hold, and a row
 * installs one (`kdos app install`, in a foot window that shows the mount and
 * the launcher count); the next open of this menu lists it as installed.
 *
 * `/mnt/iso/packs/PACKAGES` is the signed index the medium already carries
 * and it is a flat file — read here rather than asked of kdos-packd, because
 * the search half runs on every keystroke and a socket round trip per
 * character is not something a menu does. `N:` is the application's own
 * name and `G:` its category; a pack from a medium baked before those keys
 * existed lists under its id, in Accessories.
 *
 * WHAT IS INSTALLED IS READ FROM THE ALIEN-APPS TABLES, whose third field is
 * the pack: a pack named there has a launcher and is already in the list
 * above the rule, and offering it a second time would be a menu that says
 * "install GIMP" under GIMP.
 */
#define ST_MEDIUM_MAX 256
static struct {
	char id[64];
	char name[64];
	char summary[128];
	int group;
} medium[ST_MEDIUM_MAX];
static int nmedium;

static int pack_installed(const char *id)
{
	static const char *const tables[] = {
		"/usr/share/kdos/alien-apps", NULL };
	char user[512];
	const char *home = getenv("HOME");
	snprintf(user, sizeof(user), "%s/.local/share/kdos/alien-apps",
		 home ? home : "/root");
	for (int t = 0; t < 2; t++) {
		FILE *f = fopen(t ? user : tables[0], "r");
		char line[1024];
		if (!f)
			continue;
		while (fgets(line, sizeof(line), f)) {
			char *tab = strchr(line, '\t'), *tab2;
			if (*line == '#' || !tab || !(tab2 = strchr(tab + 1, '\t')))
				continue;
			tab2++;
			tab2[strcspn(tab2, "\r\n")] = 0;
			if (!strcmp(tab2, id)) {
				fclose(f);
				return 1;
			}
		}
		fclose(f);
	}
	return 0;
}

static void st_medium_load(void)
{
	char line[512];
	FILE *f;
	char id[64] = "", name[64] = "", summary[128] = "", cat[64] = "";
	int isapp = 0;

	nmedium = 0;
	f = fopen("/mnt/iso/packs/PACKAGES", "r");
	if (!f)
		return;
	for (;;) {
		int end = !fgets(line, sizeof(line), f);
		if (!end)
			line[strcspn(line, "\r\n")] = 0;
		if (end || !line[0]) {
			/* a stanza ends: an application not yet installed
			 * is a row */
			if (id[0] && isapp && nmedium < ST_MEDIUM_MAX &&
			    !pack_installed(id)) {
				snprintf(medium[nmedium].id, 64, "%s", id);
				snprintf(medium[nmedium].name, 64, "%s",
					 name[0] ? name : id);
				snprintf(medium[nmedium].summary, 128, "%s",
					 summary);
				medium[nmedium].group = sh_app_group_for(cat);
				nmedium++;
			}
			id[0] = name[0] = summary[0] = cat[0] = 0;
			isapp = 0;
			if (end)
				break;
			continue;
		}
		if (!strncmp(line, "P:", 2))
			snprintf(id, sizeof(id), "%s", line + 2);
		else if (!strcmp(line, "K:app"))
			isapp = 1;
		else if (!strncmp(line, "N:", 2))
			snprintf(name, sizeof(name), "%s", line + 2);
		else if (!strncmp(line, "T:", 2))
			snprintf(summary, sizeof(summary), "%s", line + 2);
		else if (!strncmp(line, "G:", 2))
			snprintf(cat, sizeof(cat), "%s", line + 2);
	}
	fclose(f);
}

static int medium_in_group(int g)
{
	int n = 0;
	for (int i = 0; i < nmedium; i++)
		n += medium[i].group == g;
	return n;
}

/* One row per medium pack: the name, the package mark, and an install. */
static void push_medium(int i)
{
	struct row *r = push(left, &nleft, medium[i].name);
	if (!r)
		return;
	r->icon = "package-x-generic";
	r->keys = medium[i].summary;
	r->argv[0] = "foot";
	r->argv[1] = "-e";
	r->argv[2] = "kdos";
	r->argv[3] = "app";
	r->argv[4] = "install";
	r->argv[5] = medium[i].id;
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
		struct row *fixed[2] = { right, power_row };
		int nfixed[2] = { nright, npower };

		/* The power verbs are in the footer rather than the column, so
		 * a search that walked only `right` would have taken Lock,
		 * Suspend and Restart off the keyboard entirely. */
		for (int t = 0; t < 2; t++)
			for (int i = 0; i < nfixed[t] &&
					nleft < ST_MAX_ROWS; i++) {
				if (fixed[t][i].rule ||
				    !fixed_match(&fixed[t][i], query))
					continue;
				if (!extra++)
					rule(left, &nleft);
				struct row *r = push(left, &nleft,
						     fixed[t][i].label);
				if (!r)
					break;
				*r = fixed[t][i];
			}
		/*
		 * AND THE MEDIUM. A query with no installed match that DOES
		 * match a pack on the stick gets a row that installs it. This
		 * is the whole of what an app store would have been worth on a
		 * distro whose medium IS the software library: the question is
		 * never "where do I get this", it is "is it here", and the
		 * answer is a row rather than a shopfront.
		 *
		 * The pack list is read from the medium's own index — a flat
		 * file, no daemon round trip — because this runs while
		 * somebody is typing.
		 */
		int offered = 0;
		for (int i = 0; i < nmedium && nleft < ST_MAX_ROWS; i++) {
			if (!strcasestr(medium[i].name, query) &&
			    !strcasestr(medium[i].id, query) &&
			    !strcasestr(medium[i].summary, query))
				continue;
			if (!offered++)
				rule_named(left, &nleft, "INSTALL FROM THE MEDIUM");
			push_medium(i);
		}
		if (!n && !extra && !offered)
			push(left, &nleft, "no match");
		return;
	}

	if (mode == ST_CATS) {
		const char *last = lastcat_load();

		push_back("Back");
		for (int g = 0; g < sh_app_ngroups(); g++) {
			const struct sh_app *tmp[1];
			if (!sh_apps_in_group(g, tmp, 1) && !medium_in_group(g))
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
		if (medium_in_group(cat)) {
			rule_named(left, &nleft, "ON THE MEDIUM");
			for (int i = 0; i < nmedium && nleft < ST_MAX_ROWS; i++)
				if (medium[i].group == cat)
					push_medium(i);
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
	/*
	 * THE COLD-START COLUMN SAYS WHY IT IS EMPTY.
	 *
	 * Below the pins this column is the most-frequently-used list, and
	 * that list is built from a usage count which does not exist yet on a
	 * session a minute old. The result was four hundred pixels of nothing
	 * under `All Programs`, which does not read as "no history yet", it
	 * reads as a menu that failed to draw its lower half.
	 *
	 * One dim line, and it goes away the moment there is anything to put
	 * there. A rule is not drawn above it for the same reason there is
	 * none above the pins: it would be separating something from nothing.
	 */
	if (nf <= 0) {
		r = push(left, &nleft, "No frequent apps yet");
		if (r)
			r->rule = 2;	/* a caption, not a control */
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
	/* Three groups, not one long run mixing places, settings and power.
	 * The first heading is not redundant: without it the column starts
	 * with five rows and then acquires a caption, which reads as the
	 * SECOND group being the only one that was labelled. */
	rule_named(right, &nright, "PLACES");

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

	rule_named(right, &nright, "SYSTEM");

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
	r = push(right, &nright, "Boxes");
	if (r) {
		/*
		 * The environments. Every fat application on this machine is
		 * already in one, so a row that opens the page where they are
		 * measured is the way in for a whole lane of the system — and
		 * a lane with no line in a menu is a lane nothing can reach.
		 */
		r->keys = "box container environment distrobox pack apps";
		r->icon = "package-x-generic";
		r->argv[0] = "kdos-res";
		r->argv[1] = "--page";
		r->argv[2] = "boxes";
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
}

/*
 * ── the power row ───────────────────────────────────────────────────────
 *
 * ALL FIVE VERBS IN ONE PLACE, and out of the list.
 *
 * They were split: Log Off and Shut Down were footer buttons and Lock,
 * Suspend and Restart were rows near the bottom of the right column — five
 * things of one kind in two places, and the three rows were what made that
 * column exactly full. Taking them out is what buys the third group heading
 * and four rows of height; a menu that is shorter is the point of this shape.
 *
 * THREE CELLS EACH AND NO LABEL, which is a trade this file can only make
 * because it already has somewhere to put the name: with nothing typed, the
 * search field explains the SELECTION, and a pointer resting on a power icon
 * is a selection. Hovering one turns the field into `Shut Down — turn this
 * machine off`, so the icons are self-describing without a second surface and
 * without a tooltip process.
 *
 * AND THEY ARE STILL REACHABLE FROM THE KEYBOARD, because a search walks this
 * table beside the right column's: typing `sleep` or `reboot` finds them and
 * Enter runs them. An icon row that could only be clicked would have taken
 * three verbs off the keyboard to buy four rows.
 */
static void build_power(void)
{
	struct row *r;

	npower = 0;
	r = push(power_row, &npower, "Lock Screen");
	if (r) {
		r->keys = "lock away screen";
		r->icon = "system-lock-screen";
		r->argv[0] = "kdos-lock";
	}
	/* Suspend asks nothing, because it is the one power action that undoes
	 * itself. Everything below it asks, because none of them does. */
	r = push(power_row, &npower, "Suspend");
	if (r) {
		r->keys = "sleep power suspend";
		/* A MOON, not Papirus's `system-suspend`, which is a circle
		 * with a minus in it and reads as "blocked" rather than as
		 * "asleep" — and sat two icons from `system-reboot`, which is
		 * also a ring. The moon is what every phone made since 2015
		 * uses, and it is the same name the notification applet
		 * already borrows for Do Not Disturb. */
		r->icon = "weather-clear-night";
		r->argv[0] = "kdos-power";
		r->argv[1] = "suspend";
	}
	r = push(power_row, &npower, "Restart");
	if (r) {
		r->keys = "reboot restart power";
		r->icon = "system-reboot";
		r->argv[0] = "kdos-power";
		r->argv[1] = "reboot";
		r->confirm = "Restart this machine?";
	}
	/*
	 * SIGTERM to the compositor, not a kdos-power verb: kdos-powerd knows
	 * suspend, poweroff and reboot and nothing about a session. `-x` is
	 * load-bearing — `kdos-desk` was a substring of `kdos-desktop`, which
	 * is exactly how a signal ends the wrong process.
	 */
	r = push(power_row, &npower, "Log Off");
	if (r) {
		r->keys = "logout log off session exit quit";
		/* A DOOR, not `system-log-out`: Papirus draws that as a ring
		 * with an arrow, which beside `system-reboot`'s ring with an
		 * arrow is the same picture twice — measured on the booted
		 * ISO, the third and fourth buttons were indistinguishable. */
		r->icon = "application-exit";
		r->argv[0] = "pkill";
		r->argv[1] = "-TERM";
		r->argv[2] = "-x";
		r->argv[3] = "kdos-comp";
		r->confirm = "Log out of this session?";
	}
	r = push(power_row, &npower, "Shut Down");
	if (r) {
		r->keys = "shutdown poweroff halt off power";
		r->icon = "system-shutdown";
		r->argv[0] = "kdos-power";
		r->argv[1] = "poweroff";
		r->confirm = "Shut down this machine?";
	}
}

/*
 * A THREE-LETTER TAG WHEN THERE IS NO ARTWORK, not the first letter.
 *
 * `icons = no`, a tty and a machine with no atlas all draw the fallback, and
 * five verbs whose initials are L S R L S came out as two pairs a person
 * cannot tell apart — on the row that shuts the machine down. Three cells is
 * exactly three characters, which is enough to be unambiguous, and the field
 * beside them still spells the whole name out on hover.
 */
static const char *power_tag(int i)
{
	static const char *const t[] = { "Lck", "Slp", "Rst", "Out", "Off" };

	return i >= 0 && i < 5 ? t[i] : "?";
}

/* What the search field says while the pointer is on one of them. */
static const char *power_blurb(int i)
{
	static const char *const b[] = {
		"lock the session; the windows stay where they are",
		"sleep now, and resume where you were",
		"restart this machine",
		"end this session and return to the login prompt",
		"turn this machine off",
	};

	return i >= 0 && i < 5 ? b[i] : "";
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

/* Cut to `room` columns with an ellipsis — see below, beside the search field
 * it was written for. A row's LABEL needs it just as much: `GNU Image
 * Manipulation Program` came out as `GNU Image Manipula`, which is not the
 * name of anything and reads as a rendering fault rather than as a cut. */
static void fit_words(char *dst, size_t n, const char *src, int room);

static int pin_col(int x, int w)
{
	return x + w - PIN_W - 1;
}

static void draw_row(const struct row *r, int x, int y, int w, int selected)
{
	int fg = KT_TEXT;
	int bg = KT_BG;

	if (r->rule == 2) {
		/* A caption row: it explains, it is not a control and it never
		 * takes the selection. */
		ktui_draw_text(x + 3, y, w - 3, r->label, KT_MID, KT_BG,
			       KT_A_NONE);
		return;
	}
	if (r->rule) {
		/*
		 * KT_MID, NOT KT_DIM. `dim` is a FILL at 1.63:1 against the
		 * body, so the rules this menu's whole structure rests on were
		 * the least visible thing in it.
		 *
		 * Still a GLYPH and not the pixel hairline the taskbar
		 * separates with, and the difference is what each buys. The
		 * bar's hairline reclaims a whole COLUMN on an eighty-column
		 * strip; here the row is already the rule's and nothing is
		 * saved, while `─` tiles into the same continuous line, draws
		 * at every glyph tier and keeps the committed golden a picture
		 * of the layout rather than of half of it.
		 */
		ktui_draw_hline(x, y, w, KT_G_HL, KT_MID, KT_BG);
		if (r->label[0]) {
			int lw = ktui_utf8_width(r->label);

			/* Inset by two so the rule runs into the caption from
			 * both sides rather than stopping dead at it. */
			if (lw + 4 <= w)
				ktui_draw_text(x + 2, y, lw, r->label, KT_MID,
					       KT_BG, KT_A_NONE);
		}
		return;
	}
	/*
	 * THE SELECTED ROW IS A PLATE AND AN ACCENT EDGE, not a slab of full
	 * accent — the same change the taskbar's focused window got, from the
	 * same tone table, so a selection means one thing on this desktop.
	 *
	 * A full-width 14:1 fill was the loudest object in the menu, louder
	 * than the title and the pinned marks and the thing it was pointing
	 * at. A raised plate with a two-pixel accent bar down its left edge
	 * says the same thing and lets the row's own contents be read.
	 */
	if (selected)
		kch_px_row(x, y, w, KCH_T_ACTIVE);

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
	{
		char shown[160];

		fit_words(shown, sizeof(shown), r->label, room);
		ktui_draw_text(tx, y, room, shown, fg, bg, KT_A_NONE);
	}
	if (tagw)
		ktui_draw_text(pin_col(x, w) - tagw, y, tagw - 1, "[box]",
			       selected ? fg : KT_DIM, bg, KT_A_NONE);
	if (r->submenu)
		ktui_draw_text(x + w - 2, y, 1, ktui_glyph[KT_G_RIGHT], fg, bg,
			       KT_A_NONE);
	else if (r->app && (r->pinned || selected)) {
		int px = pin_col(x, w);

		/*
		 * THE PIN MARKER IS A GLYPH, NEVER THE ICON THEME'S STAR.
		 *
		 * Papirus's `starred` is a gold star and the atlas maps yellow
		 * onto the palette's SECONDARY, so it came out entirely correct
		 * and entirely wrong: saturated amber at 10.5:1, four of them
		 * in a vertical run down a green menu, out-shouting the
		 * application names they annotate. A pin is metadata.
		 *
		 * Recolouring the sprite is not on the table either — a sprite
		 * carries its own pixels and the fg/bg handed to
		 * ktui_draw_sprite reach only the fallback, which is why
		 * changing that colour did nothing.
		 *
		 * ■ AND ·, NOT █. The full block is a whole cell, so four
		 * pinned rows in a run merge into one unbroken column down the
		 * edge of the menu and read as a scrollbar — which the menu
		 * also has, two columns over. A mark has to have air around it
		 * to be a mark.
		 */
		ktui_draw_text(px + 1, y, 1,
			       ktui_glyph[r->pinned ? KT_G_SQUARE : KT_G_DOT],
			       r->pinned ? KT_MID : KT_DIM, bg, KT_A_NONE);
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

/*
 * Fit `src` into `room` columns, cutting at the last word boundary and marking
 * the cut with an ellipsis.
 *
 * The boundary matters more than the ellipsis: a clip at the column drops the
 * reader mid-word, which reads as a rendering fault rather than as text that
 * carries on. Falls back to a hard cut when the first word is itself too long,
 * because half a word is still better than nothing at all.
 */
static void fit_words(char *dst, size_t n, const char *src, int room)
{
	int w = 0, cut = -1, cutw = 0, i;

	if (room < 2 || ktui_utf8_width(src) <= room) {
		snprintf(dst, n, "%s", src);
		return;
	}
	/*
	 * `i` COUNTS BYTES AND `w` COUNTS COLUMNS, and the word boundary has
	 * to be remembered in both. Comparing a byte index against a column
	 * budget is the `%.14s` mistake in another shape: one em dash in the
	 * string and the two run three apart, so the cut lands earlier than
	 * the room allows and a line that would have fitted is truncated.
	 */
	for (i = 0; src[i]; i++) {
		/* A continuation byte is part of the character before it and
		 * occupies no column of its own. */
		if (((unsigned char)src[i] & 0xC0) == 0x80)
			continue;
		if (w >= room - 1)
			break;
		if (src[i] == ' ') {
			cut = i;
			cutw = w;
		}
		w++;
	}
	if (cut > 0 && cutw > room / 2)
		i = cut;
	if ((size_t)i + 4 >= n)
		i = (int)n - 4;
	snprintf(dst, n, "%.*s\xe2\x80\xa6", i, src);
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

	/*
	 * A SLOT THE BACKDROP OWNS STILL HAS TO BE FILLED.
	 *
	 * The alpha decides whether the PAINTER puts pixels down; the fill is
	 * what resets the CELL's glyph, and dropping it because "KT_BG is
	 * transparent now" left every cell this frame does not write to
	 * holding what the last one put there. Measured: the search field
	 * explains the selection, so its text changes on every pointer move —
	 * a shorter line left the tail of the longer one behind it and the
	 * field read `Suspend — sleep now, and……`, with two ellipses because
	 * one of them was last frame's.
	 *
	 * ktui_draw_box draws a BORDER and nothing inside it, so this is the
	 * only clear this surface gets.
	 */
	kch_px_reset();
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	ktui_draw_box(krect(0, 0, w, h), title_text(), KT_ACCENT, KT_BG, 1);

	/* The divider between the columns, and the one above the footer. Both
	 * in KT_MID: `dim` is a fill at 1.63:1 against the body, and these two
	 * lines are what the whole layout rests on. */
	ktui_draw_vline(lw, 1, body, KT_G_VL, KT_MID, KT_BG);
	ktui_draw_hline(1, h - 3, w - 2, KT_G_HL, KT_MID, KT_BG);

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
	 * ── the footer: a search FIELD and the five power verbs ──
	 *
	 * Right-aligned, three cells each, most-destructive LAST so that Shut
	 * Down is the one against the frame and a hand that overshoots hits
	 * the border rather than the machine's power. Their names are in the
	 * field beside them — see build_power().
	 */
	int bx = w - 2 - (npower * POW_STRIDE - 1);

	pow_x = bx;
	for (int i = 0; i < npower; i++) {
		int px = bx + i * POW_STRIDE;
		int slot = icons_on && power_row[i].icon
				   ? kicon_slot(power_row[i].icon, POW_W, 1)
				   : -1;

		if (hover_power == i)
			kch_px_plate(px, h - 2, POW_W, 1, KCH_T_HOVER, 1);
		if (slot >= 0) {
			ktui_draw_sprite(krect(px, h - 2, POW_W, 1), slot,
					 KT_TEXT, KT_BG);
			continue;
		}
		ktui_draw_text(px, h - 2, POW_W, power_tag(i),
			       hover_power == i ? KT_TEXT : KT_MID, KT_BG,
			       KT_A_NONE);
	}

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
	/*
	 * THE WELL IS A PLATE, not a cell fill. A KT_SURFACE fill is opaque —
	 * a solid rectangle in the middle of a translucent menu — and it is a
	 * square-cornered shape beside a row of rounded ones. Recorded like
	 * every other plate here, with the cells left on the slot the backdrop
	 * owns so it shows through.
	 */
	int lit = search_lit || query[0];
	int fbg = KT_BG;
	int ffg = query[0] ? KT_TEXT : KT_MID;
	int fmark = lit ? KT_ACCENT : KT_MID;

	search_x = 2;
	search_end = bx - 2 < w / 2 + 8 ? bx - 2 : w / 2 + 8;
	clear_x = clear_end = 0;
	if (search_end > search_x + 6) {
		int fw = search_end - search_x;
		int tx = search_x;

		kch_px_plate(search_x, h - 2, fw, 1,
			     lit ? KCH_T_ACTIVE
				 : hover_btn == 2 ? KCH_T_HOVER : KCH_T_REST,
			     1);
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

		char field[192], shown[192];
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
			/*
			 * A POWER ICON UNDER THE POINTER IS A SELECTION, and
			 * this is what makes five unlabelled pictures
			 * self-describing without a second surface. It wins
			 * over the list's own comment because the pointer is
			 * the more recent statement of what somebody is
			 * looking at.
			 */
			if (hover_power >= 0 && hover_power < npower)
				snprintf(field, sizeof(field), "%s — %s",
					 power_row[hover_power].label,
					 power_blurb(hover_power));
			else
				snprintf(field, sizeof(field), "%s",
					 why ? why : "Type to search");
		}
		/* CUT ON A WORD BOUNDARY. ktui_draw_text clips at the column,
		 * which turned every comment that did not fit into a sentence
		 * stopped mid-syllable — `A wayland native terminal emula`,
		 * photographed. An ellipsis says the text continues; a severed
		 * word says the program is broken. */
		fit_words(shown, sizeof(shown), field, room);
		ktui_draw_text(tx, h - 2, room, shown, ffg, fbg, KT_A_NONE);
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
 * Log Out is `pkill -TERM -x kdos-comp`, which looks blunt and is the accurate
 * description of what logging out means here: the compositor IS the session,
 * labwc handles SIGTERM through its own event loop and tears down in order,
 * and kdos-desktop returns to the tty. It is an ordinary power row's argv now
 * — see build_power() — because every one of the five carries its own command
 * and its own question, and two of them being special cases in the footer is
 * what kept them out of the search.
 */

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
	int at_x = -1, at_y = 0, dump = 0, dump_cells = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[i + 1]);
			at_y = atoi(argv[i + 2]);
			i += 2;
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--no-icons")) {
			icons_on = 0;
		} else if (!strcmp(argv[i], "--dump-cells")) {
			dump = dump_cells = 1;
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			fprintf(stderr, "usage: kdos-start [--at-bottom X Y] "
					"[--font NAME] [--no-icons] [--dump]\n");
			return 2;
		}
	}

	sh_apps_load();
	st_medium_load();
	/* The fixed rows first: a search reads them, and the one at startup
	 * would otherwise run against an empty right column. */
	build_right();
	build_power();
	build_left();

	if (dump) {
		sh_theme_from_cache();
		dumping = 1;
		icons_on = 0;
		/*
		 * `--dump-cells` prints the COLOURS as well, which is what a
		 * text dump cannot: the selection here went from a slab of
		 * accent to a plate this arc, and the character grid is
		 * byte-identical across that change.
		 */
		if (dump_cells) {
			ktui_backend_set(sh_cells_backend(ST_COLS, ST_ROWS));
			ktui_draw_init();
			draw_frame();
			return 0;
		}
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
	/*
	 * The Start menu wears the SAME body as the taskbar it opens from —
	 * they are on screen together, touching, and two surfaces of one
	 * desktop painted from two different palettes is exactly the "somebody
	 * else's program" read this arc exists to remove. One call, shared
	 * with every other popup the bar opens.
	 */
	kch_px_popup(KT_BG);
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
				hover_power = -1;
				if (ev.my == ktui_h - 2 && npower > 0 &&
				    ev.mx >= pow_x &&
				    ev.mx < pow_x + npower * POW_STRIDE) {
					int k = (ev.mx - pow_x) / POW_STRIDE;

					/* The gap column belongs to neither
					 * button: a click there must do
					 * nothing rather than the thing on
					 * its left. */
					if ((ev.mx - pow_x) % POW_STRIDE
							< POW_W && k < npower)
						hover_power = k;
				}
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
				if (hover_power >= 0 &&
				    hover_power < npower) {
					if (activate(&power_row[hover_power]))
						break;
					continue;
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

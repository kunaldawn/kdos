/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-desk — the desktop itself
 *
 *      ▓  Home          ▒  notes.txt      ░  Trash
 *
 *      ▒  report.odt    ▓  Projects
 *
 * A layer-shell surface on the BACKGROUND layer, above the wallpaper the
 * compositor draws and below every window. `~/Desktop` as a grid of cells:
 * one glyph for the kind of thing it is, the name under it, arrow keys and a
 * pointer to pick, Enter or double-click to open.
 *
 * THE GLYPH IS THE ICON, and that is not a compromise made for want of an icon
 * theme — it is the same decision the tray made. A character grid has one cell,
 * and one cell of a 256-colour PNG scaled to 16x32 is mud. A filled block for a
 * directory and a light one for a file carries the distinction that actually
 * matters at a glance, in the palette everything else on this desktop is drawn
 * in.
 *
 * NO EXCLUSIVE ZONE. The desktop is what windows sit ON; reserving space for it
 * would shrink the usable box and every maximised window with it.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kicon.h"
#include "kwl.h"
#include "kwm.h"
#include "shell.h"

#define MAX_ENTRIES 256
#define CELL_W 18		/* cells per icon column, name included */
#define CELL_H 2		/* the glyph row and the name row */

struct entry {
	char name[256];		/* what is drawn — a .desktop's Name, not its file name */
	char path[1400];
	char exec[256];		/* a .desktop's Exec, field codes stripped */
	bool dir;
	bool is_trash;
	bool pinned;		/* Home and Trash: places, not files */
	bool is_app;		/* a .desktop file: launch it, do not open it */
	bool terminal;		/* ...inside foot */
	char icon[96];		/* a .desktop's Icon=, for the picture layer */
	long mtime;		/* for Sort Icons ▸ date */
};

static struct entry entries[MAX_ENTRIES];
static int nentries;
static int sel;
/* comp.conf's `icons`, through --no-icons. */
static int icons_on = 1;

/*
 * The context menu, drawn INTO the desktop's own grid.
 *
 * kdos-desk owns the whole output, so a popup here is not a second surface —
 * it is a box painted over the icons, and while it is up the desktop claims
 * the whole surface for input again (see input_region()). Its width is
 * libktui's: the widget measures its own rows and clamps itself on screen.
 */
/* Rows carry an ID and the run switch dispatches on it, so a row that is
 * hidden for this entry can never be run by its position. */
enum { CT_RENAME, CT_NEWDIR, CT_NEWFILE, CT_EMPTY, CT_REFRESH, CT_SORT,
       CT_APPS, CT_WALL, CT_DISPLAY, CT_SETTINGS, CT_RULE };

/*
 * THE TWO ID SPACES MUST NOT COLLIDE. The shared verbs are `KXDG_VERB_*` and
 * these are `CT_*`, both small and both starting at zero, so the local ones
 * are offset on the way into the menu and taken apart again on the way out.
 */
#define DESK_LOCAL 100

/*
 * WHO the row is for. Right-clicking an icon and right-clicking the wallpaper
 * are two different menus and this is one table, because two tables are two
 * places for "New Folder" to drift apart.
 *
 * The desktop half exists because there was NONE. Every click on bare
 * wallpaper fell through to the compositor, so the only way to reach New
 * Folder was to right-click an icon — and on a fresh login the only icons are
 * Home and Trash, both of which are places rather than files. A desktop you
 * cannot create anything on is a desktop that looks read-only, which is
 * exactly what it was reported as.
 */
#define SC_ITEM 1		/* an icon was under the pointer */
#define SC_DESK 2		/* bare wallpaper */
#define SC_BOTH (SC_ITEM | SC_DESK)

static const struct {
	const char *label;
	int id;
	int scope;
	int trash_only;		/* Empty Trash is not offered on a photo */
	int no_pin;		/* Rename: Home and Trash are places */
	int dir_only;		/* Add to Places: a file is not a place */
} CTX[] = {
	/*
	 * The `&` marks the accelerator, and the marks must be unique WITHIN A
	 * SCOPE rather than within the table: the desktop menu and an icon's
	 * menu are different sets of rows, and the letter picks the first
	 * SHOWN row that carries it.
	 *
	 * THE SHARED VERBS ARE IN BOTH SCOPES AND HAVE THE FIRST CLAIM on a
	 * letter — o k e t f p s g m — so these are lettered around them. `s`
	 * is Settings' here and Share's there, and the two never appear on one
	 * menu: Share is a verb on a THING and the wallpaper has none.
	 */
	/* Open, Open Terminal Here, Add to Places, Move to Trash and the rest
	 * of the FILE verbs are libkxdg's — see the pane built below. What is
	 * left here is what only a DESKTOP can answer. */
	{ "&Rename",             CT_RENAME,   SC_ITEM, 0, 1, 0 },
	{ "Empt&y Trash",        CT_EMPTY,    SC_ITEM, 1, 0, 0 },
	{ "&New Folder",         CT_NEWDIR,   SC_BOTH, 0, 0, 0 },
	{ "Ne&w File",           CT_NEWFILE,  SC_BOTH, 0, 0, 0 },
	{ "",                    CT_RULE,     SC_DESK, 0, 0, 0 },
	{ "Sort &Icons",         CT_SORT,     SC_DESK, 0, 0, 0 },
	{ "Refres&h",            CT_REFRESH,  SC_BOTH, 0, 0, 0 },
	{ "",                    CT_RULE,     SC_DESK, 0, 0, 0 },
	/* The compositor's root menu used to own this corner of the screen and
	 * now does not, so everything it offered has to be reachable here or
	 * the change is a regression. */
	{ "&Applications",       CT_APPS,     SC_DESK, 0, 0, 0 },
	{ "&Change Wallpaper",   CT_WALL,     SC_DESK, 0, 0, 0 },
	{ "&Display Settings",   CT_DISPLAY,  SC_DESK, 0, 0, 0 },
	{ "&Settings",           CT_SETTINGS, SC_DESK, 0, 0, 0 },
};
#define NCTX ((int)(sizeof(CTX) / sizeof(CTX[0])))

/*
 * THE MENU IS libktui's, and the geometry, the caret, the arrows, the hit test
 * and the click-away are all its. What stays here is the part only the desktop
 * knows: which rows are on the menu for the thing that was clicked.
 *
 * `ctx_item` is DERIVED from CTX rather than written beside it. The widget
 * needs a contiguous KtuiMenuItem array and the scope columns have nowhere to
 * live in one, and a second hand-maintained table is the drift this desktop
 * already paid for once.
 */
/* The shared verbs, a rule, then this file's own rows — see ctx_show(). */
static KtuiMenuItem ctx_item[KXDG_VERB_MAX + 1 + NCTX];
static KtuiMenuPane ctx_pane = { NULL, ctx_item, 0 };
static int nverb;
static KtuiMenu menu;
static KtuiKeys keys;
static int ctx_for;		/* which entry it belongs to, or -1: the desktop */

/* How the grid is ordered. Cycled by the desktop menu's Sort Icons and kept
 * for the session only — a desktop that remembered a sort somebody tried once
 * would need a file to forget it in. */
enum { SORT_NAME = 0, SORT_TYPE, SORT_TIME, SORT_N };
static int sort_by = SORT_TYPE;
static const char *const SORT_NAMES[SORT_N] = { "name", "type", "date" };

/* The name line-edit, pick.c's editing pattern worn by Rename, New Folder and
 * New File. It lives on the status row; keys own it while it is up. */
enum { ED_NONE = 0, ED_RENAME, ED_NEWDIR, ED_NEWFILE };
static int edit_mode;
static int edit_for;		/* the entry Rename targets */
static char edit_buf[256];

/* ── the trash ─────────────────────────────────────────────────────────── */

/*
 * The freedesktop trash is libkbase's (`kb_trash_*`), not this file's, because
 * `kdos trash` at a prompt has to mean exactly what the Delete key here means.
 * What stays local is the QUESTION — the confirm, the two pinned places that
 * are not files, and the status line.
 */

/* ── dragging a file off the desktop, and onto the trash ───────────────── */

/* Which icon is at a cell, or -1. The gap column beside an icon and the name
 * row below it are wallpaper: a drop there belongs to nothing. */
static int icon_at(int mx, int my);
static void reload(void);

static int drag_from = -1;	/* the icon a press went down on */
static int drag_cx, drag_cy;
static int dragging;

/*
 * A file:// URI, percent-encoding only what would otherwise end the URI or
 * start a fragment. Anything else is left alone: a uri-list is read by
 * programs that show the name, and over-encoding makes it unreadable there.
 */
static void uri_of(const char *path, char *out, size_t n)
{
	/* libkbase's, because the thumbnail cache is named by the MD5 of this
	 * exact string and a second escaper here would make every thumbnail
	 * this desktop wrote invisible to everything else. */
	kb_uri_file(path, out, n);
}

static void unpercent(const char *in, char *out, size_t n)
{
	size_t o = 0;

	for (; *in && o + 1 < n; in++) {
		if (in[0] == '%' && isxdigit((unsigned char)in[1]) &&
		    isxdigit((unsigned char)in[2])) {
			char b[3] = { in[1], in[2], 0 };

			out[o++] = (char)strtol(b, NULL, 16);
			in += 2;
		} else {
			out[o++] = *in;
		}
	}
	out[o] = '\0';
}

static int drag_begin(int i)
{
	char uri[1600];

	/* Home and Trash are places, not files: there is nothing to pick up. */
	if (i < 0 || i >= nentries || entries[i].pinned)
		return 0;

	uri_of(entries[i].path, uri, sizeof(uri) - 3);
	/* CRLF terminates EVERY line of a uri-list, the last one included. */
	strncat(uri, "\r\n", sizeof(uri) - strlen(uri) - 1);

	return kdisp_drag_start("text/uri-list", uri, strlen(uri)) == 0;
}

static void drop_to_trash(const char *uris, char *status, size_t n)
{
	int done = 0, failed = 0;
	const char *p = uris;

	while (*p) {
		char line[1600], path[1400];
		size_t k = 0;

		while (*p && *p != '\r' && *p != '\n' && k + 1 < sizeof(line))
			line[k++] = *p++;
		line[k] = '\0';
		while (*p == '\r' || *p == '\n')
			p++;

		/* A comment line is part of the format, not a file. */
		if (!k || line[0] == '#')
			continue;
		/* Anything that is not a local file is not ours to move, and
		 * silently doing nothing is better than reporting a failure
		 * for something that was never a file. */
		if (strncmp(line, "file://", 7))
			continue;

		unpercent(line + 7, path, sizeof(path));
		if (kb_trash_put(path) == 0)
			done++;
		else
			failed++;
	}

	if (done || failed)
		snprintf(status, n, "moved %d to the trash%s", done,
			 failed ? " — some could not be moved" : "");
	reload();
}

/* ── reading the desktop directory ─────────────────────────────────────── */

/*
 * WHICH directory. `libkxdg` owns the answer, because the Places menu and the
 * chooser's sidebar ask the same question — a second reader here is how the
 * icons ended up in the folder `user-dirs.dirs` named while the menu opened an
 * empty one beside it.
 */
static void desktop_dir(char *out, size_t n)
{
	kxdg_user_dir("DESKTOP", out, n);
}

/*
 * A `.desktop` file on the desktop is an APPLICATION, not a document.
 *
 * Every other desktop in the world shows its Name and runs its Exec; showing
 * `firefox-esr.desktop` and handing it to a MIME lookup that has no handler for
 * application/x-desktop is how a shortcut becomes a file that cannot be opened.
 * Returns 0 when the file is not a usable entry, and the caller then treats it
 * as an ordinary file.
 */
static int load_desktop_entry(struct entry *it)
{
	KxdgEntry e;
	if (kxdg_load(&e, it->path, "Desktop Entry") != 0)
		return 0;

	const char *name = kxdg_get(&e, "Name", NULL);
	const char *exec = kxdg_get(&e, "Exec", NULL);
	if (!name || !exec || !*exec) {
		kxdg_free(&e);
		return 0;
	}
	snprintf(it->name, sizeof(it->name), "%s", name);
	snprintf(it->exec, sizeof(it->exec), "%s", exec);
	it->terminal = kxdg_bool(&e, "Terminal", 0);
	it->is_app = true;
	snprintf(it->icon, sizeof(it->icon), "%s", kxdg_get(&e, "Icon", ""));
	kxdg_free(&e);

	/* The field codes are placeholders for documents this launch has none
	 * of; one shared stripper (shell.c) — three diverged copies was F17. */
	sh_strip_field_codes(it->exec);
	return 1;
}

static int cmp_entry(const void *a, const void *b)
{
	const struct entry *x = a, *y = b;

	/* Newest first, because "sort by date" is asked for to find the thing
	 * that just arrived. Ties fall through to the name so the order is
	 * total — a qsort with an inconsistent comparator reorders equal rows
	 * on every rescan, which on a desktop is icons that move by
	 * themselves. */
	if (sort_by == SORT_TIME && x->mtime != y->mtime)
		return x->mtime > y->mtime ? -1 : 1;
	/* Directories first, then by name — the order every file manager has
	 * used since Norton Commander, and the one people scan by. SORT_NAME
	 * is the one mode that does NOT group them, which is the whole
	 * difference between the two. */
	if (sort_by != SORT_NAME && x->dir != y->dir)
		return x->dir ? -1 : 1;
	return strcasecmp(x->name, y->name);
}

static void reload(void)
{
	const char *home = getenv("HOME");
	char dir[1024];

	nentries = 0;
	if (!home)
		return;
	desktop_dir(dir, sizeof(dir));
	/*
	 * MAKE IT IF IT IS NOT THERE. `/etc/skel` carries no Desktop folder,
	 * so on a fresh login this directory did not exist — the readdir below
	 * failed silently and the desktop showed Home and Trash and nothing
	 * else, forever. "New Folder" then had nowhere to put anything and
	 * dragging a file to the desktop had no destination, which reads as a
	 * desktop that cannot hold files rather than as a missing directory.
	 * Creating it costs one mkdir per rescan when it already exists.
	 */
	mkdir(dir, 0755);

	/*
	 * Home and Trash come FIRST — fixed cells at the START of the grid.
	 * The grid fills left to right, so pins at the front survive a full
	 * desktop (appended last, they were the first thing overflow dropped)
	 * and never move when a file is created; a desktop whose fixed icons
	 * move is a desktop nobody builds muscle memory on.
	 */
	struct entry *it = &entries[nentries++];
	memset(it, 0, sizeof(*it));
	snprintf(it->name, sizeof(it->name), "Home");
	snprintf(it->path, sizeof(it->path), "%s", home);
	it->dir = true;
	it->pinned = true;

	it = &entries[nentries++];
	memset(it, 0, sizeof(*it));
	snprintf(it->name, sizeof(it->name), "Trash");
	snprintf(it->path, sizeof(it->path), "%s/.local/share/Trash/files",
		 home);
	it->dir = true;
	it->is_trash = true;
	it->pinned = true;

	DIR *d = opendir(dir);
	if (d) {
		struct dirent *e;
		while ((e = readdir(d)) && nentries < MAX_ENTRIES) {
			if (e->d_name[0] == '.')
				continue;
			it = &entries[nentries];
			memset(it, 0, sizeof(*it));
			snprintf(it->name, sizeof(it->name), "%s", e->d_name);
			snprintf(it->path, sizeof(it->path), "%s/%s", dir,
				 e->d_name);
			struct stat st;
			if (stat(it->path, &st) == 0) {
				it->dir = S_ISDIR(st.st_mode);
				it->mtime = (long)st.st_mtime;
			}
			size_t n = strlen(e->d_name);
			if (!it->dir && n > 8 &&
			    !strcmp(e->d_name + n - 8, ".desktop"))
				load_desktop_entry(it);
			nentries++;
		}
		closedir(d);
	}
	qsort(entries + 2, (size_t)(nentries - 2), sizeof(entries[0]),
	      cmp_entry);
}

/* ── opening ───────────────────────────────────────────────────────────── */

static void spawn(const char *const argv[])
{
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

/*
 * Ask before doing something that cannot be undone.
 *
 * kdos-prompt is kdos-shell under another name and is the same dialog the
 * compositor's Log Out goes through, so every irreversible thing on this
 * desktop asks in the same words with the same buttons. Synchronous: there is
 * nothing to do until the answer arrives.
 */
static int confirmed(const char *question)
{
	pid_t pid = fork();
	if (pid < 0)
		return 0;
	if (pid == 0) {
		execlp("kdos-prompt", "kdos-prompt", "--message", question,
		       "--yes", "Yes", "--no", "No", (char *)NULL);
		_exit(254);
	}
	int st = 0;
	if (waitpid(pid, &st, 0) < 0 || !WIFEXITED(st))
		return 0;
	return WEXITSTATUS(st) == 0;
}

static void open_entry(const struct entry *it)
{
	/*
	 * A shortcut runs; it is not opened. argv, never a shell — an Exec line
	 * comes from a file anything can write, which is the rule everywhere in
	 * this tree.
	 */
	if (it->is_app) {
		char buf[288];
		char id[160];		/* argv points into it until the exec */
		const char *argv[34];
		int n = 0;
		if (it->terminal)
			n = sh_term_argv(argv, n, 34, it->exec, id,
					 sizeof(id));
		snprintf(buf, sizeof(buf), "%s", it->exec);
		char *save = NULL;
		for (char *tok = strtok_r(buf, " \t", &save);
		     tok && n < 32; tok = strtok_r(NULL, " \t", &save))
			argv[n++] = tok;
		argv[n] = NULL;
		if (n)
			spawn(argv);
		return;
	}
	/*
	 * THE TRASH IS NOT A DIRECTORY TO BROWSE. Its path is one, and a file
	 * manager opened on it shows the escaped names in `files/` with no
	 * origin, no deletion date and no way back — the record that carries
	 * all three lives in `info/` beside it. `kdos-trash` reads the pair.
	 */
	if (it->is_trash) {
		const char *tv[] = { "kdos-trash", NULL };

		spawn(tv);
		return;
	}

	/*
	 * Everything else — a file AND a directory — goes to the MIME handler,
	 * which on this machine is kdos-appbox. A directory used to be a
	 * hardcoded `foot -e mc` here, which is a SECOND answer to a question
	 * `inode/directory=mc.desktop` in mimeapps.list already answers: change
	 * the default file manager and the desktop would have kept opening mc.
	 */
	const char *argv[] = { "kdos-appbox", "open", it->path, NULL };
	spawn(argv);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int columns(void)
{
	int c = ktui_w / CELL_W;
	return c < 1 ? 1 : c;
}

/* How many grid cells fit on screen — mirrors the draw loop's
 * `cy + 1 >= h` guard exactly, or the keyboard selects invisible entries. */
static int cells_fit(void)
{
	int fit = ((ktui_h - 1) / CELL_H) * columns();
	return fit < 1 ? 1 : fit;
}

/* What is actually DRAWN. One cell is reserved for the `+N more` marker when
 * the directory overflows the screen — an overflow with no marker is files
 * that silently do not exist. */
static int drawn_count(void)
{
	int fit = cells_fit();
	return nentries > fit ? fit - 1 : nentries;
}

/*
 * DRAWN entries only: the cells past the overflow marker belong to nothing.
 * The row BELOW an icon and the gap column beside it are wallpaper, and a menu
 * or a drop that landed on the icon above would act on something the pointer is
 * not over.
 */
static int icon_at(int mx, int my)
{
	int i = ((my - 1) / CELL_H) * columns() + (mx - 1) / CELL_W;

	if (mx < 1 || my < 1 || (my - 1) % CELL_H != 0 ||
	    (mx - 1) % CELL_W >= CELL_W - 1 ||
	    i < 0 || i >= drawn_count())
		return -1;

	return i;
}

/* Split out so the draw loop reads as layout rather than as a lookup. */
static const char *it_glyph(int i)
{
	if (entries[i].is_trash)
		return ktui_glyph[KT_G_SHADE];
	if (entries[i].is_app)
		return ktui_glyph[KT_G_SQUARE];
	return entries[i].dir ? ktui_glyph[KT_G_FULL] : ktui_glyph[KT_G_DOT];
}

/*
 * WHICH CELLS THE DESKTOP ANSWERS FOR.
 *
 * This surface covers the whole output and now CLAIMS it. See the body for
 * why that reversed, and for what had to be added to the desktop's own menu
 * before it could.
 */
static void input_region(void)
{
	static KRect claim[MAX_ENTRIES + 1];
	/*
	 * Only when it CHANGED. Setting a region is a wl_surface commit, and
	 * this runs once round a loop that wakes every second — an
	 * unconditional call would put a commit per second back on an idle
	 * desktop, which is precisely the cadence the flicker fix took out of
	 * kwl_flush(). The signature is the layout: how many icons, how wide
	 * the screen is, and whether the menu is up.
	 */
	static long shown = -1;
	int cols = columns();
	long sig = ktui_menu_active(&menu)
			   ? -2
			   : (long)nentries * 100000 + cols * 100 + ktui_h;

	(void)claim;
	(void)cols;
	if (sig == shown)
		return;
	shown = sig;

	/*
	 * THE WHOLE SURFACE, and that is a reversal.
	 *
	 * It used to claim only the cells its icons occupy, so a click on bare
	 * wallpaper reached the compositor and labwc's root-menu mousebind
	 * fired. The cost was that the DESKTOP had no menu of its own: New
	 * Folder, Sort Icons and Refresh were reachable only by right-clicking
	 * an existing icon, and on a fresh login the only icons are Home and
	 * Trash. A desktop you cannot create anything on reads as read-only.
	 *
	 * So the desktop answers its own wallpaper now, and everything labwc's
	 * root menu offered is on it (Applications, Settings, Displays) —
	 * dropping the claim without replacing what it fed would have been the
	 * regression, and the menu is the replacement. `W-space` still opens
	 * the compositor's own menu for anyone who wants it.
	 */
	kdisp_input_cells(NULL, -1);
}

/* Whether the local half has any row on this menu, which is what decides
 * whether the rule between the halves is drawn. */
static int ctx_local_any(void);

/*
 * WHAT THE MENU IS ABOUT: the entry under the pointer, or — on bare wallpaper
 * — the desktop folder itself, which is what "here" means when there is no
 * icon under the cursor.
 */
static int ctx_target(char *out, size_t n, int *isdir)
{
	if (ctx_for < 0) {
		desktop_dir(out, n);
		*isdir = 1;
		return 1;
	}
	if (ctx_for >= nentries)
		return 0;
	snprintf(out, n, "%s", entries[ctx_for].path);
	*isdir = entries[ctx_for].dir;
	return 1;
}

/*
 * Whether a row is on this menu at all — the desktop's half of the widget's
 * contract, asked by its drawing and by its hit test from one walk.
 *
 * TWO HALVES, ONE WALK. The first `nverb` rows are libkxdg's file verbs, then
 * a rule, then this file's own. The rule is drawn only when both halves have
 * something: a separator with nothing on one side is a line for its own sake.
 */
static int ctx_show(int i, void *user)
{
	char path[1400];
	int isdir = 0;

	(void)user;
	if (i < nverb) {
		KxdgVerb v;

		if (!ctx_target(path, sizeof(path), &isdir) ||
		    !kxdg_verb_at(i, &v) || !kxdg_verb_shown(&v, path, isdir))
			return 0;
		/*
		 * ON BARE WALLPAPER, ONLY THE VERBS THAT MEAN "HERE". Open,
		 * Share and Move to Trash read as acting on THE THING, and on
		 * the wallpaper there is no thing — a Move to Trash there
		 * would act on the desktop folder itself.
		 */
		if (ctx_for < 0 && v.id != KXDG_VERB_TERM &&
		    v.id != KXDG_VERB_FIND && v.id != KXDG_VERB_PLACE &&
		    v.id != KXDG_VERB_GIT)
			return 0;
		/*
		 * HOME AND TRASH ARE PLACES, NOT FILES. The shared table has
		 * no notion of a pinned grid cell and must not grow one — two
		 * other surfaces read it and neither has pins — so the mask
		 * lives here, where the pin does.
		 */
		if (v.id == KXDG_VERB_TRASH && ctx_for >= 0 &&
		    entries[ctx_for].pinned)
			return 0;
		return 1;
	}
	if (i == nverb)
		return ctx_local_any();
	i -= nverb + 1;
	if (i < 0 || i >= NCTX)
		return 0;
	if (ctx_for < 0)
		return (CTX[i].scope & SC_DESK) != 0;
	if (ctx_for >= nentries)
		return 0;
	if (!(CTX[i].scope & SC_ITEM))
		return 0;
	if (CTX[i].trash_only && !entries[ctx_for].is_trash)
		return 0;
	if (CTX[i].no_pin && entries[ctx_for].pinned)
		return 0;
	/* A file is not a place. The row is offered on a folder only, so the
	 * column cannot grow an entry that opens a document. */
	if (CTX[i].dir_only && !entries[ctx_for].dir)
		return 0;
	return 1;
}

static int ctx_local_any(void)
{
	for (int i = 0; i < NCTX; i++)
		if (ctx_show(nverb + 1 + i, NULL))
			return 1;
	return 0;
}

/*
 * Open the menu for what was clicked. `ctx_for` is set BEFORE the open,
 * because the widget asks `ctx_show` for its first selectable row while it
 * opens and that answer depends on which entry the menu belongs to.
 */
static void ctx_popup(int for_entry, int x, int y)
{
	ctx_for = for_entry;
	ktui_menu_open(&menu, 0, x, y);
}

/*
 * THE SELECTION IS THE ONE THING Esc TAKES DOWN HERE. The desktop does not
 * close, so declaring the highlight as a layer is what keeps the row from
 * offering "Esc Close" on a surface that has no close.
 */
static int sel_up(void *user)
{
	(void)user;
	return sel > 0;
}

static void sel_clear(void *user)
{
	(void)user;
	sel = 0;
}

/* The name editor is the inner rung. It owns every printable key while it is
 * up, and none of the three the ladder answers, so declaring it costs the
 * editor nothing and buys the row an honest verb. */
static int edit_up(void *user)
{
	(void)user;
	return edit_mode != ED_NONE;
}

static void edit_cancel(void *user)
{
	(void)user;
	edit_mode = ED_NONE;
}

/* Where Shift+F10 pops it: on the selected icon, or the top-left of the grid
 * when nothing is selected. A menu at the origin names a row nobody is
 * looking at. */
static int ctx_at(int *x, int *y, void *user)
{
	(void)user;
	if (sel >= 0 && sel < drawn_count()) {
		int cols = columns();

		/* The icon's own cell, from the same arithmetic the grid is
		 * drawn with — a menu that popped one row off would cover the
		 * thing it acts on. */
		*x = (sel % cols) * CELL_W + 1;
		*y = (sel / cols) * CELL_H + 2;
		ctx_for = sel;
	} else {
		*x = 1;
		*y = 1;
		ctx_for = -1;
	}
	return 1;
}

static void draw(const char *status)
{
	int w = ktui_w, h = ktui_h;
	int cols = columns();

	/*
	 * The background is NOT painted — and wanting that is not the same as
	 * getting it.
	 *
	 * The compositor draws the wallpaper and this surface sits above it, so
	 * a flat fill here covers it. But libkcell paints EVERY cell including
	 * its background slot, and libkwl's shm buffer is XRGB8888, which has
	 * no alpha at all: whatever KT_BG is, it lands opaque and the wallpaper
	 * disappears the moment kdos-desk starts.
	 *
	 * So KDISP_ROLE_BACKGROUND asks libkwl for an ARGB surface and turns on
	 * libkcell's transparent-KT_BG mode: every cell the desktop does not
	 * write is cleared to zero and the wallpaper shows through. Cells that
	 * DO carry something — an icon glyph, a label, the selection bar — are
	 * painted normally.
	 */
	ktui_draw_clear();

	int drawn = drawn_count();
	for (int i = 0; i < drawn; i++) {
		int cx = (i % cols) * CELL_W + 1;
		int cy = (i / cols) * CELL_H + 1;
		if (cy + 1 >= h)
			break;

		bool on = i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		/*
		 * The PICTURE where there is one and the glyph where there is
		 * not, and the glyph half is not a placeholder — it is what a
		 * tty, an install with no artwork and `icons = no` all draw,
		 * so it stays first-class. Filled block for a directory, light
		 * for a file, medium for the trash: three levels of one ramp
		 * rather than three unrelated glyphs, so they read as a set.
		 *
		 * Two cells wide and one tall, which on a 16x32 cell is a
		 * 32x32 square — an icon's own shape. Asking for two rows here
		 * would put the picture over the label.
		 */
		int icon = -1;
		if (icons_on) {
			if (entries[i].is_trash)
				icon = kicon_slot("user-trash", 2, 1);
			else if (entries[i].is_app && entries[i].icon[0])
				icon = kicon_slot(entries[i].icon, 2, 1);
			if (icon < 0)
				icon = kicon_slot_for_path(entries[i].path,
							   entries[i].dir, 2, 1);
		}
		if (icon >= 0) {
			ktui_draw_fill(krect(cx, cy, 2, 1), bg);
			ktui_draw_sprite(krect(cx, cy, 2, 1), icon, fg, bg);
		} else {
			ktui_draw_text(cx, cy, 2, it_glyph(i),
				       on ? KT_SURFACE
					  : (entries[i].dir ? KT_ACCENT
							    : KT_MID),
				       bg, KT_A_NONE);
		}
		ktui_draw_text(cx + 2, cy, CELL_W - 3, entries[i].name, fg, bg,
			       KT_A_NONE);
	}

	if (nentries > drawn) {
		/* The overflow marker takes the reserved cell right after the
		 * last drawn entry. */
		int cx = (drawn % cols) * CELL_W + 1;
		int cy = (drawn / cols) * CELL_H + 1;
		if (cy + 1 < h) {
			char more[24];
			snprintf(more, sizeof(more), "+%d more",
				 nentries - drawn);
			ktui_draw_text(cx, cy, 2, ktui_glyph[KT_G_ELLIPSIS],
				       KT_MID, KT_BG, KT_A_NONE);
			ktui_draw_text(cx + 2, cy, CELL_W - 3, more, KT_MID,
				       KT_BG, KT_A_NONE);
		}
	}

	ktui_hint_if(edit_mode != ED_NONE, "Enter", "rename");
	ktui_hint_if(!edit_mode && sel >= 0 && sel < nentries, "Enter", "open");
	ktui_hint_if(!edit_mode && sel >= 0 && sel < nentries &&
			     !entries[sel].pinned,
		     "Del", "trash");
	ktui_hint_if(!edit_mode && !ktui_menu_active(&menu), "Shift+F10",
		     "menu");
	/* Only where Esc DOES something: the desktop never closes, so an
	 * unconditional hint here would read "Esc Close" on the one surface
	 * that has no close. */
	ktui_hint_if(edit_mode || ktui_menu_active(&menu) || sel > 0, "Esc",
		     ktui_esc_verb(&keys));

	if (edit_mode) {
		/* pick.c's line editor, on the status row. */
		char line[320];
		snprintf(line, sizeof(line), "%s: %s",
			 edit_mode == ED_RENAME  ? "Rename to"
			 : edit_mode == ED_NEWFILE ? "New file"
						   : "New folder",
			 edit_buf);
		ktui_draw_text(1, h - 1, w - 2, line, KT_TEXT, KT_BG,
			       KT_A_UNDERLINE);
	} else if (status && *status)
		ktui_draw_text(1, h - 1, w - 2, status, KT_WARN, KT_BG,
			       KT_A_NONE);
	/*
	 * ALWAYS CALLED, open or not: it is what pushes the menu's own hints,
	 * and the row below is what drains the pool. A draw that skipped it
	 * while the menu was down would leave a frame's hints in the pool for
	 * the next one.
	 */
	ktui_menu_draw(&menu);
	/* The status row is shared. The hints have it only while nothing else
	 * is saying anything — a message about a failed rename outranks a
	 * reminder of which key opens a file. */
	if (!edit_mode && !(status && *status))
		ktui_hint_row(&keys, krect(1, h - 1, w - 2, 1), KT_BG);
	ktui_draw_flush();
}

/*
 * Move the selection to the trash, asking first.
 *
 * Delete used to be a keystroke with no question and no undo beyond finding the
 * trash by hand. The two pinned icons are refused rather than confirmed: Home
 * and Trash are places, not files.
 */
static void trash_selected(char *status, size_t n)
{
	char q[400];

	if (sel < 0 || sel >= nentries)
		return;
	if (entries[sel].pinned) {
		snprintf(status, n, "%s is not a file you can trash",
			 entries[sel].name);
		return;
	}
	snprintf(q, sizeof(q), "Move %.200s to the trash?", entries[sel].name);
	if (!confirmed(q))
		return;
	/*
	 * The PATH, not the row's name. They differ for a `.desktop`, whose row
	 * reads `Firefox` — and a trash that renamed it on the way in would
	 * restore it to a file called Firefox that nothing launches.
	 */
	if (kb_trash_put(entries[sel].path) == 0)
		snprintf(status, n, "moved %s to the trash", entries[sel].name);
	else
		snprintf(status, n, "could not trash %s: %s", entries[sel].name,
			 strerror(errno));
	reload();
}

/*
 * Commit the line editor. Both operations stay WITHIN the desktop dir — the
 * '/' is refused at the keystroke, so a name here cannot name a path.
 */
static void edit_commit(char *status, size_t n)
{
	char dir[1024], to[1400];

	desktop_dir(dir, sizeof(dir));
	if (!edit_buf[0] || !strcmp(edit_buf, ".") || !strcmp(edit_buf, "..")) {
		snprintf(status, n, "not a usable name");
		return;
	}
	snprintf(to, sizeof(to), "%s/%s", dir, edit_buf);
	if (edit_mode == ED_NEWDIR) {
		if (mkdir(to, 0755) == 0)
			snprintf(status, n, "created %.64s", edit_buf);
		else
			snprintf(status, n, "cannot create %.64s: %s", edit_buf,
				 strerror(errno));
	} else if (edit_mode == ED_NEWFILE) {
		/* O_EXCL, so a typo that repeats a name reports the collision
		 * instead of truncating whatever is already there. */
		int fd = open(to, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd >= 0) {
			close(fd);
			snprintf(status, n, "created %.64s", edit_buf);
		} else {
			snprintf(status, n, "cannot create %.64s: %s", edit_buf,
				 strerror(errno));
		}
	} else {
		if (edit_for < 0 || edit_for >= nentries)
			return;
		/* rename(2) silently replaces an existing target; a desktop
		 * that eats a file on a name collision is data loss. */
		if (access(to, F_OK) == 0) {
			snprintf(status, n, "%.64s already exists", edit_buf);
			return;
		}
		if (rename(entries[edit_for].path, to) == 0)
			snprintf(status, n, "renamed to %.64s", edit_buf);
		else
			snprintf(status, n, "cannot rename: %s",
				 strerror(errno));
	}
	reload();
}

/* One context-menu row, named by its VERB rather than by its position: a row
 * that is skipped for this entry can then never be run by the index of the one
 * that took its place. */
static void ctx_run(int id, char *status, size_t n)
{
	/*
	 * THE SHARED VERBS FIRST, and they are the ones this file no longer
	 * decides anything about: what "Open Terminal Here" runs and where is
	 * libkxdg's answer, and the chooser and `mc` get the same one.
	 */
	if (id < DESK_LOCAL) {
		char path[1400], store[1024];
		const char *argv[12];
		int isdir = 0;

		if (!ctx_target(path, sizeof(path), &isdir))
			return;
		/*
		 * TWO VERBS KEEP A LOCAL ACTION, and the row, its label and
		 * its position are still the shared table's — only what
		 * happens is this file's.
		 *
		 * OPEN, because `open_entry()` answers two things a MIME
		 * lookup cannot: a `.desktop` icon is an APPLICATION and is
		 * run rather than opened, and the Trash icon is a place whose
		 * directory holds escaped names with no origin. Handing either
		 * to `kdos-appbox open` would make this row mean something
		 * different from Enter on the same icon.
		 */
		if (id == KXDG_VERB_OPEN && ctx_for >= 0 &&
		    ctx_for < nentries) {
			open_entry(&entries[ctx_for]);
			return;
		}
		/*
		 * TRASH, because deleting asks first and refuses the two
		 * pinned places. `kdos trash <file>` confirms nothing — which
		 * is right for a prompt and for `mc`, and wrong for the key
		 * that sits beside Delete on the same surface.
		 */
		if (id == KXDG_VERB_TRASH && ctx_for >= 0) {
			sel = ctx_for;
			trash_selected(status, n);
			return;
		}
		if (id == KXDG_VERB_PLACE) {
			/* The one verb whose ANSWER belongs on the screen: it
			 * writes a file and says nothing, and a row that looks
			 * like it did nothing is a row people press twice. */
			const char *base = strrchr(path, '/');
			int r = kxdg_places_add(base && base[1] ? base + 1
								: path, path);

			snprintf(status, n,
				 r == 0	  ? "%s is on the places list"
				 : r == 1 ? "%s is already a place"
					  : "cannot add %s to places",
				 base && base[1] ? base + 1 : path);
			return;
		}
		if (kxdg_verb_argv(id, path, isdir, sh_term(), store,
				   sizeof(store), argv, 12))
			spawn(argv);
		/* Trash moves a file out from under the grid. */
		if (id == KXDG_VERB_TRASH)
			reload();
		return;
	}
	id -= DESK_LOCAL;

	/*
	 * The DESKTOP's own menu: no entry under the pointer, so every row
	 * here acts on the folder or opens a program. Handled first and
	 * returning, because everything below dereferences `entries[ctx_for]`.
	 */
	if (ctx_for < 0) {
		char dir[1024];
		desktop_dir(dir, sizeof(dir));
		switch (id) {
		case CT_NEWDIR:
			edit_mode = ED_NEWDIR;
			edit_buf[0] = '\0';
			break;
		case CT_NEWFILE:
			edit_mode = ED_NEWFILE;
			edit_buf[0] = '\0';
			break;
		case CT_SORT:
			sort_by = (sort_by + 1) % SORT_N;
			snprintf(status, n, "sorted by %s", SORT_NAMES[sort_by]);
			reload();
			break;
		case CT_REFRESH:
			reload();
			break;
		case CT_APPS: {
			const char *argv[] = { "kdos-start", NULL };
			spawn(argv);
			break;
		}
		case CT_WALL: {
			const char *argv[] = { "kdos-settings", "--page",
					       "appearance", NULL };
			spawn(argv);
			break;
		}
		case CT_DISPLAY: {
			const char *argv[] = { "kdos-display", NULL };
			spawn(argv);
			break;
		}
		case CT_SETTINGS: {
			const char *argv[] = { "kdos-settings", NULL };
			spawn(argv);
			break;
		}
		default:
			break;
		}
		return;
	}
	if (ctx_for >= nentries)
		return;

	const struct entry *it = &entries[ctx_for];

	switch (id) {
	case CT_RENAME: {
		/* Prefill with the FILE's basename, not the row label — a
		 * .desktop row reads `Firefox` and renaming renames the
		 * file. */
		const char *base = strrchr(it->path, '/');
		edit_mode = ED_RENAME;
		edit_for = ctx_for;
		snprintf(edit_buf, sizeof(edit_buf), "%s",
			 base ? base + 1 : it->path);
		break;
	}
	case CT_NEWDIR:
		edit_mode = ED_NEWDIR;
		edit_buf[0] = '\0';
		break;
	case CT_NEWFILE:
		edit_mode = ED_NEWFILE;
		edit_buf[0] = '\0';
		break;
	case CT_EMPTY: {
		int k = confirmed("Empty the trash? This cannot be undone.")
			? kb_trash_empty() : -1;
		if (k >= 0)
			snprintf(status, n, "emptied the trash (%d items)", k);
		reload();
		break;
	}
	case CT_REFRESH:
		reload();
		break;
	default:
		break;
	}
}

int desk_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *output = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		/* One desktop per screen: layer-shell puts an unnamed surface
		 * on whichever output the compositor picks, so a second monitor
		 * got a wallpaper and no icons. */
		else if (!strcmp(argv[i], "--output") && i + 1 < argc)
			output = argv[++i];
		/* comp.conf's `icons = no`. Off is not a degraded mode: it is
		 * what a tty draws and what this file drew last week. */
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else {
			fprintf(stderr,
				"usage: kdos-desk [--output NAME] [--font NAME] "
				"[--no-icons]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		/*
		 * The background layer, anchored on all four edges, with no
		 * exclusive zone — see KDISP_ROLE_BACKGROUND in kwl.h. A panel
		 * role with the zone turned off would still be on the TOP
		 * layer, which would put the desktop over every window.
		 */
		.role = KDISP_ROLE_BACKGROUND,
		.app_id = "kdos-desk",
		.font = font,
		.output = output,
		.exclusive = 0,
		/* ON: this surface implements arrows, Enter and Delete-to-trash,
		 * and shipping it with keyboard = 0 made all three unreachable —
		 * a confirmed audit finding. ON_DEMAND, so it only holds the
		 * keyboard while the user is actually on the desktop. */
		.keyboard = 1,
	};

	/*
	 * THE PANE IS BUILT ONCE and filtered per open. Which verbs a machine
	 * HAS cannot change while the desktop runs; which of them apply to the
	 * thing under the pointer changes on every click, and that is what
	 * `ctx_show` is for.
	 */
	for (int i = 0; i < kxdg_verb_count() && i < KXDG_VERB_MAX; i++) {
		KxdgVerb v;

		if (!kxdg_verb_at(i, &v))
			break;
		ctx_item[nverb].label = v.label;
		ctx_item[nverb].id = v.id;
		ctx_item[nverb].enabled = 1;
		nverb++;
	}
	ctx_item[nverb].label = "";		/* the rule between the halves */
	ctx_item[nverb].id = 0;
	for (int i = 0; i < NCTX; i++) {
		ctx_item[nverb + 1 + i].label = CTX[i].label;
		ctx_item[nverb + 1 + i].id = CTX[i].id + DESK_LOCAL;
		ctx_item[nverb + 1 + i].enabled = 1;
	}
	ctx_pane.n = nverb + 1 + NCTX;
	menu.pane = &ctx_pane;
	menu.npane = 1;
	menu.show = ctx_show;
	keys.menu = &menu;
	keys.ctx_at = ctx_at;
	ktui_keys_layer(&keys, "Deselect", sel_up, sel_clear, NULL);
	ktui_keys_layer(&keys, "Cancel", edit_up, edit_cancel, NULL);

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-desk: no compositor or no layer-shell\n");
		return 1;
	}
	if (icons_on)
		kicon_init(kdisp_cell_w(), kdisp_cell_h(), kdisp_scale());
	ktui_draw_init();
	/*
	 * `kdos theme <accent>` SIGHUPs this too. The desktop is as long-lived
	 * as the panel — it is up for the whole session — so without the handler
	 * an accent change left the icons and their labels in the old colour
	 * until the next login, which is exactly what a photograph of the live
	 * ISO showed.
	 */
	sh_theme_watch();
	reload();

	char status[128] = { 0 };
	time_t last_scan = time(NULL);
	time_t status_at = 0;

	while (!kdisp_should_close()) {
		/*
		 * A message about a file that was trashed a minute ago is a
		 * message about nothing. Cleared on a timer rather than on the
		 * next keystroke, because the next keystroke may never come.
		 */
		if (status[0] && time(NULL) - status_at > 5)
			status[0] = '\0';
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			/* The pictures carry the accent too — tinted at load
			 * through kcol_remap — so a retint drops them, or the
			 * desktop comes up in the new palette wearing the old
			 * icons. */
			kicon_retint();
			ktui_draw_invalidate();
		}
		/* Clamped to the DRAWN count, not nentries: an overflowing
		 * desktop must not park the selection on an invisible icon. At
		 * the TOP of the loop, because a context-menu action that
		 * reloads (Trash, Empty) `continue`s from the middle of the
		 * mouse and key paths alike and would skip a clamp at the
		 * bottom — leaving the highlight on nothing at all. */
		int drawn = drawn_count();
		if (sel < 0)
			sel = 0;
		if (sel >= drawn)
			sel = drawn ? drawn - 1 : 0;

		input_region();
		draw(status);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			/*
			 * Rescanned on a timer rather than watched with inotify.
			 * A desktop folder changes when a person puts something
			 * in it, which is not often, and a readdir of a
			 * directory with twelve files in it is cheaper than the
			 * fd and the event plumbing an inotify watch costs.
			 */
			time_t now = time(NULL);
			if (now - last_scan >= 2 && !ktui_menu_active(&menu) &&
			    !edit_mode) {
				last_scan = now;
				reload();	/* sel is reclamped at the top */
			}
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		/*
		 * KT_MP_PRESS exactly, and a real button.
		 *
		 * `ev.press` is an ENUM, not a boolean: libktui reports plain
		 * pointer motion as KT_MP_DRAG (2) and a release as 0, so
		 * testing it for truth made every mouse MOVE across the desktop
		 * a click — and because a second click on the same icon opens
		 * it, moving the pointer over an already-selected icon launched
		 * it. Wheel ticks arrive as buttons too and must not select.
		 */
		if (ev.type == KT_EVT_DROP) {
			size_t dn = 0;
			const char *uris = ktui_drop_take(&dn);
			int at = icon_at(ev.mx, ev.my);

			/* Only the trash accepts, for now: dropping onto a
			 * folder is a MOVE, and a move that half-succeeds
			 * across filesystems is worse than not offering it. */
			if (uris && at >= 0 && entries[at].is_trash) {
				drop_to_trash(uris, status, sizeof(status));
				status_at = time(NULL);
			}
			continue;
		}

		/*
		 * THE CONTRACT, FIRST AND FOR EVERY EVENT — keys and pointer
		 * alike, because the menu is on this call and a second call
		 * site for it in the pointer path would be a second copy of
		 * "is the menu up" to get wrong.
		 */
		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_MENU) {
				ctx_run(keys.menu_id, status, sizeof(status));
				status_at = time(NULL);
				continue;
			}
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.mx < 0 || ev.my < 0)
				continue;	/* the pointer left the surface */
			/* The editor is keyboard-owned; a click must not
			 * change what Rename is renaming under a half-typed
			 * name. */
			if (edit_mode)
				continue;

			/*
			 * A press that leaves its cell is a drag, not a click.
			 * The threshold is libkwm's, so the console desktop
			 * picks a file up on exactly the same gesture.
			 */
			if (ev.press == KT_MP_DRAG) {
				if (drag_from >= 0 && !dragging &&
				    kwm_drag_threshold(ev.mx - drag_cx,
						       ev.my - drag_cy))
					dragging = drag_begin(drag_from);
				continue;
			}
			if (ev.press != KT_MP_PRESS) {
				drag_from = -1;
				dragging = 0;
				continue;
			}
			int i = icon_at(ev.mx, ev.my);
			if (ev.btn == KT_MB_RIGHT && i < 0) {
				/* Bare wallpaper. This used to fall through to
				 * the compositor's root menu, which is a
				 * compositor's menu rather than a desktop's —
				 * and the desktop's own New Folder was then
				 * reachable only by right-clicking an icon. */
				ctx_popup(-1, ev.mx, ev.my);
				continue;
			}
			if (i < 0)
				continue;
			if (ev.btn == KT_MB_RIGHT) {
				/* The menu belongs to the icon under the
				 * pointer, so aiming at one selects it too —
				 * a menu acting on something other than what
				 * was right-clicked is how files get deleted. */
				sel = i;
				ctx_popup(i, ev.mx, ev.my);
				continue;
			}
			if (ev.btn != KT_MB_LEFT)
				continue;
			drag_from = i;
			drag_cx = ev.mx;
			drag_cy = ev.my;
			dragging = 0;
			/* One click selects, a second on the same icon opens —
			 * the spatial model GNOME 2 shipped, and the one that
			 * does not open a folder every time somebody brushes
			 * the mouse. */
			if (i == sel)
				open_entry(&entries[i]);
			sel = i;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		int cols = columns();
		int per_page = cols * ((ktui_h - 2) / CELL_H);
		if (per_page < 1)
			per_page = 1;

		if (edit_mode) {
			size_t n2 = strlen(edit_buf);
			if (ev.key == KT_K_ENTER) {
				edit_commit(status, sizeof(status));
				status_at = time(NULL);
				edit_mode = ED_NONE;
			} else if (ev.key == KT_K_BACKSPACE) {
				if (n2)
					edit_buf[n2 - 1] = '\0';
			} else if (ev.key >= 0x20 && ev.key < 0x7f &&
				   ev.key != '/' &&
				   n2 + 1 < sizeof(edit_buf)) {
				/* '/' refused at the keystroke: the name
				 * stays within the desktop dir. */
				edit_buf[n2] = (char)ev.key;
				edit_buf[n2 + 1] = '\0';
			}
			continue;
		}

		switch (ev.key) {
		case KT_K_LEFT:  sel -= 1; break;
		case KT_K_RIGHT: sel += 1; break;
		case KT_K_UP:    sel -= cols; break;
		case KT_K_DOWN:  sel += cols; break;
		case KT_K_HOME:  sel = 0; break;
		case KT_K_END:   sel = drawn_count() - 1; break;
		case KT_K_PGUP:  sel -= per_page; break;
		case KT_K_PGDN:  sel += per_page; break;
		case KT_K_ENTER:
			if (sel >= 0 && sel < nentries)
				open_entry(&entries[sel]);
			break;
		case KT_K_DEL:
			trash_selected(status, sizeof(status));
			status_at = time(NULL);
			break;
		case KT_K_F5:
		case 'r':
			reload();
			break;
		default:
			break;
		}
	}

	kdisp_shutdown();
	return 0;
}

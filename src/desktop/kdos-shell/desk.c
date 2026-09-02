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
 * the whole surface for input again (see input_region()).
 */
#define MENU_W 26
/* Rows carry an ID and the run switch dispatches on it, so a row that is
 * hidden for this entry can never be run by its position. */
enum { CT_OPEN, CT_TERM, CT_RENAME, CT_NEWDIR, CT_NEWFILE, CT_TRASH, CT_EMPTY,
       CT_REFRESH, CT_SORT, CT_APPS, CT_WALL, CT_DISPLAY, CT_SETTINGS,
       CT_RULE };

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
} CTX[] = {
	{ "Open",                CT_OPEN,     SC_ITEM, 0, 0 },
	{ "Open Terminal Here",  CT_TERM,     SC_BOTH, 0, 0 },
	{ "Rename",              CT_RENAME,   SC_ITEM, 0, 1 },
	{ "Move to Trash",       CT_TRASH,    SC_ITEM, 0, 1 },
	{ "Empty Trash",         CT_EMPTY,    SC_ITEM, 1, 0 },
	{ "New Folder",          CT_NEWDIR,   SC_BOTH, 0, 0 },
	{ "New File",            CT_NEWFILE,  SC_BOTH, 0, 0 },
	{ "",                    CT_RULE,     SC_DESK, 0, 0 },
	{ "Sort Icons",          CT_SORT,     SC_DESK, 0, 0 },
	{ "Refresh",             CT_REFRESH,  SC_BOTH, 0, 0 },
	{ "",                    CT_RULE,     SC_DESK, 0, 0 },
	/* The compositor's root menu used to own this corner of the screen and
	 * now does not, so everything it offered has to be reachable here or
	 * the change is a regression. */
	{ "Applications",        CT_APPS,     SC_DESK, 0, 0 },
	{ "Change Wallpaper",    CT_WALL,     SC_DESK, 0, 0 },
	{ "Display Settings",    CT_DISPLAY,  SC_DESK, 0, 0 },
	{ "Settings",            CT_SETTINGS, SC_DESK, 0, 0 },
};
#define NCTX ((int)(sizeof(CTX) / sizeof(CTX[0])))

static int ctx_open;		/* the menu is up */
static int ctx_x, ctx_y;	/* its top-left, in cells */
static int ctx_sel;
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
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;

	o += (size_t)snprintf(out, n, "file://");
	for (const unsigned char *p = (const unsigned char *)path;
	     *p && o + 4 < n; p++) {
		if (*p <= 0x20 || *p >= 0x7f || strchr("%#?[]", *p)) {
			out[o++] = '%';
			out[o++] = hex[*p >> 4];
			out[o++] = hex[*p & 0x0f];
		} else {
			out[o++] = (char)*p;
		}
	}
	out[o] = '\0';
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
 * WHICH directory, from `~/.config/user-dirs.dirs`.
 *
 * KDOS seeds that file rather than generating it (there is no xdg-user-dirs),
 * and every other consumer on the machine honours it — a desktop that ignored
 * it would be the one program insisting the folder is called Desktop after the
 * user said otherwise. The value is written as `XDG_DESKTOP_DIR="$HOME/Desktop"`,
 * so `$HOME` is the one expansion that has to be understood.
 */
static void desktop_dir(char *out, size_t n)
{
	const char *home = getenv("HOME");
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char path[1024], line[512];
	FILE *f;

	snprintf(out, n, "%s/Desktop", home ? home : "");
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/user-dirs.dirs", cfg);
	else if (home)
		snprintf(path, sizeof(path), "%s/.config/user-dirs.dirs", home);
	else
		return;

	f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		char *v = strchr(line, '=');
		if (strncmp(line, "XDG_DESKTOP_DIR", 15) || !v)
			continue;
		v++;
		if (*v == '"')
			v++;
		line[strcspn(line, "\n")] = '\0';
		char *end = strchr(v, '"');
		if (end)
			*end = '\0';
		if (!strncmp(v, "$HOME", 5) && home)
			snprintf(out, n, "%s%s", home, v + 5);
		else if (*v)
			snprintf(out, n, "%s", v);
		break;
	}
	fclose(f);
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
	long sig = ctx_open ? -2 : (long)nentries * 100000 + cols * 100 + ktui_h;

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

/*
 * Whether a row is on this menu at all. `ctx_sel` indexes CTX, not the drawn
 * rows — so every walk over it has to skip the same rows the drawing does, or
 * the selection lands on `Empty Trash` for a photograph: nothing highlighted,
 * and Enter doing nothing.
 */
static bool ctx_shown(int i)
{
	if (i < 0 || i >= NCTX)
		return false;
	if (ctx_for < 0)
		return (CTX[i].scope & SC_DESK) != 0;
	if (ctx_for >= nentries)
		return false;
	if (!(CTX[i].scope & SC_ITEM))
		return false;
	if (CTX[i].trash_only && !entries[ctx_for].is_trash)
		return false;
	if (CTX[i].no_pin && entries[ctx_for].pinned)
		return false;
	return true;
}

/* A rule is drawn and is never selected — the same rule kdos-start keeps, and
 * for the same reason: a separator that can hold the caret is a menu with a
 * row that does nothing. */
static bool ctx_pickable(int i)
{
	return ctx_shown(i) && CTX[i].id != CT_RULE;
}

static void ctx_step(int dir)
{
	for (int k = 0; k < NCTX; k++) {
		ctx_sel = (ctx_sel + dir + NCTX) % NCTX;
		if (ctx_pickable(ctx_sel))
			return;
	}
}

static void draw_ctx(void)
{
	int rows = 0;
	for (int i = 0; i < NCTX; i++)
		if (ctx_shown(i))
			rows++;

	KRect r = krect(ctx_x, ctx_y, MENU_W, rows + 2);
	if (r.x + r.w > ktui_w)
		r.x = ktui_w - r.w;
	if (r.y + r.h > ktui_h)
		r.y = ktui_h - r.h;
	if (r.x < 0)
		r.x = 0;
	if (r.y < 0)
		r.y = 0;
	ctx_x = r.x;
	ctx_y = r.y;

	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, NULL, KT_ACCENT, KT_SURFACE, 0);

	int y = r.y + 1;
	for (int i = 0; i < NCTX; i++) {
		if (!ctx_shown(i))
			continue;
		if (CTX[i].id == CT_RULE) {
			ktui_draw_hline(r.x + 1, y, r.w - 2, KT_G_HL, KT_DIM,
					KT_SURFACE);
			y++;
			continue;
		}
		bool on = i == ctx_sel;
		ktui_draw_fill(krect(r.x + 1, y, r.w - 2, 1),
			       on ? KT_ACCENT : KT_SURFACE);
		ktui_draw_text(r.x + 2, y, r.w - 4, CTX[i].label,
			       on ? KT_SURFACE : KT_TEXT,
			       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		y++;
	}
}

/* Which menu row a cell is on, or -1. Walks the same skip the drawing does, so
 * the two cannot disagree about whether Empty Trash is there. */
static int ctx_row_at(int mx, int my)
{
	int rows = 0;
	for (int i = 0; i < NCTX; i++)
		if (ctx_shown(i))
			rows++;
	if (mx < ctx_x || mx >= ctx_x + MENU_W || my <= ctx_y ||
	    my > ctx_y + rows)
		return -1;

	int want = my - ctx_y - 1, seen = 0;
	for (int i = 0; i < NCTX; i++) {
		if (!ctx_shown(i))
			continue;
		if (seen++ == want)
			return i;
	}
	return -1;
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
	if (ctx_open)
		draw_ctx();
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

/* One context-menu row. Dispatches on the row's ID, so a row that is skipped
 * for this entry can never be run by its position. */
static void ctx_run(int row, char *status, size_t n)
{
	if (!ctx_pickable(row))
		return;

	/*
	 * The DESKTOP's own menu: no entry under the pointer, so every row
	 * here acts on the folder or opens a program. Handled first and
	 * returning, because everything below dereferences `entries[ctx_for]`.
	 */
	if (ctx_for < 0) {
		char dir[1024];
		desktop_dir(dir, sizeof(dir));
		switch (CTX[row].id) {
		case CT_TERM: {
			const char *argv[] = { sh_term(), "-D", dir, NULL };
			spawn(argv);
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

	switch (CTX[row].id) {
	case CT_OPEN:
		open_entry(it);
		break;
	case CT_TERM: {
		/* In the directory itself, or in the one holding the file —
		 * which is what "here" means when the pointer is on a
		 * document. */
		char dir[1400];
		snprintf(dir, sizeof(dir), "%s", it->path);
		if (!it->dir) {
			char *slash = strrchr(dir, '/');
			if (slash && slash != dir)
				*slash = '\0';
		}
		const char *argv[] = { sh_term(), "-D", dir, NULL };
		spawn(argv);
		break;
	}
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
	case CT_TRASH:
		sel = ctx_for;
		trash_selected(status, n);
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
			if (now - last_scan >= 2 && !ctx_open && !edit_mode) {
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

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.mx < 0 || ev.my < 0)
				continue;	/* the pointer left the surface */
			/* The editor is keyboard-owned; a click must not
			 * change what Rename is renaming under a half-typed
			 * name. */
			if (edit_mode)
				continue;

			if (ctx_open) {
				int row = ctx_row_at(ev.mx, ev.my);
				if (ev.press == KT_MP_DRAG) {
					if (row >= 0)
						ctx_sel = row;
					continue;
				}
				if (ev.press != KT_MP_PRESS)
					continue;
				if (row < 0) {
					ctx_open = 0;	/* click away */
					continue;
				}
				ctx_sel = row;
				ctx_run(row, status, sizeof(status));
				status_at = time(NULL);
				ctx_open = 0;
				continue;
			}

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
				ctx_for = -1;
				ctx_sel = 0;
				while (ctx_sel < NCTX && !ctx_pickable(ctx_sel))
					ctx_sel++;
				ctx_x = ev.mx;
				ctx_y = ev.my;
				ctx_open = 1;
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
				ctx_for = i;
				ctx_sel = 0;
				ctx_x = ev.mx;
				ctx_y = ev.my;
				ctx_open = 1;
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
			if (ev.key == KT_K_ESC) {
				edit_mode = ED_NONE;
			} else if (ev.key == KT_K_ENTER) {
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

		if (ctx_open) {
			switch (ev.key) {
			case KT_K_ESC:
				ctx_open = 0;
				break;
			case KT_K_UP:
				ctx_step(-1);
				break;
			case KT_K_DOWN:
				ctx_step(1);
				break;
			case KT_K_ENTER:
				ctx_run(ctx_sel, status, sizeof(status));
				status_at = time(NULL);
				ctx_open = 0;
				break;
			default:
				break;
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
		case KT_K_ESC:
			/* Nothing to close, but the selection is a highlight
			 * the user may want gone. */
			sel = 0;
			break;
		default:
			break;
		}
	}

	kdisp_shutdown();
	return 0;
}

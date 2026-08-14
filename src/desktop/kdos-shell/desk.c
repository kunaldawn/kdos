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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kwl.h"
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
	bool is_app;		/* a .desktop file: launch it, do not open it */
	bool terminal;		/* ...inside foot */
};

static struct entry entries[MAX_ENTRIES];
static int nentries;
static int sel;

/*
 * The context menu, drawn INTO the desktop's own grid.
 *
 * kdos-desk owns the whole output, so a popup here is not a second surface —
 * it is a box painted over the icons, and while it is up the desktop claims
 * the whole surface for input again (see input_region()).
 */
#define MENU_W 26
static const struct {
	const char *label;
	int trash_only;		/* Empty Trash is not offered on a photo */
} CTX[] = {
	{ "Open",                0 },
	{ "Open Terminal Here",  0 },
	{ "Move to Trash",       0 },
	{ "Empty Trash",         1 },
	{ "Refresh",             0 },
};
#define NCTX ((int)(sizeof(CTX) / sizeof(CTX[0])))

static int ctx_open;		/* the menu is up */
static int ctx_x, ctx_y;	/* its top-left, in cells */
static int ctx_sel;
static int ctx_for;		/* which entry it belongs to */

/* ── the trash, which nothing in this tree had ─────────────────────────── */

/*
 * freedesktop.org's trash spec, the part of it a desktop actually needs.
 *
 * `~/.local/share/Trash/files/NAME` is the file and
 * `~/.local/share/Trash/info/NAME.trashinfo` records where it came from and
 * when. BOTH are required: a file in `files/` with no `info/` entry cannot be
 * restored by anything, which makes "move to trash" a delete with extra steps —
 * and every other trash implementation on the machine, mc's included, will read
 * these.
 *
 * The name is made unique before either is written. Trashing two files called
 * `notes.txt` from different directories is the ordinary case, and the second
 * one silently replacing the first is data loss.
 */
static int trash_dirs(char *files, size_t fn, char *info, size_t in)
{
	const char *home = getenv("HOME");
	if (!home)
		return -1;
	snprintf(files, fn, "%s/.local/share/Trash/files", home);
	snprintf(info, in, "%s/.local/share/Trash/info", home);

	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "%s/.local/share/Trash", home);
	mkdir(tmp, 0700);
	mkdir(files, 0700);
	mkdir(info, 0700);
	return 0;
}

/* Percent-encode for the Path= line. The spec says the value is a URI, so a
 * name with a space or a percent in it must be escaped or the record cannot be
 * parsed back. */
static void uri_escape(const char *in, char *out, size_t n)
{
	static const char *hex = "0123456789ABCDEF";
	size_t o = 0;
	for (const unsigned char *p = (const unsigned char *)in; *p && o + 4 < n; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || strchr("/-_.~", *p)) {
			out[o++] = (char)*p;
		} else {
			out[o++] = '%';
			out[o++] = hex[*p >> 4];
			out[o++] = hex[*p & 0xf];
		}
	}
	out[o] = '\0';
}

static int trash_put(const char *path, const char *name)
{
	char files[1024], info[1024];
	if (trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;

	char dest[2048], meta[2400];
	char unique[300];
	snprintf(unique, sizeof(unique), "%s", name);
	for (int n = 1; n < 1000; n++) {
		snprintf(dest, sizeof(dest), "%s/%s", files, unique);
		if (access(dest, F_OK) != 0)
			break;
		snprintf(unique, sizeof(unique), "%.100s.%d", name, n);
	}
	snprintf(dest, sizeof(dest), "%s/%s", files, unique);
	snprintf(meta, sizeof(meta), "%s/%s.trashinfo", info, unique);

	/*
	 * The info file is written FIRST. If the rename then fails there is a
	 * stale record and no file, which every trash implementation ignores;
	 * the other order leaves a file nothing can restore.
	 */
	FILE *f = fopen(meta, "w");
	if (!f)
		return -1;
	char escaped[2048];
	uri_escape(path, escaped, sizeof(escaped));
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	fprintf(f, "[Trash Info]\nPath=%s\nDeletionDate=%04d-%02d-%02dT%02d:%02d:%02d\n",
		escaped, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	fclose(f);

	if (rename(path, dest) != 0) {
		/*
		 * Across a filesystem boundary rename() cannot work, and a
		 * copy-then-delete here would be a file operation this program
		 * has no business doing. The record is removed so it does not
		 * outlive the attempt, and the caller is told.
		 */
		unlink(meta);
		return -1;
	}
	return 0;
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
	kxdg_free(&e);

	/* The field codes are placeholders for documents this launch has none
	 * of; passing the literal `%U` is how a browser opens a search for it. */
	char *w = it->exec;
	for (char *r = it->exec; *r; r++) {
		if (r[0] == '%' && r[1]) {
			if (r[1] == '%')
				*w++ = '%';
			r++;
			continue;
		}
		*w++ = *r;
	}
	*w = '\0';
	while (w > it->exec && w[-1] == ' ')
		*--w = '\0';
	return 1;
}

static int cmp_entry(const void *a, const void *b)
{
	const struct entry *x = a, *y = b;
	/* Directories first, then by name — the order every file manager has
	 * used since Norton Commander, and the one people scan by. */
	if (x->dir != y->dir)
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

	DIR *d = opendir(dir);
	if (d) {
		struct dirent *e;
		while ((e = readdir(d)) && nentries < MAX_ENTRIES - 2) {
			if (e->d_name[0] == '.')
				continue;
			struct entry *it = &entries[nentries];
			memset(it, 0, sizeof(*it));
			snprintf(it->name, sizeof(it->name), "%s", e->d_name);
			snprintf(it->path, sizeof(it->path), "%s/%s", dir,
				 e->d_name);
			struct stat st;
			it->dir = stat(it->path, &st) == 0 && S_ISDIR(st.st_mode);
			size_t n = strlen(e->d_name);
			if (!it->dir && n > 8 &&
			    !strcmp(e->d_name + n - 8, ".desktop"))
				load_desktop_entry(it);
			nentries++;
		}
		closedir(d);
	}
	qsort(entries, (size_t)nentries, sizeof(entries[0]), cmp_entry);

	/*
	 * Home and Trash are appended AFTER the sort and in that order, so they
	 * are always the last two and always in the same place. A desktop whose
	 * fixed icons move when a file is created is a desktop nobody builds
	 * muscle memory on.
	 */
	if (nentries < MAX_ENTRIES) {
		struct entry *it = &entries[nentries++];
		memset(it, 0, sizeof(*it));
		snprintf(it->name, sizeof(it->name), "Home");
		snprintf(it->path, sizeof(it->path), "%s", home);
		it->dir = true;
	}
	if (nentries < MAX_ENTRIES) {
		struct entry *it = &entries[nentries++];
		memset(it, 0, sizeof(*it));
		snprintf(it->name, sizeof(it->name), "Trash");
		snprintf(it->path, sizeof(it->path),
			 "%s/.local/share/Trash/files", home);
		it->dir = true;
		it->is_trash = true;
	}
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
		const char *argv[34];
		int n = 0;
		if (it->terminal) {
			argv[n++] = "foot";
			argv[n++] = "-e";
		}
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

/* Delete everything in the trash, both halves of every record. The spec's
 * `info/` entry outliving its file is the state every implementation ignores,
 * so files go first and the records follow. */
static int empty_trash(void)
{
	char files[1024], info[1024], p[2100];
	if (trash_dirs(files, sizeof(files), info, sizeof(info)) != 0)
		return -1;

	int n = 0;
	for (int pass = 0; pass < 2; pass++) {
		const char *dir = pass ? info : files;
		DIR *d = opendir(dir);
		struct dirent *e;
		if (!d)
			continue;
		while ((e = readdir(d))) {
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
			struct stat st;
			if (!pass && stat(p, &st) == 0 && S_ISDIR(st.st_mode)) {
				/* A trashed DIRECTORY needs a recursive
				 * removal, which is a file operation this
				 * program has no business writing by hand.
				 * rm is toybox's, exec'd with argv. */
				const char *argv[] = { "rm", "-rf", p, NULL };
				spawn(argv);
				n++;
				continue;
			}
			if (unlink(p) == 0 && !pass)
				n++;
		}
		closedir(d);
	}
	return n;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int columns(void)
{
	int c = ktui_w / CELL_W;
	return c < 1 ? 1 : c;
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
 * This surface covers the whole output, so with the default input region it ate
 * every click on the root window — and the compositor's root menu (menu.xml,
 * bound to a right press on Root) therefore did not exist for anybody running
 * with desktop icons on, which is the shipped default. Claiming only the cells
 * an icon occupies gives that click back: bare wallpaper is not ours.
 *
 * While the context menu is up the whole surface IS ours, or the click that
 * dismisses it would go to the compositor instead.
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

	if (sig == shown)
		return;
	shown = sig;

	if (ctx_open) {
		/* While the menu is up the whole surface is ours, or the click
		 * that dismisses it would go to the compositor instead. */
		kwl_input_cells(NULL, -1);
		return;
	}

	int n = 0;
	for (int i = 0; i < nentries && n < MAX_ENTRIES; i++) {
		int cx = (i % cols) * CELL_W + 1;
		int cy = (i / cols) * CELL_H + 1;
		if (cy + 1 >= ktui_h)
			break;
		/* The glyph and its label, one row: the blank row under an icon
		 * and the gap column beside it are wallpaper. */
		claim[n++] = krect(cx, cy, CELL_W - 1, 1);
	}
	kwl_input_cells(claim, n);
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
	return !CTX[i].trash_only ||
	       (ctx_for >= 0 && ctx_for < nentries && entries[ctx_for].is_trash);
}

static void ctx_step(int dir)
{
	for (int k = 0; k < NCTX; k++) {
		ctx_sel = (ctx_sel + dir + NCTX) % NCTX;
		if (ctx_shown(ctx_sel))
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
	 * So KWL_ROLE_BACKGROUND asks libkwl for an ARGB surface and turns on
	 * libkcell's transparent-KT_BG mode: every cell the desktop does not
	 * write is cleared to zero and the wallpaper shows through. Cells that
	 * DO carry something — an icon glyph, a label, the selection bar — are
	 * painted normally.
	 */
	ktui_draw_clear();

	for (int i = 0; i < nentries; i++) {
		int cx = (i % cols) * CELL_W + 1;
		int cy = (i / cols) * CELL_H + 1;
		if (cy + 1 >= h)
			break;

		bool on = i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;

		/* Filled block for a directory, light for a file, medium for
		 * the trash — three levels of one ramp rather than three
		 * unrelated glyphs, so they read as a set. */
		const char *glyph = it_glyph(i);
		ktui_draw_text(cx, cy, 2, glyph,
			       on ? KT_SURFACE : (entries[i].dir ? KT_ACCENT
								: KT_MID),
			       bg, KT_A_NONE);
		ktui_draw_text(cx + 2, cy, CELL_W - 3, entries[i].name, fg, bg,
			       KT_A_NONE);
	}

	if (status && *status)
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
	if (entries[sel].is_trash || !strcmp(entries[sel].name, "Home")) {
		snprintf(status, n, "%s is not a file you can trash",
			 entries[sel].name);
		return;
	}
	snprintf(q, sizeof(q), "Move %.200s to the trash?", entries[sel].name);
	if (!confirmed(q))
		return;
	/*
	 * The FILE's name, not the row's. They differ for a `.desktop`, whose
	 * row reads `Firefox` — and a trash that renamed it on the way in would
	 * restore it to a file called Firefox that nothing launches.
	 */
	const char *base = strrchr(entries[sel].path, '/');
	base = base ? base + 1 : entries[sel].path;
	if (trash_put(entries[sel].path, base) == 0)
		snprintf(status, n, "moved %s to the trash", entries[sel].name);
	else
		snprintf(status, n, "could not trash %s: %s", entries[sel].name,
			 strerror(errno));
	reload();
}

/* One context-menu row. Indexes CTX directly, so a row that is skipped for a
 * non-trash icon can never be run by its position. */
static void ctx_run(int row, char *status, size_t n)
{
	const struct entry *it = &entries[ctx_for];

	if (ctx_for < 0 || ctx_for >= nentries || !ctx_shown(row))
		return;

	switch (row) {
	case 0:					/* Open */
		open_entry(it);
		break;
	case 1: {				/* Open Terminal Here */
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
		const char *argv[] = { "foot", "-D", dir, NULL };
		spawn(argv);
		break;
	}
	case 2:					/* Move to Trash */
		sel = ctx_for;
		trash_selected(status, n);
		break;
	case 3: {				/* Empty Trash */
		int k = confirmed("Empty the trash? This cannot be undone.")
			? empty_trash() : -1;
		if (k >= 0)
			snprintf(status, n, "emptied the trash (%d items)", k);
		reload();
		break;
	}
	case 4:					/* Refresh */
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
		else {
			fprintf(stderr,
				"usage: kdos-desk [--output NAME] [--font NAME]\n");
			return 2;
		}
	}

	KwlConfig cfg = {
		/*
		 * The background layer, anchored on all four edges, with no
		 * exclusive zone — see KWL_ROLE_BACKGROUND in kwl.h. A panel
		 * role with the zone turned off would still be on the TOP
		 * layer, which would put the desktop over every window.
		 */
		.role = KWL_ROLE_BACKGROUND,
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
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-desk: no compositor or no layer-shell\n");
		return 1;
	}
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

	while (!kwl_should_close()) {
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
			ktui_draw_invalidate();
		}
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
			if (now - last_scan >= 2 && !ctx_open) {
				last_scan = now;
				int was = nentries;
				reload();
				if (nentries != was && sel >= nentries)
					sel = nentries ? nentries - 1 : 0;
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
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.mx < 0 || ev.my < 0)
				continue;	/* the pointer left the surface */

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

			if (ev.press != KT_MP_PRESS)
				continue;
			int cols = columns();
			int i = ((ev.my - 1) / CELL_H) * cols +
				(ev.mx - 1) / CELL_W;
			if (i < 0 || i >= nentries)
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
		case KT_K_END:   sel = nentries - 1; break;
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
		if (sel < 0)
			sel = 0;
		if (sel >= nentries)
			sel = nentries ? nentries - 1 : 0;
	}

	kwl_shutdown();
	return 0;
}

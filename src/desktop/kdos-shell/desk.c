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
	char name[256];
	char path[1400];
	bool dir;
	bool is_trash;
};

static struct entry entries[MAX_ENTRIES];
static int nentries;
static int sel;

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

/* ── reading ~/Desktop ─────────────────────────────────────────────────── */

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
	snprintf(dir, sizeof(dir), "%s/Desktop", home);

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

static void open_entry(const struct entry *it)
{
	if (it->dir) {
		/* mc is the file manager — already a port, already Turbo
		 * Vision, on the same glyph grid when foot uses Terminus. */
		const char *argv[] = { "foot", "-e", "mc", it->path, NULL };
		spawn(argv);
		return;
	}
	/*
	 * A file goes to the MIME handler, and on this machine that is
	 * kdos-appbox: it owns the alien-apps table, and every launcher in
	 * /usr/local/bin is a symlink to it. xdg-open would be a second answer
	 * to the same question.
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

/* Split out so the draw loop reads as layout rather than as a lookup. */
static const char *it_glyph(int i)
{
	if (entries[i].is_trash)
		return ktui_glyph[KT_G_SHADE];
	return entries[i].dir ? ktui_glyph[KT_G_FULL] : ktui_glyph[KT_G_DOT];
}

static void draw(const char *status)
{
	int w = ktui_w, h = ktui_h;
	int cols = columns();

	/*
	 * The background is NOT painted.
	 *
	 * The compositor draws the wallpaper and this surface sits above it, so
	 * filling here would cover it with a flat colour — the desktop would
	 * lose its background the moment the icons appeared. Only the cells
	 * that carry something are written; libkcell leaves the rest
	 * transparent because the buffer starts zeroed.
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
	ktui_draw_flush();
}

int desk_main(int argc, char **argv)
{
	const char *font = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-desk [--font NAME]\n");
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
		.exclusive = 0,
		.keyboard = 0,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-desk: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	reload();

	char status[128] = { 0 };
	time_t last_scan = time(NULL);

	while (!kwl_should_close()) {
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
			if (now - last_scan >= 2) {
				last_scan = now;
				int was = nentries;
				reload();
				if (nentries != was && sel >= nentries)
					sel = nentries ? nentries - 1 : 0;
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE && ev.press) {
			int cols = columns();
			int i = ((ev.my - 1) / CELL_H) * cols +
				(ev.mx - 1) / CELL_W;
			if (i >= 0 && i < nentries) {
				/* One click selects, a second on the same icon
				 * opens — the spatial model GNOME 2 shipped,
				 * and the one that does not open a folder every
				 * time somebody brushes the mouse. */
				if (i == sel)
					open_entry(&entries[i]);
				sel = i;
			}
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		int cols = columns();
		switch (ev.key) {
		case KT_K_LEFT:  sel -= 1; break;
		case KT_K_RIGHT: sel += 1; break;
		case KT_K_UP:    sel -= cols; break;
		case KT_K_DOWN:  sel += cols; break;
		case KT_K_ENTER:
			if (sel >= 0 && sel < nentries)
				open_entry(&entries[sel]);
			break;
		case KT_K_DEL:
			if (sel >= 0 && sel < nentries && !entries[sel].is_trash &&
			    strcmp(entries[sel].name, "Home")) {
				if (trash_put(entries[sel].path,
					      entries[sel].name) == 0)
					snprintf(status, sizeof(status),
						 "moved %s to the trash",
						 entries[sel].name);
				else
					snprintf(status, sizeof(status),
						 "could not trash %s: %s",
						 entries[sel].name,
						 strerror(errno));
				reload();
			}
			break;
		case 'r':
			reload();
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

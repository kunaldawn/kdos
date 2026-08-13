/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-menu — Applications, Places, System
 *
 *   ┌ Applications ────────────────┐   ┌ Applications / Graphics ──────┐
 *   │ ▸ Accessories              ▶ │   │ ▸ GIMP                        │
 *   │   Games                    ▶ │   │   Inkscape                    │
 *   │   Graphics                 ▶ │   │   Krita                       │
 *   │   Internet                 ▶ │   │   Shotwell                    │
 *   │   Office                   ▶ │   │                               │
 *   │   Programming              ▶ │   │                               │
 *   │   Sound & Video            ▶ │   │                               │
 *   │   System Tools             ▶ │   │                               │
 *   └──────────────────────────────┘   └───────────────────────────────┘
 *
 * GNOME 2's three menus, drawn as cells. ONE COLUMN AT A TIME rather than a
 * cascade of overlapping panes: a cascade needs a surface per level and a
 * pointer-tracking policy to decide when a level goes away, and on a character
 * grid it buys nothing — Enter or Right descends, Escape or Left comes back,
 * and the breadcrumb in the title says where you are. That is also why it works
 * identically from the keyboard and from the mouse, which a hover cascade never
 * quite does.
 *
 * ITS OWN PROCESS, like kdos-launcher, and for the same reason: the panel's
 * event loop owns one surface and one cell buffer. A second surface inside it
 * would mean libktui rendering two grids from one buffer, which it cannot do —
 * see the KWL_ROLE_LOCK note in kwl.h about exactly that limitation. Spawning
 * is cheaper than the alternative and it means a menu that wedges cannot take
 * the panel with it.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kwl.h"
#include "kxdg.h"
#include "shell.h"

#define MAX_ITEMS 512
#define NAME_MAX_LEN 64
#define EXEC_MAX_LEN 256

/*
 * GNOME 2's Applications submenus, in its order.
 *
 * The `match` list is the freedesktop Categories a `.desktop` file may carry;
 * the FIRST bucket that matches wins, so the order here is also the priority.
 * An entry matching nothing lands in Accessories, which is what that category
 * has always been for — an app with no home is still an app you have to be
 * able to launch.
 */
static const struct {
	const char *name;
	const char *match[8];
} GROUPS[] = {
	{ "Accessories",  { "Utility", "Accessibility", "Core", NULL } },
	{ "Games",        { "Game", NULL } },
	{ "Graphics",     { "Graphics", "Photography", "Scanning", NULL } },
	{ "Internet",     { "Network", "WebBrowser", "Email", NULL } },
	{ "Office",       { "Office", "TextEditor", "Spreadsheet", NULL } },
	{ "Programming",  { "Development", "IDE", NULL } },
	{ "Sound & Video",{ "AudioVideo", "Audio", "Video", "Player", NULL } },
	{ "System Tools", { "System", "Settings", "Emulator", "Security", NULL } },
	{ "Education",    { "Education", "Science", "Engineering", NULL } },
};
#define NGROUPS ((int)(sizeof(GROUPS) / sizeof(GROUPS[0])))

struct item {
	char name[NAME_MAX_LEN];
	char exec[EXEC_MAX_LEN];
	int group;			/* index into GROUPS, or -1 */
	int submenu;			/* -1, or the group this row opens */
	int terminal;			/* Terminal=true — run it inside foot */
};

static struct item items[MAX_ITEMS];
static int nitems;

/* ── reading the applications ──────────────────────────────────────────── */

/*
 * Strip the field codes.
 *
 * A `.desktop` Exec is a template: %f is a file, %U a list of URLs, %i the icon
 * option, %c the name. Launching with them still in argv passes the LITERAL
 * "%U" to the program, which browsers open as a search and everything else
 * reports as a missing file. Nothing here substitutes them, because a menu
 * launch has no document to substitute.
 */
static void strip_field_codes(char *s)
{
	char *w = s;
	for (char *r = s; *r; r++) {
		if (r[0] == '%' && r[1]) {
			if (r[1] == '%') {	/* an escaped percent is a percent */
				*w++ = '%';
				r++;
				continue;
			}
			r++;			/* drop the code */
			continue;
		}
		*w++ = *r;
	}
	*w = '\0';
	/* Trailing space left by a dropped code at the end. */
	while (w > s && w[-1] == ' ')
		*--w = '\0';
}

static int group_for(const char *categories)
{
	if (!categories)
		return 0;
	for (int g = 0; g < NGROUPS; g++)
		for (int m = 0; GROUPS[g].match[m]; m++) {
			const char *p = strstr(categories, GROUPS[g].match[m]);
			/*
			 * Bounded on both sides, because Categories is a
			 * semicolon-separated list and a substring test alone
			 * puts "Settings" into anything tagged "TextSettings"
			 * — and, worse, matches "Audio" inside "AudioVideo"
			 * for whichever bucket comes first.
			 */
			if (!p)
				continue;
			size_t n = strlen(GROUPS[g].match[m]);
			bool left = p == categories || p[-1] == ';';
			bool right = p[n] == '\0' || p[n] == ';';
			if (left && right)
				return g;
		}
	return 0;				/* Accessories */
}

static void add_desktop_file(const char *path)
{
	KxdgEntry e;
	if (nitems >= MAX_ITEMS || kxdg_load(&e, path, "Desktop Entry") != 0)
		return;

	const char *type = kxdg_get(&e, "Type", "Application");
	const char *name = kxdg_get(&e, "Name", NULL);
	const char *exec = kxdg_get(&e, "Exec", NULL);

	/* NoDisplay is the entry saying "I am not for a menu" — wine's is the
	 * example kdos-appbox already documents. Hidden means deleted. */
	if (strcmp(type, "Application") || !name || !exec ||
	    kxdg_bool(&e, "NoDisplay", 0) || kxdg_bool(&e, "Hidden", 0)) {
		kxdg_free(&e);
		return;
	}

	struct item *it = &items[nitems];
	memset(it, 0, sizeof(*it));
	snprintf(it->name, sizeof(it->name), "%s", name);
	snprintf(it->exec, sizeof(it->exec), "%s", exec);
	strip_field_codes(it->exec);
	it->group = group_for(kxdg_get(&e, "Categories", NULL));
	it->submenu = -1;
	it->terminal = kxdg_bool(&e, "Terminal", 0);
	if (*it->exec)
		nitems++;
	kxdg_free(&e);
}

static void scan_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	if (!d)
		return;
	while ((e = readdir(d)) && nitems < MAX_ITEMS) {
		size_t n = strlen(e->d_name);
		if (n < 9 || strcmp(e->d_name + n - 8, ".desktop"))
			continue;
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		add_desktop_file(path);
	}
	closedir(d);
}

static int cmp_name(const void *a, const void *b)
{
	return strcasecmp(((const struct item *)a)->name,
			  ((const struct item *)b)->name);
}

static void load_applications(void)
{
	const char *home = getenv("HOME");
	char user[512];

	scan_dir("/usr/share/applications");
	scan_dir("/usr/local/share/applications");
	if (home) {
		snprintf(user, sizeof(user), "%s/.local/share/applications", home);
		scan_dir(user);
	}
	qsort(items, (size_t)nitems, sizeof(items[0]), cmp_name);
}

/* ── Places and System, which are lists rather than a scan ─────────────── */

static void add(const char *name, const char *exec, int submenu)
{
	if (nitems >= MAX_ITEMS)
		return;
	struct item *it = &items[nitems++];
	memset(it, 0, sizeof(*it));
	snprintf(it->name, sizeof(it->name), "%s", name);
	if (exec)
		snprintf(it->exec, sizeof(it->exec), "%s", exec);
	it->group = -1;
	it->submenu = submenu;
}

/*
 * Places opens mc, in foot, at a path.
 *
 * mc IS the file manager on this desktop — already a port, already Turbo
 * Vision, and on the same glyph grid as everything else when foot is
 * configured with Terminus. See docs/KDOS-TEXTMODE.md §7.2.
 */
static void add_place(const char *label, const char *path)
{
	char cmd[EXEC_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "foot -e mc %s", path);
	add(label, cmd, -1);
}

static void load_places(void)
{
	const char *home = getenv("HOME");
	char p[512];

	if (home) {
		add_place("Home", home);
		static const char *dirs[] = { "Desktop", "Documents", "Downloads",
					      "Music", "Pictures", "Videos", NULL };
		for (int i = 0; dirs[i]; i++) {
			snprintf(p, sizeof(p), "%s/%s", home, dirs[i]);
			/* Only if it exists: XDG user dirs are created on
			 * demand, and a menu entry that opens an error is worse
			 * than one that is not there. */
			if (!access(p, F_OK))
				add_place(dirs[i], p);
		}
		snprintf(p, sizeof(p), "%s/.local/share/Trash/files", home);
		add_place("Trash", p);
	}
	add_place("Computer", "/");

	/*
	 * Mounted volumes, from /proc/mounts.
	 *
	 * Filtered to the mountpoints a person would recognise — /mnt, /media
	 * and /run/media — rather than every line, because the full list on a
	 * running system is forty cgroup, proc, sysfs and tmpfs entries and
	 * none of them is a place.
	 */
	FILE *f = fopen("/proc/mounts", "r");
	if (!f)
		return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		char dev[256], dir[256];
		if (sscanf(line, "%255s %255s", dev, dir) != 2)
			continue;
		if (strncmp(dir, "/mnt", 4) && strncmp(dir, "/media", 6) &&
		    strncmp(dir, "/run/media", 10))
			continue;
		if (!strcmp(dir, "/mnt") || !strcmp(dir, "/media"))
			continue;
		const char *base = strrchr(dir, '/');
		add_place(base && base[1] ? base + 1 : dir, dir);
	}
	fclose(f);
}

static void load_system(void)
{
	add("Theme — phosphor", "foot -e kdos theme phosphor", -1);
	add("Theme — amber",    "foot -e kdos theme amber", -1);
	add("Theme — ice",      "foot -e kdos theme ice", -1);
	add("Theme — bone",     "foot -e kdos theme bone", -1);
	add("System status",    "foot -e kdos status", -1);
	add("Doctor",           "foot -e kdos doctor", -1);
	add("Terminal",         "foot", -1);
	add("",                 NULL, -2);		/* separator */
	add("Lock Screen",      "kdos-lock", -1);
	/*
	 * Log Out is `pkill -TERM -x kdos-comp`, which looks blunt and is the
	 * honest route: the compositor ends the session on SIGTERM — the same
	 * path Super+Escape takes — and nothing publishes its pid. An exec of
	 * pkill is not a shell, so the no-shell rule holds.
	 */
	add("Log Out",          "pkill -TERM -x kdos-comp", -1);
	add("Suspend",          "kdos-power suspend", -1);
	add("Restart",          "kdos-power reboot", -1);
	add("Shut Down",        "kdos-power poweroff", -1);
}

/* ── launching ─────────────────────────────────────────────────────────── */

/*
 * argv, never a command string, and never /bin/sh -c.
 *
 * The same rule kdos-comp, kpkg, kinstall and kdos-appbox all follow: an Exec
 * line comes from a file anything can write, and a shell in the middle turns it
 * into an injection point. Splitting on whitespace loses quoted arguments,
 * which is a real limitation and the correct trade — a launcher that runs
 * `rm -rf ~` because a .desktop asked it to is not a launcher.
 */
static void launch(const struct item *it)
{
	char buf[EXEC_MAX_LEN];
	char *argv[32];
	int n = 0;

	if (!*it->exec)
		return;

	if (it->terminal)
		snprintf(buf, sizeof(buf), "foot -e %s", it->exec);
	else
		snprintf(buf, sizeof(buf), "%s", it->exec);

	for (char *p = strtok(buf, " \t"); p && n < 31; p = strtok(NULL, " \t"))
		argv[n++] = p;
	argv[n] = NULL;
	if (!n)
		return;

	/* Double fork, so the menu never has to reap and the application is not
	 * killed when the menu exits — which it is about to. */
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

/* ── drawing ───────────────────────────────────────────────────────────── */

struct view {
	int rows[MAX_ITEMS];		/* item indices, or -(group+2) for a group */
	int n;
	int sel;
	int top;
	int group;			/* -1 at the root of Applications */
	char title[96];
};

static void build_view(struct view *v, int which, int group)
{
	v->n = 0;
	v->sel = 0;
	v->top = 0;
	v->group = group;

	if (which == 0 && group < 0) {
		snprintf(v->title, sizeof(v->title), "Applications");
		for (int g = 0; g < NGROUPS; g++) {
			/* A group with nothing in it is not shown. An empty
			 * submenu is a promise the menu cannot keep. */
			int any = 0;
			for (int i = 0; i < nitems; i++)
				if (items[i].group == g)
					any = 1;
			if (any)
				v->rows[v->n++] = -(g + 2);
		}
		return;
	}
	if (which == 0) {
		snprintf(v->title, sizeof(v->title), "Applications / %s",
			 GROUPS[group].name);
		for (int i = 0; i < nitems; i++)
			if (items[i].group == group)
				v->rows[v->n++] = i;
		return;
	}
	snprintf(v->title, sizeof(v->title), "%s",
		 which == 1 ? "Places" : "System");
	for (int i = 0; i < nitems; i++)
		v->rows[v->n++] = i;
}

static void draw(const struct view *v)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), v->title, KT_ACCENT, KT_SURFACE, 0);

	int rows = h - 2;
	for (int r = 0; r < rows; r++) {
		int idx = v->top + r;
		if (idx >= v->n)
			break;
		int row = v->rows[idx];
		bool sel = idx == v->sel;
		uint8_t fg = sel ? KT_SURFACE : KT_TEXT;
		uint8_t bg = sel ? KT_ACCENT : KT_SURFACE;

		ktui_draw_fill(krect(1, 1 + r, w - 2, 1), bg);

		if (row <= -2) {			/* a group */
			int g = -row - 2;
			ktui_draw_text(2, 1 + r, w - 6, GROUPS[g].name, fg, bg,
				       KT_A_NONE);
			ktui_draw_text(w - 3, 1 + r, 1, ktui_glyph[KT_G_RIGHT],
				       fg, bg, KT_A_NONE);
		} else if (items[row].submenu == -2) {	/* a separator */
			for (int x = 1; x < w - 1; x++)
				ktui_draw_text(x, 1 + r, 1, ktui_glyph[KT_G_HL],
					       KT_DIM, KT_SURFACE, KT_A_NONE);
		} else {
			ktui_draw_text(2, 1 + r, w - 4, items[row].name, fg, bg,
				       KT_A_NONE);
		}
	}
	ktui_draw_flush();
}

/* Separators are drawn but never selected — stepping onto one and having Enter
 * do nothing is how a menu feels broken. */
static void step(struct view *v, int dir)
{
	for (int i = 0; i < v->n; i++) {
		v->sel += dir;
		if (v->sel < 0)
			v->sel = v->n - 1;
		if (v->sel >= v->n)
			v->sel = 0;
		int row = v->rows[v->sel];
		if (row <= -2 || items[row].submenu != -2)
			return;
	}
}

int menu_main(int argc, char **argv)
{
	const char *font = NULL;
	int which = 0;				/* 0 apps, 1 places, 2 system */

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "applications"))
			which = 0;
		else if (!strcmp(argv[i], "places"))
			which = 1;
		else if (!strcmp(argv[i], "system"))
			which = 2;
		else {
			fprintf(stderr, "usage: kdos-menu "
					"[applications|places|system] "
					"[--font NAME]\n");
			return 2;
		}
	}

	if (which == 0)
		load_applications();
	else if (which == 1)
		load_places();
	else
		load_system();

	struct view v;
	build_view(&v, which, -1);

	/*
	 * Sized to the content, capped so it cannot be taller than a small
	 * screen. Anchored top-left, under the menu bar it came from — a
	 * dropdown that appeared in the middle of the screen would not read as
	 * belonging to the word that was clicked.
	 */
	int rows = v.n + 2;
	if (rows > 24)
		rows = 24;
	if (rows < 4)
		rows = 4;

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 42,
		.rows = rows,
		.app_id = "kdos-menu",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-menu: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	while (!kwl_should_close()) {
		int rows_vis = ktui_h - 2;
		if (v.sel < v.top)
			v.top = v.sel;
		if (rows_vis > 0 && v.sel >= v.top + rows_vis)
			v.top = v.sel - rows_vis + 1;
		draw(&v);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;
		if (ev.type != KT_EVT_KEY)
			continue;

		switch (ev.key) {
		case KT_K_ESC:
			/* Escape backs out one level before it closes, which is
			 * what makes a one-column menu feel like a cascade. */
			if (which == 0 && v.group >= 0) {
				int was = v.group;
				build_view(&v, which, -1);
				for (int i = 0; i < v.n; i++)
					if (v.rows[i] == -(was + 2))
						v.sel = i;
				break;
			}
			goto done;
		case KT_K_UP:
			step(&v, -1);
			break;
		case KT_K_DOWN:
			step(&v, 1);
			break;
		case KT_K_LEFT:
			if (which == 0 && v.group >= 0)
				build_view(&v, which, -1);
			break;
		case KT_K_RIGHT:
		case KT_K_ENTER: {
			if (v.sel < 0 || v.sel >= v.n)
				break;
			int row = v.rows[v.sel];
			if (row <= -2) {
				build_view(&v, which, -row - 2);
				break;
			}
			if (items[row].submenu == -2)
				break;
			launch(&items[row]);
			goto done;
		}
		default:
			break;
		}
	}
done:
	kwl_shutdown();
	return 0;
}

/* ── the panel's half ──────────────────────────────────────────────────── */

const char *const sh_menu_labels[SH_NMENUS] = {
	"Applications", "Places", "System"
};

void sh_spawn_menu(int which)
{
	static const char *const names[SH_NMENUS] = {
		"applications", "places", "system"
	};
	if (which < 0 || which >= SH_NMENUS)
		return;

	/*
	 * Double fork, so the panel never reaps and never waits. Scanning four
	 * hundred desktop files takes a moment, and a panel that blocked on it
	 * would stop the clock every time somebody opened a menu.
	 */
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execlp("kdos-menu", "kdos-menu", names[which], (char *)NULL);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

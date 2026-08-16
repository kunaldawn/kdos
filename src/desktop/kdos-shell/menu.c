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

#include <wayland-client.h>

#include "kwl.h"
#include "kxdg.h"
#include "shell.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

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
	/* The desktop-file id, applications only — what the dedupe below keys
	 * on. Empty for Places and System rows. */
	char id[64];
	/* Places only: the directory, as its OWN field. It used to ride inside
	 * `exec` and be re-split on whitespace at launch, which turned
	 * "/run/media/kdos/My Disk" into two bogus argv entries — a mount with
	 * a space in its name simply could not be opened. */
	char path[256];
	int group;			/* index into GROUPS, or -1 */
	int submenu;			/* -1, or the group this row opens */
	int terminal;			/* Terminal=true — run it inside foot */
	/* Ask before running it. Three rows in System end the session or the
	 * machine, and they sit one cell apart from "Terminal" in a menu
	 * people navigate with a mouse. */
	const char *confirm;
};

static struct item items[MAX_ITEMS];
static int nitems;

/* ── reading the applications ──────────────────────────────────────────── */

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

static int have_id(const char *id)
{
	for (int i = 0; i < nitems; i++)
		if (*id && !strcmp(items[i].id, id))
			return 1;
	return 0;
}

static void add_desktop_file(const char *path)
{
	KxdgEntry e;
	char id[64];

	/* The desktop-file id, from the file name. The user's directory is
	 * scanned first and wins: a ~/.local/share entry exists precisely to
	 * REPLACE the system one of the same id, and without this check an
	 * override showed the app twice. */
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	size_t blen = strlen(base);		/* ends in .desktop — scan_dir checked */
	snprintf(id, sizeof(id), "%.*s", (int)(blen - 8), base);
	if (have_id(id))
		return;

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
	snprintf(it->id, sizeof(it->id), "%s", id);
	sh_strip_field_codes(it->exec);
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

	/* User first — the dedupe in add_desktop_file keeps the FIRST entry
	 * per id, so this order is what makes an override an override. */
	if (home) {
		snprintf(user, sizeof(user), "%s/.local/share/applications", home);
		scan_dir(user);
	}
	scan_dir("/usr/local/share/applications");
	scan_dir("/usr/share/applications");
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
	if (nitems >= MAX_ITEMS)
		return;
	add(label, NULL, -1);
	/* The path is its own field: launch() builds the argv directly from
	 * it, so a mount with a space in its name is one argument. */
	snprintf(items[nitems - 1].path, sizeof(items[0].path), "%s", path);
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

/* Same as add(), plus the question to ask first. */
static void add_confirmed(const char *name, const char *exec,
			  const char *question)
{
	add(name, exec, -1);
	if (nitems > 0)
		items[nitems - 1].confirm = question;
}

static void load_system(void)
{
	add("Theme — phosphor", "foot -e kdos theme phosphor", -1);
	add("Theme — amber",    "foot -e kdos theme amber", -1);
	add("Theme — ice",      "foot -e kdos theme ice", -1);
	add("Theme — bone",     "foot -e kdos theme bone", -1);
	add("",                 NULL, -2);		/* separator */
	add("Applications…",    "kdos-appbox tui", -1);
	add("Displays",         "kdos-display", -1);
	add("Network",          "foot -e nmtui", -1);
	add("Files",            "foot -e mc", -1);
	add("Task Manager",     "foot -e btop", -1);
	add("",                 NULL, -2);		/* separator */
	add("System status",    "foot -e kdos status", -1);
	add("Energy Impact",    "foot -e kdos-energy", -1);
	add("Doctor",           "foot -e kdos doctor", -1);
	add("Help — keys and commands", "foot -e kdos help --pager", -1);
	add("Terminal",         "foot", -1);
	add("",                 NULL, -2);		/* separator */
	add("Lock Screen",      "kdos-lock", -1);
	/*
	 * Log Out is `pkill -TERM -x kdos-comp`, which looks blunt and is the
	 * honest route: the compositor ends the session on SIGTERM — the same
	 * path Super+Escape takes — and nothing publishes its pid. An exec of
	 * pkill is not a shell, so the no-shell rule holds.
	 */
	add_confirmed("Log Out", "pkill -TERM -x kdos-comp",
		      "End the KDOS session? Unsaved work will be lost.");
	add("Suspend",          "kdos-power suspend", -1);
	add_confirmed("Restart", "kdos-power reboot",
		      "Restart this machine?");
	add_confirmed("Shut Down", "kdos-power poweroff",
		      "Shut this machine down?");
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
/*
 * Ask, and wait for the answer.
 *
 * SYNCHRONOUS on purpose: the menu is about to exit and the question is
 * whether to do the thing at all, so there is nothing to do in the meantime.
 * kdos-prompt is the same dialog kdos-comp's own Log Out uses, so the two
 * routes to ending a session ask the same question in the same words.
 * Returns non-zero when the user said yes; anything else — No, Escape, a
 * prompt that could not start — is a no.
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

static void launch(const struct item *it)
{
	char buf[EXEC_MAX_LEN + 16];
	const char *argv[32];
	int n = 0;

	if (!*it->exec && !it->path[0])
		return;
	if (it->confirm && !confirmed(it->confirm))
		return;

	if (it->path[0]) {
		/* A Places row: the path travels as ONE argument, whatever is
		 * in it — no re-split, no truncation. */
		argv[n++] = "foot";
		argv[n++] = "-e";
		argv[n++] = "mc";
		argv[n++] = it->path;
	} else {
		if (it->terminal)
			snprintf(buf, sizeof(buf), "foot -e %s", it->exec);
		else
			snprintf(buf, sizeof(buf), "%s", it->exec);

		for (char *p = strtok(buf, " \t"); p && n < 31;
		     p = strtok(NULL, " \t"))
			argv[n++] = p;
	}
	argv[n] = NULL;
	if (!n)
		return;

	/* Double fork, so the menu never has to reap and the application is not
	 * killed when the menu exits — which it is about to. */
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

/* What a row reads as: a group's name, an item's name, or nothing for a
 * separator. One place, because the drawing, the type-ahead and any future
 * search all have to agree about it. */
static const char *row_label(const struct view *v, int i)
{
	if (i < 0 || i >= v->n)
		return "";
	int row = v->rows[i];
	if (row <= -2)
		return GROUPS[-row - 2].name;
	if (items[row].submenu == -2)
		return "";
	return items[row].name;
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

/* A page, or an end. `step` rather than an assignment, so a separator is never
 * what a page lands on. */
static void jump(struct view *v, int dir, int n)
{
	for (int i = 0; i < n; i++)
		step(v, dir);
}

/*
 * Type-ahead: a letter goes to the next row that starts with it.
 *
 * Applications has nine groups and some of them have forty entries; walking to
 * `wireshark` with the down arrow is not navigation. Wrapping from the current
 * position rather than from the top is what makes pressing the same letter
 * twice cycle through the matches, which is the behaviour every list on every
 * desktop has.
 */
static void typeahead(struct view *v, int ch)
{
	int lc = ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;

	for (int k = 1; k <= v->n; k++) {
		int i = (v->sel + k) % v->n;
		const char *l = row_label(v, i);
		int f = *l >= 'A' && *l <= 'Z' ? *l + 32 : *l;
		if (*l && f == lc) {
			v->sel = i;
			return;
		}
	}
}

/* ── --windows: one app's windows, from the panel's grouped chip ────────── */

/*
 * `kdos-menu --windows <app_id>` lists the titles of that app's toplevels
 * plus "Close all" and "Minimize all", and the panel spawns it when a grouped
 * task chip with more than one window is clicked. Foreign-toplevel is bound
 * HERE, on libkwl's connection (the same pattern shell.c uses and for the
 * same reason — a second wl_display would be a second seat and a second set
 * of globals), only in this mode: the ordinary menus never need it.
 */
#define MAX_WIN 64

struct win {
	struct zwlr_foreign_toplevel_handle_v1 *handle;
	char app_id[64];
	char title[128];
};

static struct win wins[MAX_WIN];
static int nwins;
static struct zwlr_foreign_toplevel_manager_v1 *win_mgr;

static struct win *win_for(struct zwlr_foreign_toplevel_handle_v1 *h)
{
	for (int i = 0; i < nwins; i++)
		if (wins[i].handle == h)
			return &wins[i];
	return NULL;
}

static void wtl_title(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
		      const char *title)
{
	struct win *w = win_for(h);
	(void)d;
	if (w)
		snprintf(w->title, sizeof(w->title), "%s", title);
}

static void wtl_app_id(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
		       const char *app_id)
{
	struct win *w = win_for(h);
	(void)d;
	if (w)
		snprintf(w->app_id, sizeof(w->app_id), "%s", app_id);
}

static void wtl_output_enter(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
			     struct wl_output *o)
{ (void)d; (void)h; (void)o; }
static void wtl_output_leave(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
			     struct wl_output *o)
{ (void)d; (void)h; (void)o; }
static void wtl_state(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
		      struct wl_array *states)
{ (void)d; (void)h; (void)states; }
static void wtl_done(void *d, struct zwlr_foreign_toplevel_handle_v1 *h)
{ (void)d; (void)h; }

static void wtl_closed(void *d, struct zwlr_foreign_toplevel_handle_v1 *h)
{
	(void)d;
	for (int i = 0; i < nwins; i++) {
		if (wins[i].handle != h)
			continue;
		memmove(&wins[i], &wins[i + 1],
			(size_t)(nwins - i - 1) * sizeof(wins[0]));
		nwins--;
		break;
	}
	zwlr_foreign_toplevel_handle_v1_destroy(h);
}

static void wtl_parent(void *d, struct zwlr_foreign_toplevel_handle_v1 *h,
		       struct zwlr_foreign_toplevel_handle_v1 *p)
{ (void)d; (void)h; (void)p; }

static const struct zwlr_foreign_toplevel_handle_v1_listener win_tl_listener = {
	.title = wtl_title,
	.app_id = wtl_app_id,
	.output_enter = wtl_output_enter,
	.output_leave = wtl_output_leave,
	.state = wtl_state,
	.done = wtl_done,
	.closed = wtl_closed,
	.parent = wtl_parent,
};

static void wmgr_toplevel(void *d, struct zwlr_foreign_toplevel_manager_v1 *m,
			  struct zwlr_foreign_toplevel_handle_v1 *h)
{
	(void)d; (void)m;
	if (nwins >= MAX_WIN) {
		zwlr_foreign_toplevel_handle_v1_destroy(h);
		return;
	}
	struct win *w = &wins[nwins++];
	memset(w, 0, sizeof(*w));
	w->handle = h;
	zwlr_foreign_toplevel_handle_v1_add_listener(h, &win_tl_listener, NULL);
}

static void wmgr_finished(void *d, struct zwlr_foreign_toplevel_manager_v1 *m)
{ (void)d; (void)m; }

static const struct zwlr_foreign_toplevel_manager_v1_listener win_mgr_listener = {
	.toplevel = wmgr_toplevel,
	.finished = wmgr_finished,
};

static void win_reg_global(void *d, struct wl_registry *r, uint32_t name,
			   const char *iface, uint32_t version)
{
	(void)d; (void)version;
	if (!strcmp(iface, zwlr_foreign_toplevel_manager_v1_interface.name)) {
		win_mgr = wl_registry_bind(
			r, name, &zwlr_foreign_toplevel_manager_v1_interface, 3);
		zwlr_foreign_toplevel_manager_v1_add_listener(win_mgr,
							      &win_mgr_listener,
							      NULL);
	}
}

static void win_reg_remove(void *d, struct wl_registry *r, uint32_t name)
{ (void)d; (void)r; (void)name; }

static const struct wl_registry_listener win_registry_listener = {
	.global = win_reg_global,
	.global_remove = win_reg_remove,
};

/* Rows are 0..n-1 the windows, n a separator, n+1 Close all, n+2 Minimize
 * all. One separator, so stepping over it is one extra hop. */
static int win_step(int sel, int dir, int n, int total)
{
	sel += dir;
	if (sel < 0)
		sel = total - 1;
	if (sel >= total)
		sel = 0;
	if (sel == n) {
		sel += dir >= 0 ? 1 : -1;
		if (sel < 0)
			sel = total - 1;
		if (sel >= total)
			sel = 0;
	}
	return sel;
}

static void windows_draw(const char *app, const int *ord, int n, int sel,
			 int top)
{
	int w = ktui_w, h = ktui_h;
	int total = n + 3;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), app, KT_ACCENT, KT_SURFACE, 0);

	int rows = h - 2;
	for (int r = 0; r < rows; r++) {
		int idx = top + r;
		if (idx >= total)
			break;
		bool is_sel = idx == sel;
		uint8_t fg = is_sel ? KT_SURFACE : KT_TEXT;
		uint8_t bg = is_sel ? KT_ACCENT : KT_SURFACE;

		ktui_draw_fill(krect(1, 1 + r, w - 2, 1), bg);
		if (idx < n) {
			const struct win *wn = &wins[ord[idx]];
			ktui_draw_text(2, 1 + r, w - 4,
				       wn->title[0] ? wn->title : wn->app_id,
				       fg, bg, KT_A_NONE);
		} else if (idx == n) {
			for (int x = 1; x < w - 1; x++)
				ktui_draw_text(x, 1 + r, 1, ktui_glyph[KT_G_HL],
					       KT_DIM, KT_SURFACE, KT_A_NONE);
		} else {
			ktui_draw_text(2, 1 + r, w - 4,
				       idx == n + 1 ? "Close all"
						    : "Minimize all",
				       fg, bg, KT_A_NONE);
		}
	}
	ktui_draw_flush();
}

static int windows_main(const char *app, int at_x, int at_y, const char *font)
{
	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 42,
		.rows = 7,		/* provisional; resized once counted */
		.corner = at_x >= 0 ? KWL_CORNER_TOP_LEFT : KWL_CORNER_CENTER,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-menu",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-menu: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	struct wl_display *dpy = kwl_display();
	struct wl_registry *reg = wl_display_get_registry(dpy);
	wl_registry_add_listener(reg, &win_registry_listener, NULL);
	wl_display_roundtrip(dpy);		/* the globals */
	wl_display_roundtrip(dpy);		/* and the toplevels they announce */
	if (!win_mgr) {
		fprintf(stderr, "kdos-menu: no foreign-toplevel manager\n");
		kwl_shutdown();
		return 1;
	}

	int sel = 0, top = 0, want_rows = -1;
	int done = 0;

	while (!kwl_should_close() && !done) {
		wl_display_dispatch_pending(dpy);
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}

		int ord[MAX_WIN], n = 0;
		struct zwlr_foreign_toplevel_handle_v1 *shown[MAX_WIN];
		for (int i = 0; i < nwins; i++)
			if (!strcmp(wins[i].app_id, app)) {
				shown[n] = wins[i].handle;
				ord[n++] = i;
			}
		/* Every window of the app closed under us: a menu of nothing
		 * has nothing left to offer. */
		if (n == 0)
			break;
		int total = n + 3;
		int wr = total + 2;
		if (wr > 24)
			wr = 24;
		if (wr != want_rows) {
			want_rows = wr;
			kwl_overlay_resize(42, wr);
		}

		if (sel >= total)
			sel = total - 1;
		if (sel == n)
			sel = n - 1;
		int rows_vis = ktui_h - 2;
		if (sel < top)
			top = sel;
		if (rows_vis > 0 && sel >= top + rows_vis)
			top = sel - rows_vis + 1;
		windows_draw(app, ord, n, sel, top);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;

		int act = -1;			/* row to activate */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + top;
			bool on_row = ev.my >= 1 && ev.my < ktui_h - 1 &&
				      row >= 0 && row < total && row != n;
			if (ev.press == KT_MP_DRAG) {
				if (on_row)
					sel = row;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				sel = win_step(sel, -1, n, total);
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				sel = win_step(sel, 1, n, total);
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT || !on_row)
				continue;
			act = row;
		} else if (ev.type == KT_EVT_KEY) {
			switch (ev.key) {
			case KT_K_ESC:
				done = 1;
				break;
			case KT_K_UP:
				sel = win_step(sel, -1, n, total);
				break;
			case KT_K_DOWN:
				sel = win_step(sel, 1, n, total);
				break;
			case KT_K_HOME:
				sel = 0;
				break;
			case KT_K_END:
				sel = total - 1;
				break;
			case KT_K_ENTER:
				act = sel;
				break;
			default:
				break;
			}
		}
		if (act < 0)
			continue;
		/* The poll may have dispatched a closed window, compacting
		 * wins[] under ord[]. Rebuild the match set and compare the
		 * HANDLES, not the count: one window closing and another of the
		 * same app opening in one batch leaves the count unchanged while
		 * every row names a different window. */
		{
			int n2 = 0, i;
			for (i = 0; i < nwins && n2 < MAX_WIN; i++)
				if (!strcmp(wins[i].app_id, app))
					ord[n2++] = i;
			if (n2 != n)
				continue;
			for (i = 0; i < n; i++)
				if (wins[ord[i]].handle != shown[i])
					break;
			if (i != n)
				continue;
		}

		if (act < n) {
			struct wl_seat *seat = kwl_seat();
			if (seat) {
				zwlr_foreign_toplevel_handle_v1_activate(
					wins[ord[act]].handle, seat);
				wl_display_flush(dpy);
			}
		} else if (act == n + 1) {
			for (int i = 0; i < n; i++)
				zwlr_foreign_toplevel_handle_v1_close(
					wins[ord[i]].handle);
			wl_display_flush(dpy);
		} else if (act == n + 2) {
			for (int i = 0; i < n; i++)
				zwlr_foreign_toplevel_handle_v1_set_minimized(
					wins[ord[i]].handle);
			wl_display_flush(dpy);
		}
		break;
	}

	kwl_shutdown();
	return 0;
}

int menu_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *win_app = NULL;		/* --windows: one app's toplevels */
	int which = 0;				/* 0 apps, 1 places, 2 system */
	int at_x = -1, at_y = 0;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		/* One frame, offscreen, as text: see kdos-launcher --dump. */
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		/*
		 * Where the word that opened it is, in pixels. The panel knows
		 * and the menu does not: they are two processes, and a
		 * dropdown that appears in the middle of the screen does not
		 * read as belonging to anything. Absent, it centres, which is
		 * right for `kdos-menu system` typed at a prompt.
		 */
		else if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--windows") && i + 1 < argc)
			win_app = argv[++i];
		else if (!strcmp(argv[i], "applications"))
			which = 0;
		else if (!strcmp(argv[i], "places"))
			which = 1;
		else if (!strcmp(argv[i], "system"))
			which = 2;
		else {
			fprintf(stderr, "usage: kdos-menu "
					"[applications|places|system] "
					"[--windows APP_ID] "
					"[--at X Y] [--dump] [--font NAME]\n");
			return 2;
		}
	}

	if (win_app)
		return windows_main(win_app, at_x, at_y, font);

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
	 * screen. Anchored top-left under the word it came from when the panel
	 * said where that was — layer-shell has no coordinates, so "at x" is
	 * an anchor plus a margin, which is what KWL_CORNER_TOP_LEFT is.
	 */
	int rows = v.n + 2;
	if (rows > 24)
		rows = 24;
	if (rows < 4)
		rows = 4;

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(42, rows);
		draw(&v);
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = 42,
		.rows = rows,
		.corner = at_x >= 0 ? KWL_CORNER_TOP_LEFT : KWL_CORNER_CENTER,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-menu",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-menu: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();

	while (!kwl_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int rows_vis = ktui_h - 2;
		if (v.sel < v.top)
			v.top = v.sel;
		if (rows_vis > 0 && v.sel >= v.top + rows_vis)
			v.top = v.sel - rows_vis + 1;
		draw(&v);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			/* A configure is not applied until the loop that owns
			 * the surface applies it — `rows_vis` above is computed
			 * from a height that no longer exists otherwise. */
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		/*
		 * Full mouse, because a menu opened BY mouse must be drivable by
		 * it — the first live boot shipped keyboard-only menus, which
		 * read as "mouse not working". Hover selects (motion arrives as
		 * KT_MP_DRAG — libkwl's spelling for plain movement), a left
		 * PRESS activates, the wheel scrolls, and a right press backs
		 * out like Escape.
		 */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + v.top;
			bool on_row = ev.my >= 1 && ev.my < ktui_h - 1 &&
				      row >= 0 && row < v.n &&
				      (v.rows[row] <= -2 ||
				       items[v.rows[row]].submenu != -2);
			if (ev.press == KT_MP_DRAG) {
				if (on_row)
					v.sel = row;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_WHEEL_UP) {
				step(&v, -1);
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				step(&v, 1);
				continue;
			}
			if (ev.btn == KT_MB_RIGHT) {
				if (which == 0 && v.group >= 0)
					build_view(&v, which, -1);
				else
					goto done;
				continue;
			}
			if (ev.btn != KT_MB_LEFT || !on_row)
				continue;
			v.sel = row;
			int r = v.rows[row];
			if (r <= -2) {
				build_view(&v, which, -r - 2);
				continue;
			}
			launch(&items[r]);
			goto done;
		}
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
		case KT_K_PGUP:
			jump(&v, -1, ktui_h - 2);
			break;
		case KT_K_PGDN:
			jump(&v, 1, ktui_h - 2);
			break;
		case KT_K_HOME:
			v.sel = 0;
			if (v.n && row_label(&v, 0)[0] == '\0')
				step(&v, 1);
			break;
		case KT_K_END:
			v.sel = v.n ? v.n - 1 : 0;
			if (v.n && row_label(&v, v.sel)[0] == '\0')
				step(&v, -1);
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
			if (ev.key >= 0x20 && ev.key < 0x7f)
				typeahead(&v, ev.key);
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

void sh_spawn_menu(int which, int x, int y)
{
	static const char *const names[SH_NMENUS] = {
		"applications", "places", "system"
	};
	char xs[16], ys[16];

	if (which < 0 || which >= SH_NMENUS)
		return;
	snprintf(xs, sizeof(xs), "%d", x);
	snprintf(ys, sizeof(ys), "%d", y);

	/*
	 * Double fork, so the panel never reaps and never waits. Scanning four
	 * hundred desktop files takes a moment, and a panel that blocked on it
	 * would stop the clock every time somebody opened a menu.
	 */
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execlp("kdos-menu", "kdos-menu", names[which], "--at",
			       xs, ys, (char *)NULL);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

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
 * see the KDISP_ROLE_LOCK note in kwl.h about exactly that limitation. Spawning
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
	KxdgPlace places[KXDG_PLACES_MAX];
	int np;

	/*
	 * FROM libkxdg, NOT FROM $HOME. These used to be six hardcoded names
	 * under the home directory while `kdos-desk` read `user-dirs.dirs` for
	 * the desktop folder — so on a machine where somebody had renamed one,
	 * the icons were in the folder the file named and this menu opened an
	 * empty one beside it. The user's own `~/.config/kdos/places` rows come
	 * back in the same call.
	 */
	np = kxdg_places(places, KXDG_PLACES_MAX);
	for (int i = 0; i < np; i++)
		add_place(places[i].name, places[i].path);

	if (home) {
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

/* Same as add(), for a command that draws in a terminal: the emulator's name
 * is this desktop's, not a constant. See sh_term(). */
static void add_term(const char *name, const char *cmd)
{
	char buf[128 + SH_TERM_PREFIX_MAX];

	sh_term_cmd(buf, sizeof(buf), cmd);
	add(name, buf, -1);
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
	char logout[64];

	add_term("Theme — phosphor", "kdos theme phosphor");
	add_term("Theme — amber",    "kdos theme amber");
	add_term("Theme — ice",      "kdos theme ice");
	add_term("Theme — bone",     "kdos theme bone");
	add("",                 NULL, -2);		/* separator */
	add("Boxes…",           "kdos-settings --page boxes", -1);
	add("Displays",         "kdos-display", -1);
	add_term("Network",          "nmtui");
	add_term("Files",            "mc");
	add_term("Task Manager",     "btop");
	add("",                 NULL, -2);		/* separator */
	add_term("System status",    "kdos status");
	add_term("Energy Impact",    "kdos-energy");
	add_term("Doctor",           "kdos doctor");
	add_term("Help — keys and commands", "kdos help --pager");
	add("Terminal",         sh_term(), -1);
	add("",                 NULL, -2);		/* separator */
	add("Lock Screen",      "kdos-lock", -1);
	/*
	 * Log Out is `pkill -TERM -x` on the program that IS this session,
	 * which looks blunt and is the honest route: it ends on SIGTERM — the
	 * same path the quit chord takes — and nothing publishes its pid. An
	 * exec of pkill is not a shell, so the no-shell rule holds.
	 */
	snprintf(logout, sizeof(logout), "pkill -TERM -x %s",
		 sh_session_prog());
	add_confirmed("Log Out", logout,
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
	char buf[EXEC_MAX_LEN + SH_TERM_PREFIX_MAX];
	char id[160];			/* argv points into it until the exec */
	const char *argv[32];
	int n = 0;

	if (!*it->exec && !it->path[0])
		return;
	if (it->confirm && !confirmed(it->confirm))
		return;

	if (it->path[0]) {
		/* A Places row: the path travels as ONE argument, whatever is
		 * in it — no re-split, no truncation. */
		n = sh_term_argv(argv, n, 32, "mc", id, sizeof(id));
		argv[n++] = "mc";
		argv[n++] = it->path;
	} else {
		if (it->terminal)
			sh_term_cmd(buf, sizeof(buf), it->exec);
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

/* The viewport follows the SELECTION only when the selection is what moved —
 * see kch_list_wheel. Shared by both menus in this file; only one runs. */
static int sel_follow = 1;

/*
 * NO HINT ROW ON EITHER SURFACE, and the reason is measured rather than
 * argued. Both are overlays sized to their content, so the row has to be
 * bought out of the height they ask for — and the System menu is 22 items
 * against a 24-row cap, which is exactly full. Reserving a row leaves 21
 * visible and scrolls `Shut Down` off the bottom of the menu somebody opened
 * to shut down with.
 *
 * The row would also be saying what a menu already says. Enter, the arrows
 * and Escape are what a column of labels MEANS; `ktui_menu`'s own popup draws
 * no hint row for the same reason. A row is worth a row where a surface
 * answers keys its shape does not imply — which is every other surface here
 * and not this one.
 *
 * The Esc LADDER still applies: menu_main's rung backs out of an open group,
 * and windows_main has none.
 */
static KtuiKeys keys;
static KtuiKeys wkeys;

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
	sel_follow = 1;
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

/*
 * ONE RUNG: an open Applications group. Escape backs out one level before it
 * closes, which is what makes a one-column menu feel like a cascade — and
 * declaring it is what lets the row read `Esc Back` there and `Esc Close` at
 * the top.
 *
 * Applications is the only mode that ever has a group open — build_view()
 * takes its group from an argument and every other mode is built with -1 — so
 * the rebuild names mode 0 outright rather than carrying `which` into a
 * callback.
 */
static int group_up(void *user)
{
	const struct view *v = user;

	return v->group >= 0;
}

static void group_close(void *user)
{
	struct view *v = user;
	int was = v->group;

	build_view(v, 0, -1);
	for (int i = 0; i < v->n; i++)
		if (v->rows[i] == -(was + 2))
			v->sel = i;
}

static void draw(const struct view *v)
{
	int w = ktui_w, h = ktui_h;

	kch_px_reset();
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), v->title, KT_ACCENT, KT_SURFACE, 0);

	int rows = h - 2;
	for (int r = 0; r < rows; r++) {
		int idx = v->top + r;
		if (idx >= v->n)
			break;
		int row = v->rows[idx];
		bool sel = idx == v->sel;
		/*
		 * A PLATE AND AN ACCENT EDGE, not a slab of full accent — the
		 * same selection the Start menu and the taskbar wear, from the
		 * same tone table, so a highlight means one thing on this
		 * desktop. The cells stay on the page's own slot, which the
		 * backdrop owns, so the plate shows through under the label.
		 */
		uint8_t fg = KT_TEXT;
		uint8_t bg = KT_SURFACE;

		if (sel)
			kch_px_row(1, 1 + r, w - 2, KCH_T_ACTIVE);

		if (row <= -2) {			/* a group */
			int g = -row - 2;
			ktui_draw_text(2, 1 + r, w - 6, GROUPS[g].name, fg, bg,
				       KT_A_NONE);
			ktui_draw_text(w - 3, 1 + r, 1, ktui_glyph[KT_G_RIGHT],
				       fg, bg, KT_A_NONE);
		} else if (items[row].submenu == -2) {	/* a separator */
			for (int x = 1; x < w - 1; x++)
				ktui_draw_text(x, 1 + r, 1, ktui_glyph[KT_G_HL],
					       KT_MID, KT_SURFACE, KT_A_NONE);
		} else {
			ktui_draw_text(2, 1 + r, w - 4, items[row].name, fg, bg,
				       KT_A_NONE);
		}
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE, on the frame's own right edge —
	 * see kch_scrollbar. It matters more since the wheel started
	 * moving the PAGE rather than the cursor: without it the content
	 * slides for no visible reason.
	 */
	kch_scrollbar(0, w - 1, 1, rows, v->n, v->top, KT_SURFACE);
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
	sel_follow = 1;
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
 * task chip with more than one window is clicked.
 *
 * THE LIST COMES FROM libkdisp, so this menu is the same menu on both
 * desktops: the console answers it from the session's management messages and
 * a compositor from wlr-foreign-toplevel-management, and neither protocol
 * appears here.
 *
 * AN ID IDENTIFIES A WINDOW, NOT AN INDEX AND NOT A POINTER. The list is
 * rebuilt every turn and a window can close between the frame that drew a row
 * and the click on it; an index would then name whichever window moved into
 * that slot, which is the worst possible failure for a menu whose entries
 * close things.
 */
#define MAX_WIN 64

struct win {
	unsigned id;
	char app_id[64];
	char title[128];
	/* The server's own answer, not a guess. A control menu whose Maximize
	 * is a toggle has to KNOW which way the window is, or the entry does
	 * nothing every second time it is used and reads as broken. */
	int maximized, minimized, activated, fullscreen;
};

static struct win wins[MAX_WIN];
static int nwins;

/* Re-read the list. Called every turn: it is a copy of at most sixty-four
 * short rows, and anything cleverer would be a cache to keep in step with a
 * list the server already keeps. */
static void win_refresh(void)
{
	KDispWin w;

	nwins = 0;
	for (int i = 0; nwins < MAX_WIN && kdisp_win_at(i, &w); i++) {
		wins[nwins].id = w.id;
		snprintf(wins[nwins].app_id, sizeof(wins[0].app_id), "%s",
			 w.app_id);
		snprintf(wins[nwins].title, sizeof(wins[0].title), "%s",
			 w.title);
		wins[nwins].maximized = (w.flags & KDISP_WIN_MAXIMISED) != 0;
		wins[nwins].minimized = (w.flags & KDISP_WIN_MINIMISED) != 0;
		wins[nwins].activated = (w.flags & KDISP_WIN_FOCUSED) != 0;
		wins[nwins].fullscreen = (w.flags & KDISP_WIN_FULLSCREEN) != 0;
		nwins++;
	}
}

/* ── the rows ──────────────────────────────────────────────────────────────
 *
 * TWO MENUS, ONE ROW MODEL. `--windows` lists an app's toplevels and is what a
 * left click on a grouped task button opens; `--winmenu` is the WINDOW CONTROL
 * menu a right click opens, which is what every taskbar has offered since
 * Windows 95 and this one did not — a right click simply minimised the button
 * and there was no way to maximise, restore or close a window from the bar at
 * all.
 *
 * Building a row LIST rather than indexing 0..n-1/n+1/n+2 is the difference
 * between adding an entry and re-deriving three pieces of arithmetic. A rule
 * is a row that draws and cannot be selected.
 *
 * WHAT IS NOT HERE, and deliberately: Move and Size. XP has them and
 * wlr-foreign-toplevel-management has no request for either — it can maximize,
 * minimize, fullscreen, activate, close and set a rectangle, and that is all.
 * Offering a Move that did nothing would be worse than not offering it; the
 * compositor's own titlebar drag and `A-` drag are the move.
 */
enum { WR_WIN = 0, WR_RULE, WR_RESTORE, WR_MIN, WR_MAX, WR_FULL, WR_CLOSE,
       WR_MIN_ALL, WR_CLOSE_ALL, WR_PIN,
       /* The jump list: a new instance, and the files this application was
        * last used with. */
       WR_NEW, WR_RECENT };

struct wrow {
	int kind;
	int win;		/* index into wins[], for WR_WIN */
	int recent;		/* index into recents[], for WR_RECENT */
	char label[128];
};

#define MAX_WROW (MAX_WIN + 10)

static int wrow_pickable(const struct wrow *r)
{
	return r->kind != WR_RULE;
}

static int win_step(const struct wrow *rows, int nrows, int sel, int dir)
{
	sel_follow = 1;
	for (int k = 0; k < nrows; k++) {
		sel += dir;
		if (sel < 0)
			sel = nrows - 1;
		if (sel >= nrows)
			sel = 0;
		if (wrow_pickable(&rows[sel]))
			return sel;
	}
	return sel;
}

/*
 * The box these windows came from, or "". Set by `--box`, which the panel
 * passes because IT is the half that grouped them: two GIMPs from two boxes
 * are two buttons and the app_id alone cannot say which one was clicked.
 */
static char win_box[64];

static void windows_draw(const char *app, const struct wrow *rows, int nrows,
			 int sel, int top)
{
	int w = ktui_w, h = ktui_h;

	kch_px_reset();
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	{
		/* The title on the top edge names the box when there is one:
		 * ` GIMP (arch) `, the same qualification the taskbar button
		 * wears, so the two cannot disagree about which window this
		 * menu will act on. */
		char t[128];
		if (win_box[0])
			snprintf(t, sizeof(t), "%s (%s)", app, win_box);
		else
			snprintf(t, sizeof(t), "%s", app);
		ktui_draw_box(krect(0, 0, w, h), t, KT_ACCENT, KT_SURFACE, 0);
	}

	int vis = h - 2;
	for (int r = 0; r < vis; r++) {
		int idx = top + r;
		if (idx >= nrows)
			break;
		if (rows[idx].kind == WR_RULE) {
			ktui_draw_hline(1, 1 + r, w - 2, KT_G_HL, KT_DIM,
					KT_SURFACE);
			continue;
		}
		bool is_sel = idx == sel;
		/* The same plate the cascading menu above draws — see there. */
		uint8_t fg = KT_TEXT;
		uint8_t bg = KT_SURFACE;

		if (is_sel)
			kch_px_row(1, 1 + r, w - 2, KCH_T_ACTIVE);
		ktui_draw_text(2, 1 + r, w - 4, rows[idx].label, fg, bg,
			       KT_A_NONE);
	}

	/*
	 * ONE COLUMN THAT SAYS THERE IS MORE, on the frame's own right edge —
	 * see kch_scrollbar. It matters more since the wheel started
	 * moving the PAGE rather than the cursor: without it the content
	 * slides for no visible reason.
	 */
	kch_scrollbar(0, w - 1, 1, vis, nrows, top, KT_SURFACE);
	ktui_draw_flush();
}

/*
 * The rows this app's windows deserve right now.
 *
 * The control half acts on ONE window — the app's activated one, or its first
 * — because "Maximize" on four windows at once is not what anybody aiming at a
 * taskbar button means. The `all` rows are the group operations and say so.
 */
static char pin_id[128];	/* the desktop id behind `app`, or empty */
/* The jump list. `new_exec` is the entry's own Exec, for New Window; the
 * destinations come from freedesktop's recent-files store. Both are resolved
 * ONCE when the menu is built — a lookup per frame would be a file read per
 * frame, on a surface that redraws for every pointer motion. */
static char new_exec[256];
#define WIN_RECENT_MAX 6
static char recents[WIN_RECENT_MAX][512];
static int nrecent;

/*
 * Double fork, so this menu never has to reap and what it started is not
 * killed when it exits — which it is about to. The same shape launch() uses;
 * argv, never a command string, for the reason stated there.
 */
static void spawn_argv(const char *const *argv)
{
	pid_t pid;

	if (!argv[0])
		return;
	pid = fork();
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

static int windows_rows(const char *app, int ctrl, struct wrow *rows,
			int *ord, int *nwin_out, int *target)
{
	int n = 0, nrows = 0, tgt = -1;

	/* The one the controls act on: the focused window if the app has one,
	 * its first otherwise. */
	for (int i = 0; i < nwins && n < MAX_WIN; i++) {
		if (strcmp(wins[i].app_id, app))
			continue;
		if (tgt < 0 || (wins[i].activated && !wins[tgt].activated))
			tgt = i;
		ord[n++] = i;
	}
	*nwin_out = n;
	*target = tgt;
	if (n == 0)
		return 0;

	if (ctrl) {
		const struct win *t = &wins[tgt];
		rows[nrows].kind = WR_RESTORE;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label),
			 "Restore");
		nrows++;
		rows[nrows].kind = WR_MIN;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label),
			 "Minimize");
		nrows++;
		rows[nrows].kind = WR_MAX;
		/* The label says which way the toggle goes, so nobody has to
		 * click it to find out. */
		snprintf(rows[nrows].label, sizeof(rows[nrows].label), "%s",
			 t->maximized ? "Restore Down" : "Maximize");
		nrows++;
		rows[nrows].kind = WR_FULL;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label), "%s",
			 t->fullscreen ? "Leave Fullscreen" : "Fullscreen");
		nrows++;
		rows[nrows].kind = WR_RULE;
		rows[nrows].label[0] = '\0';
		nrows++;
		/*
		 * PIN, above Close, because it is the row somebody comes to
		 * this menu for a second time to find. The id is the desktop
		 * entry's, resolved from the app_id the same way the taskbar
		 * resolves a NAME — the favorites file is a list of ids and an
		 * app_id is not always one.
		 */
		/*
		 * A JUMP LIST, which is what a right click on a taskbar button
		 * opens on every desktop that has one.
		 *
		 * NEW WINDOW is here rather than on middle-click, and that is a
		 * deliberate departure from the plan this came from. Middle on
		 * this bar already means "close politely", which is documented
		 * and which people use; a second meaning for the same button
		 * would be a coin toss. A row in a menu is also the only one of
		 * the two that says what it does.
		 */
		if (pin_id[0] && new_exec[0]) {
			rows[nrows].kind = WR_NEW;
			snprintf(rows[nrows].label, sizeof(rows[nrows].label),
				 "New Window");
			nrows++;
		}
		if (nrecent > 0) {
			rows[nrows].kind = WR_RULE;
			rows[nrows].label[0] = '\0';
			nrows++;
			for (int i = 0; i < nrecent &&
					nrows < MAX_WROW - 12; i++) {
				const char *b = strrchr(recents[i], '/');

				rows[nrows].kind = WR_RECENT;
				rows[nrows].recent = i;
				/* The BASENAME: a jump list is read at a
				 * glance and a full path is mostly the parts
				 * that are the same for every row. */
				snprintf(rows[nrows].label,
					 sizeof(rows[nrows].label), "%s",
					 b && b[1] ? b + 1 : recents[i]);
				nrows++;
			}
			rows[nrows].kind = WR_RULE;
			rows[nrows].label[0] = '\0';
			nrows++;
		}
		if (pin_id[0]) {
			rows[nrows].kind = WR_PIN;
			snprintf(rows[nrows].label, sizeof(rows[nrows].label),
				 "%s taskbar",
				 sh_fav_has(pin_id) ? "Unpin from"
						    : "Pin to");
			nrows++;
		}
		rows[nrows].kind = WR_CLOSE;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label), "Close");
		nrows++;
		if (n > 1) {
			rows[nrows].kind = WR_RULE;
			rows[nrows].label[0] = '\0';
			nrows++;
			rows[nrows].kind = WR_MIN_ALL;
			snprintf(rows[nrows].label, sizeof(rows[nrows].label),
				 "Minimize all (%d)", n);
			nrows++;
			rows[nrows].kind = WR_CLOSE_ALL;
			snprintf(rows[nrows].label, sizeof(rows[nrows].label),
				 "Close all (%d)", n);
			nrows++;
			rows[nrows].kind = WR_RULE;
			rows[nrows].label[0] = '\0';
			nrows++;
		}
	}

	for (int i = 0; i < n && nrows < MAX_WROW - 4; i++) {
		const struct win *wn = &wins[ord[i]];
		rows[nrows].kind = WR_WIN;
		rows[nrows].win = ord[i];
		/* A minimised window is marked, not hidden: the list is how
		 * you get one back and it has to say which ones are gone. */
		snprintf(rows[nrows].label, sizeof(rows[nrows].label), "%s %s",
			 wn->minimized	? ktui_glyph[KT_G_DOT]
			 : wn->activated ? ktui_glyph[KT_G_SQUARE]
					 : " ",
			 wn->title[0] ? wn->title : wn->app_id);
		nrows++;
	}
	if (!ctrl) {
		rows[nrows].kind = WR_RULE;
		rows[nrows].label[0] = '\0';
		nrows++;
		rows[nrows].kind = WR_CLOSE_ALL;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label),
			 "Close all");
		nrows++;
		rows[nrows].kind = WR_MIN_ALL;
		snprintf(rows[nrows].label, sizeof(rows[nrows].label),
			 "Minimize all");
		nrows++;
	}
	return nrows;
}

static int windows_main(const char *app, int ctrl, int at_x, int at_y,
			int at_bottom, const char *font)
{
	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = 42,
		.rows = 7,		/* provisional; resized once counted */
		.corner = at_x < 0	? KDISP_CORNER_CENTER
			  : at_bottom	? KDISP_CORNER_BOTTOM_LEFT
					: KDISP_CORNER_TOP_LEFT,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-menu",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
		/*
		 * ASKED FOR BY EVERY MODE, not only --windows. The mode is
		 * chosen from the command line and the surface is opened once;
		 * a conditional here would be a launcher menu that worked and
		 * a window menu that silently listed nothing, which is the
		 * failure this flag exists to make impossible to hit by
		 * accident.
		 */
		.manage = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-menu: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	/*
	 * The desktop id behind this app_id, for the Pin row. `favorites` is a
	 * list of IDS and an app_id is not always one — `gimp` names an entry
	 * called `gimp.desktop` and `org.gnome.Meld` does not. sh_desktop_entry
	 * answers only when the id really resolves, so a window with no entry
	 * gets no Pin row rather than a row that pins nothing.
	 */
	if (ctrl) {
		char nm[64], ex[256];
		if (sh_desktop_entry(app, nm, sizeof(nm), ex, sizeof(ex)) == 0 &&
		    ex[0]) {
			snprintf(pin_id, sizeof(pin_id), "%s", app);
			snprintf(new_exec, sizeof(new_exec), "%s", ex);
		}
		/*
		 * The destinations are looked up by the APP_ID, which is what
		 * a writer of recently-used.xbel conventionally records as the
		 * application name. An app that records something else simply
		 * has no jump list, which is the honest failure for a
		 * convenience: an empty list costs nothing and a wrong one
		 * sends somebody to the wrong file.
		 */
		nrecent = kxdg_recent(app, recents, WIN_RECENT_MAX);
	}

	/*
	 * THE FIRST CALL IS WHAT BINDS. Under a compositor the manager is
	 * bound and its windows announced inside this call, so a count taken
	 * before it is always zero; on the console the list arrived with the
	 * attach. Either way there is nothing to ask a display server for
	 * here, which is why neither protocol appears in this file.
	 */
	win_refresh();
	if (nwins == 0 && kdisp_win_count() == 0) {
		/* No window list at all is a display server that cannot
		 * enumerate one — not a desktop with nothing open, which
		 * cannot happen when a panel chip was just clicked. */
		fprintf(stderr, "kdos-menu: this display server does not "
				"offer a window list\n");
		kdisp_shutdown();
		return 1;
	}

	int sel = 0, top = 0, want_rows = -1;
	int done = 0;

	while (!kdisp_should_close() && !done) {
		kdisp_pump();
		win_refresh();
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}

		struct wrow rows[MAX_WROW];
		int ord[MAX_WIN], n = 0, tgt = -1;
		unsigned shown[MAX_WIN];
		int nrows = windows_rows(app, ctrl, rows, ord, &n, &tgt);

		/* Every window of the app closed under us: a menu of nothing
		 * has nothing left to offer. */
		if (n == 0 || nrows == 0)
			break;
		for (int i = 0; i < n; i++)
			shown[i] = wins[ord[i]].id;

		int wr = nrows + 2;
		if (wr > 24)
			wr = 24;
		if (wr != want_rows) {
			want_rows = wr;
			kdisp_overlay_resize(42, wr);
		}

		if (sel >= nrows)
			sel = nrows - 1;
		if (sel < 0)
			sel = 0;
		if (!wrow_pickable(&rows[sel]))
			sel = win_step(rows, nrows, sel, 1);
		int rows_vis = ktui_h - 2;
		kch_list_clamp(&top, sel, nrows, rows_vis, sel_follow);
		sel_follow = 0;
		windows_draw(app, rows, nrows, sel, top);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000))
			continue;

		int act = -1;			/* row to activate */
		if (ev.type == KT_EVT_MOUSE) {
			int row = ev.my - 1 + top;
			bool on_row = ev.my >= 1 && ev.my < ktui_h - 1 &&
				      row >= 0 && row < nrows &&
				      wrow_pickable(&rows[row]);
			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				if (on_row) {
					sel = row;
					sel_follow = 1;
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
				int bt = kch_scrollbar_press(0, ev.mx, ev.my);

				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;
				if (!kch_list_wheel(up, &top, nrows, rows_vis)) {
					sel = win_step(rows, nrows, sel,
						       up ? -1 : 1);
					sel_follow = 1;
				}
				continue;
			}
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn != KT_MB_LEFT || !on_row)
				continue;
			act = row;
		} else if (ev.type == KT_EVT_KEY) {
			if (ktui_keys(&wkeys, &ev) == KTUI_KEY_CLOSE)
				break;

			switch (ev.key) {
			case KT_K_UP:
				sel = win_step(rows, nrows, sel, -1);
				break;
			case KT_K_DOWN:
				sel = win_step(rows, nrows, sel, 1);
				break;
			case KT_K_HOME:
				sel = win_step(rows, nrows, nrows - 1, 1);
				break;
			case KT_K_END:
				sel = win_step(rows, nrows, 0, -1);
				break;
			case KT_K_ENTER:
				act = sel;
				break;
			default:
				break;
			}
		}
		if (act < 0 || act >= nrows)
			continue;
		/* The poll may have taken a closed window out of the list,
		 * compacting wins[] under ord[]. Rebuild the match set and
		 * compare the IDS, not the count: one window closing and
		 * another of the same app opening in one batch leaves the
		 * count unchanged while every row names a different window,
		 * and the click then lands on whatever moved into the slot. */
		kdisp_pump();
		win_refresh();
		{
			int n2 = 0, i;
			for (i = 0; i < nwins && n2 < MAX_WIN; i++)
				if (!strcmp(wins[i].app_id, app))
					ord[n2++] = i;
			if (n2 != n)
				continue;
			for (i = 0; i < n; i++)
				if (wins[ord[i]].id != shown[i])
					break;
			if (i != n)
				continue;
			/* tgt is an index into the list that was just
			 * rebuilt, so it is re-derived rather than carried:
			 * the focused window may have changed while the menu
			 * was open. */
			tgt = -1;
			for (i = 0; i < n; i++)
				if (tgt < 0 || (wins[ord[i]].activated &&
						!wins[tgt].activated))
					tgt = ord[i];
		}

		/* The window the controls act on, or none. libkdisp orders
		 * every verb by id, so nothing here holds a display object. */
		unsigned tid = tgt >= 0 ? wins[tgt].id : 0;

		switch (rows[act].kind) {
		case WR_WIN:
			/* Restore FIRST, then focus. Activating a minimised
			 * window is not defined to unminimise it, and a list
			 * whose entries did nothing for exactly the windows
			 * you needed the list to reach would be worthless. */
			kdisp_win_minimise(wins[rows[act].win].id, 0);
			kdisp_win_activate(wins[rows[act].win].id);
			break;
		case WR_RESTORE:
			if (!tid)
				break;
			kdisp_win_minimise(tid, 0);
			if (wins[tgt].fullscreen)
				kdisp_win_fullscreen(tid, 0);
			else if (wins[tgt].maximized)
				kdisp_win_maximise(tid, 0);
			kdisp_win_activate(tid);
			break;
		case WR_MIN:
			if (tid)
				kdisp_win_minimise(tid, 1);
			break;
		case WR_MAX:
			if (!tid)
				break;
			/* A minimised window cannot show itself maximised, so
			 * the restore comes first — otherwise Maximize on a
			 * minimised button appears to do nothing at all. */
			kdisp_win_minimise(tid, 0);
			kdisp_win_maximise(tid, !wins[tgt].maximized);
			kdisp_win_activate(tid);
			break;
		case WR_FULL:
			if (!tid)
				break;
			kdisp_win_minimise(tid, 0);
			kdisp_win_fullscreen(tid, !wins[tgt].fullscreen);
			break;
		case WR_CLOSE:
			if (tid)
				kdisp_win_close(tid);
			break;
		case WR_PIN:
			/* The panel notices by stat: see its favorites
			 * reload. Nothing here talks to it. */
			if (pin_id[0])
				sh_fav_set(pin_id, !sh_fav_has(pin_id));
			break;
		case WR_NEW: {
			/* The entry's own Exec, with the field codes gone —
			 * there is no document to substitute and a stray %U
			 * opens the application on a file called "%U". */
			const char *av[32];
			char buf[512];
			int na = 0;

			snprintf(buf, sizeof(buf), "%s", new_exec);
			sh_strip_field_codes(buf);
			for (char *t = strtok(buf, " \t");
			     t && na < 31; t = strtok(NULL, " \t"))
				av[na++] = t;
			av[na] = NULL;
			if (na)
				spawn_argv(av);
			break;
		}
		case WR_RECENT:
			/*
			 * `kdos-appbox open` IS what "open this on this
			 * machine" means here — it is the MIME route the
			 * portal's OpenURI uses too. Handing the path to the
			 * application's own Exec would open a text file in
			 * whatever last touched it rather than in its handler.
			 */
			if (rows[act].recent >= 0 &&
			    rows[act].recent < nrecent) {
				const char *av[] = { "kdos-appbox", "open",
						     recents[rows[act].recent],
						     NULL };
				spawn_argv(av);
			}
			break;
		case WR_CLOSE_ALL:
			for (int i = 0; i < n; i++)
				kdisp_win_close(wins[ord[i]].id);
			break;
		case WR_MIN_ALL:
			for (int i = 0; i < n; i++)
				kdisp_win_minimise(wins[ord[i]].id, 1);
			break;
		default:
			break;
		}
		break;
	}

	kdisp_shutdown();
	return 0;
}

int menu_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *win_app = NULL;		/* --windows: one app's toplevels */
	int win_ctrl = 0;			/* --winmenu: with the controls */
	int which = 0;				/* 0 apps, 1 places, 2 system */
	int at_x = -1, at_y = 0, at_bottom = 0;
	int dump = 0, dump_cells = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		/* One frame, offscreen, as text: see kdos-launcher --dump. */
		else if (!strcmp(argv[i], "--dump-cells"))
			dump = dump_cells = 1;
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
		}
		/*
		 * The SAME anchor, measured from the bottom edge — what a menu
		 * belonging to a bar on the bottom of the screen needs, because
		 * a client cannot express "grow upwards" by anchoring TOP: it
		 * does not know the output's pixel height.
		 *
		 * The panel has spawned `kdos-menu system --at-bottom X Y` and
		 * `kdos-menu --windows APP --at-bottom X Y` for a release and
		 * this program did not accept the flag, so both fell into the
		 * usage branch below and exited before a surface existed. The
		 * System menu and every grouped task button did nothing at all
		 * when clicked; kdos-start and kdos-clip took the same
		 * argument and worked, which is what made it look like a panel
		 * fault rather than a missing flag.
		 */
		else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--windows") && i + 1 < argc)
			win_app = argv[++i];
		/* The right-click menu: the same window list with the window
		 * CONTROLS above it. See the rows block. */
		else if (!strcmp(argv[i], "--box") && i + 1 < argc) {
			snprintf(win_box, sizeof(win_box), "%s", argv[++i]);
		}
		else if (!strcmp(argv[i], "--winmenu") && i + 1 < argc) {
			win_app = argv[++i];
			win_ctrl = 1;
		} else if (!strcmp(argv[i], "applications"))
			which = 0;
		else if (!strcmp(argv[i], "places"))
			which = 1;
		else if (!strcmp(argv[i], "system"))
			which = 2;
		else {
			fprintf(stderr, "usage: kdos-menu "
					"[applications|places|system] "
					"[--windows APP_ID] "
					"[--winmenu APP_ID] [--box NAME]\n"
					"                  "
					"[--at X Y] [--at-bottom X Y] "
					"[--dump] [--font NAME]\n");
			return 2;
		}
	}

	if (win_app)
		return windows_main(win_app, win_ctrl, at_x, at_y, at_bottom,
				    font);

	if (which == 0)
		load_applications();
	else if (which == 1)
		load_places();
	else
		load_system();

	struct view v;
	build_view(&v, which, -1);
	/* BEFORE the dump branch, so a dumped frame reads the same Esc verb
	 * the live surface does. */
	ktui_keys_layer(&keys, "Back", group_up, group_close, &v);

	/*
	 * Sized to the content, capped so it cannot be taller than a small
	 * screen. Anchored top-left under the word it came from when the panel
	 * said where that was — layer-shell has no coordinates, so "at x" is
	 * an anchor plus a margin, which is what KDISP_CORNER_TOP_LEFT is.
	 */
	int rows = v.n + 2;
	if (rows > 24)
		rows = 24;
	if (rows < 4)
		rows = 4;

	if (dump) {
		sh_theme_from_cache();
		/* Colours too — see kdos-start's own branch, and cells.c. */
		if (dump_cells) {
			ktui_backend_set(sh_cells_backend(42, rows));
			ktui_draw_init();
			draw(&v);
			return 0;
		}
		ktui_offscreen_init(42, rows);
		draw(&v);
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = 42,
		.rows = rows,
		.corner = at_x < 0	? KDISP_CORNER_CENTER
			  : at_bottom	? KDISP_CORNER_BOTTOM_LEFT
					: KDISP_CORNER_TOP_LEFT,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-menu",
		.font = font,
		.keyboard = 1,
		/* A menu, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-menu: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		int rows_vis = ktui_h - 2;
		kch_list_clamp(&v.top, v.sel, v.n, rows_vis, sel_follow);
		sel_follow = 0;
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
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					v.top = bt;
					sel_follow = 0;
					continue;
				}
				if (on_row) {
					v.sel = row;
					sel_follow = 1;
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
				int bt = kch_scrollbar_press(0, ev.mx, ev.my);

				if (bt >= 0) {
					v.top = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;
				if (!kch_list_wheel(up, &v.top, v.n, rows_vis)) {
					step(&v, up ? -1 : 1);
					sel_follow = 1;
				}
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

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				goto done;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		switch (ev.key) {
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
	kdisp_shutdown();
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

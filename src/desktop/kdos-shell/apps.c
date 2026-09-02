/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   apps.c — one index of what is installed
 *
 * `kdos-start`, `kdos-launcher`, `kdos-run` and `kdos-openwith` each used to
 * walk /usr/share/applications for themselves, which is four answers to "what
 * is installed on this machine" and four places for a rule about NoDisplay to
 * be slightly different. This is the one answer.
 *
 * WHAT IS HERE THAT WAS NOT ANYWHERE: a USAGE COUNT. A Start menu whose left
 * column is "the things you actually run" cannot be built without one, and
 * nothing on this desktop recorded a launch. It is a plain text file —
 * `$XDG_STATE_HOME/kdos/appusage`, `count last-used id` per line — written the
 * way every other state file in this tree is written (temp, fsync the file,
 * fsync the DIRECTORY, rename), and capped, because an unbounded history of
 * every program ever run is a file nobody can read and a linear scan on every
 * menu open.
 *
 * NO DAEMON AND NO WATCH. The index is built when a surface opens and thrown
 * away when it closes; these are short-lived processes and a 400-file scan is
 * two milliseconds. A cache would need invalidating by a package install, and
 * the first stale entry would be a launcher for something that is not there.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kcon.h"
#include "kxdg.h"
#include "shell.h"

static struct sh_app apps[SH_MAX_APPS];
static int napps;

/*
 * GNOME 2's Applications submenus, in its order — the same table menu.c has
 * always used, moved here so the Start menu and the menu bar cannot disagree
 * about which bucket GIMP is in.
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
	{ "Accessories",   { "Utility", "Accessibility", "Core", NULL } },
	{ "Games",         { "Game", NULL } },
	{ "Graphics",      { "Graphics", "Photography", "Scanning", NULL } },
	{ "Internet",      { "Network", "WebBrowser", "Email", NULL } },
	{ "Office",        { "Office", "TextEditor", "Spreadsheet", NULL } },
	{ "Programming",   { "Development", "IDE", NULL } },
	{ "Sound & Video", { "AudioVideo", "Audio", "Video", "Player", NULL } },
	{ "System Tools",  { "System", "Settings", "Emulator", "Security",
			     NULL } },
	{ "Education",     { "Education", "Science", "Engineering", NULL } },
};
#define NGROUPS ((int)(sizeof(GROUPS) / sizeof(GROUPS[0])))

int sh_app_ngroups(void)
{
	return NGROUPS;
}

const char *sh_app_group_name(int g)
{
	return g >= 0 && g < NGROUPS ? GROUPS[g].name : "";
}

int sh_app_group_for(const char *categories)
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
			int left = p == categories || p[-1] == ';';
			int right = p[n] == '\0' || p[n] == ';';
			if (left && right)
				return g;
		}
	return 0;				/* Accessories */
}

/* ── the usage file ────────────────────────────────────────────────────── */

static int usage_path(char *buf, size_t n)
{
	const char *state = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");

	if (state && *state)
		return snprintf(buf, n, "%s/kdos/appusage", state) < (int)n;
	if (home && *home)
		return snprintf(buf, n, "%s/.local/state/kdos/appusage", home) <
		       (int)n;
	return 0;
}

static struct sh_app *find_id(const char *id)
{
	for (int i = 0; i < napps; i++)
		if (!strcmp(apps[i].id, id))
			return &apps[i];
	return NULL;
}

static void usage_load(void)
{
	char path[512], line[320];
	FILE *f;

	if (!usage_path(path, sizeof(path)))
		return;
	f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		long count = 0, last = 0;
		char id[SH_APP_ID];
		if (sscanf(line, "%ld %ld %127s", &count, &last, id) != 3)
			continue;
		struct sh_app *a = find_id(id);
		if (!a)
			continue;	/* uninstalled since: not resurrected */
		a->uses = (int)count;
		a->last = last;
	}
	fclose(f);
}

/*
 * Written the way kdos-bootctl writes the boot state: temp file, fsync the
 * FILE, fsync the DIRECTORY, rename. The directory fsync is the step people
 * leave out, and without it the rename can be lost while the data survives.
 * This one is only a menu's ordering, but there is one correct way to replace
 * a file on this machine and having two would mean choosing per caller.
 */
static void usage_save(void)
{
	char path[512], tmp[540], dir[512];
	FILE *f;

	if (!usage_path(path, sizeof(path)))
		return;
	snprintf(dir, sizeof(dir), "%s", path);
	char *slash = strrchr(dir, '/');
	if (!slash)
		return;
	*slash = '\0';
	/* mkdir -p of one level: $XDG_STATE_HOME itself is the session's to
	 * make, and 15_userdirs.sh already did. */
	mkdir(dir, 0700);

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return;
	int written = 0;
	for (int i = 0; i < napps && written < SH_APP_USAGE_MAX; i++) {
		if (apps[i].uses <= 0)
			continue;
		fprintf(f, "%d %ld %s\n", apps[i].uses, apps[i].last,
			apps[i].id);
		written++;
	}
	if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
		fclose(f);
		unlink(tmp);
		return;
	}
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return;
	}
	int dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dfd >= 0) {
		fsync(dfd);
		close(dfd);
	}
}

/* ── the scan ──────────────────────────────────────────────────────────── */

static void add_desktop_file(const char *path)
{
	KxdgEntry e;
	char id[SH_APP_ID];

	/* The desktop-file id, from the file name. The user's directory is
	 * scanned first and wins: a ~/.local/share entry exists precisely to
	 * REPLACE the system one of the same id, and without this check an
	 * override shows the app twice. */
	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;
	size_t blen = strlen(base);	/* ends in .desktop — scan_dir checked */
	snprintf(id, sizeof(id), "%.*s", (int)(blen - 8), base);
	if (find_id(id))
		return;

	if (napps >= SH_MAX_APPS || kxdg_load(&e, path, "Desktop Entry") != 0)
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

	struct sh_app *a = &apps[napps];
	memset(a, 0, sizeof(*a));
	snprintf(a->id, sizeof(a->id), "%s", id);
	snprintf(a->name, sizeof(a->name), "%s", name);
	snprintf(a->exec, sizeof(a->exec), "%s", exec);
	snprintf(a->icon, sizeof(a->icon), "%s",
		 kxdg_get(&e, "Icon", ""));
	snprintf(a->comment, sizeof(a->comment), "%s",
		 kxdg_get(&e, "Comment", ""));
	/* Keywords and GenericName are what makes searching for "browser"
	 * find Firefox — the entry says so and nothing here has to know. */
	snprintf(a->keywords, sizeof(a->keywords), "%s %s",
		 kxdg_get(&e, "Keywords", ""), kxdg_get(&e, "GenericName", ""));
	sh_strip_field_codes(a->exec);
	a->group = sh_app_group_for(kxdg_get(&e, "Categories", NULL));
	a->terminal = kxdg_bool(&e, "Terminal", 0);
	/*
	 * WHICH ENTRIES COST A CONTAINER START, which is a question only this
	 * distro's menus can answer and only this distro's users need asked.
	 * An entry whose Exec IS the box launcher is a boxed app whatever the
	 * alien-apps table is keyed by — that table's first column is the SHIM
	 * name (`calibre`, `mousepad`) while a desktop id is upstream's own
	 * (`calibre-gui`, `org.xfce.mousepad`), so matching on the id alone
	 * tagged the minority where the two happen to coincide. The launcher
	 * learned this the hard way; the shared index knows it now, so the
	 * Start menu and kdos-menu cannot disagree with it.
	 */
	a->alien = !strncmp(a->exec, "kdos-appbox run ", 16) ||
		   strstr(a->exec, "/kdos-appbox run ") != NULL;
	if (*a->exec)
		napps++;
	kxdg_free(&e);
}

static void scan_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;

	if (!d)
		return;
	while ((e = readdir(d)) && napps < SH_MAX_APPS) {
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
	return strcasecmp(((const struct sh_app *)a)->name,
			  ((const struct sh_app *)b)->name);
}

int sh_apps_load(void)
{
	const char *home = getenv("HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	char buf[512];

	napps = 0;

	/* User first — the dedupe in add_desktop_file keeps the FIRST entry
	 * per id, so this order is what makes an override an override. */
	const char *dh = getenv("XDG_DATA_HOME");
	if (dh && *dh) {
		snprintf(buf, sizeof(buf), "%s/applications", dh);
		scan_dir(buf);
	} else if (home && *home) {
		snprintf(buf, sizeof(buf), "%s/.local/share/applications", home);
		scan_dir(buf);
	}

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	while (*dirs) {
		const char *c = strchr(dirs, ':');
		size_t l = c ? (size_t)(c - dirs) : strlen(dirs);
		if (l && l < sizeof(buf) - 16) {
			snprintf(buf, sizeof(buf), "%.*s/applications", (int)l,
				 dirs);
			scan_dir(buf);
		}
		if (!c)
			break;
		dirs = c + 1;
	}

	qsort(apps, (size_t)napps, sizeof(apps[0]), cmp_name);
	usage_load();
	return napps;
}

int sh_apps_count(void)
{
	return napps;
}

const struct sh_app *sh_apps_get(int i)
{
	return i >= 0 && i < napps ? &apps[i] : NULL;
}

const struct sh_app *sh_apps_find(const char *id)
{
	return find_id(id);
}

/* ── ranking ───────────────────────────────────────────────────────────── */

/*
 * Most-used first, and RECENCY breaks the tie rather than the name.
 *
 * A frequency list that never forgets is a list of what somebody used in their
 * first week — so the score halves for every fortnight since the app was last
 * launched, which is the cheapest decay there is and needs no history beyond
 * the two numbers already stored.
 */
static long score(const struct sh_app *a, long now)
{
	long age_days = a->last ? (now - a->last) / 86400 : 3650;
	long halvings = age_days / 14;

	if (halvings > 20)
		return 0;
	return ((long)a->uses * 1024) >> halvings;
}

static int cmp_rank(const void *pa, const void *pb)
{
	const struct sh_app *const *a = pa, *const *b = pb;
	long now = time(NULL);
	long sa = score(*a, now), sb = score(*b, now);

	if (sa != sb)
		return sa < sb ? 1 : -1;
	return strcasecmp((*a)->name, (*b)->name);
}

int sh_apps_frequent(const struct sh_app **out, int max)
{
	const struct sh_app *tmp[SH_MAX_APPS];
	int n = 0;

	for (int i = 0; i < napps; i++)
		if (apps[i].uses > 0)
			tmp[n++] = &apps[i];
	qsort(tmp, (size_t)n, sizeof(tmp[0]), cmp_rank);
	if (n > max)
		n = max;
	for (int i = 0; i < n; i++)
		out[i] = tmp[i];
	return n;
}

int sh_apps_in_group(int group, const struct sh_app **out, int max)
{
	int n = 0;

	for (int i = 0; i < napps && n < max; i++)
		if (apps[i].group == group)
			out[n++] = &apps[i];
	return n;
}

/*
 * A substring match over the name, the id, the keywords and the command,
 * case-insensitively — and RANKED, because "fi" matching forty entries in
 * alphabetical order is a list nobody reads to the end of.
 *
 * The rank is a prefix of the name first, then a word start inside it, then
 * anywhere at all, and the usage count breaks ties inside each band. That is
 * the order a person means when they type two letters.
 */
static const char *ci_str(const char *hay, const char *needle)
{
	size_t n = strlen(needle);

	if (!n)
		return hay;
	for (const char *p = hay; *p; p++)
		if (!strncasecmp(p, needle, n))
			return p;
	return NULL;
}

struct hit {
	const struct sh_app *app;
	int band;
};

static int cmp_hit(const void *pa, const void *pb)
{
	const struct hit *a = pa, *b = pb;
	long now = time(NULL);

	if (a->band != b->band)
		return a->band - b->band;
	long sa = score(a->app, now), sb = score(b->app, now);
	if (sa != sb)
		return sa < sb ? 1 : -1;
	return strcasecmp(a->app->name, b->app->name);
}

int sh_apps_match(const char *needle, const struct sh_app **out, int max)
{
	struct hit hits[SH_MAX_APPS];
	int n = 0;

	if (!needle || !*needle) {
		int k = sh_apps_frequent(out, max);
		if (k)
			return k;
		for (int i = 0; i < napps && i < max; i++)
			out[i] = &apps[i];
		return napps < max ? napps : max;
	}

	for (int i = 0; i < napps; i++) {
		const struct sh_app *a = &apps[i];
		const char *p = ci_str(a->name, needle);
		int band = -1;

		if (p == a->name)
			band = 0;
		else if (p && p[-1] == ' ')
			band = 1;
		else if (p)
			band = 2;
		else if (ci_str(a->id, needle))
			band = 3;
		else if (ci_str(a->keywords, needle))
			band = 4;
		else if (ci_str(a->exec, needle))
			band = 5;
		if (band < 0)
			continue;
		hits[n].app = a;
		hits[n].band = band;
		n++;
	}
	qsort(hits, (size_t)n, sizeof(hits[0]), cmp_hit);
	if (n > max)
		n = max;
	for (int i = 0; i < n; i++)
		out[i] = hits[i].app;
	return n;
}

/* ── launching ─────────────────────────────────────────────────────────── */

/*
 * NO SHELL, ever. The Exec line is split by kxdg_exec_split and exec'd
 * directly — every other launch path in this tree keeps that rule and this is
 * the one that runs the most things.
 *
 * IT IS NOT A `strtok(" ")`, and that was a bug rather than a simplification.
 * An Exec line carries quoting and it carries FIELD CODES, and a whitespace
 * split gets both wrong in a way that reads to a person as "the app does not
 * launch": `mpv --player-operation-mode=pseudo-gui -- %U` was handed a literal
 * `%U` to play and exited at once, `gimp-3.0 %U` opened an error dialog
 * instead of an image, and `"/usr/bin/gsmartcontrol-root"` was exec'd with the
 * quotes still on the path. Measured against the shipped appbox: nine of its
 * ninety-two entries were affected. See kxdg.h.
 */
void sh_apps_launch(const struct sh_app *a)
{
	sh_apps_launch_with(a, NULL, 0);
}

/*
 * The same, opening something. `%f`/`%u` take the first path and `%F`/`%U`
 * take them all; an entry with NO field code at all gets the paths appended,
 * which is what every other launcher does and is the only way `Exec=xterm`
 * can be handed a file.
 */
void sh_apps_launch_with(const struct sh_app *a, const char *const *files,
			 int nfiles)
{
	char store[SH_APP_EXEC * 2];
	char id[160];			/* argv points into it until the exec */
	const char *argv[48];
	int n = 0;

	if (!a || !a->exec[0])
		return;

	/* Record BEFORE the fork: the count is what the next menu open reads,
	 * and a launch that failed still tells you what was asked for. */
	struct sh_app *m = find_id(a->id);
	if (m) {
		m->uses++;
		m->last = time(NULL);
		usage_save();
	}

	/*
	 * WHICH DESKTOP THIS IS. $KDOS_CON is the console session's surface
	 * socket, set by the session for everything started inside it, and it
	 * decides how a NON-terminal entry is started below. A terminal entry
	 * needs no branch: sh_term() already names the right emulator.
	 */
	const char *con = getenv("KDOS_CON");

	if (a->terminal)
		n = sh_term_argv(argv, n, (int)(sizeof(argv) / sizeof(*argv)),
				 a->exec, id, sizeof(id));
	int got = kxdg_exec_split(a->exec, files, nfiles, store, sizeof(store),
				  argv + n, (int)(sizeof(argv) / sizeof(*argv))
						    - n - 1 - nfiles);
	if (got <= 0)
		return;
	n += got;
	int has_code = 0;
	for (const char *p = strchr(a->exec, '%'); p; p = strchr(p + 2, '%'))
		if (p[1] == 'f' || p[1] == 'F' || p[1] == 'u' || p[1] == 'U')
			has_code = 1;
	if (nfiles > 0 && !has_code)
		for (int i = 0; i < nfiles && n < 47; i++)
			argv[n++] = files[i];
	argv[n] = NULL;

	/*
	 * A GRAPHICAL APPLICATION ON THE CONSOLE GETS A TERMINAL OF ITS OWN.
	 * This desktop composites character cells and a Wayland client's
	 * surface is pixels; the session allocates a VT, kdos-cage holds it,
	 * and the guest is full screen there. Everything else about the launch
	 * — the desktop entry, kdos-appbox, the box's tagged socket — is the
	 * same path the graphical desktop uses, which is the point.
	 *
	 * A terminal entry is not one of these: it became a kdos-term window
	 * above and belongs on this grid.
	 */
	if (con && *con && !a->terminal) {
		if (kcon_run(con, argv, a->name[0] ? a->name : argv[0], 0) < 0)
			fprintf(stderr,
				"kdos-shell: cannot start '%s' — the session "
				"has no free terminal to give it\n",
				a->name[0] ? a->name : argv[0]);
		return;
	}

	sh_spawn(argv);
}

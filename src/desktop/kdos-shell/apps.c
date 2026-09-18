/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   apps.c — one index of what is installed
 *
 * `kdos-start`, `kdos-launcher`, `kdos-run` and `kdos-openwith` each walked
 * /usr/share/applications for themselves, which is four answers to "what is
 * installed on this machine" and four places for a rule about NoDisplay to be
 * slightly different. This is the answer `kdos-start` uses.
 *
 * IT IS NOT YET THE ONLY ONE. `kdos-launcher` still keeps its own index —
 * frecency and the alien mark ride on its entries — and `kdos-run` and
 * `kdos-openwith` have their own reasons. What the launcher no longer keeps is
 * its own idea of WHERE applications live: it reads the XDG data directories in
 * this file's order, because ignoring `XDG_DATA_DIRS` made it the one surface
 * that could not find what the others listed.
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
#include <limits.h>
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

#include "launch.h"

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

/* The next whitespace-delimited token of an Exec line: `*len` is its length,
 * `*p` is advanced past it, and NULL comes back at the end of the line. */
static const char *exec_token(const char **p, size_t *len)
{
	const char *s = *p, *t;

	while (*s == ' ' || *s == '\t')
		s++;
	if (!*s)
		return NULL;
	t = s;
	while (*s && *s != ' ' && *s != '\t')
		s++;
	*p = s;
	*len = (size_t)(s - t);
	return t;
}

/*
 * IS THIS EXEC LINE THE BOX LAUNCHER — matched as a BINARY and a VERB, never
 * as a fixed prefix. A generated launcher names the app's pack box between
 * the two (`kdos-appbox -b app.gimp run gimp-3.0`) and one for an app with no
 * pack does not, so a test on a literal head tags only the second kind and
 * the whole packed set silently loses its mark. The option walk is
 * kdos-appbox's own: `-b`/`--box` takes a name, every other switch takes
 * none, and both spellings are accepted or a hand-edited entry goes unmarked.
 */
int sh_exec_is_boxed(const char *exec)
{
	const char *p = exec, *tok;
	size_t n;

	if (!exec)
		return 0;
	tok = exec_token(&p, &n);
	if (!tok)
		return 0;
	/* An absolute path is the same launcher: compare the basename. */
	for (size_t i = n; i > 0; i--)
		if (tok[i - 1] == '/') {
			tok += i;
			n -= i;
			break;
		}
	if (n != 11 || strncmp(tok, "kdos-appbox", 11))
		return 0;
	while ((tok = exec_token(&p, &n))) {
		if (n == 3 && !strncmp(tok, "run", 3))
			return 1;
		if (*tok != '-')
			return 0;
		if ((n == 2 && !strncmp(tok, "-b", 2)) ||
		    (n == 5 && !strncmp(tok, "--box", 5)))
			exec_token(&p, &n);	/* the box name */
	}
	return 0;
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
	/*
	 * THE LINE THE ENTRY WROTE, FIELD CODES AND ALL. `sh_launch` spends
	 * `%f`/`%F`/`%u`/`%U` on the documents a launch carries and decides
	 * from those same codes that a line carrying none takes its documents
	 * APPENDED — so an index that deleted them here would make every entry
	 * look like the second kind, and `--open=%f` would run as `--open=`
	 * with the path as a word of its own. With no document to open,
	 * `kxdg_exec_split` drops every code and leaves no empty argument
	 * behind, which is the whole reason nothing has to be deleted first.
	 */
	snprintf(a->exec, sizeof(a->exec), "%s", exec);
	snprintf(a->icon, sizeof(a->icon), "%s",
		 kxdg_get(&e, "Icon", ""));
	snprintf(a->comment, sizeof(a->comment), "%s",
		 kxdg_get(&e, "Comment", ""));
	/* Keywords and GenericName are what makes searching for "browser"
	 * find Firefox — the entry says so and nothing here has to know. */
	snprintf(a->keywords, sizeof(a->keywords), "%s %s",
		 kxdg_get(&e, "Keywords", ""), kxdg_get(&e, "GenericName", ""));
	a->group = sh_app_group_for(kxdg_get(&e, "Categories", NULL));
	a->terminal = kxdg_bool(&e, "Terminal", 0);
	/*
	 * WHICH TERMINAL, for the few entries that need one in particular.
	 * A program drawing pictures in the grid needs the emulator that links
	 * the decoders; everything else gets the session's own, which is
	 * lighter. Validated in sh_term_named(), so the key names one of two
	 * emulators and never a program.
	 */
	snprintf(a->term, sizeof(a->term), "%s",
		 kxdg_get(&e, "X-KDOS-Term", ""));
	/*
	 * HOW THE WINDOW SHOULD OPEN, for the entries that have a shape rather
	 * than a size somebody drags. Read here and in `desk.c`, which parses
	 * an entry of its own — a key read in one and not the other is a
	 * desktop icon that behaves differently from the same row in the Start
	 * menu.
	 */
	a->floating = kxdg_bool(&e, "X-KDOS-Float", 0);
	snprintf(a->size, sizeof(a->size), "%s",
		 kxdg_get(&e, "X-KDOS-Size", ""));
	/*
	 * WHICH ENTRIES COST A CONTAINER START, which is a question only this
	 * distro's menus can answer and only this distro's users need asked.
	 * An entry whose Exec IS the box launcher is a boxed app whatever the
	 * alien-apps table is keyed by — that table's first column is the SHIM
	 * name (`calibre`, `mousepad`) while a desktop id is upstream's own
	 * (`calibre-gui`, `org.xfce.mousepad`), so matching on the id alone
	 * tags only the minority where the two happen to coincide. The shared
	 * index answers instead, so the Start menu and kdos-menu cannot
	 * disagree with it.
	 */
	a->alien = sh_exec_is_boxed(a->exec);
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
 * A FUZZY match over the name, the id, the keywords and the command — and
 * RANKED, because "fi" matching forty entries in alphabetical order is a list
 * nobody reads to the end of.
 *
 * `kb_fuzzy()` AND NOT A MATCHER OF OUR OWN. This used to be a
 * case-insensitive SUBSTRING in six bands, which meant `sm` found nothing at
 * all where a person plainly meant System Monitor — and it meant the launcher,
 * which had a subsequence matcher of its own, answered the same query
 * differently. One function in libkbase is what stops three surfaces ranking
 * one query three ways; see kbase.h for the ladder it scores by.
 *
 * HIGHER IS BETTER HERE, which is the opposite of what the launcher's private
 * matcher meant by a score. The comparison below sorts descending, and a sort
 * left the other way round would rank a correct list backwards.
 *
 * The usage count still breaks ties, and the name breaks those: that is the
 * order a person means when two rows are equally good matches.
 */
struct hit {
	const struct sh_app *app;
	int fuzz;
};

static int cmp_hit(const void *pa, const void *pb)
{
	const struct hit *a = pa, *b = pb;
	long now = time(NULL);

	if (a->fuzz != b->fuzz)
		return a->fuzz < b->fuzz ? 1 : -1;	/* DESCENDING */
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
		/*
		 * THE NAME IS WORTH MORE THAN THE COMMAND. All four fields are
		 * searched, because somebody typing `gimp` may mean any of
		 * them, but a hit in the name is what they almost always mean
		 * — so the weaker fields are scored and then discounted rather
		 * than being a separate band that outranks a good name match.
		 */
		static const int DISCOUNT[4] = { 0, 4, 8, 8 };
		const char *field[4];
		char cmd[SH_APP_EXEC];
		int best = 0;

		/*
		 * THE COMMAND IS SEARCHED WITHOUT ITS FIELD CODES, and the
		 * index keeps them: a `%U` in the haystack is two more letters
		 * for a subsequence matcher to travel through, so `fu` would
		 * find every entry whose Exec ends in one. Stripped into
		 * scratch and never in place — the launch reads the same
		 * buffer and needs the codes.
		 */
		snprintf(cmd, sizeof(cmd), "%s", a->exec);
		sh_strip_field_codes(cmd);

		field[0] = a->name;
		field[1] = a->id;
		field[2] = a->keywords;
		field[3] = cmd;
		for (int k = 0; k < 4; k++) {
			int sc = kb_fuzzy(field[k], needle);

			/* Discounted, never floored to nothing: a hit in the
			 * keywords is a weaker reason than a hit in the name
			 * and is still a reason. */
			if (!sc)
				continue;
			sc = sc > DISCOUNT[k] ? sc - DISCOUNT[k] : 1;
			if (sc > best)
				best = sc;
		}
		if (!best)
			continue;
		hits[n].app = a;
		hits[n].fuzz = best;
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
 * THE ONE PATH A LAUNCH SURFACE TAKES. The rule, and what breaks when a
 * surface writes its own, is in `launch.h`; this is where it lives because the
 * launcher runs the most things and because the application index and the
 * launch must not disagree about what an entry means.
 */
int sh_launch(const struct sh_launch *l, const char *const *files, int nfiles)
{
	/*
	 * The split's scratch: the line itself plus a path for every document
	 * substituted into it. A store that will not hold the result yields no
	 * arguments at all, which is a launch that silently does nothing — so
	 * it is sized for the worst line this can be handed.
	 */
	char store[SH_APP_EXEC * 2 + SH_LAUNCH_FILES * PATH_MAX];
	char id[160];			/* argv points into it until the exec */
	const char *argv[48];
	const int max = (int)(sizeof(argv) / sizeof(*argv));
	int n = 0;

	if (!l || !l->exec || !l->exec[0])
		return -1;
	if (!files || nfiles < 0)
		nfiles = 0;
	if (nfiles > SH_LAUNCH_FILES)
		nfiles = SH_LAUNCH_FILES;

	/*
	 * WHICH DESKTOP THIS IS. $KDOS_CON is the console session's surface
	 * socket, set by the session for everything started inside it, and it
	 * decides how a NON-terminal program is started below. A terminal one
	 * needs no branch here: sh_term_argv_in() names the emulator, from the
	 * entry's own X-KDOS-Term when it asked for one.
	 */
	const char *con = getenv("KDOS_CON");

	if (l->terminal)
		n = sh_term_argv_in(l->term, l->floating, l->size, argv, n,
				    max, l->exec, id, sizeof(id));

	/*
	 * A TYPED LINE KEEPS ITS FIELD CODES AND A DESKTOP ENTRY SPENDS THEM.
	 * `nfiles < 0` is what kxdg_exec_split reads as "expand nothing", so
	 * the `%` somebody typed into the run box reaches the program.
	 */
	int got = kxdg_exec_split(l->exec, files, l->verbatim ? -1 : nfiles,
				  store, sizeof(store), argv + n,
				  max - n - 1 - nfiles);

	if (got <= 0)
		return -1;
	n += got;

	/*
	 * AN ENTRY WITH NO FIELD CODE STILL OPENS THE FILE. Every other
	 * launcher appends the paths in that case, and it is the only way
	 * `Exec=xterm` can be handed one.
	 *
	 * THE DECISION IS READ OFF THE LINE, so the line must be the one the
	 * entry wrote: a caller that hands a pre-stripped Exec looks exactly
	 * like `Exec=xterm` from here and gets its documents appended where
	 * the entry asked for them SUBSTITUTED — `--open=%f` running as
	 * `--open=` with the path as a word of its own. Every surface's copy
	 * of the line keeps its codes for this reason; see launch.h.
	 *
	 * The scan steps TWO bytes past a `%` so that `%%` — a literal percent
	 * — is not read as a code, and stops on a trailing one: `p + 2` there
	 * is a byte past the terminator and not an address this may read.
	 */
	int append = l->verbatim;

	if (!append) {
		append = 1;
		for (const char *p = strchr(l->exec, '%'); p && p[1];
		     p = strchr(p + 2, '%'))
			if (p[1] == 'f' || p[1] == 'F' || p[1] == 'u' ||
			    p[1] == 'U')
				append = 0;
	}
	if (append)
		for (int i = 0; i < nfiles && n < max - 1; i++)
			argv[n++] = files[i];
	argv[n] = NULL;

	/*
	 * A GRAPHICAL APPLICATION ON THE CONSOLE IS THE SESSION'S TO START.
	 * This desktop composites character cells and a Wayland client's
	 * surface is pixels; the session gives the guest a cage — embedded in
	 * a window, or full screen on a terminal of its own — and with it the
	 * display the guest connects to. Forked from here it would have
	 * neither, and a boxed application would exit at once with nothing on
	 * the screen to say why.
	 *
	 * A terminal program is not one of these: it becomes a kdos-term
	 * window above and belongs on this grid.
	 */
	if (con && *con && !l->terminal) {
		const char *what = l->title && l->title[0] ? l->title : argv[0];

		if (kcon_run(con, argv, what, 0) < 0) {
			fprintf(stderr,
				"kdos-shell: cannot start '%s' — the session "
				"has no free terminal to give it\n", what);
			return -1;
		}
		return 0;
	}

	sh_spawn(argv);
	return 0;
}

/*
 * NO SHELL, ever. The Exec line is split by kxdg_exec_split and exec'd
 * directly — every launch surface in this tree keeps that rule and this is the
 * one that runs the most things.
 *
 * IT IS NOT A `strtok(" ")`. An Exec line carries quoting and it carries FIELD
 * CODES, and a whitespace split gets both wrong in a way that reads to a
 * person as "the app does not launch": `mpv --player-operation-mode=pseudo-gui
 * -- %U` is handed a literal `%U` to play and exits at once, `gimp-3.0 %U`
 * opens an error dialog instead of an image, and
 * `"/usr/bin/gsmartcontrol-root"` is exec'd with the quotes still on the path.
 * Measured against the shipped appbox: nine of its ninety-two entries carry
 * one of the two. See kxdg.h.
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
	if (!a || !a->exec[0])
		return;

	/* Record BEFORE the launch: the count is what the next menu open
	 * reads, and a launch that failed still tells you what was asked
	 * for. */
	struct sh_app *m = find_id(a->id);

	if (m) {
		m->uses++;
		m->last = time(NULL);
		usage_save();
	}

	struct sh_launch l = {
		.exec = a->exec,
		.title = a->name,
		.term = a->term,
		.size = a->size,
		.terminal = a->terminal,
		.floating = a->floating,
	};

	sh_launch(&l, files, nfiles);
}

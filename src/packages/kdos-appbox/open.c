/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-appbox open — a file, in whatever opens it
 *
 *     $ kdos-appbox open report.odt        -> libreoffice, in the box
 *     $ kdos-appbox open ~/Pictures        -> mc, in the desktop's terminal
 *
 * WHY THIS IS HERE AND NOT xdg-open. kdos-appbox owns the alien-app table and
 * every launcher in /usr/local/bin is a symlink to it, so it is already the
 * program that knows what "open with GIMP" means on this machine. xdg-open is
 * still installed and is still the right answer for a URL; this is the answer
 * for a PATH, and kdos-desk called it for a release before it existed.
 *
 * The resolution is the freedesktop one and nothing clever:
 *
 *   path -> MIME     /usr/share/mime/globs (shared-mime-info is a host port)
 *   MIME -> entry    mimeapps.list [Default Applications], then
 *                    [Added Associations], then each applications/
 *                    mimeinfo.cache — which is the file kdos-appbox
 *                    genlaunchers already writes for the box's own apps
 *   entry -> argv    the Exec line, field codes substituted
 *
 * NO SHELL ANYWHERE, the same rule the rest of this program keeps: a file name
 * arrives from a desktop and a .desktop Exec arrives from a file anything can
 * write, and a shell in the middle turns either into an injection point.
 * ---------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "kdos-appbox.h"
#include "kxdg.h"

#define MIME_MAX 128
#define CAND_MAX 32

/* A handler for a type: the desktop id somebody wrote in a list, and the
 * entry it resolved to. Both, because the chooser is addressed by id and the
 * Exec line is read out of the file. */
typedef struct {
	char id[192];
	char path[600];
} OpenCand;

/* The "open with" chooser, a kdos-shell front end. Absent on a tree built
 * before it landed, which is why every use of it is guarded. */
#define OPENWITH_PROG "kdos-openwith"

/* ── path -> MIME ──────────────────────────────────────────────────────── */

/*
 * shared-mime-info's `globs`, which is one `type:glob` per line and is the
 * whole of what a launcher needs from that package. The full `globs2` adds
 * weights and case-sensitivity flags; nothing here would use either.
 *
 * The LONGEST matching suffix wins, so `.tar.gz` beats `.gz` — get that
 * backwards and every compressed tarball opens in a decompressor.
 */
static int mime_from_globs(const char *base, char *out, size_t n)
{
	size_t len = 0;
	char *data = kb_read_all("/usr/share/mime/globs", &len);
	size_t best = 0;
	int found = 0;

	if (!data)
		return 0;

	for (char *p = data; *p;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		if (*p == '#' || !*p)
			goto next;

		char *colon = strchr(p, ':');
		if (!colon)
			goto next;
		*colon = '\0';
		const char *type = p, *glob = colon + 1;

		if (glob[0] == '*' && glob[1] == '.') {
			const char *suffix = glob + 1;	/* ".odt" */
			size_t sl = strlen(suffix), bl = strlen(base);
			if (bl > sl && !strcasecmp(base + bl - sl, suffix) &&
			    sl > best) {
				best = sl;
				snprintf(out, n, "%s", type);
				found = 1;
			}
		} else if (!strchr(glob, '*') && !strchr(glob, '?') &&
			   !strcasecmp(glob, base) && best == 0) {
			/* An exact name — `Makefile`, `.bashrc`. Only when no
			 * suffix matched, because a suffix is the more specific
			 * statement of the two. */
			snprintf(out, n, "%s", type);
			found = 1;
		}
next:
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return found;
}

static void mime_for_path(const char *path, char *out, size_t n)
{
	if (kb_is_dir(path)) {
		snprintf(out, n, "inode/directory");
		return;
	}
	if (mime_from_globs(kb_basename(path), out, n))
		return;
	/* Not a failure: it is what every desktop calls a file it cannot
	 * name, and a handler may still claim it. */
	snprintf(out, n, "application/octet-stream");
}

/* ── the search path ───────────────────────────────────────────────────── */

/*
 * XDG_DATA_HOME first, then XDG_DATA_DIRS in order — the same order the
 * launcher and the menu scan, which is what makes a user's own .desktop
 * override the shipped one rather than duplicating it.
 */
static int data_dirs(char out[][512], int max)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int n = 0;

	if (home && *home)
		snprintf(out[n++], 512, "%s", home);
	else
		snprintf(out[n++], 512, "%.480s/.local/share", kb_home_dir());

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *p = dirs; *p && n < max;) {
		const char *sep = strchr(p, ':');
		size_t len = sep ? (size_t)(sep - p) : strlen(p);
		/* A trailing slash is legal in XDG_DATA_DIRS and common in the
		 * wild; leaving it makes every path this builds carry a `//`,
		 * which works and reads like a bug in `--print`. */
		while (len > 1 && p[len - 1] == '/')
			len--;
		if (len && len < 512) {
			memcpy(out[n], p, len);
			out[n][len] = '\0';
			n++;
		}
		if (!sep)
			break;
		p = sep + 1;
	}
	return n;
}

/* `<datadir>/applications/<id>`, first one that exists. */
static int desktop_find(const char *id, char *out, size_t n)
{
	char dirs[16][512];
	int nd = data_dirs(dirs, 16);

	for (int i = 0; i < nd; i++) {
		snprintf(out, n, "%.400s/applications/%.150s", dirs[i], id);
		if (kb_path_exists(out))
			return 1;
	}
	return 0;
}

/*
 * A mimeapps/mimeinfo value is a `;`-separated preference list. Every entry
 * that is actually installed is kept, in the order it was written: a
 * mimeapps.list naming an app that was removed must fall through to the next
 * one rather than fail, which is the state every machine ends up in
 * eventually, and the ones behind it are the "open with" menu.
 */
static int collect_installed(const char *list, OpenCand *c, int *n, int max)
{
	char buf[1024], *save = NULL;
	int seen = 0;
	kb_strlcpy(buf, list, sizeof(buf));
	for (char *p = strtok_r(buf, ";", &save); p && *n < max;
	     p = strtok_r(NULL, ";", &save)) {
		char path[600];
		int dup = 0;
		while (*p == ' ')
			p++;
		if (!*p || !desktop_find(p, path, sizeof(path)))
			continue;
		/* Counted before the de-duplication, because the caller is
		 * asking whether this SECTION named a handler, not how many
		 * new ones it contributed. */
		seen++;
		for (int i = 0; i < *n; i++)
			if (!strcmp(c[i].id, p))
				dup = 1;
		/* The same id is named by mimeapps AND by every cache that
		 * carries it; counting it twice would put a one-handler type in
		 * front of the chooser. */
		if (dup)
			continue;
		kb_strlcpy(c[*n].id, p, sizeof(c[*n].id));
		kb_strlcpy(c[*n].path, path, sizeof(c[*n].path));
		(*n)++;
	}
	return seen;
}

/* How many INSTALLED handlers this section named for this type — which is not
 * how far `n` moved, since the same id can already be in the list. */
static int collect_in(const char *path, const char *section, const char *mime,
		      OpenCand *c, int *n, int max)
{
	KxdgEntry e;
	if (kxdg_load(&e, path, section) != 0)
		return 0;
	const char *v = kxdg_get(&e, mime, NULL);
	int seen = v ? collect_installed(v, c, n, max) : 0;
	kxdg_free(&e);
	return seen;
}

/*
 * Every handler for a type, best first, plus whether the best one is a
 * CONFIGURED DEFAULT — a `[Default Applications]` line somebody wrote — rather
 * than merely the first thing that claimed the type.
 *
 * That distinction is the whole of G7. A default, or a single candidate, means
 * the answer is known and nothing may interrupt: a chooser that appears when
 * the answer is known is a worse desktop, not a better one. Two or more
 * candidates and no default means nobody has decided yet, and picking the
 * first silently is how a machine ends up opening PDFs in an image viewer for
 * a year.
 */
static int handlers_for_mime(const char *mime, OpenCand *c, int max,
			     int *defaulted)
{
	char dirs[16][512], path[600], dpre[80];
	int nd = data_dirs(dirs, 16);
	int n = 0;
	const char *cfg = getenv("XDG_CONFIG_HOME");
	char cfgdir[512];

	*defaulted = 0;

	if (cfg && *cfg)
		snprintf(cfgdir, sizeof(cfgdir), "%.500s", cfg);
	else
		snprintf(cfgdir, sizeof(cfgdir), "%.500s/.config",
			 kb_home_dir());

	/*
	 * THE DESKTOP'S OWN LIST FIRST, at each level, which is what the spec
	 * says and what makes one image open in `timg` on the console and in a
	 * boxed viewer under the compositor without either desktop editing the
	 * other's choices.
	 */
	int have_pre = kb_desktop_prefix(dpre, sizeof(dpre));

	if (have_pre) {
		snprintf(path, sizeof(path), "%.500s/%s-mimeapps.list", cfgdir,
			 dpre);
		collect_in(path, "Default Applications", mime, c, &n, max);
		if (n) {
			*defaulted = 1;
			return n;
		}
	}

	/* The user's declared default beats everything, including a launcher
	 * the box shipped a moment ago. */
	snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfgdir);
	collect_in(path, "Default Applications", mime, c, &n, max);
	if (n) {
		*defaulted = 1;
		return n;
	}
	if (have_pre) {
		snprintf(path, sizeof(path), "%.500s/%s-mimeapps.list", cfgdir,
			 dpre);
		collect_in(path, "Added Associations", mime, c, &n, max);
	}
	snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfgdir);
	collect_in(path, "Added Associations", mime, c, &n, max);

	/* The distro's default counts as a decision too, whatever the user's
	 * additions found: XDG puts Default Applications ahead of Added
	 * Associations at every level, so an association that masked one would
	 * turn a type somebody had decided into a chooser prompt. */
	if (have_pre) {
		snprintf(path, sizeof(path), "/etc/xdg/%s-mimeapps.list", dpre);
		if (collect_in(path, "Default Applications", mime, c, &n, max))
			*defaulted = 1;
	}
	if (collect_in("/etc/xdg/mimeapps.list", "Default Applications",
		       mime, c, &n, max))
		*defaulted = 1;

	/*
	 * Then the caches. `mimeinfo.cache` is the file genlaunchers writes
	 * beside the box's launchers precisely so this lookup can find them —
	 * without it the MimeType lines it carried through from Debian would
	 * never be consulted by anything.
	 */
	for (int i = 0; i < nd; i++) {
		snprintf(path, sizeof(path), "%.500s/applications/mimeinfo.cache",
			 dirs[i]);
		collect_in(path, "MIME Cache", mime, c, &n, max);
	}
	return n;
}

/* ── entry -> argv ─────────────────────────────────────────────────────── */

/*
 * Substitute the field codes rather than stripping them, which is the whole
 * difference between a launcher and an opener: `%f` IS the file, and dropping
 * it opens the application with an empty document.
 *
 * %f/%u take one path, %F/%U the whole list. %i, %c and %k are dropped — an
 * icon, a name and the entry's own path, none of which this has to supply.
 * `%%` is a literal percent.
 */
static void exec_to_argv(const char *exec, char *const *files, int nfiles,
			 int terminal, KbArgv *a)
{
	/*
	 * STATIC, and that is not laziness: kb_argv_add stores the POINTER it
	 * is given and does not copy, so every word handed to it has to
	 * outlive the exec. A stack buffer here would be freed at the return
	 * and the argv would be `foot -e ' ;'`-shaped garbage — the same
	 * defect CLAUDE.md already records against kdosbuild. `open` is a
	 * one-shot command, so one buffer is all there ever is.
	 */
	static char buf[1024];
	kb_strlcpy(buf, exec, sizeof(buf));

	if (terminal) {
		/* THE TERMINAL FOLLOWS THE DESKTOP. `foot` needs a compositor,
		 * so a console session that wrapped an entry in it would pick
		 * the right program and then fail to open a window for it. */
		kb_argv_add(a, kb_terminal());
		kb_argv_add(a, "-e");
	}

	for (char *w = strtok(buf, " \t"); w; w = strtok(NULL, " \t")) {
		if (w[0] != '%' || !w[1] || w[2]) {
			kb_argv_add(a, w);
			continue;
		}
		switch (w[1]) {
		case 'f':
		case 'u':
			if (nfiles > 0)
				kb_argv_add(a, files[0]);
			break;
		case 'F':
		case 'U':
			for (int i = 0; i < nfiles; i++)
				kb_argv_add(a, files[i]);
			break;
		case '%':
			kb_argv_add(a, "%");
			break;
		default:
			break;		/* %i %c %k and anything new */
		}
	}
}

/* ── the command ───────────────────────────────────────────────────────── */

/*
 * Hand the whole question to the chooser. It reads the same chain this file
 * does, so there is nothing to pass but the files — a candidate list on the
 * command line would be a second copy of the resolution, going stale the
 * moment an app is installed.
 *
 * exec, for the same reason the launch below does: `open` is one shot and its
 * caller already decided who owns the process.
 */
static int run_openwith(int argc, char **argv)
{
	KbArgv a = {0};
	kb_argv_add(&a, OPENWITH_PROG);
	for (int i = 0; i < argc; i++)
		kb_argv_add(&a, argv[i]);
	kb_argv_end(&a);
	execvp(a.v[0], (char *const *)a.v);
	kb_die("cannot run %s", OPENWITH_PROG);
	return 127;
}

int cmd_open(int argc, char **argv)
{
	OpenCand cand[CAND_MAX];
	char mime[MIME_MAX];
	const char *entry;
	int print = 0, choose = 0, defaulted = 0, ncand;

	/*
	 * `--print` resolves and prints instead of executing. Every other
	 * decision-making program here has one — kinstall's --dump, kdosbuild's
	 * --preview, kdos-shell's --dump — and for the same reason: the answer
	 * is worth checking without launching LibreOffice to find out what it
	 * was. It is also what testing/selftest.sh can assert on.
	 *
	 * `--choose` forces the chooser whatever the resolution says, which is
	 * what a desktop's "Open with…" menu item means.
	 */
	for (; argc > 0 && argv[0][0] == '-'; argc--, argv++) {
		if (!strcmp(argv[0], "--print"))
			print = 1;
		else if (!strcmp(argv[0], "--choose"))
			choose = 1;
		else
			break;
	}
	if (argc < 1)
		kb_die("usage: kdos-appbox open [--print] [--choose] "
		       "<path> [path...]");

	/* One MIME type for the set, taken from the first: a handler is chosen
	 * once and handed every file, which is what %F means. Mixed types are
	 * the caller's business. */
	mime_for_path(argv[0], mime, sizeof(mime));

	if (print)
		printf("mime\t%s\n", mime);

	ncand = handlers_for_mime(mime, cand, CAND_MAX, &defaulted);

	if (print && ncand) {
		printf("candidates");
		for (int i = 0; i < ncand; i++)
			printf("\t%s", cand[i].id);
		printf("\ndefault\t%s\n", defaulted ? "yes" : "no");
	}

	/*
	 * MORE THAN ONE ANSWER AND NOBODY HAS CHOSEN — ask. Exactly one
	 * candidate, or a configured default, keeps today's behaviour and opens
	 * the file: the dialog exists for the case where the machine genuinely
	 * does not know, and for no other.
	 *
	 * A tree without the chooser falls through to the first candidate
	 * rather than failing. Losing a dialog is a worse desktop; losing the
	 * click entirely is a broken one.
	 */
	if ((choose || (ncand > 1 && !defaulted)) && kb_have_prog(OPENWITH_PROG)) {
		/* Under --print the resolution below is still printed: the
		 * `choose` line says the chooser would run, and what follows is
		 * what a tree without it would do. Both are worth knowing and
		 * one of them is what actually happens. */
		if (print) {
			printf("choose\t%s\n", OPENWITH_PROG);
		} else {
			tracef("open: %d handlers for %s and no default, asking",
			       ncand, mime);
			return run_openwith(argc, argv);
		}
	}

	if (!ncand) {
		if (print) {
			printf("entry\t-\nexec\txdg-open\n");
			return 0;
		}
		/*
		 * Nothing claims it. xdg-open is the last resort rather than
		 * an error, because it knows about URL schemes and about the
		 * user's own additions to files this does not read.
		 */
		tracef("open: no handler for %s, falling back to xdg-open",
		       mime);
		if (!kb_have_prog("xdg-open"))
			kb_die("nothing on this machine opens %s", mime);
		KbArgv a = {0};
		kb_argv_add(&a, "xdg-open");
		for (int i = 0; i < argc; i++)
			kb_argv_add(&a, argv[i]);
		kb_argv_end(&a);
		return kb_run(&a);
	}

	entry = cand[0].path;

	KxdgEntry e;
	if (kxdg_load(&e, entry, "Desktop Entry") != 0)
		kb_die("%s is not a desktop entry", entry);
	const char *exec = kxdg_get(&e, "Exec", NULL);
	if (!exec || !*exec)
		kb_die("%s has no Exec line", entry);

	KbArgv a = {0};
	exec_to_argv(exec, argv, argc, kxdg_bool(&e, "Terminal", 0), &a);
	kb_argv_end(&a);
	if (!a.v[0])
		kb_die("%s has an empty Exec line", entry);

	if (print) {
		printf("entry\t%s\nexec", entry);
		for (int i = 0; a.v[i]; i++)
			printf("\t%s", a.v[i]);
		putchar('\n');
		return 0;
	}

	tracef("open: %s -> %s", mime, kb_basename(entry));

	/*
	 * RECORDED HERE AND NOWHERE ELSE. Every open on this desktop passes
	 * through this function — the desktop's icons, the file chooser, a
	 * menu, `xdg-open` — so this is the one place a recent-files store can
	 * be written without a second copy of the rule going stale beside it.
	 *
	 * BEFORE the exec, which never returns. Only an absolute path is
	 * recorded: a URL has no file to go back to, and a relative one means
	 * nothing to whatever reads the store next.
	 *
	 * A failed write is ignored on purpose. A convenience list is not a
	 * reason to refuse to open a file.
	 */
	{
		/* The ENTRY'S STEM, not its filename: the store's convention
		 * is the program's own name, and `kxdg_recent("nvim", …)` is
		 * what a jump list asks with. */
		char who[128], *dot;

		kb_strlcpy(who, kb_basename(entry), sizeof(who));
		dot = strrchr(who, '.');
		if (dot && !strcmp(dot, ".desktop"))
			*dot = '\0';
		for (int i = 0; i < argc; i++)
			if (argv[i][0] == '/')
				kxdg_recent_add(who, argv[i], mime);
	}

	/*
	 * exec, not fork: `open` is a one-shot command and the caller — a
	 * double-forked kdos-desk, a menu, a shell — already decided what
	 * should own the process. Wrapping it in another fork would only add
	 * a pid nobody reaps.
	 */
	execvp(a.v[0], (char *const *)a.v);
	kb_die("cannot run %s", a.v[0]);
	return 127;
}

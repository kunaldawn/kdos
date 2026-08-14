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
 *     $ kdos-appbox open ~/Pictures        -> mc, in foot
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
 * A mimeapps/mimeinfo value is a `;`-separated preference list. Take the
 * first entry that is actually installed: a mimeapps.list naming an app that
 * was removed must fall through to the next one rather than fail, which is
 * the state every machine ends up in eventually.
 */
static int first_installed(const char *list, char *out, size_t n)
{
	char buf[1024];
	kb_strlcpy(buf, list, sizeof(buf));
	for (char *p = strtok(buf, ";"); p; p = strtok(NULL, ";")) {
		while (*p == ' ')
			p++;
		if (*p && desktop_find(p, out, n))
			return 1;
	}
	return 0;
}

static int lookup_in(const char *path, const char *section, const char *mime,
		     char *out, size_t n)
{
	KxdgEntry e;
	if (kxdg_load(&e, path, section) != 0)
		return 0;
	const char *v = kxdg_get(&e, mime, NULL);
	int ok = v && first_installed(v, out, n);
	kxdg_free(&e);
	return ok;
}

static int handler_for_mime(const char *mime, char *out, size_t n)
{
	char dirs[16][512], path[600];
	int nd = data_dirs(dirs, 16);
	const char *cfg = getenv("XDG_CONFIG_HOME");

	/* The user's declared default beats everything, including a launcher
	 * the box shipped a moment ago. */
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfg);
	else
		snprintf(path, sizeof(path), "%.500s/.config/mimeapps.list",
			 kb_home_dir());
	if (lookup_in(path, "Default Applications", mime, out, n))
		return 1;
	if (lookup_in(path, "Added Associations", mime, out, n))
		return 1;
	if (lookup_in("/etc/xdg/mimeapps.list", "Default Applications", mime,
		      out, n))
		return 1;

	/*
	 * Then the caches. `mimeinfo.cache` is the file genlaunchers writes
	 * beside the box's launchers precisely so this lookup can find them —
	 * without it the MimeType lines it carried through from Debian would
	 * never be consulted by anything.
	 */
	for (int i = 0; i < nd; i++) {
		snprintf(path, sizeof(path), "%.500s/applications/mimeinfo.cache",
			 dirs[i]);
		if (lookup_in(path, "MIME Cache", mime, out, n))
			return 1;
	}
	return 0;
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
		kb_argv_add(a, "foot");
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

int cmd_open(int argc, char **argv)
{
	char mime[MIME_MAX], entry[600];
	int print = 0;

	/*
	 * `--print` resolves and prints instead of executing. Every other
	 * decision-making program here has one — kinstall's --dump, kdosbuild's
	 * --preview, kdos-shell's --dump — and for the same reason: the answer
	 * is worth checking without launching LibreOffice to find out what it
	 * was. It is also what testing/selftest.sh can assert on.
	 */
	if (argc > 0 && !strcmp(argv[0], "--print")) {
		print = 1;
		argc--;
		argv++;
	}
	if (argc < 1)
		kb_die("usage: kdos-appbox open [--print] <path> [path...]");

	/* One MIME type for the set, taken from the first: a handler is chosen
	 * once and handed every file, which is what %F means. Mixed types are
	 * the caller's business. */
	mime_for_path(argv[0], mime, sizeof(mime));

	if (print)
		printf("mime\t%s\n", mime);

	if (!handler_for_mime(mime, entry, sizeof(entry))) {
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
	 * exec, not fork: `open` is a one-shot command and the caller — a
	 * double-forked kdos-desk, a menu, a shell — already decided what
	 * should own the process. Wrapping it in another fork would only add
	 * a pid nobody reaps.
	 */
	execvp(a.v[0], (char *const *)a.v);
	kb_die("cannot run %s", a.v[0]);
	return 127;
}

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
 * THIS IS xdg-open ON THIS MACHINE. /usr/local/bin/xdg-open is a name on this
 * binary and /usr/local/bin comes first on the shipped $PATH, so everything
 * that opens a link or a file by that word arrives here. kdos-appbox owns the
 * alien-app table and every launcher in /usr/local/bin is a symlink to it, so
 * it is already the program that knows what "open with GIMP" means. xdg-utils'
 * own script stays installed and is the last resort, reached BY ABSOLUTE PATH
 * because naming it otherwise would find this binary again.
 *
 * The resolution is the freedesktop one and nothing clever:
 *
 *   argument -> MIME kxdg_mime_for_arg: a scheme is x-scheme-handler/<scheme>,
 *                    `file:` names a path, anything else IS a path
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "kdos-appbox.h"
#include "kxdg.h"

#define MIME_MAX 128
#define CAND_MAX 32
/*
 * HOW MANY DOCUMENTS ONE OPEN CARRIES. The split writes the substituted paths
 * into a fixed store, and a store that will not hold them yields NO arguments
 * and opens nothing — so the count is a cap the caller's files are clamped to
 * rather than a promise.
 */
#define OPEN_MAX_FILES 4

/*
 * THE LAST RESORT, BY ABSOLUTE PATH. `/usr/local/bin/xdg-open` is a second
 * name on THIS binary and `/usr/local/bin` comes first on the shipped $PATH,
 * so naming it here without a path would re-enter this function until the
 * process died.
 */
#define XDG_OPEN_PROG "/usr/bin/xdg-open"

/*
 * THE "OPEN WITH" CHOOSER IS THE PORTAL'S. OpenURI with `ask` set puts up the
 * desktop's own application chooser — xdg-desktop-portal-kde's — and launches
 * what is picked through its desktop entry, which for a boxed application is
 * the generated launcher, so the chooser lists host and boxed handlers alike.
 * This process is not sandboxed, so the portal takes a `file://` URI from it.
 */
#define PORTAL_DEST   "org.freedesktop.portal.Desktop"
#define PORTAL_PATH   "/org/freedesktop/portal/desktop"
#define PORTAL_METHOD "org.freedesktop.portal.OpenURI.OpenURI"

/* ── path -> MIME ──────────────────────────────────────────────────────── */

/* ── what an argument is ───────────────────────────────────────────────── */

/*
 * libkxdg ANSWERS THIS, and this binary already compiles it. Do not resolve
 * here by basename alone: `mailto:a@b.c` has a basename that matches the `*.c`
 * glob, so a URL would be offered to an editor. kxdg_mime_for_arg tells a URL
 * from a path, and hands back the path a `file:` URL names, which is what the
 * handler must be given.
 */

/* ── MIME -> entry ─────────────────────────────────────────────────────── */

/*
 * kxdg_handlers ANSWERS THIS, from the same mimeapps.list chain KDE's own
 * resolution reads, so the handler the Default Applications page shows is the
 * one this starts. `defaulted`
 * decides whether the chooser appears: a default, or a single candidate,
 * means the answer is known and nothing may interrupt; two or more candidates
 * and no default means nobody has decided yet, and picking the first silently
 * is how a machine ends up opening PDFs in an image viewer for a year.
 */

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
/*
 * THE SPLIT IS kxdg_exec_split'S, NOT A `strtok(" ")`.
 *
 * An Exec line carries quoting as well as field codes, and the two are the
 * format: `Exec=konsole -p tabtitle="Install KDOS" -e sudo kinstall` split on
 * whitespace reaches konsole as `tabtitle="Install` with a stray `KDOS"`,
 * and `Exec="/usr/bin/gsmartcontrol-root"` execs a file whose name begins with
 * a quote. A code is also not always a word of its own — `--open=%f` is one
 * argument — which a per-word switch cannot express at all.
 */
static void exec_to_argv(const char *exec, char *const *files, int nfiles,
			 int terminal, KbArgv *a)
{
	/*
	 * STATIC, and that is not laziness: kb_argv_add stores the POINTER it
	 * is given and does not copy, so every word handed to it has to
	 * outlive the exec. A stack buffer here would be freed at the return
	 * and the argv would be `konsole -e ' ;'`-shaped garbage. `open` is a
	 * one-shot command, so one buffer is all there ever is.
	 *
	 * The store is sized for the line plus a path per document, because a
	 * split that will not fit yields NO arguments — which is an open that
	 * silently does nothing.
	 */
	static char buf[1024 + OPEN_MAX_FILES * PATH_MAX];
	const char *split[KB_MAX_ARGV];
	int n;

	if (nfiles > OPEN_MAX_FILES)
		nfiles = OPEN_MAX_FILES;

	if (terminal) {
		/* THE TERMINAL FOLLOWS THE DESKTOP — Konsole, which draws sixel,
		 * iTerm2 and kitty pictures, so a picture-showing terminal
		 * program needs nothing of its entry but `Terminal=true`.
		 *
		 * AND A BARE VIRTUAL TERMINAL HAS NONE, so the entry runs
		 * where this process already is: `Ctrl+Alt+F2` is a terminal,
		 * and wrapping a terminal program in an emulator that cannot
		 * start is the one outcome worse than not wrapping it. */
		const char *term = kb_terminal();

		if (term) {
			kb_argv_add(a, term);
			kb_argv_add(a, "-e");
		}
	}

	n = kxdg_exec_split(exec, (const char *const *)files, nfiles, buf,
			    sizeof(buf), split,
			    (int)(sizeof(split) / sizeof(*split)));
	for (int i = 0; i < n; i++)
		kb_argv_add(a, split[i]);

	/*
	 * AN ENTRY WITH NO FIELD CODE STILL OPENS THE FILE. Every launcher
	 * appends the paths in that case and it is the only way `Exec=xterm`
	 * can be handed one — the same decision KIO's launcher makes, read off
	 * the same line, so the opener and the Start menu cannot disagree about
	 * where a document goes.
	 *
	 * The scan steps TWO bytes past a `%` so a literal `%%` is not read as
	 * a code, and stops on a trailing one.
	 */
	if (n > 0) {
		int append = 1;

		for (const char *p = strchr(exec, '%'); p && p[1];
		     p = strchr(p + 2, '%'))
			if (p[1] == 'f' || p[1] == 'F' || p[1] == 'u' ||
			    p[1] == 'U')
				append = 0;
		if (append)
			for (int i = 0; i < nfiles; i++)
				kb_argv_add(a, files[i]);
	}
}

/* ── the command ───────────────────────────────────────────────────────── */

/*
 * Hand the whole question to the portal's chooser. It reads the same
 * associations this file does, so there is nothing to pass but the files — a
 * candidate list would be a second copy of the resolution, going stale the
 * moment an app is installed. One request per file, because OpenURI takes one.
 * 0 when every request was accepted; the caller falls back to the first
 * candidate otherwise, because losing a dialog is a worse desktop and losing
 * the click a broken one.
 */
static int portal_choose(int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		char m[MIME_MAX], uri[PATH_MAX * 3 + 16], out[256];
		const char *arg = kxdg_mime_for_arg(argv[i], m, sizeof(m));
		KbArgv a = {0};

		if (arg[0] == '/')
			kb_uri_file(arg, uri, sizeof(uri));
		else
			kb_strlcpy(uri, arg, sizeof(uri));
		kb_argv_add(&a, "gdbus");
		kb_argv_add(&a, "call");
		kb_argv_add(&a, "--session");
		kb_argv_add(&a, "--timeout");
		kb_argv_add(&a, "5");
		kb_argv_add(&a, "--dest");
		kb_argv_add(&a, PORTAL_DEST);
		kb_argv_add(&a, "--object-path");
		kb_argv_add(&a, PORTAL_PATH);
		kb_argv_add(&a, "--method");
		kb_argv_add(&a, PORTAL_METHOD);
		kb_argv_add(&a, "");
		kb_argv_add(&a, uri);
		kb_argv_add(&a, "{'ask': <true>}");
		kb_argv_end(&a);
		if (kb_run_capture(&a, out, sizeof(out)) != 0)
			return -1;
	}
	return 0;
}

int cmd_open(int argc, char **argv)
{
	KxdgHandler cand[CAND_MAX];
	char mime[MIME_MAX];
	const char *entry;
	int print = 0, choose = 0, defaulted = 0, ncand;

	/*
	 * `--print` resolves and prints instead of executing. Every other
	 * decision-making program here has one — kinstall's --dump, kdosbuild's
	 * --preview — and for the same reason: the answer
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
	/*
	 * TYPED HERE, UNWRAPPED LATER. The arguments are left alone until a
	 * handler has been chosen, because the xdg-open fallback below must
	 * receive what the caller typed: that script decodes the percent
	 * escapes in a `file:///` URL itself, and handing it the raw path
	 * instead loses every name with a space in it.
	 */
	kxdg_mime_for_arg(argv[0], mime, sizeof(mime));

	if (print)
		printf("mime\t%s\n", mime);

	ncand = kxdg_handlers(mime, cand, CAND_MAX, &defaulted);

	if (print && ncand) {
		printf("candidates");
		for (int i = 0; i < ncand; i++)
			printf("\t%s", cand[i].id);
		printf("\ndefault\t%s\n", defaulted ? "yes" : "no");
	}

	/*
	 * MORE THAN ONE ANSWER AND NOBODY HAS CHOSEN — ask. Exactly one
	 * candidate, or a configured default, opens the file with no question:
	 * the dialog exists for the case where the machine genuinely does not
	 * know, and for no other.
	 *
	 * A tree without the chooser falls through to the first candidate
	 * rather than failing. Losing a dialog is a worse desktop; losing the
	 * click entirely is a broken one.
	 */
	if ((choose || (ncand > 1 && !defaulted)) && kb_have_prog("gdbus")) {
		/* Under --print the resolution below is still printed: the
		 * `choose` line says the chooser would run, and what follows is
		 * what happens when the portal does not answer. Both are worth
		 * knowing and one of them is what actually happens. */
		if (print) {
			printf("choose\tportal\n");
		} else {
			tracef("open: %d handlers for %s and no default, asking",
			       ncand, mime);
			if (portal_choose(argc, argv) == 0)
				return 0;
			tracef("open: the portal did not answer, taking the first");
		}
	}

	if (!ncand) {
		if (print) {
			printf("entry\t-\nexec\t%s\n", XDG_OPEN_PROG);
			return 0;
		}
		/*
		 * Nothing claims it. xdg-open is the last resort rather than
		 * an error, because it knows about URL schemes and about the
		 * user's own additions to files this does not read.
		 */
		tracef("open: no handler for %s, falling back to xdg-open",
		       mime);
		if (access(XDG_OPEN_PROG, X_OK) != 0)
			kb_die("nothing on this machine opens %s", mime);
		KbArgv a = {0};
		kb_argv_add(&a, XDG_OPEN_PROG);
		for (int i = 0; i < argc; i++)
			kb_argv_add(&a, argv[i]);
		kb_argv_end(&a);
		return kb_run(&a);
	}

	entry = cand[0].path;

	/*
	 * NOW the arguments are unwrapped, and in place. kxdg_mime_for_arg
	 * returns the path a `file:` URL names, or the argument itself — a
	 * pointer that outlives this call and is never written through, so the
	 * handler is given what the argument meant with nothing copied and
	 * nothing truncated on the way.
	 */
	for (int i = 0; i < argc; i++) {
		char m[MIME_MAX];

		argv[i] = (char *)kxdg_mime_for_arg(argv[i], m, sizeof(m));
	}

	KxdgEntry e;
	KxdgLaunch kl;

	if (kxdg_load(&e, entry, "Desktop Entry") != 0)
		kb_die("%s is not a desktop entry", entry);
	/* libkxdg's one reader, so every KDOS program that starts an entry
	 * comes to the same conclusion about it. See kxdg.h. */
	if (kxdg_launch_read(&e, &kl) != 0)
		kb_die("%s has no Exec line", entry);

	KbArgv a = {0};

	exec_to_argv(kl.exec, argv, argc, kl.terminal, &a);
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
	 * RECORDED HERE, for the opens that pass through this function —
	 * `xdg-open` from a terminal, a box's link handed to the host, a
	 * program reading $BROWSER — which KIO's own launcher, recording the
	 * opens made from Dolphin and the Start menu into the same store,
	 * never sees.
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
	 * portal, a menu, a shell — already decided what
	 * should own the process. Wrapping it in another fork would only add
	 * a pid nobody reaps.
	 */
	execvp(a.v[0], (char *const *)a.v);
	kb_die("cannot run %s", a.v[0]);
	return 127;
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos theme --audit — is what is on this machine what the palette says?
 *
 * KDOS claims that every colour on the system comes from one table:
 * `KCOL_SCHEMES` in libkcolor. Eleven thousand icons, 32 cursor shapes, six
 * stylesheets, a terminal theme, a system-monitor theme and a prompt palette are
 * generated from it — and generated artwork that nobody re-checks is a claim,
 * not a fact. A hand-edited gtk.css, a half-finished accent switch, an icon
 * theme left behind by an interrupted `kdos theme`, a package upgrade that
 * reinstalled the phosphor build over an amber session: every one of those is
 * silent, and every one of them makes the claim false.
 *
 * THE AUDIT IS A REGENERATION, NOT AN INSPECTION. It does not try to recognise
 * "palette colours" in the installed files — that test would have to know which
 * mixes are legal and would drift away from the generators the moment one of
 * them changed. Instead it runs the SAME generators, with `$HOME` and the XDG
 * variables pointed at a temporary directory, and compares the result byte for
 * byte with what is installed. Anything that differs, differs from what this
 * machine's own palette produces right now, whatever the reason.
 *
 * That also makes it exact rather than approximate: the generators were verified
 * byte-identical against the python ones they replaced, so "identical" is a
 * meaningful word here, and the comparison covers symlinks (the icon theme is
 * 12 462 of them) as well as file contents.
 *
 * Nothing is written outside the temporary directory and nothing is signalled:
 * an audit that repaired what it found would be a `kdos theme` with a
 * misleading name.
 * ---------------------------------
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kdos-tools.h"

/* ── comparing two trees ───────────────────────────────────────────────── */

typedef struct {
	long same;
	long differ;		/* present in both, different content or target */
	long missing;		/* the generator produced it, the machine has not */
	long extra;		/* the machine has it, the generator does not    */
} AuditCount;

static int same_link(const char *a, const char *b)
{
	char ta[1024], tb[1024];
	ssize_t na = readlink(a, ta, sizeof(ta) - 1);
	ssize_t nb = readlink(b, tb, sizeof(tb) - 1);
	if (na < 0 || nb < 0)
		return 0;
	ta[na] = tb[nb] = '\0';
	return !strcmp(ta, tb);
}

static int same_file(const char *a, const char *b)
{
	size_t na = 0, nb = 0;
	char *da = kb_read_all(a, &na);
	char *db = kb_read_all(b, &nb);
	int eq = da && db && na == nb && !memcmp(da, db, na);
	free(da);
	free(db);
	return eq;
}

/*
 * `want` is what the generator just produced, `have` is what is installed.
 *
 * A symlink is compared as a symlink and never followed: the icon theme is
 * mostly aliases, and following them would compare the same few hundred real
 * files over and over while missing an alias pointing somewhere new.
 */
static void walk(const char *want, const char *have, AuditCount *c)
{
	struct stat sw, sh;
	int okw = lstat(want, &sw) == 0;
	int okh = lstat(have, &sh) == 0;

	if (!okw && !okh)
		return;
	if (!okh) {
		c->missing++;
		return;
	}
	if (!okw) {
		c->extra++;
		return;
	}

	if (S_ISLNK(sw.st_mode) || S_ISLNK(sh.st_mode)) {
		if (S_ISLNK(sw.st_mode) && S_ISLNK(sh.st_mode) &&
		    same_link(want, have))
			c->same++;
		else
			c->differ++;
		return;
	}

	if (S_ISDIR(sw.st_mode) != S_ISDIR(sh.st_mode)) {
		c->differ++;
		return;
	}

	if (!S_ISDIR(sw.st_mode)) {
		if (same_file(want, have))
			c->same++;
		else
			c->differ++;
		return;
	}

	/* Both directories: the union of their entries, so a file the machine has
	 * and the generator does not is counted rather than passed over. Both
	 * listings are sorted (kb_listdir sorts), so the merge is a walk. */
	int nw = 0, nh = 0;
	char **lw = kb_listdir(want, &nw);
	char **lh = kb_listdir(have, &nh);
	int i = 0, j = 0;
	while ((lw && i < nw) || (lh && j < nh)) {
		const char *a = (lw && i < nw) ? lw[i] : NULL;
		const char *b = (lh && j < nh) ? lh[j] : NULL;
		int cmp = !a ? 1 : !b ? -1 : strcmp(a, b);
		const char *name = cmp <= 0 ? a : b;
		char *pw = kb_path_join(want, name);
		char *ph = kb_path_join(have, name);
		walk(pw, ph, c);
		free(pw);
		free(ph);
		if (cmp <= 0)
			i++;
		if (cmp >= 0)
			j++;
	}
	kb_strv_free(lw);
	kb_strv_free(lh);
}

/* ── the artefacts ─────────────────────────────────────────────────────── */

enum { A_HOME, A_CONFIG, A_DATA };

static const struct {
	int base;
	const char *rel;
	const char *label;
} ARTEFACTS[] = {
	{ A_HOME,   ".themes/KDOS",             "GTK stylesheet"     },
	{ A_HOME,   ".icons/KDOS",              "icon theme"         },
	{ A_HOME,   ".icons/KDOS-cursors",      "cursor theme"       },
	{ A_CONFIG, "gtk-3.0/gtk.css",          "GTK3 palette"       },
	{ A_CONFIG, "gtk-4.0/gtk.css",          "GTK4 palette"       },
	{ A_CONFIG, "foot/themes/kdos",         "foot"               },
	{ A_CONFIG, "btop/themes/kdos.theme",   "btop"               },
	{ A_CONFIG, "starship.toml",            "starship palette"   },
	{ A_CONFIG, "kdeglobals",               "KDE palette"        },
	{ A_DATA,   "color-schemes/KDOS.colors", "KDE colour scheme" },
};
#define NARTEFACTS ((int)(sizeof(ARTEFACTS) / sizeof(ARTEFACTS[0])))

static char *live_path(int base, const char *rel)
{
	if (base == A_CONFIG)
		return kdt_cfg_home(rel);
	if (base == A_DATA)
		return kdt_data_home(rel);
	return kb_path_join(kb_home_dir(), rel);
}

static char *tmp_path(const char *root, int base, const char *rel)
{
	const char *sub = base == A_CONFIG ? ".config"
			  : base == A_DATA   ? ".local/share"
					     : NULL;
	if (sub) {
		char *dir = kb_path_join(root, sub);
		char *p = kb_path_join(dir, rel);
		free(dir);
		return p;
	}
	return kb_path_join(root, rel);
}

/* ── the run ───────────────────────────────────────────────────────────── */

/*
 * Regenerate into `root`, in a CHILD process.
 *
 * The redirection is four environment variables, which is exactly how
 * `06_packaging/00_theme.sh` generates /etc/skel — so this is the same trick the
 * build already relies on rather than a second way of doing it. It happens in a
 * child so the parent's environment, and anything that read it earlier, is
 * untouched.
 */
static int regenerate(const char *root, const KcolScheme *sc,
		      void (*apply)(const KcolScheme *))
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		char *cfg = kb_path_join(root, ".config");
		char *cache = kb_path_join(root, ".cache");
		char *data = kb_path_join(root, ".local/share");
		setenv("HOME", root, 1);
		setenv("XDG_CONFIG_HOME", cfg, 1);
		setenv("XDG_CACHE_HOME", cache, 1);
		setenv("XDG_DATA_HOME", data, 1);
		kb_mkdir_p(cfg);
		kb_mkdir_p(cache);
		kb_mkdir_p(data);
		/* The generators are chatty on their own account; an audit is not
		 * the place for it. Errors still reach stderr. */
		if (!freopen("/dev/null", "w", stdout))
			_exit(1);
		apply(sc);
		_exit(0);
	}
	int st = 0;
	if (waitpid(pid, &st, 0) < 0)
		return -1;
	return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

int kdt_theme_audit(const KcolScheme *sc, void (*apply)(const KcolScheme *),
		    const char *tty_accent, const char *tty_reset)
{
	const char *tmpdir = getenv("TMPDIR");
	if (!tmpdir || !*tmpdir)
		tmpdir = "/tmp";
	char root[512];
	snprintf(root, sizeof(root), "%s/kdos-theme-audit.XXXXXX", tmpdir);
	if (!mkdtemp(root)) {
		fprintf(stderr, "kdos: cannot create a scratch directory in %s\n",
			tmpdir);
		return 2;
	}

	/*
	 * Two artefacts are EDITED rather than written, and both have to be
	 * copied in before the generators run or the comparison asks the wrong
	 * question.
	 *
	 *   starship.toml   only the block between the markers is ours
	 *   kdeglobals      KDE apps write their own settings into it, so the
	 *                   generator merges; a user's [Dolphin] section is not
	 *                   drift, and regenerating into an EMPTY home would call
	 *                   it drift on every machine that has ever run dolphin
	 *
	 * A machine with neither file produces neither on either side and is
	 * reported as absent, which is also correct.
	 */
	char *cfg = kb_path_join(root, ".config");
	kb_mkdir_p(cfg);
	static const char *const CARRIED[] = { "starship.toml", "kdeglobals" };
	for (size_t i = 0; i < sizeof(CARRIED) / sizeof(CARRIED[0]); i++) {
		char *live = kdt_cfg_home(CARRIED[i]);
		char *tmp = kb_path_join(cfg, CARRIED[i]);
		kb_copy_file(live, tmp);	/* absent is fine */
		free(live);
		free(tmp);
	}
	free(cfg);

	printf("%s==> theme audit — %s%s\n", tty_accent, sc->name, tty_reset);
	printf("    regenerating into a scratch directory and comparing; this "
	       "reads nothing but files\n\n");
	fflush(stdout);

	if (regenerate(root, sc, apply) != 0) {
		fprintf(stderr, "kdos: the generators did not finish\n");
		kb_rmtree(root);
		return 2;
	}

	int bad = 0;
	for (int i = 0; i < NARTEFACTS; i++) {
		char *have = live_path(ARTEFACTS[i].base, ARTEFACTS[i].rel);
		char *want = tmp_path(root, ARTEFACTS[i].base, ARTEFACTS[i].rel);

		AuditCount c = {0};
		walk(want, have, &c);

		const char *verdict;
		if (!c.same && !c.differ && !c.missing && !c.extra)
			verdict = "not generated on this machine";
		else if (c.differ || c.missing || c.extra)
			verdict = NULL;			/* printed below */
		else
			verdict = "matches the palette";

		printf("  %-18s %6ld file%s  ", ARTEFACTS[i].label,
		       c.same + c.differ + c.missing + c.extra,
		       (c.same + c.differ + c.missing + c.extra) == 1 ? " " : "s");
		if (verdict) {
			printf("%s\n", verdict);
		} else {
			bad = 1;
			printf("%sDRIFTED%s —", tty_accent, tty_reset);
			if (c.differ)
				printf(" %ld differ", c.differ);
			if (c.missing)
				printf(" %ld missing", c.missing);
			if (c.extra)
				printf(" %ld not ours", c.extra);
			printf("\n");
		}
		free(have);
		free(want);
	}

	kb_rmtree(root);

	printf("\n");
	if (bad) {
		printf("  Something on this machine is not what the palette "
		       "generates.\n"
		       "  `kdos theme %s` rewrites all of it.\n", sc->name);
		return 1;
	}
	printf("  Every generated artefact is byte-identical to what "
	       "KCOL_SCHEMES produces.\n");
	/* The system copies under /usr/share are built at package time and are
	 * always the phosphor build; the home copies are what a session and the
	 * appbox actually read, which is why they are what is audited. Said out
	 * loud so a green audit is not mistaken for a claim about /usr/share. */
	printf("  (/usr/share/icons and /usr/share/kdos hold the phosphor build "
	       "from packaging;\n   $HOME is what the session and the appbox "
	       "read, and is what was checked.)\n");
	return 0;
}

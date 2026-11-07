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
 * `KCOL_SCHEMES` in libkcolor. Plasma's colour schemes, the Konsole schemes
 * and profiles, the qt5ct palettes and the global themes are generated from it
 * when the image is built, and the terminal programs' files in a home are
 * generated from it by `kdos theme` — and generated artwork that nobody
 * re-checks is a claim, not a fact. So the audit asks three things.
 *
 * THE INSTALLED SCHEMES STILL MEET THE CONTRAST FLOORS. `kdos-theme contrast`
 * is the gate the build itself ran; run again here it catches a scheme file
 * edited or replaced after the image was made.
 *
 * THE ACCENT'S INSTALLED FILES ARE ALL THERE, found the way KDE finds them:
 * along $XDG_DATA_DIRS. A global theme whose colour scheme is missing applies
 * Breeze's colours under the KDOS name, which looks like a broken accent
 * rather than a missing file.
 *
 * THE HOME'S GENERATED FILES ARE WHAT THE PALETTE MAKES. That half is a
 * REGENERATION, NOT AN INSPECTION: it runs the same generators with `$HOME`
 * and the XDG variables pointed at a temporary directory, and compares the
 * result byte for byte with what is in the home. Anything that differs,
 * differs from what this machine's own palette produces right now, whatever
 * the reason.
 *
 * Nothing is written outside the temporary directory and nothing is
 * signalled: an audit that repaired what it found would be a `kdos theme` with
 * a misleading name.
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
		struct stat tw, th;

		if (!S_ISLNK(sw.st_mode) || !S_ISLNK(sh.st_mode) ||
		    !same_link(want, have)) {
			c->differ++;
			return;
		}
		c->same++;
		/* AND THROUGH IT, when it names a directory on both sides:
		 * counting the link alone would leave everything under it
		 * unaudited. */
		if (stat(want, &tw) == 0 && stat(have, &th) == 0 &&
		    S_ISDIR(tw.st_mode) && S_ISDIR(th.st_mode))
			goto both_dirs;
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

both_dirs:
	/* Both directories: the union of their entries, so a file the machine has
	 * and the generator does not is counted rather than passed over. Both
	 * listings are sorted (kb_listdir sorts), so the merge is a walk. */
	;
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

enum { A_CONFIG, A_DATA };

static const struct {
	int base;
	const char *rel;
	const char *label;
} ARTEFACTS[] = {
	{ A_CONFIG, "bat/themes/kdos.tmTheme",  "bat"                },
	{ A_CONFIG, "micro/colorschemes/kdos.micro", "micro"         },
	/* Dropping the row would leave a generated file nothing audits: the
	 * theme is written whether or not anything selects it. */
	{ A_CONFIG, "helix/themes/kdos.toml",   "helix"              },
	{ A_CONFIG, "nvim/colors/kdos.vim",     "neovim"             },
	{ A_CONFIG, "git/kdos-delta",           "delta"              },
	{ A_CONFIG, "newsboat/kdos-colors",     "newsboat"           },
	{ A_CONFIG, "aerc/stylesets/kdos",      "aerc"               },
	{ A_CONFIG, "kdos/fzf-colors",          "fzf"                },
	{ A_CONFIG, "tmux/themes/kdos.conf",    "tmux"               },
	{ A_CONFIG, "btop/themes/kdos.theme",   "btop"               },
	{ A_CONFIG, "starship.toml",            "starship palette"   },
	{ A_CONFIG, "kdos/ls-colors",           "LS_COLORS"          },
	{ A_CONFIG, "mc/ini",                   "mc skin selection"  },
	{ A_DATA,   "mc/skins/kdos.ini",        "mc skin"            },
	{ A_CONFIG, "yazi/theme.toml",          "yazi"               },
};
#define NARTEFACTS ((int)(sizeof(ARTEFACTS) / sizeof(ARTEFACTS[0])))

static char *live_path(int base, const char *rel)
{
	return base == A_CONFIG ? kdt_cfg_home(rel) : kdt_data_home(rel);
}

static char *tmp_path(const char *root, int base, const char *rel)
{
	char *dir = kb_path_join(root, base == A_CONFIG ? ".config"
							: ".local/share");
	char *p = kb_path_join(dir, rel);

	free(dir);
	return p;
}

/* ── the run ───────────────────────────────────────────────────────────── */

/*
 * Regenerate into `root`, in a CHILD process.
 *
 * The redirection is four environment variables, which is exactly how
 * `70_image/050_theme.sh` generates /etc/skel — so this is the same trick the
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

/* ── what the image installed ──────────────────────────────────────────── */

/* The first $XDG_DATA_DIRS entry (default /usr/local/share:/usr/share) that
 * holds `rel`, or NULL: the search KDE itself makes for a colour scheme, a
 * Konsole profile or a global theme. */
static char *data_find(const char *rel)
{
	const char *dirs = getenv("XDG_DATA_DIRS");
	char *copy = kb_strdup(dirs && *dirs ? dirs
				: "/usr/local/share:/usr/share");
	char *save = NULL, *hit = NULL;

	for (char *d = strtok_r(copy, ":", &save); d && !hit;
	     d = strtok_r(NULL, ":", &save)) {
		char *p = kb_path_join(d, rel);

		if (*d && kb_path_exists(p))
			hit = p;
		else
			free(p);
	}
	free(copy);
	return hit;
}

/* Every installed KDOS*.colors along the search path, for the contrast gate:
 * a scheme the user's own data directory adds is audited too, because KDE
 * offers it beside the shipped ones. */
static int contrast_gate(void)
{
	const char *dirs = getenv("XDG_DATA_DIRS");
	char *copy = kb_strdup(dirs && *dirs ? dirs
				: "/usr/local/share:/usr/share");
	char *save = NULL;
	KbArgv a = { 0 };
	int files = 0, rc;

	kb_argv_add(&a, "kdos-theme");
	kb_argv_add(&a, "contrast");
	kb_argv_add(&a, "-q");
	for (char *d = strtok_r(copy, ":", &save); d;
	     d = strtok_r(NULL, ":", &save)) {
		char *dir = kb_path_join(d, "color-schemes");
		int n = 0;
		char **v = kb_listdir(dir, &n);

		for (int i = 0; i < n; i++) {
			size_t l = strlen(v[i]);

			if (strncmp(v[i], "KDOS", 4) || l < 8 ||
			    strcmp(v[i] + l - 7, ".colors") ||
			    a.n >= KB_MAX_ARGV - 2)
				continue;
			kb_argv_addf(&a, "%s/%s", dir, v[i]);
			files++;
		}
		kb_strv_free(v);
		free(dir);
	}
	free(copy);
	kb_argv_end(&a);
	if (!files) {
		printf("  %-18s no KDOS colour scheme is installed\n",
		       "contrast floors");
		return 1;
	}
	if (!kb_have_prog("kdos-theme")) {
		printf("  %-18s not checked: kdos-theme is not installed\n",
		       "contrast floors");
		return 0;
	}
	{
		/* Its refusals are the finding, so they reach the terminal. */
		int was = kb_proc_verbose;

		kb_proc_verbose = 1;
		rc = kb_run_tty(&a);
		kb_proc_verbose = was;
	}
	printf("  %-18s %d scheme%s %s\n", "contrast floors", files,
	       files == 1 ? " " : "s", rc == 0 ? "pass" : "FAIL");
	return rc != 0;
}

static int installed(const KcolScheme *sc)
{
	const char *title = kdt_accent_title(sc);
	char rel[5][160];
	static const char *const LABEL[] = {
		"colour scheme", "Konsole colours", "Konsole profile",
		"qt5ct palette", "global theme",
	};
	int bad = 0;

	snprintf(rel[0], sizeof(rel[0]), "color-schemes/KDOS%s.colors", title);
	snprintf(rel[1], sizeof(rel[1]), "konsole/%s.colorscheme",
		 sc->theme_name);
	snprintf(rel[2], sizeof(rel[2]), "konsole/%s.profile", sc->theme_name);
	snprintf(rel[3], sizeof(rel[3]), "qt5ct/colors/%s.conf",
		 sc->theme_name);
	snprintf(rel[4], sizeof(rel[4]),
		 "plasma/look-and-feel/org.kdos.%s.desktop", sc->name);
	for (int i = 0; i < 5; i++) {
		char *hit = data_find(rel[i]);

		printf("  %-18s %s\n", LABEL[i], hit ? hit : "MISSING");
		if (!hit)
			bad = 1;
		free(hit);
	}
	return bad;
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
	 * These artefacts are EDITED rather than written, and each has to be
	 * copied in before the generators run or the comparison asks the wrong
	 * question.
	 *
	 *   starship.toml   only the block between the markers is ours
	 *   mc/ini          mc keeps real user state there and the generator
	 *                   only sets `skin`
	 *
	 * A machine with none of the files produces none on either side and is
	 * reported as absent, which is also correct.
	 */
	char *cfg = kb_path_join(root, ".config");
	kb_mkdir_p(cfg);
	static const char *const CARRIED[] = { "starship.toml", "mc/ini" };
	for (size_t i = 0; i < sizeof(CARRIED) / sizeof(CARRIED[0]); i++) {
		char *live = kdt_cfg_home(CARRIED[i]);
		char *tmp = kb_path_join(cfg, CARRIED[i]);
		char *slash = strrchr(tmp, '/');
		if (slash && slash != tmp) {
			*slash = 0;
			kb_mkdir_p(tmp);
			*slash = '/';
		}
		kb_copy_file(live, tmp);	/* absent is fine */
		free(live);
		free(tmp);
	}
	free(cfg);

	printf("%s==> theme audit — %s%s\n", tty_accent, sc->name, tty_reset);
	printf("    what the image installed, then the home regenerated into a "
	       "scratch directory and compared\n\n");
	fflush(stdout);

	int bad = contrast_gate();

	bad |= installed(sc);
	printf("\n");
	fflush(stdout);

	if (regenerate(root, sc, apply) != 0) {
		fprintf(stderr, "kdos: the generators did not finish\n");
		kb_rmtree(root);
		return 2;
	}

	int drift = 0;
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
			drift = 1;
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
	if (drift)
		printf("  A file in this home is not what the palette "
		       "generates.\n"
		       "  `kdos theme %s` rewrites all of it.\n", sc->name);
	if (bad)
		printf("  What the image installed for %s is incomplete or "
		       "fails its floors;\n"
		       "  reinstalling kdos-plasma-look puts it back.\n",
		       sc->name);
	if (drift || bad)
		return 1;
	printf("  The installed schemes pass their floors, every file the "
	       "accent needs is installed,\n"
	       "  and every generated file in this home is what KCOL_SCHEMES "
	       "produces.\n");
	return 0;
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos appid — does the icon match the window?
 *
 * A shell matches a running window to its launcher by the desktop entry's FILE
 * ID: `firefox-esr.desktop` matches a toplevel whose app_id is `firefox-esr`.
 * When they differ by so much as a capital letter the launcher and the running
 * window become two separate icons, the second one a grey cog. It is endemic
 * upstream — Bitwarden #17760, Godot #96074, Mozilla bug 1782448, Firefox
 * shipping `Firefox-esr` against `firefox-esr.desktop` — and every desktop's
 * answer is a hand-written per-app rule after a user complains.
 *
 * The seam exists because the app author, the toolkit, the packager and the
 * shell author are four different people. On KDOS three of them are us, so the
 * mismatch is checkable rather than reportable.
 *
 * WHAT MAKES THIS DIFFERENT FROM GUESSING. The right-hand side is
 * `~/.local/share/kdos/observed-app-ids`, which kdos-comp appends to every time
 * a window actually maps. It is a MEASUREMENT. KDOS learned that the hard way:
 * GIMP's own entry declares `StartupWMClass=gimp-3.0` while its Wayland
 * toplevel calls `set_app_id("gimp")`, and nothing short of WAYLAND_DEBUG=1
 * would have told anyone. A checker built on StartupWMClass would confidently
 * have blessed the broken case.
 *
 * Hence three verdicts, and `unknown` is never folded into `ok`:
 *
 *   ok       an entry whose id was observed on a real window
 *   MISMATCH an app_id was observed that no installed entry is named after
 *   unknown  an entry never yet observed — it may be perfect, or it may be
 *            broken and simply not have been launched since the file was last
 *            emptied. Reporting it as ok would be a confident wrong answer,
 *            which is the one thing this must not give.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kxdg.h"

#define OBSERVED_REL "/.local/share/kdos/observed-app-ids"

/* ANSI, but only when someone is looking — the same rule kdos.c follows. */
static const char *C_A = "", *C_D = "", *C_W = "", *C_0 = "";

static void colours(void)
{
	if (!isatty(STDOUT_FILENO))
		return;
	C_A = "\033[1;32m";
	C_D = "\033[2;32m";
	C_W = "\033[1;33m";
	C_0 = "\033[0m";
}

/* Where launchers live. The alien apps' are seeded into the user's tree from
 * /etc/skel, so both are real and both are checked. */
static const char *const ENTRY_DIRS[] = {
	"/usr/share/applications",
	"/usr/local/share/applications",
};

typedef struct {
	char **v;
	int n, cap;
} StrList;

static void sl_add(StrList *l, const char *s)
{
	for (int i = 0; i < l->n; i++)
		if (!strcmp(l->v[i], s))
			return;			/* deduplicated on the way in */
	if (l->n == l->cap) {
		int cap = l->cap ? l->cap * 2 : 32;
		char **grown = kb_calloc((size_t)cap, sizeof(*grown));
		for (int i = 0; i < l->n; i++)
			grown[i] = l->v[i];
		free(l->v);
		l->v = grown;
		l->cap = cap;
	}
	l->v[l->n++] = kb_strdup(s);
}

static int sl_has(const StrList *l, const char *s)
{
	for (int i = 0; i < l->n; i++)
		if (!strcmp(l->v[i], s))
			return 1;
	return 0;
}

static void sl_free(StrList *l)
{
	for (int i = 0; i < l->n; i++)
		free(l->v[i]);
	free(l->v);
}

/* The desktop FILE ID: the basename with `.desktop` removed. That — not Name,
 * not Exec, not StartupWMClass — is what a shell matches on. */
static int entry_id(const char *fname, char *out, size_t len)
{
	size_t n = strlen(fname);
	if (n < 9 || strcmp(fname + n - 8, ".desktop"))
		return -1;
	if (n - 8 >= len)
		return -1;
	memcpy(out, fname, n - 8);
	out[n - 8] = '\0';
	return 0;
}

static void collect_entries(StrList *ids, const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char id[256], path[1024];

	if (!d)
		return;
	while ((e = readdir(d))) {
		KxdgEntry ent = {0};
		if (entry_id(e->d_name, id, sizeof(id)) != 0)
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (kxdg_load(&ent, path, "Desktop Entry") != 0)
			continue;
		/* NoDisplay entries never appear in a shell, so they can never
		 * produce the mismatch this looks for. Checking them would be
		 * noise that trains people to ignore the output. */
		if (!kxdg_bool(&ent, "NoDisplay", 0))
			sl_add(ids, id);
		kxdg_free(&ent);
	}
	closedir(d);
}

static void collect_observed(StrList *seen)
{
	char path[1024], *buf, *line, *save;
	const char *home = kb_home_dir();

	snprintf(path, sizeof(path), "%s%s", home, OBSERVED_REL);
	buf = kb_read_all(path, NULL);
	if (!buf)
		return;
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *t = line;
		while (*t == ' ' || *t == '\t')
			t++;
		if (*t)
			sl_add(seen, t);
	}
	free(buf);
}

int appid_main(int argc, char **argv)
{
	StrList ids = {0}, seen = {0};
	int quiet = 0, mismatches = 0, ok = 0, unknown = 0;

	colours();

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q")) {
			quiet = 1;
		} else {
			fprintf(stderr, "usage: kdos appid [--quiet]\n");
			return 2;
		}
	}

	for (size_t i = 0; i < sizeof(ENTRY_DIRS) / sizeof(ENTRY_DIRS[0]); i++)
		collect_entries(&ids, ENTRY_DIRS[i]);
	{
		char user_dir[1024];
		snprintf(user_dir, sizeof(user_dir), "%s/.local/share/applications",
			 kb_home_dir());
		collect_entries(&ids, user_dir);
	}
	collect_observed(&seen);

	if (seen.n == 0) {
		printf("%sNo windows have been observed yet.%s\n", C_D, C_0);
		printf("kdos-comp records an app_id the first time each window "
		       "maps; launch some apps and run this again.\n");
		sl_free(&ids);
		sl_free(&seen);
		return 0;
	}

	/*
	 * The direction that matters is observed -> entry, not entry ->
	 * observed. An app_id with no entry named after it is a window that
	 * WILL show up as a second, unnamed icon; an entry never observed is
	 * merely an app nobody has opened.
	 */
	for (int i = 0; i < seen.n; i++) {
		if (sl_has(&ids, seen.v[i])) {
			ok++;
			if (!quiet)
				printf("  %sok%s        %s\n", C_A, C_0, seen.v[i]);
			continue;
		}
		mismatches++;
		printf("  %sMISMATCH%s  %s — observed on a window, but no "
		       "%s.desktop is installed\n",
		       C_W, C_0, seen.v[i], seen.v[i]);
	}
	for (int i = 0; i < ids.n; i++)
		if (!sl_has(&seen, ids.v[i]))
			unknown++;

	printf("\n%d matched, %d mismatched, %d entries not yet observed\n",
	       ok, mismatches, unknown);
	if (mismatches)
		printf("%sA mismatch shows in the shell as a second, unnamed "
		       "icon beside the launcher.%s\n"
		       "Rename the .desktop file to the observed app_id — the "
		       "file ID is what is matched on,\nnot Name and not "
		       "StartupWMClass.\n", C_D, C_0);

	sl_free(&ids);
	sl_free(&seen);
	return mismatches ? 1 : 0;
}

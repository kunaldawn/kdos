/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The places column, from one reader.
 *
 * ONE ANSWER FOR THE WHOLE DESKTOP. This was two: `kdos-desk` read
 * `~/.config/user-dirs.dirs` for the desktop folder while `kdos-menu`'s Places
 * list assumed `$HOME/Desktop`, `$HOME/Documents` and four more. On a machine
 * where somebody had renamed one of them the two disagreed — the icons were in
 * the folder the file named and the menu opened an empty one beside it, which
 * reads as a broken menu rather than as two readers.
 *
 * KDOS SEEDS `user-dirs.dirs` RATHER THAN GENERATING IT: there is no
 * xdg-user-dirs on this system, so the file is shipped in `/etc/skel` and is
 * the user's to edit. `$HOME` is the one expansion it may contain, because it
 * is the one the file's own format defines.
 *
 * A PLACE THAT IS NOT THERE IS NOT A PLACE. Every row is checked before it is
 * returned: the user directories are created on demand, and a row that opens
 * an error is worse than a row that is not offered.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "kxdg.h"

/* The order the column is shown in, and the only spelling of these keys. */
static const struct {
	const char *key;	/* the XDG_<key>_DIR name */
	const char *name;	/* what the row reads */
	const char *fallback;	/* under $HOME, when the file says nothing */
} USER_DIRS[] = {
	{ "DESKTOP",		"Desktop",	"Desktop" },
	{ "DOCUMENTS",		"Documents",	"Documents" },
	{ "DOWNLOAD",		"Downloads",	"Downloads" },
	{ "MUSIC",		"Music",	"Music" },
	{ "PICTURES",		"Pictures",	"Pictures" },
	{ "VIDEOS",		"Videos",	"Videos" },
};
#define NUSER_DIRS ((int)(sizeof(USER_DIRS) / sizeof(USER_DIRS[0])))

static int places_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		return snprintf(out, n, "%s/kdos/places", cfg) < (int)n;
	return snprintf(out, n, "%s/.config/kdos/places", kb_home_dir()) <
	       (int)n;
}

static int user_dirs_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");

	if (cfg && *cfg)
		return snprintf(out, n, "%s/user-dirs.dirs", cfg) < (int)n;
	return snprintf(out, n, "%s/.config/user-dirs.dirs", kb_home_dir()) <
	       (int)n;
}

int kxdg_user_dir(const char *key, char *out, size_t n)
{
	const char *home = kb_home_dir();
	char path[1024], want[64], line[512];
	FILE *f;
	int found = 0;

	if (!key || !out || n < 2)
		return 0;

	/* The default first, so a caller that ignores the return value still
	 * has a usable path rather than whatever was in its buffer. */
	for (int i = 0; i < NUSER_DIRS; i++)
		if (!strcmp(USER_DIRS[i].key, key)) {
			snprintf(out, n, "%s/%s", home,
				 USER_DIRS[i].fallback);
			found = 1;
			break;
		}
	if (!found)
		return 0;
	snprintf(want, sizeof(want), "XDG_%s_DIR", key);
	if (!user_dirs_path(path, sizeof(path)))
		return 1;
	f = fopen(path, "r");
	if (!f)
		return 1;		/* no file is not an error */
	while (fgets(line, sizeof(line), f)) {
		char *v = strchr(line, '=');
		char *end;

		if (!v || strncmp(line, want, strlen(want)))
			continue;
		v++;
		if (*v == '"')
			v++;
		line[strcspn(line, "\r\n")] = '\0';
		end = strchr(v, '"');
		if (end)
			*end = '\0';
		/* `$HOME` is the one expansion the format defines. Anything
		 * else is taken as written, because a reader that guessed at
		 * shell expansion would be a shell. */
		if (!strncmp(v, "$HOME", 5))
			snprintf(out, n, "%s%s", home, v + 5);
		else if (*v)
			snprintf(out, n, "%s", v);
		break;
	}
	fclose(f);
	return 1;
}

/* Already in the list, by path. Two rows on one directory is one row a person
 * clicks twice and one they never do. */
static int listed(const KxdgPlace *p, int n, const char *path)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(p[i].path, path))
			return 1;
	return 0;
}

static void place_add(KxdgPlace *out, int *n, int max, const char *name,
		      const char *path)
{
	if (*n >= max || !path || !*path || listed(out, *n, path))
		return;
	if (access(path, F_OK) != 0)
		return;
	snprintf(out[*n].name, sizeof(out[0].name), "%s", name);
	snprintf(out[*n].path, sizeof(out[0].path), "%s", path);
	(*n)++;
}

int kxdg_places(KxdgPlace *out, int max)
{
	char path[1024], line[640];
	FILE *f;
	int n = 0;

	if (!out || max < 1)
		return 0;

	place_add(out, &n, max, "Home", kb_home_dir());
	for (int i = 0; i < NUSER_DIRS; i++) {
		char dir[512];

		if (kxdg_user_dir(USER_DIRS[i].key, dir, sizeof(dir)))
			place_add(out, &n, max, USER_DIRS[i].name, dir);
	}

	/*
	 * AND THE USER'S OWN, OVER THEM. Appended rather than merged by name:
	 * a row whose path is already listed is dropped, so adding a place
	 * that happens to be Documents does not put Documents on the column
	 * twice under two names.
	 */
	if (!places_path(path, sizeof(path)))
		return n;
	f = fopen(path, "r");
	if (!f)
		return n;
	while (n < max && fgets(line, sizeof(line), f)) {
		char *eq, *name, *val;

		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '#' || !line[0])
			continue;
		eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = '\0';
		name = line;
		val = eq + 1;
		while (*name == ' ' || *name == '\t')
			name++;
		while (*val == ' ' || *val == '\t')
			val++;
		for (char *e = name + strlen(name); e > name; e--)
			if (e[-1] == ' ' || e[-1] == '\t')
				e[-1] = '\0';
			else
				break;
		if (!strncmp(val, "$HOME", 5)) {
			char full[512];

			snprintf(full, sizeof(full), "%s%s", kb_home_dir(),
				 val + 5);
			place_add(out, &n, max, name, full);
		} else {
			place_add(out, &n, max, name, val);
		}
	}
	fclose(f);
	return n;
}

/*
 * The directories a shell has actually been in, newest-in-frecency first.
 *
 * SEPARATE FROM `kxdg_places()`, and that is not tidiness. `kxdg_places_add()`
 * decides whether a directory is already a place by asking `kxdg_places()` —
 * so a folder that was merely somewhere a shell had visited would make *Add to
 * Places* answer "already a place" and write nothing. A frecency list is a
 * guess about where somebody has been; the places column is what they said.
 *
 * SILENT WITHOUT zoxide. It is a port and it is on the image, but a machine
 * that has not run it has an empty database and gets an empty group — which is
 * the right answer either way, and is why the absence is not reported.
 *
 * `have` is what the caller already collected, so a directory that is Home or
 * Documents or one of the user's own rows is not offered a second time under
 * its basename.
 */
int kxdg_places_recent(KxdgPlace *out, int max, const KxdgPlace *have, int nhave)
{
	char buf[8192];
	KbArgv a = { 0 };
	int n = 0;

	if (!out || max < 1 || !kb_have_prog("zoxide"))
		return 0;
	kb_argv_add(&a, "zoxide");
	kb_argv_add(&a, "query");
	kb_argv_add(&a, "-l");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || !buf[0])
		return 0;

	for (char *p = buf, *nl; p && *p; p = nl) {
		int dup = 0;

		nl = strchr(p, '\n');
		if (nl)
			*nl++ = '\0';
		/*
		 * THE LAST LINE IS DROPPED WHEN THE BUFFER FILLED. A capture
		 * that ran out of room truncates without saying so, and half a
		 * path is a PREFIX — `/home/u/proj` cut from `/home/u/projects`
		 * is a directory that exists and is not the one zoxide named.
		 */
		if (!nl && strlen(buf) >= sizeof(buf) - 1)
			break;
		if (*p != '/')
			continue;
		for (int i = 0; i < nhave; i++)
			if (have && !strcmp(have[i].path, p))
				dup = 1;
		for (int i = 0; i < n; i++)
			if (!strcmp(out[i].path, p))
				dup = 1;
		if (dup)
			continue;
		/* place_add's own rules: it exists, and it is not listed. */
		place_add(out, &n, max, kb_basename(p), p);
		if (n >= max)
			break;
	}
	return n;
}

int kxdg_places_add(const char *name, const char *path)
{
	KxdgPlace have[KXDG_PLACES_MAX];
	char file[1024], dir[1024], *slash;
	FILE *f;
	int n;

	if (!name || !*name || !path || !*path)
		return -1;
	/* THE NAME IS CHECKED FIRST, before anything else can answer. An `=`
	 * or a newline would split the row somewhere else on the next read,
	 * and that is true whether or not the path is already a place. */
	if (strchr(name, '=') || strchr(name, '\n'))
		return -1;
	if (access(path, F_OK) != 0)
		return -1;
	/* Refused rather than duplicated: the column is read from this file
	 * every time it is drawn, and a second row on one directory is a row
	 * nobody ever clicks. */
	n = kxdg_places(have, KXDG_PLACES_MAX);
	if (listed(have, n, path))
		return 1;
	if (!places_path(file, sizeof(file)))
		return -1;
	snprintf(dir, sizeof(dir), "%s", file);
	slash = strrchr(dir, '/');
	if (slash) {
		*slash = '\0';
		kb_mkdir_p(dir);
	}
	f = fopen(file, "a");
	if (!f)
		return -1;
	fprintf(f, "%s = %s\n", name, path);
	fclose(f);
	return 0;
}

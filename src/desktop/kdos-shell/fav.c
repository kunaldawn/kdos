/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The pinned list.
 *
 * `~/.config/kdos/favorites` is one desktop-entry id per line and it is what
 * the quick-launch row draws. Pinning is a menu action in one process and the
 * panel is another, so the WRITER lives here, beside nothing, and both sides
 * call it — a second implementation would be a second answer to what a pin is.
 *
 * The write is temp + rename, like every other state file on this desktop: a
 * half-written favorites file is a quick-launch row that comes up empty at the
 * next login, and the whole file is a few dozen bytes.
 *
 * It is shell state rather than chrome, which is why it stayed behind when the
 * header band, the button bar and the list helpers moved to libkchrome.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kbase.h"
#include "shell.h"

int sh_fav_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%s/kdos/favorites", cfg);
	else if (home && *home)
		snprintf(out, n, "%s/.config/kdos/favorites", home);
	else
		return -1;
	return 0;
}

/*
 * THE TERMINAL FOLLOWS THE DESKTOP, so one favourites file serves both.
 *
 * `foot` is a Wayland client and the console session has no compositor to run
 * it on, so a pinned row naming it there is a row that launches nothing; the
 * mirror case is `kdos-term` on the graphical desktop, where `foot` is the
 * terminal that has run on real hardware. The file names *the terminal* and
 * this resolves which one — the same decision `sh_term()` makes for every
 * chord, menu row and `Terminal=true` entry.
 *
 * Two favourites files would be two things to keep in agreement, and the one
 * nobody is looking at is the one that goes stale.
 */
const char *sh_fav_id(const char *id)
{
	if (!id)
		return NULL;
	if (!strcmp(id, "foot") || !strcmp(id, "kdos-term"))
		return sh_term();
	return id;
}

/*
 * THE ID IS THE FIRST WORD; THE REST OF THE LINE IS METADATA.
 *
 * A favourites line is `mc code=FM` since the two-letter launch codes landed,
 * and every comparison here was against the WHOLE trimmed line. So `sh_fav_has`
 * answered no for all seven seeded favourites, and three things followed from
 * that one mistake: the context menu offered to Pin what was already pinned,
 * clicking it appended a second line WITHOUT the code rather than unpinning,
 * and `sh_fav_move` could not find a coded row to reorder it. The whole line is
 * still what gets KEPT on a rewrite, or the codes would be dropped by the first
 * pin of anything else.
 */
static int line_is(const char *line, const char *id)
{
	size_t n = strlen(id);

	return !strncmp(line, id, n) &&
	       (line[n] == '\0' || line[n] == ' ' || line[n] == '\t');
}

/*
 * The two-letter code a favourites line carries, or NULL. Typing both letters
 * with nothing else in the field opens it — in the menu and in the palette,
 * from this one reader, because a second parse of this file is a second answer
 * to what a code is.
 */
const char *sh_fav_code(const char *id)
{
	static char code[3];
	char path[512], line[256];
	FILE *f;
	int got = 0;

	code[0] = '\0';
	if (!id || !*id || sh_fav_path(path, sizeof(path)) != 0)
		return NULL;
	f = fopen(path, "r");
	if (!f)
		return NULL;
	while (!got && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *p = line;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '#' || !line_is(p, id))
			continue;
		got = 1;

		const char *c = strstr(p, "code=");

		if (c && c[5] && c[6]) {
			code[0] = (char)toupper((unsigned char)c[5]);
			code[1] = (char)toupper((unsigned char)c[6]);
			code[2] = '\0';
		}
	}
	fclose(f);
	return code[0] ? code : NULL;
}

int sh_fav_has(const char *id)
{
	char path[512], line[256];
	FILE *f;
	int found = 0;

	if (!id || !*id || sh_fav_path(path, sizeof(path)) != 0)
		return 0;
	f = fopen(path, "r");
	if (!f)
		return 0;
	while (!found && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p && *p != '#' && line_is(p, id))
			found = 1;
	}
	fclose(f);
	return found;
}

/*
 * MOVE one id to a position. Returns 0 when the file was rewritten.
 *
 * The quick-launch row is a row of icons and the order of a row of icons is
 * something people expect to be able to change by dragging one — it is the
 * only property of that row a user can have an opinion about. Same file, same
 * temp-fsync-rename, and the same writer, because a second implementation
 * would be a second answer to what the order is.
 */
int sh_fav_move(const char *id, int to)
{
	char path[512], tmp[544], line[256];
	char keep[64][128];
	int nkeep = 0, from = -1;
	FILE *f;

	if (!id || !*id || sh_fav_path(path, sizeof(path)) != 0)
		return -1;
	f = fopen(path, "r");
	if (!f)
		return -1;
	while (nkeep < 64 && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '#')
			continue;
		if (line_is(p, id))
			from = nkeep;
		snprintf(keep[nkeep++], sizeof(keep[0]), "%.*s",
			 (int)sizeof(keep[0]) - 1, p);
	}
	fclose(f);
	if (from < 0 || to < 0 || to >= nkeep || to == from)
		return -1;	/* nothing to do is not a rewrite */

	char moved[128];
	snprintf(moved, sizeof(moved), "%s", keep[from]);
	if (to > from)
		for (int i = from; i < to; i++)
			memcpy(keep[i], keep[i + 1], sizeof(keep[0]));
	else
		for (int i = from; i > to; i--)
			memcpy(keep[i], keep[i - 1], sizeof(keep[0]));
	memcpy(keep[to], moved, sizeof(keep[0]));

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return -1;
	for (int i = 0; i < nkeep; i++)
		fprintf(f, "%s\n", keep[i]);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

/*
 * Add or remove one id. Returns 0 when the file now says what was asked,
 * including when it already did.
 */
int sh_fav_set(const char *id, int pinned)
{
	char path[512], tmp[544], line[256];
	char keep[64][128];
	int nkeep = 0, have = 0;
	FILE *f;

	if (!id || !*id || strchr(id, '/') || strchr(id, '\n'))
		return -1;	/* a line, not a path */
	if (sh_fav_path(path, sizeof(path)) != 0)
		return -1;

	f = fopen(path, "r");
	if (f) {
		while (nkeep < 64 && fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\n")] = '\0';
			char *p = line;
			while (*p == ' ' || *p == '\t')
				p++;
			if (!*p || *p == '#')
				continue;
			if (line_is(p, id)) {
				have = 1;
				if (!pinned)
					continue;	/* dropped */
			}
			/* An explicit precision: `line` is 256 bytes and a kept
			 * entry is 128, and the gate treats a possible
			 * truncation as an error. A desktop id longer than
			 * this is not one. */
			snprintf(keep[nkeep++], sizeof(keep[0]), "%.*s",
				 (int)sizeof(keep[0]) - 1, p);
		}
		fclose(f);
	}
	if (pinned && !have && nkeep < 64)
		/* Appended, not inserted: a pin goes where the user last
		 * looked for a new thing, which is the end of the row. */
		snprintf(keep[nkeep++], sizeof(keep[0]), "%.*s",
			 (int)sizeof(keep[0]) - 1, id);
	if (pinned == have && pinned)
		return 0;			/* already there */

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return -1;
	for (int i = 0; i < nkeep; i++)
		fprintf(f, "%s\n", keep[i]);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

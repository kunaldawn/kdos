/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos appid — does every open window have the launcher it claims?
 *
 * The Task Manager matches a window to its launcher by FILE ID: the window's
 * app id, which KWin reports as the desktop file name the application set or
 * else its resource class, must be the basename of an installed .desktop
 * entry. A window whose app id names no entry shows as a second, unnamed icon
 * beside the launcher it came from, with no pin and no name — and nothing
 * reports it, because the window works.
 *
 * So this asks KWin for every window open now (windows.c, as `kdos hey`
 * does) and looks for `<app id>.desktop` in the applications
 * directory of $XDG_DATA_HOME and of each $XDG_DATA_DIRS entry:
 *
 *   <app id> TAB OK|MISMATCH TAB <title>
 *
 * NoDisplay=true IS A MATCH: it keeps an entry out of the menu, not out of the
 * Task Manager, which resolves a window to it all the same. Hidden=true is the
 * spec's "this entry is deleted" and matches nothing.
 *
 * Exit 0 with no MISMATCH, 1 with any, 2 when there is no session to ask.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-tools.h"
#include "kxdg.h"

/* An entry loads and is not deleted. */
static int entry_ok(const char *dir, const char *id)
{
	char leaf[512], *path;
	KxdgEntry ent = { 0 };
	int ok = 0;

	if (strchr(id, '/') || !strcmp(id, ".") || !strcmp(id, ".."))
		return 0;
	snprintf(leaf, sizeof(leaf), "applications/%s.desktop", id);
	path = kb_path_join(dir, leaf);
	if (kb_path_exists(path) && kxdg_load(&ent, path, "Desktop Entry") == 0) {
		ok = !kxdg_bool(&ent, "Hidden", 0);
		kxdg_free(&ent);
	}
	free(path);
	return ok;
}

static int has_entry(const char *id)
{
	const char *dh = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	char *home = dh && *dh ? kb_strdup(dh)
			       : kb_path_join(kb_home_dir(), ".local/share");
	char *copy, *save = NULL;
	int ok = *id && entry_ok(home, id);

	free(home);
	copy = kb_strdup(dirs && *dirs ? dirs : "/usr/local/share:/usr/share");
	for (char *d = strtok_r(copy, ":", &save); d && !ok;
	     d = strtok_r(NULL, ":", &save))
		ok = *d && *id && entry_ok(d, id);
	free(copy);
	return ok;
}

int appid_main(int argc, char **argv)
{
	KdtWindow *w;
	int n, bad = 0;

	(void)argv;
	if (argc > 1) {
		fprintf(stderr, "usage: kdos appid\n");
		return 2;
	}
	n = kdt_windows(&w);
	if (n < 0) {
		fprintf(stderr, "kdos appid: there is no Plasma session to "
				"ask\n");
		return 2;
	}
	for (int i = 0; i < n; i++) {
		int ok = has_entry(w[i].app_id);

		printf("%s\t%s\t%s\n", *w[i].app_id ? w[i].app_id : "-",
		       ok ? "OK" : "MISMATCH", w[i].title);
		bad |= !ok;
	}
	kdt_windows_free(w, n);
	return bad ? 1 : 0;
}

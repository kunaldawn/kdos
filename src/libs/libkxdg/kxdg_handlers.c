/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   What opens a type, from one resolver.
 *
 * ONE ANSWER FOR THE WHOLE DESKTOP. `kdos-appbox open` acts on it and the
 * Settings window's Apps page shows it, so the handler a person is told about
 * is the handler a double-click starts. A page that read only the user's own
 * `mimeapps.list` would report "nothing is set" on a machine where the shipped
 * defaults answer for every common type.
 *
 * The order is freedesktop's, the desktop-named list ahead of the plain one at
 * each level:
 *
 *   ~/.config/kdos-mimeapps.list, mimeapps.list   [Default Applications]
 *   ~/.config/kdos-mimeapps.list, mimeapps.list   [Added Associations]
 *   /etc/xdg/kdos-mimeapps.list, mimeapps.list    [Default Applications]
 *   <data dir>/applications/mimeinfo.cache        [MIME Cache]
 *
 * An id counts only when its entry is installed, so a list naming a removed
 * application falls through to the next one.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kxdg.h"

/*
 * XDG_DATA_HOME first, then XDG_DATA_DIRS in order — the order the launcher
 * and the menu scan, which is what makes a user's own .desktop override the
 * shipped one rather than duplicating it.
 */
static int data_dirs(char out[][512], int max)
{
	const char *home = getenv("XDG_DATA_HOME");
	const char *dirs = getenv("XDG_DATA_DIRS");
	int n = 0;

	if (home && *home)
		snprintf(out[n++], 512, "%.500s", home);
	else
		snprintf(out[n++], 512, "%.480s/.local/share", kb_home_dir());

	if (!dirs || !*dirs)
		dirs = "/usr/local/share:/usr/share";
	for (const char *p = dirs; *p && n < max;) {
		const char *sep = strchr(p, ':');
		size_t len = sep ? (size_t)(sep - p) : strlen(p);
		/* A trailing slash is legal in XDG_DATA_DIRS; kept, every path
		 * built from it carries a `//` into `--print`. */
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
 * A value is a `;`-separated preference list. Every installed entry is kept,
 * in the order written; the ones behind the first are the "open with" menu.
 * Returns how many INSTALLED handlers the list named, counted before the
 * de-duplication: the caller asks whether this section answered, not how many
 * new rows it added.
 */
static int collect_installed(const char *list, int layer, KxdgHandler *c,
			     int *n, int max)
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
		seen++;
		/* The same id is named by mimeapps AND by every cache that
		 * carries it; counted twice, a one-handler type would put a
		 * chooser in front of the user. */
		for (int i = 0; i < *n; i++)
			if (!strcmp(c[i].id, p))
				dup = 1;
		if (dup)
			continue;
		kb_strlcpy(c[*n].id, p, sizeof(c[*n].id));
		kb_strlcpy(c[*n].path, path, sizeof(c[*n].path));
		c[*n].layer = layer;
		(*n)++;
	}
	return seen;
}

static int collect_in(const char *path, const char *section, int layer,
		      const char *mime, KxdgHandler *c, int *n, int max)
{
	KxdgEntry e;
	int seen = 0;

	if (kxdg_load(&e, path, section) != 0)
		return 0;
	const char *v = kxdg_get(&e, mime, NULL);
	if (v)
		seen = collect_installed(v, layer, c, n, max);
	kxdg_free(&e);
	return seen;
}

/* See kxdg.h. */
int kxdg_handlers(const char *mime, KxdgHandler *c, int max, int *defaulted)
{
	char dirs[16][512], path[600], dpre[80], cfgdir[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	int nd = data_dirs(dirs, 16);
	int have_pre = kb_desktop_prefix(dpre, sizeof(dpre));
	int n = 0, dflt = 0;

	if (cfg && *cfg)
		snprintf(cfgdir, sizeof(cfgdir), "%.500s", cfg);
	else
		snprintf(cfgdir, sizeof(cfgdir), "%.480s/.config",
			 kb_home_dir());

	/* The user's declared default beats everything, including a launcher a
	 * box shipped a moment ago, and stops the search: nothing behind it is
	 * a candidate the user has to be asked about. */
	if (have_pre) {
		snprintf(path, sizeof(path), "%.500s/%.40s-mimeapps.list",
			 cfgdir, dpre);
		collect_in(path, "Default Applications", KXDG_LAYER_USER,
			   mime, c, &n, max);
	}
	if (!n) {
		snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfgdir);
		collect_in(path, "Default Applications", KXDG_LAYER_USER,
			   mime, c, &n, max);
	}
	if (n) {
		if (defaulted)
			*defaulted = 1;
		return n;
	}

	if (have_pre) {
		snprintf(path, sizeof(path), "%.500s/%.40s-mimeapps.list",
			 cfgdir, dpre);
		collect_in(path, "Added Associations", KXDG_LAYER_ADDED, mime,
			   c, &n, max);
	}
	snprintf(path, sizeof(path), "%.500s/mimeapps.list", cfgdir);
	collect_in(path, "Added Associations", KXDG_LAYER_ADDED, mime, c, &n,
		   max);

	/* The system default counts as a decision whatever the user's
	 * additions found: XDG puts Default Applications ahead of Added
	 * Associations at every level, so an association that masked one would
	 * turn a decided type into a chooser prompt. */
	if (have_pre) {
		snprintf(path, sizeof(path), "/etc/xdg/%.40s-mimeapps.list",
			 dpre);
		if (collect_in(path, "Default Applications", KXDG_LAYER_SYSTEM,
			       mime, c, &n, max))
			dflt = 1;
	}
	if (collect_in("/etc/xdg/mimeapps.list", "Default Applications",
		       KXDG_LAYER_SYSTEM, mime, c, &n, max))
		dflt = 1;

	/* Then the caches. `mimeinfo.cache` is the file genlaunchers writes
	 * beside the box's launchers so this lookup finds them; without it the
	 * MimeType lines carried through from Debian are consulted by
	 * nothing. */
	for (int i = 0; i < nd; i++) {
		snprintf(path, sizeof(path),
			 "%.500s/applications/mimeinfo.cache", dirs[i]);
		collect_in(path, "MIME Cache", KXDG_LAYER_CACHE, mime, c, &n,
			   max);
	}

	if (defaulted)
		*defaulted = dflt;
	return n;
}

/* See kxdg.h. */
const char *kxdg_layer_name(int layer)
{
	switch (layer) {
	case KXDG_LAYER_USER:
		return "your default";
	case KXDG_LAYER_ADDED:
		return "your addition";
	case KXDG_LAYER_SYSTEM:
		return "KDOS default";
	case KXDG_LAYER_CACHE:
		return "installed";
	}
	return "nothing";
}

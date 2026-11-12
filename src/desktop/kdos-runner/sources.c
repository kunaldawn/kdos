/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   sources.c — the files the palette answers from
 *
 *   ~/.config/kdos/favorites      desktop-entry ids; `code=XX` on a line is
 *                                 the two letters that open it
 *   /etc/kdos/menu.conf           routes, `verb.noun = argv`; the user's
 *   ~/.config/kdos/menu.conf      copy replaces a route by name and adds
 *   ~/.config/kdos/boxes/         one <name>.conf profile per box
 *   /mnt/iso/packs                the packs and .ktar sets the boot medium
 *                                 carries, as Discover's KDOS backend sees it
 *   kglobalshortcutsrc            the chords as KGlobalAccel keeps them, the
 *                                 user's over $XDG_CONFIG_DIRS' copies, then
 *   /usr/share/kglobalaccel/      the defaults no component has registered
 *
 * KRUNNER ASKS ON EVERY KEYSTROKE, so nothing here is parsed per query. Each
 * reader keeps what it parsed with the identity (device, inode, size,
 * modification time) of every file it read, and parses again only when one
 * of them changed or appeared. A keystroke costs a few stat() calls.
 *
 * THESE ARE A PERSON'S FILES and are parsed as hostile: a line too long for
 * its field is cut, a name that is not a plain name is skipped, and nothing
 * read here ever reaches a shell (run.c execs vectors).
 *
 * KDOS_RUNNER_ROOT moves every absolute system path under a fixture tree.
 * The per-user paths follow $HOME and the XDG variables as usual.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "kxdg.h"
#include "runner.h"

/* ── paths ─────────────────────────────────────────────────────────────── */

const char *rn_sys(const char *abs, char *out, size_t n)
{
	const char *root = getenv("KDOS_RUNNER_ROOT");

	/* A path that does not fit is no path: a cut one names another file. */
	if (snprintf(out, n, "%s%s", root && *root ? root : "", abs) >= (int)n)
		out[0] = '\0';
	return out;
}

void rn_config_home(char *out, size_t n)
{
	const char *x = getenv("XDG_CONFIG_HOME");

	if (x && *x == '/')
		kb_strlcpy(out, x, n);
	else
		snprintf(out, n, "%s/.config", kb_home_dir());
}

void rn_data_home(char *out, size_t n)
{
	const char *x = getenv("XDG_DATA_HOME");

	if (x && *x == '/')
		kb_strlcpy(out, x, n);
	else
		snprintf(out, n, "%s/.local/share", kb_home_dir());
}

/* The colon list in `var`, or `def`; each element through rn_sys() when it
 * came from the default, since a fixture root replaces the system's dirs. */
static int dir_list(const char *var, const char *def, char dirs[][512],
		    int max)
{
	const char *v = getenv(var);
	int fromdef = !(v && *v);
	char buf[2048];
	int n = 0;

	kb_strlcpy(buf, fromdef ? def : v, sizeof(buf));
	for (char *s = buf, *e; s && *s && n < max; s = e) {
		e = strchr(s, ':');
		if (e)
			*e++ = '\0';
		if (*s != '/')
			continue;
		if (fromdef)
			rn_sys(s, dirs[n], 512);
		else
			kb_strlcpy(dirs[n], s, 512);
		n++;
	}
	return n;
}

/* ── change detection ──────────────────────────────────────────────────── */

typedef struct {
	char sig[4096];
	size_t n;
} Sig;

static void sig_add(Sig *s, const char *path)
{
	struct stat st;
	int r;

	if (stat(path, &st) == 0)
		r = snprintf(s->sig + s->n, sizeof(s->sig) - s->n,
			     "%lx:%lx:%lld:%lld.%ld;", (unsigned long)st.st_dev,
			     (unsigned long)st.st_ino, (long long)st.st_size,
			     (long long)st.st_mtim.tv_sec,
			     (long)st.st_mtim.tv_nsec);
	else
		r = snprintf(s->sig + s->n, sizeof(s->sig) - s->n, "-;");
	if (r > 0 && (size_t)r < sizeof(s->sig) - s->n)
		s->n += (size_t)r;
}

static unsigned g_epoch;	/* bumped by rn_sources_reset()   */

typedef struct {
	char sig[4096];
	unsigned epoch;
	int valid;
} Cache;

/* 1 when `c` still describes what `s` measured. Records `s` otherwise. */
static int cache_fresh(Cache *c, const Sig *s)
{
	if (c->valid && c->epoch == g_epoch && !strcmp(c->sig, s->sig))
		return 1;
	kb_strlcpy(c->sig, s->sig, sizeof(c->sig));
	c->epoch = g_epoch;
	c->valid = 1;
	return 0;
}

void rn_sources_reset(void)
{
	g_epoch++;
}

static char *trim(char *s)
{
	char *e;

	while (*s == ' ' || *s == '\t')
		s++;
	e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ||
			 e[-1] == '\n'))
		*--e = '\0';
	return s;
}

static int plain_name(const char *s, int dots)
{
	if (!*s || *s == '.' || *s == '-')
		return 0;
	for (; *s; s++)
		if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-' &&
		    !(dots && *s == '.'))
			return 0;
	return 1;
}

/* ── desktop entries ───────────────────────────────────────────────────── */

int rn_desktop_entry(const char *id, char *name, size_t nn, char *icon,
		     size_t in)
{
	char dirs[17][512], path[1024];
	int nd;

	if (!plain_name(id, 1))
		return 0;
	rn_data_home(dirs[0], sizeof(dirs[0]));
	nd = 1 + dir_list("XDG_DATA_DIRS", "/usr/local/share:/usr/share",
			  dirs + 1, 16);
	for (int i = 0; i < nd; i++) {
		KxdgEntry e = {0};
		int hidden;

		if (snprintf(path, sizeof(path), "%s/applications/%s.desktop",
			     dirs[i], id) >= (int)sizeof(path) ||
		    !kb_path_exists(path))
			continue;
		/* The first file of that id is the entry, hidden or not: a
		 * user's copy with Hidden=true deletes the system's. */
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			return 0;
		hidden = kxdg_bool(&e, "Hidden", 0);
		if (name)
			kb_strlcpy(name, kxdg_get(&e, "Name", id), nn);
		if (icon)
			kb_strlcpy(icon, kxdg_get(&e, "Icon", ""), in);
		kxdg_free(&e);
		return !hidden;
	}
	return 0;
}

/* ── favorites and their two-letter codes ──────────────────────────────── */

static RnFavorite *g_fav;
static int g_nfav;
static Cache g_fav_cache;

/* The user's list, or the first list on $XDG_CONFIG_DIRS when the user has
 * none: the codes a fresh account starts with are the distribution's. */
static int favorites_path(char *out, size_t n, Sig *s)
{
	char home[512], dirs[16][512];
	int nd;

	rn_config_home(home, sizeof(home));
	snprintf(out, n, "%s/kdos/favorites", home);
	sig_add(s, out);
	if (kb_path_exists(out))
		return 1;
	nd = dir_list("XDG_CONFIG_DIRS", "/etc/xdg", dirs, 16);
	for (int i = 0; i < nd; i++) {
		if (snprintf(out, n, "%s/kdos/favorites", dirs[i]) >= (int)n)
			continue;
		sig_add(s, out);
		if (kb_path_exists(out))
			return 1;
	}
	return 0;
}

int rn_favorites(const RnFavorite **out)
{
	char path[1024];
	Sig s = {0};
	int have = favorites_path(path, sizeof(path), &s);
	char *text, *save = NULL, dirs[17][512], apps[600];
	int nd;

	/* A favourite whose application is installed or removed later
	 * changes the list as much as an edit of the file does. */
	rn_data_home(dirs[0], sizeof(dirs[0]));
	nd = 1 + dir_list("XDG_DATA_DIRS", "/usr/local/share:/usr/share",
			  dirs + 1, 16);
	for (int i = 0; i < nd; i++) {
		if (snprintf(apps, sizeof(apps), "%s/applications", dirs[i]) <
		    (int)sizeof(apps))
			sig_add(&s, apps);
	}
	*out = g_fav;
	if (cache_fresh(&g_fav_cache, &s))
		return g_nfav;
	free(g_fav);
	g_fav = NULL;
	g_nfav = 0;
	*out = NULL;
	if (!have || !(text = kb_read_whole(path, NULL)))
		return 0;
	for (char *l = strtok_r(text, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char *hash = strchr(l, '#'), *tok, *ts = NULL;
		RnFavorite f = {0};

		if (hash)
			*hash = '\0';
		tok = strtok_r(l, " \t\r", &ts);
		if (!tok || !plain_name(tok, 1) || strlen(tok) >= sizeof(f.id))
			continue;
		kb_strlcpy(f.id, tok, sizeof(f.id));
		while ((tok = strtok_r(NULL, " \t\r", &ts))) {
			if (strncasecmp(tok, "code=", 5) || strlen(tok) != 7 ||
			    !isalnum((unsigned char)tok[5]) ||
			    !isalnum((unsigned char)tok[6]))
				continue;
			f.code[0] = (char)tolower((unsigned char)tok[5]);
			f.code[1] = (char)tolower((unsigned char)tok[6]);
		}
		/* A line whose application is not installed is skipped in
		 * silence: the same file names boxes to warm up, and a pack
		 * not installed leaves a line behind that opens nothing. */
		if (!rn_desktop_entry(f.id, f.name, sizeof(f.name), f.icon,
				      sizeof(f.icon)))
			continue;
		g_fav = kb_realloc(g_fav, (size_t)(g_nfav + 1) * sizeof(*g_fav));
		g_fav[g_nfav++] = f;
		if (g_nfav >= 64)
			break;
	}
	free(text);
	*out = g_fav;
	return g_nfav;
}

/* ── routes ────────────────────────────────────────────────────────────── */

static RnRoute *g_routes;
static int g_nroutes;
static Cache g_route_cache;

static void routes_read(const char *path)
{
	char *text = kb_read_whole(path, NULL), *save = NULL;

	if (!text)
		return;
	for (char *l = strtok_r(text, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char *eq, *k, *v;
		int at = -1;

		l = trim(l);
		if (!*l || *l == '#' || *l == '@')
			continue;
		eq = strchr(l, '=');
		if (!eq)
			continue;
		*eq = '\0';
		k = trim(l);
		v = trim(eq + 1);
		if (!plain_name(k, 1) || !*v || strlen(k) >= sizeof(g_routes[0].name))
			continue;
		for (int i = 0; i < g_nroutes; i++)
			if (!strcmp(g_routes[i].name, k))
				at = i;
		if (at < 0) {
			if (g_nroutes >= 256)
				continue;
			g_routes = kb_realloc(g_routes, (size_t)(g_nroutes + 1) *
							    sizeof(*g_routes));
			at = g_nroutes++;
			kb_strlcpy(g_routes[at].name, k, sizeof(g_routes[at].name));
		}
		kb_strlcpy(g_routes[at].value, v, sizeof(g_routes[at].value));
	}
	free(text);
}

int rn_routes(const RnRoute **out)
{
	char sys[1024], user[1024], home[512];
	Sig s = {0};

	rn_sys("/etc/kdos/menu.conf", sys, sizeof(sys));
	rn_config_home(home, sizeof(home));
	snprintf(user, sizeof(user), "%s/kdos/menu.conf", home);
	sig_add(&s, sys);
	sig_add(&s, user);
	if (!cache_fresh(&g_route_cache, &s)) {
		free(g_routes);
		g_routes = NULL;
		g_nroutes = 0;
		routes_read(sys);
		routes_read(user);
	}
	*out = g_routes;
	return g_nroutes;
}

/* ── boxes ─────────────────────────────────────────────────────────────── */

static char **g_boxes;
static const char *g_boxv[129];
static int g_nboxes;
static Cache g_box_cache;

int rn_boxes(const char *const **out)
{
	char dir[1024], home[512];
	Sig s = {0};

	rn_config_home(home, sizeof(home));
	snprintf(dir, sizeof(dir), "%s/kdos/boxes", home);
	sig_add(&s, dir);
	if (!cache_fresh(&g_box_cache, &s)) {
		kb_strv_free(g_boxes);
		g_boxes = kb_listdir(dir, NULL);
		g_nboxes = 0;
		for (char **f = g_boxes; f && *f && g_nboxes < 128; f++) {
			size_t l = strlen(*f);

			if (l < 6 || strcmp(*f + l - 5, ".conf"))
				continue;
			(*f)[l - 5] = '\0';
			if (plain_name(*f, 1))
				g_boxv[g_nboxes++] = *f;
		}
		g_boxv[g_nboxes] = NULL;
	}
	*out = g_boxv;
	return g_nboxes;
}

/* ── packs on the boot medium ──────────────────────────────────────────── */

static RnMediumPack *g_medium;
static int g_nmedium;
static Cache g_medium_cache;

int rn_medium(const RnMediumPack **out)
{
	const char *env = getenv("KDOS_PACK_MEDIUM");
	char dir[512];
	Sig s = {0};

	if (env && *env == '/')
		kb_strlcpy(dir, env, sizeof(dir));
	else
		rn_sys("/mnt/iso/packs", dir, sizeof(dir));
	sig_add(&s, dir);
	if (!cache_fresh(&g_medium_cache, &s)) {
		char **names = kb_listdir(dir, NULL);

		free(g_medium);
		g_medium = NULL;
		g_nmedium = 0;
		for (char **f = names; f && *f && g_nmedium < 256; f++) {
			RnMediumPack m = {0};
			size_t l = strlen(*f);
			int set = l > 5 && !strcmp(*f + l - 5, ".ktar");

			if (!set && !(l > 6 && !strcmp(*f + l - 6, ".kpack")))
				continue;
			l -= set ? 5 : 6;
			if (l >= sizeof(m.id))
				continue;
			memcpy(m.id, *f, l);
			m.id[l] = '\0';
			if (!plain_name(m.id, 1) ||
			    snprintf(m.path, sizeof(m.path), "%s/%s", dir, *f) >=
				    (int)sizeof(m.path))
				continue;
			m.set = set;
			g_medium = kb_realloc(g_medium, (size_t)(g_nmedium + 1) *
								  sizeof(*g_medium));
			g_medium[g_nmedium++] = m;
		}
		kb_strv_free(names);
	}
	*out = g_medium;
	return g_nmedium;
}

/* ── chords ────────────────────────────────────────────────────────────── */

static RnChord *g_chords;
static int g_nchords;
static Cache g_chord_cache;

static RnChord *chord_slot(const char *comp, const char *action)
{
	for (int i = 0; i < g_nchords; i++)
		if (!strcmp(g_chords[i].component, comp) &&
		    !strcmp(g_chords[i].action, action))
			return &g_chords[i];
	if (g_nchords >= 2048)
		return NULL;
	g_chords = kb_realloc(g_chords, (size_t)(g_nchords + 1) *
					    sizeof(*g_chords));
	memset(&g_chords[g_nchords], 0, sizeof(g_chords[0]));
	kb_strlcpy(g_chords[g_nchords].component, comp,
		   sizeof(g_chords[0].component));
	kb_strlcpy(g_chords[g_nchords].action, action,
		   sizeof(g_chords[0].action));
	return &g_chords[g_nchords++];
}

/*
 * One KGlobalAccel shortcut list, as the file spells it, into the form a
 * person reads. KConfig writes a tab between alternatives as `\t` and keeps
 * `none` for an action with no key; a key that IS a comma follows a `+`.
 */
static void keys_human(const char *in, size_t len, char *out, size_t n)
{
	size_t o = 0;

	out[0] = '\0';
	if ((len == 4 && !strncmp(in, "none", 4)) || !len)
		return;
	for (size_t i = 0; i < len && o + 3 < n; i++) {
		if (in[i] == '\\' && i + 1 < len && in[i + 1] == 't') {
			out[o++] = ',';
			out[o++] = ' ';
			i++;
		} else if (in[i] == '\\' && i + 1 < len && in[i + 1] == 's') {
			out[o++] = ' ';
			i++;
		} else if (in[i] == '\t') {
			out[o++] = ',';
			out[o++] = ' ';
		} else {
			out[o++] = in[i];
		}
	}
	out[o] = '\0';
}

static const char *next_field(const char *start)
{
	const char *p = start;

	for (; *p; p++) {
		if (*p != ',')
			continue;
		/* `Meta+,` — the comma is the key, not the separator. */
		if (p > start && p[-1] == '+')
			continue;
		return p;
	}
	return p;
}

static void shortcuts_read(const char *path)
{
	char *text = kb_read_whole(path, NULL), *save = NULL;
	char comp[96] = "", complabel[160] = "";
	int services = 0;

	if (!text)
		return;
	for (char *l = strtok_r(text, "\n", &save); l;
	     l = strtok_r(NULL, "\n", &save)) {
		char *eq, *k, *v;
		RnChord *c;

		l = trim(l);
		if (!*l || *l == '#')
			continue;
		if (*l == '[') {
			char *e = strrchr(l, ']');

			services = !strncmp(l, "[services][", 11);
			if (e)
				*e = '\0';
			/* `[services][org.kde.konsole.desktop]` is one
			 * component in two groups; the last name is the one
			 * a person recognises. */
			kb_strlcpy(comp, services ? l + 11 : l + 1, sizeof(comp));
			kb_strlcpy(complabel, comp, sizeof(complabel));
			continue;
		}
		eq = strchr(l, '=');
		if (!*comp || !eq)
			continue;
		*eq = '\0';
		k = trim(l);
		v = trim(eq + 1);
		if (!strcmp(k, "_k_friendly_name")) {
			kb_strlcpy(complabel, v, sizeof(complabel));
			continue;
		}
		if (!(c = chord_slot(comp, k)))
			continue;
		if (services) {
			/* A service group holds the keys alone; what they
			 * do is the entry's to say. */
			char app[128] = "", id[160];

			snprintf(id, sizeof(id), "%s", comp);
			if (strlen(id) > 8 && !strcmp(id + strlen(id) - 8, ".desktop"))
				id[strlen(id) - 8] = '\0';
			if (!rn_desktop_entry(id, app, sizeof(app), NULL, 0))
				kb_strlcpy(app, id, sizeof(app));
			if (!strcmp(k, "_launch"))
				snprintf(c->label, sizeof(c->label), "Launch %s", app);
			else
				snprintf(c->label, sizeof(c->label), "%s: %s", app, k);
			keys_human(v, strlen(v), c->keys, sizeof(c->keys));
			continue;
		}
		{
			const char *f1 = next_field(v);
			const char *f2 = *f1 ? next_field(f1 + 1) : f1;

			keys_human(v, (size_t)(f1 - v), c->keys, sizeof(c->keys));
			if (*f2)
				kb_strlcpy(c->label, f2 + 1, sizeof(c->label));
			else if (!c->label[0])
				kb_strlcpy(c->label, k, sizeof(c->label));
		}
	}
	free(text);
}

/* /usr/share/kglobalaccel/<component>.desktop: the defaults of a component
 * that has not registered with KGlobalAccel yet, so an action nothing has
 * started still answers `keys`. Never over what a file above recorded. */
static void kglobalaccel_read(const char *dir, Sig *s)
{
	char **files = kb_listdir(dir, NULL);

	for (char **f = files; f && *f; f++) {
		char path[1024], comp[96], acts[1024], *save = NULL;
		KxdgEntry e = {0};
		size_t l = strlen(*f);

		if (l < 9 || strcmp(*f + l - 8, ".desktop") || l - 8 >= sizeof(comp))
			continue;
		memcpy(comp, *f, l - 8);
		comp[l - 8] = '\0';
		if (snprintf(path, sizeof(path), "%s/%s", dir, *f) >=
		    (int)sizeof(path))
			continue;
		if (s)
			sig_add(s, path);
		if (kxdg_load(&e, path, "Desktop Entry") != 0)
			continue;
		kb_strlcpy(acts, kxdg_get(&e, "Actions", ""), sizeof(acts));
		kxdg_free(&e);
		for (char *a = strtok_r(acts, ";", &save); a;
		     a = strtok_r(NULL, ";", &save)) {
			char sect[160];
			KxdgEntry ae = {0};
			RnChord *c;

			if (!*a)
				continue;
			snprintf(sect, sizeof(sect), "Desktop Action %s", a);
			if (kxdg_load(&ae, path, sect) != 0)
				continue;
			for (int i = 0; i < g_nchords; i++)
				if (!strcmp(g_chords[i].component, comp) &&
				    !strcmp(g_chords[i].action, a))
					goto next;
			if ((c = chord_slot(comp, a))) {
				const char *ks = kxdg_get(&ae, "X-KDE-Shortcuts", "");
				size_t o = 0;

				kb_strlcpy(c->label, kxdg_get(&ae, "Name", a),
					   sizeof(c->label));
				for (const char *p = ks; *p && o + 3 < sizeof(c->keys); p++) {
					if (*p == ',' && p > ks && p[-1] != '+') {
						c->keys[o++] = ',';
						c->keys[o++] = ' ';
					} else {
						c->keys[o++] = *p;
					}
				}
				c->keys[o] = '\0';
			}
next:
			kxdg_free(&ae);
		}
	}
	kb_strv_free(files);
}

int rn_chords(const RnChord **out)
{
	char home[512], user[1024], dirs[16][512], path[16][1024], kga[1024];
	int nd;
	Sig s = {0};

	rn_config_home(home, sizeof(home));
	snprintf(user, sizeof(user), "%s/kglobalshortcutsrc", home);
	nd = dir_list("XDG_CONFIG_DIRS", "/etc/xdg", dirs, 16);
	for (int i = 0; i < nd; i++) {
		if (snprintf(path[i], sizeof(path[i]), "%s/kglobalshortcutsrc",
			     dirs[i]) >= (int)sizeof(path[i]))
			path[i][0] = '\0';
		sig_add(&s, path[i]);
	}
	sig_add(&s, user);
	rn_sys("/usr/share/kglobalaccel", kga, sizeof(kga));
	sig_add(&s, kga);
	if (!cache_fresh(&g_chord_cache, &s)) {
		free(g_chords);
		g_chords = NULL;
		g_nchords = 0;
		/* Lowest priority first: $XDG_CONFIG_DIRS is in falling
		 * order, so the last directory is read first. */
		for (int i = nd - 1; i >= 0; i--)
			shortcuts_read(path[i]);
		shortcuts_read(user);
		kglobalaccel_read(kga, NULL);
	}
	*out = g_chords;
	return g_nchords;
}


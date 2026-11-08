/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos help --keys — the global shortcuts, grouped as the key card does
 *
 * THE KEYS ARE KGLOBALACCEL'S, read from the files it reads: kglobalshortcutsrc
 * along the configuration cascade, where every component that ever registered
 * left its actions with the chord in force, its default and its name; and the
 * desktop entries under kglobalaccel/ along $XDG_DATA_DIRS, which are how a
 * program's launch key (KDOS's own among them) is declared before anything
 * has run. A shortcut the person rebound in System Settings is in their own
 * file and wins, so the sheet says what the keyboard does on this login.
 *
 * GROUPED BY THE KEY CARD'S OWN TABLE, /usr/share/kdos/keycard-groups.conf,
 * which kwin-effect-kdos-keys installs: the first rule whose component and
 * action patterns match places the shortcut, the rules under `---` are the
 * fallbacks, and the groups print in the card's order, so Meta+F1 and this
 * page put each key under the same heading. Without the table every shortcut
 * is listed under its component.
 *
 * Nothing here needs a session: the sheet is most useful exactly where none
 * is running — a console, an ssh login, a printer.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-tools.h"

#define GROUPS_CONF "/usr/share/kdos/keycard-groups.conf"

static const char *const ORDER[] = { "windows", "desktops", "launch", "kdos",
				     "accessibility", "system" };
#define NORDER ((int)(sizeof(ORDER) / sizeof(ORDER[0])))

typedef struct {
	char *component, *action, *keys, *label;
	int group;		/* index in ORDER, or -1 for unplaced */
} Key;

typedef struct {
	Key *k;
	int n, cap;
} Keys;

typedef struct {
	char group[32], component[256], action[512];
	int fallback;
} Rule;

/* ── glob ──────────────────────────────────────────────────────────────── */

static int glob(const char *p, const char *s)
{
	for (; *p; p++, s++) {
		if (*p == '*') {
			for (const char *t = s;; t++) {
				if (glob(p + 1, t))
					return 1;
				if (!*t)
					return 0;
			}
		}
		if (!*s || (*p != '?' && *p != *s))
			return 0;
	}
	return !*s;
}

/* ── the table ─────────────────────────────────────────────────────────── */

static Rule *load_rules(int *n)
{
	const char *path = getenv("KDOS_KEYCARD_GROUPS");
	char *data = kb_read_all(path && *path ? path : GROUPS_CONF, NULL);
	Rule *r = NULL;
	int fallback = 0, cap = 0;

	*n = 0;
	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n'), *p = line, *comp, *act;
		Rule *x;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '#')
			continue;
		if (!strncmp(p, "---", 3)) {
			fallback = 1;
			continue;
		}
		if (*n == cap) {
			cap = cap ? cap * 2 : 64;
			r = kb_realloc(r, (size_t)cap * sizeof(*r));
		}
		x = &r[*n];
		memset(x, 0, sizeof(*x));
		/* the group */
		comp = p + strcspn(p, " \t");
		if (!*comp)
			continue;
		*comp++ = '\0';
		kb_strlcpy(x->group, p, sizeof(x->group));
		while (*comp == ' ' || *comp == '\t')
			comp++;
		/* the component, quoted when it holds a space */
		if (*comp == '"') {
			act = strchr(++comp, '"');
			if (!act)
				continue;
			*act++ = '\0';
		} else {
			act = comp + strcspn(comp, " \t");
			if (*act)
				*act++ = '\0';
		}
		kb_strlcpy(x->component, comp, sizeof(x->component));
		while (*act == ' ' || *act == '\t')
			act++;
		{
			char *e = act + strlen(act);

			while (e > act && (e[-1] == ' ' || e[-1] == '\t' ||
					   e[-1] == '\r'))
				*--e = '\0';
		}
		if (!*act)
			continue;
		kb_strlcpy(x->action, act, sizeof(x->action));
		x->fallback = fallback;
		(*n)++;
	}
	free(data);
	return r;
}

static int place(const Rule *r, int nr, const char *component,
		 const char *action)
{
	for (int pass = 0; pass < 2; pass++)
		for (int i = 0; i < nr; i++) {
			if (r[i].fallback != pass ||
			    !glob(r[i].component, component) ||
			    !glob(r[i].action, action))
				continue;
			for (int g = 0; g < NORDER; g++)
				if (!strcmp(ORDER[g], r[i].group))
					return g;
			return -1;
		}
	return -1;
}

/* ── the shortcuts ─────────────────────────────────────────────────────── */

static Key *find(Keys *k, const char *component, const char *action)
{
	for (int i = 0; i < k->n; i++)
		if (!strcmp(k->k[i].component, component) &&
		    !strcmp(k->k[i].action, action))
			return &k->k[i];
	return NULL;
}

/* First writer wins: the files are read from the person's own down, so a
 * rebound key is never replaced by the default under it. */
static void add(Keys *k, const char *component, const char *action,
		const char *keys, const char *label)
{
	Key *x = find(k, component, action);

	if (x) {
		if (!x->label && label && *label)
			x->label = kb_strdup(label);
		return;
	}
	if (k->n == k->cap) {
		k->cap = k->cap ? k->cap * 2 : 128;
		k->k = kb_realloc(k->k, (size_t)k->cap * sizeof(*k->k));
	}
	x = &k->k[k->n++];
	x->component = kb_strdup(component);
	x->action = kb_strdup(action);
	x->keys = kb_strdup(keys);
	x->label = label && *label ? kb_strdup(label) : NULL;
	x->group = -1;
}

/* KConfig's list form: elements split at an unescaped comma, `\,` a comma. */
static int split_list(char *v, char **out, int max)
{
	int n = 0;
	char *w = v;

	out[n++] = w;
	for (char *p = v; *p; p++) {
		if (*p == '\\' && p[1]) {
			*w++ = *++p;
			continue;
		}
		if (*p == ',' && n < max) {
			*w++ = '\0';
			out[n++] = w;
			continue;
		}
		*w++ = *p;
	}
	*w = '\0';
	return n;
}

/* The chords of one action as a person reads them: KConfig keeps several
 * separated by tabs, and `none` is the spelling of no key at all. */
static void readable(const char *in, char *out, size_t n)
{
	out[0] = '\0';
	for (const char *p = in; *p;) {
		size_t len = strcspn(p, "\t");

		if (len && strncmp(p, "none", len)) {
			size_t used = strlen(out);

			snprintf(out + used, n - used, "%s%.*s",
				 used ? " / " : "", (int)len, p);
		}
		p += len;
		if (*p)
			p++;
	}
}

static void read_rc(Keys *k, const char *path)
{
	char *data = kb_read_all(path, NULL), group[512] = "";

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n'), *eq, *el[3] = { 0 };
		char chords[256];
		int n;

		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = '\0';
		if (line[0] == '[') {
			char *end = strrchr(line, ']');

			if (end) {
				*end = '\0';
				kb_strlcpy(group, line + 1, sizeof(group));
			}
			continue;
		}
		if (!*group || line[0] == '#' || !(eq = strchr(line, '=')))
			continue;
		*eq = '\0';
		if (line[0] == '_' && strcmp(line, "_launch"))
			continue;	/* _k_friendly_name and the like */
		if (!strncmp(group, "services][", 10)) {
			char *v = kdt_kcfg_unescape(eq + 1);

			readable(v, chords, sizeof(chords));
			add(k, group + 10, line, chords, NULL);
			free(v);
			continue;
		}
		n = split_list(eq + 1, el, 3);
		readable(el[0], chords, sizeof(chords));
		add(k, group, line, chords, n > 2 ? el[2] : NULL);
	}
	free(data);
}

/* A desktop entry under kglobalaccel/: its launch key, and a key for each
 * action it declares. */
static void read_entry(Keys *k, const char *dir, const char *file)
{
	char *path = kb_path_join(dir, file), *name, *sc;
	char chords[256];

	name = kdt_kcfg_get(path, "Desktop Entry", "Name");
	sc = kdt_kcfg_get(path, "Desktop Entry", "X-KDE-Shortcuts");
	if (sc) {
		for (char *c = sc; *c; c++)
			if (*c == ',')
				*c = '\t';
		readable(sc, chords, sizeof(chords));
		add(k, file, "_launch", chords, name);
	}
	free(sc);
	free(name);
	{
		char *data = kb_read_all(path, NULL), *save = NULL;
		const char *pfx = "[Desktop Action ";

		for (char *l = data ? strtok_r(data, "\n", &save) : NULL; l;
		     l = strtok_r(NULL, "\n", &save)) {
			char grp[256], *end;

			if (strncmp(l, pfx, strlen(pfx)) ||
			    !(end = strrchr(l, ']')))
				continue;
			*end = '\0';
			snprintf(grp, sizeof(grp), "Desktop Action %s",
				 l + strlen(pfx));
			sc = kdt_kcfg_get(path, grp, "X-KDE-Shortcuts");
			name = kdt_kcfg_get(path, grp, "Name");
			if (sc) {
				for (char *c = sc; *c; c++)
					if (*c == ',')
						*c = '\t';
				readable(sc, chords, sizeof(chords));
				add(k, file, l + strlen(pfx), chords, name);
			}
			free(sc);
			free(name);
		}
		free(data);
	}
	free(path);
}

static void collect(Keys *k)
{
	char *user = kdt_cfg_home("kglobalshortcutsrc");
	const char *cdirs = getenv("XDG_CONFIG_DIRS");
	const char *ddirs = getenv("XDG_DATA_DIRS");
	char *copy, *save = NULL;

	read_rc(k, user);
	free(user);
	copy = kb_strdup(cdirs && *cdirs ? cdirs : "/etc/xdg");
	for (char *d = strtok_r(copy, ":", &save); d;
	     d = strtok_r(NULL, ":", &save)) {
		char *p = kb_path_join(d, "kglobalshortcutsrc");

		read_rc(k, p);
		free(p);
	}
	free(copy);

	save = NULL;
	copy = kb_strdup(ddirs && *ddirs ? ddirs
			 : "/usr/local/share:/usr/share");
	for (char *d = strtok_r(copy, ":", &save); d;
	     d = strtok_r(NULL, ":", &save)) {
		char *dir = kb_path_join(d, "kglobalaccel");
		int n = 0;
		char **v = kb_listdir(dir, &n);

		for (int i = 0; i < n; i++) {
			size_t l = strlen(v[i]);

			if (l > 8 && !strcmp(v[i] + l - 8, ".desktop"))
				read_entry(k, dir, v[i]);
		}
		kb_strv_free(v);
		free(dir);
	}
	free(copy);
}

static int by_group(const void *a, const void *b)
{
	const Key *x = a, *y = b;
	int gx = x->group < 0 ? NORDER : x->group;
	int gy = y->group < 0 ? NORDER : y->group;

	if (gx != gy)
		return gx - gy;
	return strcmp(x->label ? x->label : x->action,
		      y->label ? y->label : y->action);
}

int kdt_keys_print(FILE *o, const char *head, const char *key,
		   const char *reset)
{
	Keys k = { 0 };
	Rule *r;
	int nr = 0, shown = 0, last = -2;

	collect(&k);
	r = load_rules(&nr);
	for (int i = 0; i < k.n; i++)
		k.k[i].group = nr ? place(r, nr, k.k[i].component,
					  k.k[i].action) : -1;
	qsort(k.k, (size_t)k.n, sizeof(*k.k), by_group);
	for (int i = 0; i < k.n; i++) {
		const Key *x = &k.k[i];

		if (!*x->keys)
			continue;
		if (x->group != last) {
			fprintf(o, "%s%s%s%s\n", shown ? "\n" : "", head,
				x->group >= 0 ? ORDER[x->group] : "other",
				reset);
			last = x->group;
		}
		fprintf(o, "  %s%-28s%s %s", key, x->keys, reset,
			x->label ? x->label : x->action);
		if (x->group < 0 || !strcmp(x->action, "_launch"))
			fprintf(o, " (%s)", x->component);
		fputc('\n', o);
		shown++;
	}
	for (int i = 0; i < k.n; i++) {
		free(k.k[i].component);
		free(k.k[i].action);
		free(k.k[i].keys);
		free(k.k[i].label);
	}
	free(k.k);
	free(r);
	return shown;
}

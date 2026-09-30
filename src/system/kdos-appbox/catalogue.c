/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * The catalogue: every application this system knows how to build, as a parent
 * chain of apt packages.
 *
 * ONE PASS, AND THE ORDER OF THE FILE IS THE ORDER OF THE ARRAY. A parent must
 * appear above its children, which is what lets a chain be walked upward with
 * no second pass and no cycle check: a row can only name something already
 * read. Reorder the file and cat_chain() returns -1 naming the missing parent
 * rather than looping.
 *
 * A CHAIN IS RETURNED BASE-FIRST because that is the BUILD order — podman
 * builds a runtime FROM its base, so handing the caller the other order names
 * an image that does not exist yet.
 *
 * `meta` IS OPTIONAL AND ITS ABSENCE IS NOT AN ERROR. A row with none presents
 * as its own id, category Other, no tagline and size 0, and every surface
 * renders that. A row added today has no meta until somebody writes one, and
 * refusing to load would make adding software a two-file change for no
 * benefit.
 *
 * THE PACKAGE LIST IS EMPTY, NEVER "-". A row with no packages emits no apt
 * RUN at all, which is what lets a non-Debian base exist; "-" reaching apt is
 * a package name it cannot find.
 *
 * THE RAW TEXT IS KEPT AND RESCANNED for env, cmd, needs, deb and graft. These
 * are asked once per install over a two-hundred-row file; an index would be a
 * second structure to keep in step with the first.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-appbox.h"

static CatPack   g_pack[CAT_MAX_PACKS];
static int       g_npack;
static CatGroup  g_group[CAT_MAX_GROUPS];
static int       g_ngroup;
static char      g_snapshot[64] = "auto";
static char     *g_text;		/* the file, kept for the rescans   */

/* ── reading the file ──────────────────────────────────────────────────── */

/*
 * A `#` IS A COMMENT ONLY AS THE FIRST NON-BLANK CHARACTER. One inside a
 * tagline is ordinary text, and stripping it there truncates the tagline at
 * the first `#` somebody writes in a description.
 */
static char *skip_ws(char *p)
{
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

static int is_blank_or_comment(const char *line)
{
	const char *p = line;

	while (*p == ' ' || *p == '\t')
		p++;
	return !*p || *p == '#';
}

/* Next whitespace-delimited word, NUL-terminated in place. Returns NULL at
 * end of line. */
static char *word(char **p)
{
	char *s = skip_ws(*p), *e;

	if (!*s)
		return NULL;
	e = s;
	while (*e && *e != ' ' && *e != '\t')
		e++;
	if (*e)
		*e++ = 0;
	*p = e;
	return s;
}

static CatPack *pack_by_id(const char *id)
{
	for (int i = 0; i < g_npack; i++)
		if (!strcmp(g_pack[i].id, id))
			return &g_pack[i];
	return NULL;
}

static CatGroup *group_by_id(const char *id)
{
	for (int i = 0; i < g_ngroup; i++)
		if (!strcmp(g_group[i].id, id))
			return &g_group[i];
	return NULL;
}

/* `meta <id> <name>|<category>|<bytes>|<tagline>`. The id is taken as a word
 * first, so name and tagline keep their spaces; the rest splits on `|`. */
static void meta_row(CatPack *p, char *rest)
{
	char *f[4] = { NULL, NULL, NULL, NULL };
	int n = 0;

	rest = skip_ws(rest);
	f[n++] = rest;
	for (char *q = rest; *q && n < 4; q++)
		if (*q == '|') {
			*q = 0;
			f[n++] = q + 1;
		}
	if (f[0])
		kb_strlcpy(p->name, f[0], sizeof(p->name));
	if (f[1] && *f[1]) {
		/* A desktop entry's Categories is `Graphics;2DGraphics;` and a
		 * surface shows one. */
		char *semi = strchr(f[1], ';');
		if (semi)
			*semi = 0;
		kb_strlcpy(p->category, f[1], sizeof(p->category));
	}
	if (f[2])
		p->bytes = strtoull(f[2], NULL, 10);
	if (f[3])
		kb_strlcpy(p->tagline, f[3], sizeof(p->tagline));
}

int cat_load(const char *path, char *err, size_t errn)
{
	size_t len = 0;
	char *line, *save;

	cat_free();
	if (!path)
		path = getenv("KDOS_CATALOGUE");
	if (!path || !*path)
		path = CAT_PATH;

	g_text = kb_read_all(path, &len);
	if (!g_text) {
		snprintf(err, errn, "%s: cannot read the catalogue", path);
		return -1;
	}

	/* strtok_r writes NULs into g_text; the rescans below re-read it, so
	 * every scan walks the buffer itself rather than a tokenised copy. */
	char *copy = kb_calloc(1, len + 1);
	memcpy(copy, g_text, len);

	for (line = strtok_r(copy, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *p, *kind, *id, *parent;

		if (is_blank_or_comment(line))
			continue;
		p = line;
		kind = word(&p);
		if (!kind)
			continue;

		if (!strcmp(kind, "snapshot")) {
			/* `snapshot = auto` — skip the `=`. */
			char *eq = word(&p);
			char *v = eq && !strcmp(eq, "=") ? word(&p) : eq;
			if (v)
				kb_strlcpy(g_snapshot, v, sizeof(g_snapshot));
			continue;
		}

		if (!strcmp(kind, "meta")) {
			id = word(&p);
			if (id) {
				CatPack *q = pack_by_id(id);
				/* A meta row for a row that is not here names
				 * software the catalogue does not carry. It is
				 * dropped rather than refused: the surfaces
				 * list packs, so an orphan meta is invisible
				 * either way. */
				if (q)
					meta_row(q, p);
			}
			continue;
		}

		if (!strcmp(kind, "group")) {
			id = word(&p);
			if (!id)
				continue;
			CatGroup *g = group_by_id(id);
			if (!g) {
				if (g_ngroup >= CAT_MAX_GROUPS)
					continue;
				g = &g_group[g_ngroup++];
				kb_strlcpy(g->id, id, sizeof(g->id));
			}
			/* THE FIRST ROW FOR A GROUP IS ITS DESCRIPTION and
			 * every later one lists members. What tells them apart
			 * is the `app.`/`data.` prefix, so a description may
			 * be any prose that does not start a word with one.
			 *
			 * THE DESCRIPTION IS TAKEN BEFORE THE SCAN. word()
			 * NUL-terminates in place, so reading the rest of the
			 * line afterwards yields only its first word. */
			char desc[128];
			char *w, *rest = p;
			int any = 0;

			kb_strlcpy(desc, skip_ws(p), sizeof(desc));
			while ((w = word(&rest))) {
				if (strncmp(w, "app.", 4) &&
				    strncmp(w, "data.", 5))
					continue;
				any = 1;
				if (g->nmember < CAT_MAX_PACKS)
					kb_strlcpy(g->members[g->nmember++], w,
						   sizeof(g->members[0]));
			}
			if (!any && !g->desc[0])
				kb_strlcpy(g->desc, desc, sizeof(g->desc));
			continue;
		}

		/* Everything else is either a pack row or a directive the
		 * rescans read; the directives are skipped here. */
		if (strcmp(kind, "base") && strcmp(kind, "runtime") &&
		    strcmp(kind, "app") && strcmp(kind, "data"))
			continue;

		if (g_npack >= CAT_MAX_PACKS) {
			snprintf(err, errn,
				 "%s: more than %d rows", path, CAT_MAX_PACKS);
			free(copy);
			return -1;
		}
		id = word(&p);
		parent = word(&p);
		if (!id || !parent) {
			snprintf(err, errn, "%s: '%s' row is short", path, kind);
			free(copy);
			return -1;
		}
		CatPack *q = &g_pack[g_npack++];
		kb_strlcpy(q->kind, kind, sizeof(q->kind));
		kb_strlcpy(q->id, id, sizeof(q->id));
		if (strcmp(parent, "-"))
			kb_strlcpy(q->parent, parent, sizeof(q->parent));
		kb_strlcpy(q->name, id, sizeof(q->name));
		kb_strlcpy(q->category, "Other", sizeof(q->category));
		/* "-" means no packages and must present as empty: it reaching
		 * apt is a package name it cannot find. */
		char *pkgs = skip_ws(p);
		if (!strcmp(pkgs, "-"))
			pkgs = (char *)"";
		q->packages = strdup(pkgs);
	}
	free(copy);

	/* The `image` rows, after every pack is known, so a base may be named
	 * before or after its image. */
	copy = kb_calloc(1, len + 1);
	memcpy(copy, g_text, len);
	for (line = strtok_r(copy, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *p = line, *k, *id, *ref;

		if (is_blank_or_comment(line))
			continue;
		k = word(&p);
		if (!k || strcmp(k, "image"))
			continue;
		id = word(&p);
		ref = word(&p);
		if (!id || !ref)
			continue;
		CatPack *q = pack_by_id(id);
		if (q)
			kb_strlcpy(q->image, ref, sizeof(q->image));
	}
	free(copy);
	return 0;
}

void cat_free(void)
{
	for (int i = 0; i < g_npack; i++)
		free(g_pack[i].packages);
	memset(g_pack, 0, sizeof(g_pack));
	memset(g_group, 0, sizeof(g_group));
	g_npack = g_ngroup = 0;
	free(g_text);
	g_text = NULL;
	kb_strlcpy(g_snapshot, "auto", sizeof(g_snapshot));
}

/* ── what is in it ─────────────────────────────────────────────────────── */

int cat_count(void) { return g_npack; }
const CatPack *cat_find(const char *id) { return pack_by_id(id); }
int cat_ngroups(void) { return g_ngroup; }
const CatGroup *cat_group_at(int i)
{
	return i >= 0 && i < g_ngroup ? &g_group[i] : NULL;
}
const CatGroup *cat_group_find(const char *id) { return group_by_id(id); }
const char *cat_snapshot(void) { return g_snapshot; }

/*
 * The catalogue has no version field, so its identity is what it holds: the
 * row count and its byte length, which move together on any edit. Provenance
 * for an exported set — the packs in one carry their own hashes, so this never
 * has to be trusted, only read.
 */
const char *cat_version(void)
{
	static char v[64];

	snprintf(v, sizeof(v), "%d rows, %zu bytes", g_npack,
		 g_text ? strlen(g_text) : (size_t)0);
	return v;
}

int cat_chain(const char *id, const CatPack *out[CAT_CHAIN_MAX])
{
	const CatPack *up[CAT_CHAIN_MAX];
	int n = 0;

	for (const CatPack *q = pack_by_id(id); q; q = q->parent[0]
						 ? pack_by_id(q->parent) : NULL) {
		if (n >= CAT_CHAIN_MAX)
			return -1;
		up[n++] = q;
		if (!q->parent[0])
			break;
		/* A parent that is not in the file is a chain that cannot be
		 * closed, and the row above would otherwise be built against
		 * an image nothing creates. */
		if (!pack_by_id(q->parent))
			return -1;
	}
	if (!n)
		return -1;
	for (int i = 0; i < n; i++)
		out[i] = up[n - 1 - i];	/* base-first: the build order */
	return n;
}

/* ── the rescans ───────────────────────────────────────────────────────── */

/*
 * Walk every line whose first word is `want` and whose second is `id`, handing
 * the rest to `fn`. One walk shared by five directives, so a row type added to
 * the file needs one caller rather than one parser.
 */
static void each_row(const char *want, const char *id,
		     void (*fn)(char *rest, void *ctx), void *ctx)
{
	char *copy, *line, *save;
	size_t len;

	if (!g_text)
		return;
	len = strlen(g_text);
	copy = kb_calloc(1, len + 1);
	memcpy(copy, g_text, len);
	for (line = strtok_r(copy, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *p = line, *k, *who;

		if (is_blank_or_comment(line))
			continue;
		k = word(&p);
		if (!k || strcmp(k, want))
			continue;
		who = word(&p);
		if (!who || strcmp(who, id))
			continue;
		fn(p, ctx);
	}
	free(copy);
}

struct collect {
	char (*out)[256];
	int max, n;
};

static void take_line(char *rest, void *ctx)
{
	struct collect *c = ctx;

	if (c->n < c->max)
		kb_strlcpy(c->out[c->n++], skip_ws(rest), 256);
}

int cat_env(const char *id, char out[][256], int max)
{
	const CatPack *chain[CAT_CHAIN_MAX];
	struct collect c = { out, max, 0 };
	int n = cat_chain(id, chain);

	if (n < 0)
		return 0;
	/* BASE-FIRST, so a runtime's variable is set for an application that
	 * never declares one and an application's own row wins a collision by
	 * being exported last. */
	for (int i = 0; i < n; i++)
		each_row("env", chain[i]->id, take_line, &c);
	return c.n;
}

struct collect64 {
	char (*out)[64];
	int max, n;
};

static void take_word64(char *rest, void *ctx)
{
	struct collect64 *c = ctx;
	char *w = word(&rest);

	if (w && c->n < c->max)
		kb_strlcpy(c->out[c->n++], w, 64);
}

int cat_cmds(const char *id, char out[][64], int max)
{
	struct collect64 c = { out, max, 0 };

	each_row("cmd", id, take_word64, &c);
	return c.n;
}

struct collectid {
	char (*out)[CAT_ID_MAX];
	int max, n;
};

static void take_words_id(char *rest, void *ctx)
{
	struct collectid *c = ctx;
	char *w;

	while ((w = word(&rest)) && c->n < c->max)
		kb_strlcpy(c->out[c->n++], w, CAT_ID_MAX);
}

int cat_needs(const char *id, char out[][CAT_ID_MAX], int max)
{
	struct collectid c = { out, max, 0 };

	each_row("needs", id, take_words_id, &c);
	return c.n;
}

struct debrow {
	char *url; size_t un;
	char *pat; size_t pn;
	int found;
};

static void take_deb(char *rest, void *ctx)
{
	struct debrow *d = ctx;
	char *u = word(&rest), *p = word(&rest);

	if (!u || !p)
		return;
	kb_strlcpy(d->url, u, d->un);
	kb_strlcpy(d->pat, p, d->pn);
	d->found = 1;
}

int cat_deb(const char *id, char *url, size_t un, char *pat, size_t pn)
{
	struct debrow d = { url, un, pat, pn, 0 };

	each_row("deb", id, take_deb, &d);
	return d.found ? 0 : -1;
}

struct graftrow {
	char (*from)[256];
	char (*to)[256];
	int max, n;
};

static void take_graft(char *rest, void *ctx)
{
	struct graftrow *g = ctx;
	char *f = word(&rest), *t = word(&rest);

	if (!f || !t || g->n >= g->max)
		return;
	kb_strlcpy(g->from[g->n], f, 256);
	kb_strlcpy(g->to[g->n], t, 256);
	g->n++;
}

int cat_grafts(const char *id, int box, char from[][256], char to[][256],
	       int max)
{
	struct graftrow g = { from, to, max, 0 };

	each_row(box ? "boxgraft" : "graft", id, take_graft, &g);
	return g.n;
}

/* ── expanding a selection ─────────────────────────────────────────────── */

static int already(char out[][CAT_ID_MAX], int n, const char *id)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(out[i], id))
			return 1;
	return 0;
}

int cat_expand(const char *const *ids, int n, char out[][CAT_ID_MAX], int max,
	       char *err, size_t errn)
{
	int nout = 0;

	for (int i = 0; i < n; i++) {
		const CatGroup *g = group_by_id(ids[i]);

		if (g) {
			for (int m = 0; m < g->nmember && nout < max; m++)
				if (!already(out, nout, g->members[m]))
					kb_strlcpy(out[nout++], g->members[m],
						   CAT_ID_MAX);
			continue;
		}
		const CatPack *p = pack_by_id(ids[i]);
		if (!p || (strcmp(p->kind, "app") && strcmp(p->kind, "data"))) {
			snprintf(err, errn,
				 "%s: not an application or a group", ids[i]);
			return -1;
		}
		if (nout < max && !already(out, nout, p->id))
			kb_strlcpy(out[nout++], p->id, CAT_ID_MAX);
	}
	return nout;
}

/* ── the verb ──────────────────────────────────────────────────────────── */

/*
 * TAB-SEPARATED, because a tagline has spaces and every other reader in this
 * tree splits a table on tab.
 *
 * THE STATE IS REPORTED HERE AND NOWHERE ELSE. Three surfaces ask what is
 * installed — the store, kinstall and `kdos app` — and three joins against
 * podman would be three chances to disagree about what "installed" means.
 * A box that podman lists under a catalogue id IS that application; nothing
 * else counts, because nothing else could be launched.
 *
 * A machine with no podman reports everything `available`, which is what such
 * a machine can honestly say.
 */
static void installed_set(char *buf, size_t n)
{
	KbArgv a = {0};

	buf[0] = 0;
	kb_argv_add(&a, "podman");
	kb_argv_add(&a, "ps");
	kb_argv_add(&a, "--all");
	kb_argv_add(&a, "--format");
	kb_argv_add(&a, "{{.Names}}");
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, n) != 0)
		buf[0] = 0;
}

static int in_set(const char *set, const char *id)
{
	const char *p = set;
	size_t l = strlen(id);

	while ((p = strstr(p, id))) {
		int left = (p == set) || p[-1] == '\n';
		int right = p[l] == 0 || p[l] == '\n';
		if (left && right)
			return 1;
		p += l;
	}
	return 0;
}

int cmd_catalogue(int argc, char **argv)
{
	char err[256];
	char *inst;

	if (cat_load(NULL, err, sizeof(err)) != 0) {
		fprintf(stderr, "kdos-appbox: %s\n", err);
		return 1;
	}
	if (argc > 0 && !strcmp(argv[0], "--groups")) {
		for (int i = 0; i < g_ngroup; i++) {
			const CatGroup *g = &g_group[i];
			printf("%s\t%s\t", g->id, g->desc);
			for (int m = 0; m < g->nmember; m++)
				printf("%s%s", m ? " " : "", g->members[m]);
			printf("\n");
		}
		cat_free();
		return 0;
	}
	inst = kb_calloc(1, 1 << 16);
	installed_set(inst, 1 << 16);
	for (int i = 0; i < g_npack; i++) {
		const CatPack *p = &g_pack[i];

		if (strcmp(p->kind, "app") && strcmp(p->kind, "data"))
			continue;
		printf("%s\t%s\t%s\t%llu\t%s\t%s\t%s\n", p->id, p->name,
		       p->category, p->bytes,
		       p->parent[0] ? p->parent : "-",
		       in_set(inst, p->id) ? "installed" : "available",
		       p->tagline);
	}
	free(inst);
	cat_free();
	return 0;
}

/* ── --selftest ────────────────────────────────────────────────────────── */

/*
 * The parser against testing/fixtures/catalogue, offline and pure. Every
 * assertion is a rule a surface depends on: a chain that is not base-first
 * builds a runtime on top of the application that needs it, and an expand that
 * keeps duplicates installs the same box twice.
 */
int cat_fail;

void cat_chk(int cond, const char *what)
{
	if (!cond) {
		fprintf(stderr, "  FAIL %s\n", what);
		cat_fail++;
	}
}

/* A lookup that cannot return NULL, so an assertion below reads a field
 * rather than crashing on a row the fixture lost. A miss is a failure here and
 * every field then reads empty, which fails its own assertion too. */
static const CatPack *must(const char *id)
{
	static const CatPack none;
	const CatPack *p = cat_find(id);

	if (!p) {
		cat_chk(0, id);
		return &none;
	}
	return p;
}

static const CatGroup *must_group(const char *id)
{
	static const CatGroup none;
	const CatGroup *g = cat_group_find(id);

	if (!g) {
		cat_chk(0, id);
		return &none;
	}
	return g;
}

int cat_selftest(void)
{
	const CatPack *chain[CAT_CHAIN_MAX];
	char err[256], env[8][256], ids[16][CAT_ID_MAX];
	char from[4][256], to[4][256], cmds[4][64];
	char nd[4][CAT_ID_MAX];
	char url[256], pat[64];
	const char *want[] = { "app.one", "beta" };
	const char *nosuch[] = { "nosuch" };
	int n;

	if (cat_load(NULL, err, sizeof(err)) != 0) {
		fprintf(stderr, "cat_load: %s\n", err);
		return 1;
	}

	cat_chk(cat_count() == 8, "eight packs");
	cat_chk(cat_ngroups() == 2, "two groups");
	cat_chk(!strcmp(cat_snapshot(), "20260824T000000Z"), "snapshot literal");

	cat_chk(!strcmp(must("alpine")->image, "alpine:3.24.1"),
		"alpine names its own image");
	cat_chk(must("base")->image[0] == 0, "base has no image row");

	/* BASE-FIRST IS THE BUILD ORDER. */
	n = cat_chain("app.two", chain);
	cat_chk(n == 4, "app.two chain is four deep");
	if (n == 4) {
		cat_chk(!strcmp(chain[0]->id, "base"), "chain[0] is the base");
		cat_chk(!strcmp(chain[1]->id, "rt-gtk"), "chain[1] is rt-gtk");
		cat_chk(!strcmp(chain[2]->id, "rt-extra"), "chain[2] is rt-extra");
		cat_chk(!strcmp(chain[3]->id, "app.two"), "chain[3] is app.two");
	}
	cat_chk(cat_chain("app.nosuch", chain) == -1, "an unknown id has no chain");

	cat_chk(!strcmp(must("app.bare")->packages, ""),
		"a '-' package list presents as empty");
	cat_chk(!strcmp(must("app.one")->packages, "one"), "one's packages");

	cat_chk(!strcmp(must("app.one")->name, "One"), "app.one name");
	cat_chk(!strcmp(must("app.one")->category, "Graphics"), "app.one category");
	cat_chk(must("app.one")->bytes == 1048576, "app.one bytes");
	cat_chk(!strcmp(must("app.one")->tagline, "The first application"),
		"app.one tagline keeps its spaces");
	cat_chk(!strcmp(must("app.bare")->name, "app.bare"),
		"no meta falls back to the id");
	cat_chk(!strcmp(must("app.bare")->category, "Other"),
		"no meta falls back to Other");
	cat_chk(must("app.bare")->bytes == 0, "no meta is size 0");

	n = cat_env("app.two", env, 8);
	cat_chk(n == 2, "app.two has two env rows");
	if (n == 2) {
		cat_chk(!strcmp(env[0], "GDK_BACKEND=wayland"),
			"the runtime's env comes first");
		cat_chk(!strcmp(env[1], "QT_STYLE_OVERRIDE=Fusion"),
			"the app's env comes second");
	}

	cat_chk(cat_cmds("app.bare", cmds, 4) == 1 && !strcmp(cmds[0], "bare"),
		"bare's cmd");
	cat_chk(cat_needs("app.one", nd, 4) == 1 && !strcmp(nd[0], "data.dat"),
		"one needs dat");
	cat_chk(cat_deb("app.two", url, sizeof(url), pat, sizeof(pat)) == 0 &&
		!strcmp(pat, "amd64.deb"), "app.two's deb row");
	cat_chk(cat_deb("app.one", url, sizeof(url), pat, sizeof(pat)) == -1,
		"one has no deb row");

	cat_chk(cat_grafts("data.dat", 0, from, to, 4) == 1 &&
		!strcmp(to[0], "dat"), "graft");
	cat_chk(cat_grafts("data.dat", 1, from, to, 4) == 1 &&
		!strcmp(to[0], "boxdat"), "boxgraft");

	cat_chk(cat_group_find("alpha") != NULL, "alpha is a group");
	cat_chk(!strcmp(must_group("alpha")->desc, "The first group"),
		"the first row is the description");
	cat_chk(must_group("alpha")->nmember == 2, "alpha has two members");

	/* EXPAND DEDUPES: app.two is in both groups, and installing it twice
	 * is a second podman build of an image that already exists. */
	n = cat_expand(want, 2, ids, 16, err, sizeof(err));
	cat_chk(n == 3, "app.one + beta expands to three, not four");

	cat_chk(cat_expand(nosuch, 1, ids, 16, err, sizeof(err)) == -1,
		"an unknown name is refused");

	cat_free();
	printf("catalogue: %s\n", cat_fail ? "FAILED" : "ok");
	return cat_fail ? 1 : 0;
}

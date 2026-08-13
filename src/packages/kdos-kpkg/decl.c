/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The recipe — declarative metadata, and nothing else
 *
 *   name        = zlib
 *   version     = 1.3.1
 *   release     = 1
 *   source      = https://zlib.net/$name-$version.tar.gz
 *   description = Compression library implementing the deflate method
 *   homepage    = https://zlib.net
 *   depends     = musl
 *
 * The BUILD is `build.sh` beside this file, and the hook is `postinstall.sh`.
 * Presence is the contract — there is no key naming them, so there is nothing
 * to keep in sync across 396 recipes.
 *
 * WHY THE SHELL LIVES IN ITS OWN FILE. It is a shell script, so it should be
 * one: `bash -n`, shellcheck, syntax highlighting and `git diff` all work on a
 * file and none of them work on a blob inside a config format. And no parser
 * ever has to understand it — `ports/core/rust/build.sh` contains a heredoc
 * whose body has a line reading exactly `[build]`, and `podman`'s has
 * `[engine]`, `[network]`, `[storage]`. Any format that carried the shell
 * inline would have to tell those apart from its own syntax.
 *
 * WHAT THIS BUYS, and it is the read path rather than the build path: kpkg no
 * longer runs bash to READ a recipe. It used to source the file and ask bash
 * to print the fields back, and serialise `postinstall()` with `declare -f`.
 * Metadata is parsed here, statically; bash is exec'd only to run a build.
 *
 * Any key that is not one of ours is a recipe-local helper — `_tag`, `vrsn`,
 * `_triplet` — which is how a version gets reshaped for a URL that `$version`
 * alone cannot spell. Values expand `$var`, `${var}` and the parameter forms
 * the recipes actually use: `${var#pat}` `${var##pat}` `${var%pat}`
 * `${var%%pat}` `${var/a/b}` `${var//a/b}` `${var:off:len}`.
 * ---------------------------------
 */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-kpkg.h"

#define MAX_VARS 32

struct KpDecl {
	char name[128];
	char version[128];
	char release[64];
	char source[2048];
	char sha256[4096];
	char description[512];
	char homepage[256];
	char depends[1024];
	char vendoring[64];
	char pypackages[512];
	/* The Alpine package name this port maps onto, for `kdos cve`. Declared
	 * only when it differs — the kernel is `linux` here and `linux-lts`
	 * there — and it is a KEY rather than a helper so it is not expanded and
	 * not carried into the build environment. */
	char secdb[128];
	/* How `kdos march` measures this port: a setup line that is run once and
	 * not timed, and the line that IS timed. Keys rather than helpers so
	 * they are not expanded and never reach the build environment. */
	char bench[512];
	char bench_setup[512];
	/* "KEY=VALUE", in declaration order: a later helper may refer to an
	 * earlier one, which `_cargo = $_rust` does. */
	char var[MAX_VARS][512];
	int nvars;
};

/* ──────────────────────────────────────────────────────────────────────── */
/* Pattern matching for ${var#pat} and friends
 *
 * Shell glob against a whole string: `*` and `?` and nothing else. The
 * recipes use `${version%.*}`, `${version##*.}`, `${version#????}` — that is
 * the entire vocabulary in the tree. */

static int glob_match(const char *pat, size_t plen, const char *s, size_t slen)
{
	size_t p = 0, i = 0, star = (size_t)-1, mark = 0;
	while (i < slen) {
		if (p < plen && (pat[p] == '?' || pat[p] == s[i])) {
			p++;
			i++;
		} else if (p < plen && pat[p] == '*') {
			star = p++;
			mark = i;
		} else if (star != (size_t)-1) {
			p = star + 1;
			i = ++mark;
		} else {
			return 0;
		}
	}
	while (p < plen && pat[p] == '*')
		p++;
	return p == plen;
}

/* `#` shortest / `##` longest leading, `%` shortest / `%%` longest trailing.
 * Nothing matching means the value comes back untouched, as in the shell. */
static void strip_pattern(const char *val, const char *pat, size_t plen,
			  int trailing, int longest, KbBuf *out)
{
	size_t vlen = strlen(val);

	if (!trailing) {
		/* Cut off val[0..n). Shortest match wants the smallest n. */
		for (size_t k = 0; k <= vlen; k++) {
			size_t n = longest ? vlen - k : k;
			if (glob_match(pat, plen, val, n)) {
				kb_buf_str(out, val + n);
				return;
			}
		}
		kb_buf_str(out, val);
		return;
	}

	/* Keep val[0..n). Shortest match wants the largest n. */
	for (size_t k = 0; k <= vlen; k++) {
		size_t n = longest ? k : vlen - k;
		if (glob_match(pat, plen, val + n, vlen - n)) {
			kb_buf_add(out, val, n);
			return;
		}
	}
	kb_buf_str(out, val);
}

/* `${var/a/b}` replaces the first occurrence, `${var//a/b}` all of them. The
 * needle is literal here: no recipe uses a glob on the left of a `/`. */
static void substitute(const char *val, const char *pat, size_t plen, int all,
		       KbBuf *out)
{
	const char *slash = memchr(pat, '/', plen);
	size_t nlen = slash ? (size_t)(slash - pat) : plen;
	const char *rep = slash ? slash + 1 : "";
	size_t rlen = slash ? plen - nlen - 1 : 0;

	if (!nlen) {
		kb_buf_str(out, val);
		return;
	}
	for (const char *s = val; *s;) {
		if (!strncmp(s, pat, nlen)) {
			kb_buf_add(out, rep, rlen);
			s += nlen;
			if (!all) {
				kb_buf_str(out, s);
				return;
			}
			continue;
		}
		kb_buf_add(out, s++, 1);
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static const char *lookup(const KpDecl *d, const char *var, const KpPaths *p)
{
	if (!strcmp(var, "name"))
		return d->name;
	if (!strcmp(var, "version"))
		return d->version;
	if (!strcmp(var, "release"))
		return d->release;
	if (p) {
		if (!strcmp(var, "PORT_SRC"))
			return p->port;
		if (!strcmp(var, "SRC"))
			return p->src;
		if (!strcmp(var, "SRC_ROOT"))
			return p->src_root;
		if (!strcmp(var, "PKG"))
			return p->pkg;
	}
	/* A recipe helper beats the environment: the recipe is the thing being
	 * read, and an unrelated variable of the same name in the build
	 * container is not it. */
	for (int i = 0; i < d->nvars; i++) {
		const char *eq = strchr(d->var[i], '=');
		size_t n = eq ? (size_t)(eq - d->var[i]) : 0;
		if (n && !strncmp(d->var[i], var, n) && !var[n])
			return eq + 1;
	}
	return getenv(var);
}

/* Expanded INTO the element, never across it: a value with a space in it
 * stays one value, and nothing here ever becomes shell syntax. */
char *kp_decl_expand(const KpDecl *d, const char *text, const KpPaths *p)
{
	KbBuf b = {0};
	for (const char *s = text; *s;) {
		if (*s != '$') {
			kb_buf_add(&b, s++, 1);
			continue;
		}
		s++;

		char var[64];
		size_t n = 0;
		if (*s != '{') {
			while (*s && n < sizeof(var) - 1 &&
			       (isalnum((unsigned char)*s) || *s == '_'))
				var[n++] = *s++;
			var[n] = 0;
			const char *v = lookup(d, var, p);
			if (v)
				kb_buf_str(&b, v);
			continue;
		}

		s++;			/* past the '{' */
		while (*s && n < sizeof(var) - 1 &&
		       (isalnum((unsigned char)*s) || *s == '_'))
			var[n++] = *s++;
		var[n] = 0;

		const char *op = s;
		const char *close = strchr(s, '}');
		if (!close) {		/* unterminated: emit nothing */
			s += strlen(s);
			continue;
		}
		s = close + 1;

		const char *v = lookup(d, var, p);
		if (!v)
			continue;
		if (op == close) {	/* plain ${var} */
			kb_buf_str(&b, v);
			continue;
		}

		size_t oplen = (size_t)(close - op);
		if (*op == '#') {
			int longest = oplen > 1 && op[1] == '#';
			strip_pattern(v, op + 1 + longest, oplen - 1 - longest,
				      0, longest, &b);
		} else if (*op == '%') {
			int longest = oplen > 1 && op[1] == '%';
			strip_pattern(v, op + 1 + longest, oplen - 1 - longest,
				      1, longest, &b);
		} else if (*op == '/') {
			int all = oplen > 1 && op[1] == '/';
			substitute(v, op + 1 + all, oplen - 1 - all, all, &b);
		} else if (*op == ':') {
			/* ${var:off:len} — `linux` builds its mirror path from
			 * `v${version:0:1}.x`. */
			char spec[64];
			size_t sn = oplen - 1 < sizeof(spec) ? oplen - 1
							     : sizeof(spec) - 1;
			memcpy(spec, op + 1, sn);
			spec[sn] = 0;
			char *colon = strchr(spec, ':');
			if (colon)
				*colon = 0;
			long off = strtol(spec, NULL, 10);
			size_t vlen = strlen(v);
			if (off < 0)
				off = 0;
			if ((size_t)off > vlen)
				off = (long)vlen;
			size_t want = vlen - (size_t)off;
			if (colon) {
				long take = strtol(colon + 1, NULL, 10);
				if (take < 0)
					take = 0;
				if ((size_t)take < want)
					want = (size_t)take;
			}
			kb_buf_add(&b, v + off, want);
		} else {
			kb_buf_str(&b, v);	/* unknown form: the value */
		}
	}
	char *out = kb_strdup(b.p ? b.p : "");
	kb_buf_free(&b);
	return out;
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Parsing                                                                  */

static char *trim(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
		s[--n] = 0;
	return s;
}

static int identifier(const char *s, size_t n)
{
	if (!n || (!isalpha((unsigned char)s[0]) && s[0] != '_'))
		return 0;
	for (size_t i = 0; i < n; i++)
		if (!isalnum((unsigned char)s[i]) && s[i] != '_')
			return 0;
	return 1;
}

/* `key = value`. The spaces are required: this is a config file, not a shell
 * assignment, and demanding them keeps the two from ever being confused. */
static const char *assignment(const char *line, char *name, size_t cap)
{
	const char *s = line;
	while (*s && *s != ' ' && *s != '\t' && *s != '=')
		s++;
	size_t n = (size_t)(s - line);
	if (!identifier(line, n) || n >= cap)
		return NULL;

	const char *v = s;
	while (*v == ' ' || *v == '\t')
		v++;
	if (*v != '=')
		return NULL;
	v++;
	while (*v == ' ' || *v == '\t')
		v++;

	memcpy(name, line, n);
	name[n] = 0;
	return v;
}

static void set_key(KpDecl *d, const char *key, const char *val)
{
	struct {
		const char *key;
		char *dst;
		size_t cap;
		int append;
	} F[] = {
		{ "name", d->name, sizeof(d->name), 0 },
		{ "version", d->version, sizeof(d->version), 0 },
		{ "release", d->release, sizeof(d->release), 0 },
		/* Repeatable: a port with two tarballs writes two lines, and
		 * the extractor splits this on whitespace anyway. */
		{ "source", d->source, sizeof(d->source), 1 },
		/* Repeatable, and matched to a source by BASENAME rather than
		 * by position: `sha256 = <64 hex>  <filename>`. Void and
		 * Chimera use positional arrays, which silently miscompare the
		 * moment someone reorders the source lines — and KDOS recipes
		 * already have an order-sensitive helper block above them.
		 * Appending joins entries with a space, so the accumulated
		 * value reads as alternating <hex> <name> tokens; a hash is
		 * always 64 characters and a tarball name never contains
		 * whitespace, so that stays unambiguous. */
		{ "sha256", d->sha256, sizeof(d->sha256), 1 },
		{ "description", d->description, sizeof(d->description), 0 },
		{ "homepage", d->homepage, sizeof(d->homepage), 0 },
		{ "depends", d->depends, sizeof(d->depends), 1 },
		{ "vendoring", d->vendoring, sizeof(d->vendoring), 0 },
		{ "pypackages", d->pypackages, sizeof(d->pypackages), 1 },
		{ "secdb", d->secdb, sizeof(d->secdb), 0 },
		{ "bench", d->bench, sizeof(d->bench), 0 },
		{ "bench_setup", d->bench_setup, sizeof(d->bench_setup), 0 },
		{ NULL, NULL, 0, 0 }
	};
	for (int i = 0; F[i].key; i++) {
		if (strcmp(key, F[i].key))
			continue;
		if (F[i].append && F[i].dst[0]) {
			size_t at = strlen(F[i].dst);
			snprintf(F[i].dst + at, F[i].cap - at, " %s", val);
		} else {
			kb_strlcpy(F[i].dst, val, F[i].cap);
		}
		return;
	}

	if (d->nvars < MAX_VARS)
		snprintf(d->var[d->nvars++], sizeof(d->var[0]), "%s=%s", key,
			 val);
}

KpDecl *kp_decl_parse(const char *path)
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	if (!data)
		return NULL;

	KpDecl *d = kb_calloc(1, sizeof(*d));

	for (char *line = data, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		line = trim(line);
		if (!*line || *line == '#')
			continue;

		char key[128];
		const char *val = assignment(line, key, sizeof(key));
		if (!val)
			continue;

		/* Helpers may refer to earlier ones, and `source` refers to
		 * `$name`/`$version`, so a value is expanded as it is read. */
		char *e = kp_decl_expand(d, val, NULL);
		set_key(d, key, e);
		free(e);
	}

	free(data);
	if (!d->name[0] || !d->version[0] || !d->release[0]) {
		free(d);
		return NULL;
	}
	return d;
}

void kp_decl_free(KpDecl *d)
{
	free(d);
}

const char *kp_decl_name(const KpDecl *d) { return d->name; }
const char *kp_decl_version(const KpDecl *d) { return d->version; }
const char *kp_decl_release(const KpDecl *d) { return d->release; }
const char *kp_decl_source(const KpDecl *d) { return d->source; }
const char *kp_decl_sha256(const KpDecl *d) { return d->sha256; }
const char *kp_decl_description(const KpDecl *d) { return d->description; }
const char *kp_decl_depends(const KpDecl *d) { return d->depends; }

/* ──────────────────────────────────────────────────────────────────────── */

/* Single-quoted for a shell prelude: the only character that cannot appear
 * inside '' is ', and `'\''` is how it is spelled. */
static void quote_into(KbBuf *b, const char *s)
{
	kb_buf_str(b, "'");
	for (; *s; s++) {
		if (*s == '\'')
			kb_buf_str(b, "'\\''");
		else
			kb_buf_add(b, s, 1);
	}
	kb_buf_str(b, "'");
}

/* The variables a build.sh expects, as shell assignments. The recipe no
 * longer sources itself, so kpkg injects what it parsed — and every value is
 * quoted, so nothing in a recipe can become syntax on the way through. */
void kp_decl_prelude(const KpDecl *d, KbBuf *b)
{
	static const struct { const char *n; size_t off; } SIMPLE[] = {
		{ "name", offsetof(struct KpDecl, name) },
		{ "version", offsetof(struct KpDecl, version) },
		{ "release", offsetof(struct KpDecl, release) },
		{ "source", offsetof(struct KpDecl, source) },
		{ "sha256", offsetof(struct KpDecl, sha256) },
		{ "vendoring", offsetof(struct KpDecl, vendoring) },
		{ "pypackages", offsetof(struct KpDecl, pypackages) },
		{ "secdb", offsetof(struct KpDecl, secdb) },
		{ "bench", offsetof(struct KpDecl, bench) },
		{ "bench_setup", offsetof(struct KpDecl, bench_setup) },
	};
	for (size_t i = 0; i < sizeof(SIMPLE) / sizeof(SIMPLE[0]); i++) {
		const char *v = (const char *)d + SIMPLE[i].off;
		if (!*v)
			continue;
		kb_buf_printf(b, "%s=", SIMPLE[i].n);
		quote_into(b, v);
		kb_buf_str(b, "\n");
	}
	for (int i = 0; i < d->nvars; i++) {
		const char *eq = strchr(d->var[i], '=');
		if (!eq)
			continue;
		kb_buf_printf(b, "%.*s=", (int)(eq - d->var[i]), d->var[i]);
		quote_into(b, eq + 1);
		kb_buf_str(b, "\n");
	}
}

/* `kpkg meta` — the same assignments, for ports/fetch to eval. It used to
 * `. ./kpkgbuild`, which stopped being possible when a recipe stopped being
 * a shell script. */
void kp_decl_meta(const KpDecl *d)
{
	KbBuf b = {0};
	kp_decl_prelude(d, &b);
	fputs(b.p ? b.p : "", stdout);
	kb_buf_free(&b);
}

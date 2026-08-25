/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — CLI, review loop and transactional accept
 *
 * Everything upstream of this file is a pure decision (pu_check) or a pure
 * edit (pu_rewrite_version). This file is where a maintainer's "yes" turns
 * into disk state, so it owns two things nothing else in the tool does: the
 * cache (so a repeat run is instant) and the transaction (so a bad fetch
 * never leaves a recipe pointing at a tarball that isn't there).
 * ---------------------------------
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kbuild.h"
#include "kpkg.h"
#include "portup.h"

#define MAX_PORTS  512		/* kp_all_ports' own internal ceiling       */
#define GROUP_MAX  64		/* generous headroom over the 17-member max */
#define CACHE_TTL  86400	/* 24h, per the design                      */

/* ────────────────────────────────────────────────────────────────────────
 * CLI options
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int check;
	int cve;
	int json;
	int no_fetch;
	int refresh;
	const char *fixture;
} Opts;

/* ────────────────────────────────────────────────────────────────────────
 * Repo location and the on-demand kpkg-meta build
 *
 * `ports/update` (the wrapper) always runs this binary from
 * <repo_root>/ports/.portup, so walking two directories up from
 * /proc/self/exe finds repo_root without the caller ever having to say so —
 * the same trick ports/fetch's own compile-on-demand block relies on via
 * $SCRIPT_DIR. KDOS_PORTUP_REPO is a maintenance escape hatch, not part of
 * the documented CLI: it is what lets this binary be pointed at a scratch
 * copy of the tree for testing without touching /proc/self/exe at all.
 * ──────────────────────────────────────────────────────────────────────── */

static void find_repo_root(char *out, size_t cap)
{
	const char *env = getenv("KDOS_PORTUP_REPO");
	if (env && *env) {
		kb_strlcpy(out, env, cap);
		return;
	}

	char exe[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (n <= 0) {
		kb_strlcpy(out, ".", cap);
		return;
	}
	exe[n] = 0;

	char *slash = strrchr(exe, '/');	/* strip the binary's own name */
	if (slash)
		*slash = 0;
	slash = strrchr(exe, '/');		/* strip "ports"                */
	if (slash)
		*slash = 0;
	kb_strlcpy(out, exe, cap);
}

/* Every *.c file directly inside `dir`, as argv elements. The paths kb_argv
 * ends up holding are kb_path_join's own allocations, independent of the
 * listing, so freeing the listing right after is safe. */
static void add_c_files(KbArgv *a, const char *dir)
{
	char **names = kb_listdir(dir, NULL);
	for (char **p = names; p && *p; p++) {
		size_t l = strlen(*p);
		if (l > 2 && !strcmp(*p + l - 2, ".c"))
			kb_argv_add(a, kb_path_join(dir, *p));
	}
	kb_strv_free(names);
}

/* `kpkg meta` is the one parser this tool trusts for a recipe's expanded
 * fields (see portup.h). Building it here mirrors ports/fetch's own
 * compile-on-demand block for ports/.kpkg-meta — same sources, same flags —
 * except for the OUTPUT NAME, and that difference is load-bearing.
 * kdos-kpkg dispatches on argv[0]'s basename (main.c's `self`), falling back
 * to argv[1] only when self itself is not one of the five tool names; a
 * binary named anything other than exactly "kpkg" therefore has no way to
 * reach the "meta" subcommand by invoking `<bin> meta <dir>` the way
 * pu_recipe_read's run_meta() (and, it turns out, ports/fetch's own `"$KPKG"
 * meta .`) does — verified live: `.kpkg-meta meta <dir>` prints "no tool
 * named '.kpkg-meta'" while a binary named plain "kpkg" resolves the same
 * argv straight to front_main. So this tool builds its own copy under a
 * name that satisfies the dispatcher, in a directory of its own rather than
 * reusing ports/.kpkg-meta's (broken) path. */
static void ensure_kpkg_bin(const char *repo_root, char *out, size_t cap)
{
	char dir[1536];
	snprintf(dir, sizeof(dir), "%s/ports/.portup-tools", repo_root);
	kb_mkdir_p(dir);
	snprintf(out, cap, "%s/kpkg", dir);
	/* A CACHED BINARY IS ONLY A CACHE IF IT RUNS HERE. This directory is
	 * inside the repo, and the same repo is built both on the host and
	 * inside the musl build container — so the copy left behind may have
	 * been linked against the other libc, where execve fails on the
	 * missing interpreter and every recipe read comes back empty. Probing
	 * it costs one fork and turns that into a rebuild. */
	if (kb_path_exists(out)) {
		KbArgv probe = {0};
		kb_argv_add(&probe, out);
		kb_argv_add(&probe, "meta");
		kb_argv_add(&probe, repo_root);
		kb_argv_end(&probe);
		KbBuf sink = {0};
		int rc = kb_run_capture_buf(&probe, &sink);
		kb_buf_free(&sink);
		if (rc != 127)
			return;
		unlink(out);
	}

	/*
	 * kdos-kpkg links THREE libraries — libkbase, libkpkg and libksig —
	 * because kdos-kpkg.h includes ksig.h. This command line is duplicated
	 * in ports/fetch and testing/selftest.sh; all three must list the same
	 * set, or the ones that do not fail to compile the recipe reader and
	 * the tool exits before doing any work.
	 *
	 * monocypher is a separate add_c_files: it is vendored third-party
	 * source in a subdirectory of libksig rather than beside it.
	 */
	char kdir[1536], lbase[1536], lpkg[1536], lsig[1536], lmono[1600];
	snprintf(kdir, sizeof(kdir), "%s/src/packages/kdos-kpkg", repo_root);
	snprintf(lbase, sizeof(lbase), "%s/src/libs/libkbase", repo_root);
	snprintf(lpkg, sizeof(lpkg), "%s/src/libs/libkpkg", repo_root);
	snprintf(lsig, sizeof(lsig), "%s/src/libs/libksig", repo_root);
	snprintf(lmono, sizeof(lmono), "%s/src/libs/libksig/monocypher", repo_root);

	fprintf(stderr, "==> Building the recipe reader...\n");

	KbArgv a = {0};
	kb_argv_add(&a, "cc");
	kb_argv_add(&a, "-O2");
	kb_argv_add(&a, "-std=gnu11");
	kb_argv_add(&a, "-D_GNU_SOURCE");
	kb_argv_addf(&a, "-I%s", kdir);
	kb_argv_addf(&a, "-I%s", lbase);
	kb_argv_addf(&a, "-I%s", lpkg);
	kb_argv_addf(&a, "-I%s", lsig);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	add_c_files(&a, kdir);
	add_c_files(&a, lbase);
	add_c_files(&a, lpkg);
	add_c_files(&a, lsig);
	add_c_files(&a, lmono);
	kb_argv_end(&a);

	if (kb_run_tty(&a) != 0)
		kb_die("failed to build the recipe reader (kpkg meta)");
}

/* Runs the real ports/fetch for exactly one port, inheriting stdio so a
 * download's progress is visible — the same reasoning kpkg's own build_port
 * has for leaving a recipe's stdio inherited. No shell, no system(): argv is
 * built element by element and execvp'd. */
static int run_ports_fetch(const char *repo_root, const char *portname)
{
	char script[1536];
	snprintf(script, sizeof(script), "%s/ports/fetch", repo_root);

	KbArgv a = {0};
	kb_argv_add(&a, script);
	kb_argv_add(&a, portname);
	kb_argv_end(&a);
	return kb_run_tty(&a);
}

/* ────────────────────────────────────────────────────────────────────────
 * The cache — ports/.update-cache.json, 24h TTL
 *
 * Read with libkbuild's kj_parse, which refuses anything that does not parse
 * whole: a corrupt cache reads as an empty one, never as a partial one that
 * silently drops entries. There is no kj_write — this tool is the only thing
 * that ever writes this file, so the writer is a plain KbBuf builder with its
 * own minimal string escaper, not a general JSON encoder.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char name[64];
	char version[PU_MAX_VER];
	int state;
	char candidate[PU_MAX_VER];
	char url[1024];
	char reason[128];
	int low_confidence;
	long long checked;
} CacheEntry;

typedef struct {
	CacheEntry e[MAX_PORTS + 32];
	int n;
} Cache;

static const char *state_name(int state)
{
	switch (state) {
	case PU_NEWER:   return "newer";
	case PU_UNKNOWN: return "unknown";
	default:         return "current";
	}
}

static int state_from_name(const char *s)
{
	if (!strcmp(s, "newer"))
		return PU_NEWER;
	if (!strcmp(s, "unknown"))
		return PU_UNKNOWN;
	return PU_CURRENT;
}

/* Every string this tool ever writes into JSON is program-generated or comes
 * from a recipe/URL — none of it is expected to carry control characters or
 * quotes, but the escaper does not trust that: an unescaped byte here would
 * make kj_parse (or any other JSON reader) read the cache as corrupt, and a
 * corrupt cache is meant to be a rare disk accident, not a routine outcome
 * of checking one oddly-named port. */
static void json_escape(KbBuf *b, const char *s)
{
	kb_buf_add(b, "\"", 1);
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		switch (*p) {
		case '"':  kb_buf_add(b, "\\\"", 2); break;
		case '\\': kb_buf_add(b, "\\\\", 2); break;
		case '\n': kb_buf_add(b, "\\n", 2); break;
		case '\r': kb_buf_add(b, "\\r", 2); break;
		case '\t': kb_buf_add(b, "\\t", 2); break;
		default:
			if (*p < 0x20)
				kb_buf_printf(b, "\\u%04x", *p);
			else
				kb_buf_add(b, p, 1);
		}
	}
	kb_buf_add(b, "\"", 1);
}

static void cache_load(const char *path, Cache *c)
{
	c->n = 0;
	size_t len = 0;
	char *text = kb_read_all(path, &len);
	if (!text)
		return;

	KjNode *root = kj_parse(text);
	free(text);
	if (!root || root->type != KJ_OBJ) {
		kj_free(root);
		return;
	}

	for (const KjNode *e = root->child; e && c->n < (int)(sizeof(c->e) / sizeof(c->e[0])); e = e->next) {
		if (!e->key || e->type != KJ_OBJ)
			continue;
		CacheEntry *ce = &c->e[c->n++];
		memset(ce, 0, sizeof(*ce));
		kb_strlcpy(ce->name, e->key, sizeof(ce->name));
		kb_strlcpy(ce->version, kj_str(e, "version", ""), sizeof(ce->version));
		ce->state = state_from_name(kj_str(e, "state", "current"));
		kb_strlcpy(ce->candidate, kj_str(e, "candidate", ""), sizeof(ce->candidate));
		kb_strlcpy(ce->url, kj_str(e, "url", ""), sizeof(ce->url));
		kb_strlcpy(ce->reason, kj_str(e, "reason", ""), sizeof(ce->reason));
		ce->low_confidence = kj_bool(e, "low_confidence", 0);
		ce->checked = (long long)kj_num(e, "checked", 0);
	}
	kj_free(root);
}

static CacheEntry *cache_find(Cache *c, const char *name)
{
	for (int i = 0; i < c->n; i++)
		if (!strcmp(c->e[i].name, name))
			return &c->e[i];
	return NULL;
}

static void cache_upsert(Cache *c, const char *name, const char *version,
			 const PuResult *r, long long now)
{
	CacheEntry *ce = cache_find(c, name);
	if (!ce) {
		if (c->n >= (int)(sizeof(c->e) / sizeof(c->e[0])))
			return;		/* the cache is advisory; dropping one entry costs nothing */
		ce = &c->e[c->n++];
		memset(ce, 0, sizeof(*ce));
		kb_strlcpy(ce->name, name, sizeof(ce->name));
	}
	kb_strlcpy(ce->version, version, sizeof(ce->version));
	ce->state = r->state;
	kb_strlcpy(ce->candidate, r->candidate, sizeof(ce->candidate));
	kb_strlcpy(ce->url, r->url, sizeof(ce->url));
	kb_strlcpy(ce->reason, r->reason, sizeof(ce->reason));
	ce->low_confidence = r->low_confidence;
	ce->checked = now;
}

static void cache_save(const char *path, const Cache *c)
{
	KbBuf b = {0};
	kb_buf_str(&b, "{\n");
	for (int i = 0; i < c->n; i++) {
		const CacheEntry *e = &c->e[i];
		kb_buf_str(&b, "  ");
		json_escape(&b, e->name);
		kb_buf_str(&b, ": {\"version\": ");
		json_escape(&b, e->version);
		kb_buf_str(&b, ", \"state\": ");
		json_escape(&b, state_name(e->state));
		kb_buf_str(&b, ", \"candidate\": ");
		json_escape(&b, e->candidate);
		kb_buf_str(&b, ", \"url\": ");
		json_escape(&b, e->url);
		kb_buf_str(&b, ", \"reason\": ");
		json_escape(&b, e->reason);
		kb_buf_printf(&b, ", \"low_confidence\": %s, \"checked\": %lld}",
			      e->low_confidence ? "true" : "false", e->checked);
		kb_buf_str(&b, i + 1 < c->n ? ",\n" : "\n");
	}
	kb_buf_str(&b, "}\n");
	kb_write_all(path, b.p ? b.p : "{}\n", b.p ? b.n : 3);
	kb_buf_free(&b);
}

/* A hit needs three things to agree: the port was checked before, the
 * recipe's version has not moved since (an accepted bump invalidates a
 * cached "current" for the OLD version), and the entry is inside its 24h
 * window. Any of those failing is an ordinary miss, not an error. */
static int try_cache(Cache *c, const PuRecipe *r, int refresh, long long now,
		     PuResult *out)
{
	if (refresh)
		return 0;
	CacheEntry *e = cache_find(c, r->name);
	if (!e || strcmp(e->version, r->version))
		return 0;
	if (now - e->checked >= CACHE_TTL)
		return 0;

	memset(out, 0, sizeof(*out));
	out->state = e->state;
	kb_strlcpy(out->candidate, e->candidate, sizeof(out->candidate));
	kb_strlcpy(out->url, e->url, sizeof(out->url));
	kb_strlcpy(out->reason, e->reason, sizeof(out->reason));
	out->low_confidence = e->low_confidence;
	return 1;
}

/* ────────────────────────────────────────────────────────────────────────
 * Per-port state carried through discovery, grouping and review
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	PuRecipe r;
	PuResult res;
	char group[64];
	int has_group;
	int in_complete_group;	/* set by compute_groups()                  */
} PortEntry;

/* A group is only ever offered when every one of its ≥2 members is NEWER
 * with the SAME candidate — the design's "all-or-nothing" rule applies to
 * the OFFER, not just the accept: a partial epoch offered piecemeal is the
 * broken desktop this whole mechanism exists to prevent. Anything short of
 * that falls through to individual review, same as an ungrouped port. */
static void compute_groups(PortEntry *pe, int n)
{
	for (int i = 0; i < n; i++)
		pe[i].in_complete_group = 0;

	for (int i = 0; i < n; i++) {
		if (!pe[i].has_group)
			continue;
		int already = 0;
		for (int k = 0; k < i && !already; k++)
			already = pe[k].has_group && !strcmp(pe[k].group, pe[i].group);
		if (already)
			continue;

		int count = 0, complete = 1;
		const char *cand0 = NULL;
		for (int j = 0; j < n; j++) {
			if (!pe[j].has_group || strcmp(pe[j].group, pe[i].group))
				continue;
			count++;
			if (pe[j].res.state != PU_NEWER) {
				complete = 0;
				continue;
			}
			if (!cand0)
				cand0 = pe[j].res.candidate;
			else if (strcmp(cand0, pe[j].res.candidate))
				complete = 0;
		}
		if (count >= 2 && complete)
			for (int j = 0; j < n; j++)
				if (pe[j].has_group && !strcmp(pe[j].group, pe[i].group))
					pe[j].in_complete_group = 1;
	}
}

/* ────────────────────────────────────────────────────────────────────────
 * The review line
 *
 * Column positions were measured off the spec's own worked examples rather
 * than invented: the version field always ends at column 26, the arrow (or
 * its blank equivalent) is a fixed 6-column gap, the candidate/"current"
 * word starts at 32, and any risk flags or the state reason start at 43 —
 * every one of curl/mesa/pop-os/imagemagick/aalib's sample lines lands on
 * exactly those columns.
 * ──────────────────────────────────────────────────────────────────────── */

static void append_name_version(KbBuf *b, const char *name, const char *version)
{
	kb_buf_str(b, "  ");
	kb_buf_str(b, name);
	int used = 2 + (int)strlen(name);
	int target = 26 - (int)strlen(version);
	int pad = target - used;
	if (pad < 1)
		pad = 1;
	for (int i = 0; i < pad; i++)
		kb_buf_add(b, " ", 1);
	kb_buf_str(b, version);
}

/* `state` may be PU_NEWER (write `cand`), PU_UNKNOWN (write "?") or
 * PU_CURRENT (write "current" with no arrow and no trailing pad — nothing
 * follows it on the line). */
static void append_arrow_and_candidate(KbBuf *b, int state, const char *cand)
{
	if (state == PU_CURRENT) {
		kb_buf_str(b, "      current");
		return;
	}
	kb_buf_str(b, "  ->  ");
	const char *shown = (state == PU_NEWER) ? cand : "?";
	kb_buf_str(b, shown);
	int pad = 11 - (int)strlen(shown);
	if (pad < 1)
		pad = 1;
	for (int i = 0; i < pad; i++)
		kb_buf_add(b, " ", 1);
}

/* A NEWER line with no risk flags and no prompt (--check/--json-adjacent
 * human output) would otherwise end in the candidate field's own padding —
 * harmless, but every other terminal tool trims a line before printing it. */
static void rtrim(KbBuf *b)
{
	while (b->n && b->p[b->n - 1] == ' ')
		b->p[--b->n] = 0;
}

static void append_risk_flags(KbBuf *b, const PuRecipe *r, int low_confidence)
{
	int any = 0;
	if (r->vendoring[0]) {
		kb_buf_printf(b, "vendored(%s)", r->vendoring);
		any = 1;
	}
	if (r->npatches > 0) {
		kb_buf_printf(b, "%spatches(%d)", any ? " " : "", r->npatches);
		any = 1;
	}
	if (low_confidence)
		kb_buf_str(b, any ? " low-confidence" : "low-confidence");
}

/* `prompt` adds the trailing "[y/n/d/a/q]" — only when this line is about to
 * be interactively decided, never in --check output and never for a
 * current/unknown line, which nothing can be done about. */
static void print_port_line(const PortEntry *e, int prompt)
{
	KbBuf b = {0};
	append_name_version(&b, e->r.name, e->r.version);
	append_arrow_and_candidate(&b, e->res.state, e->res.candidate);

	if (e->res.state == PU_NEWER) {
		size_t before = b.n;
		append_risk_flags(&b, &e->r, e->res.low_confidence);
		int had_flags = b.n > before;
		if (prompt)
			kb_buf_str(&b, had_flags ? " [y/n/d/a/q]" : "[y/n/d/a/q]");
	} else if (e->res.state == PU_UNKNOWN) {
		kb_buf_printf(&b, "unknown: %s", e->res.reason);
	}
	rtrim(&b);
	printf("%s\n", b.p ? b.p : "");
	kb_buf_free(&b);
}

/* The one line offered for a complete group. `org` is the group key with any
 * trailing "@<version>" trimmed — pu_group_key's auto-detected keys are
 * "<org>@<version>", and the version is already shown in its own column. */
static void print_group_line(PortEntry **m, int n, int prompt)
{
	char org[64];
	kb_strlcpy(org, m[0]->group, sizeof(org));
	char *at = strchr(org, '@');
	if (at)
		*at = 0;

	char label[160];
	snprintf(label, sizeof(label), "%s group (%d)", org, n);

	KbBuf b = {0};
	append_name_version(&b, label, m[0]->r.version);
	append_arrow_and_candidate(&b, PU_NEWER, m[0]->res.candidate);
	kb_buf_str(&b, "group, all-or-nothing");
	if (prompt)
		kb_buf_str(&b, " [y/n/d/a/q]");
	printf("%s\n", b.p ? b.p : "");
	kb_buf_free(&b);
}

/* ────────────────────────────────────────────────────────────────────────
 * `d` — the proved URL and a preview of the recipe edit
 *
 * Uses recipe.c's pu_is_version_line rather than a local copy of the same
 * key-boundary rule: this preview and the real rewrite need to agree about
 * which line "the version line" is, and two definitions that happen to
 * agree today are one refactor away from silently disagreeing.
 * ──────────────────────────────────────────────────────────────────────── */

static void print_recipe_diff(const PortEntry *e)
{
	printf("    url: %s\n", e->res.url);

	char path[700];
	snprintf(path, sizeof(path), "%s/kpkgbuild", e->r.portdir);
	size_t n = 0;
	char *text = kb_read_all(path, &n);
	if (!text) {
		printf("    (could not read %s)\n", path);
		return;
	}
	printf("    --- %s\n", path);
	printf("    +++ %s (candidate)\n", path);
	for (char *line = text, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (pu_is_version_line(line)) {
			char *eq = strchr(line, '=');
			size_t pre = (size_t)(eq - line) + 1;
			printf("    -%s\n", line);

			char pre_buf[300];
			size_t pl = pre < sizeof(pre_buf) - 1 ? pre : sizeof(pre_buf) - 1;
			memcpy(pre_buf, line, pl);
			pre_buf[pl] = 0;

			char spaces[64];
			size_t si = 0;
			const char *v = eq + 1;
			while (*v == ' ' && si + 1 < sizeof(spaces)) {
				spaces[si++] = ' ';
				v++;
			}
			spaces[si] = 0;
			printf("    +%s%s%s\n", pre_buf, spaces, e->res.candidate);
			break;
		}
	}
	free(text);
}

/* ────────────────────────────────────────────────────────────────────────
 * Accept, transactionally
 *
 * Every accept function returns one of three things, and the distinction is
 * the point: 0 (applied), -1 (not applied, but the tree is exactly as it
 * was — an ordinary, recoverable failure), or -2 (a REVERT itself failed:
 * the recipe is left bumped with no tarball fetched, the precise state that
 * breaks `make build --network none`). Silently discarding the revert's own
 * return value would let the one failure this whole mechanism exists to
 * prevent be reported to the user as "restored" — a caller has to be able
 * to tell -1 and -2 apart, so both accept_one and its exit status do.
 * ──────────────────────────────────────────────────────────────────────── */

/* The message a caller sees when the safety net itself tears: the fix is a
 * manual edit, so the port, the file and both version numbers all have to be
 * in the one line the user is going to read. */
static void warn_revert_failed(const PortEntry *e, const char *cand,
			       const char *old_ver)
{
	kb_warn("%s: FETCH FAILED AND THE REVERT FAILED TOO — %s/kpkgbuild "
		"still says %s but no tarball was fetched. Edit it back "
		"to %s by hand before building.",
		e->r.name, e->r.portdir, cand, old_ver);
}

static int accept_one(const char *repo_root, PortEntry *e, int no_fetch)
{
	char old_ver[PU_MAX_VER], cand[PU_MAX_VER];
	kb_strlcpy(old_ver, e->r.version, sizeof(old_ver));
	kb_strlcpy(cand, e->res.candidate, sizeof(cand));

	/* Rewrite, fetch, and put the old version back if the fetch fails.
	 * The failure this exists to prevent is a recipe naming a tarball
	 * that is not on disk: that breaks `make build --network none`,
	 * which is one of the four properties the whole distro is built on. */
	if (pu_rewrite_version(e->r.portdir, cand) != 0) {
		kb_warn("%s: could not rewrite the recipe", e->r.name);
		return -1;
	}
	if (!no_fetch && run_ports_fetch(repo_root, e->r.name) != 0) {
		if (pu_rewrite_version(e->r.portdir, old_ver) != 0) {
			warn_revert_failed(e, cand, old_ver);
			return -2;
		}
		kb_warn("%s: fetch failed, version restored to %s", e->r.name,
			old_ver);
		return -1;
	}
	/* The fetch has the new files on disk now, so this is where the
	 * recipe's hashes stop naming the old ones. Same operation as the
	 * version bump from the maintainer's point of view: the tree is never
	 * left with an archive nothing verifies. */
	if (!no_fetch && pu_rewrite_sha256(e->r.portdir, old_ver, cand) != 0)
		kb_warn("%s: bumped and fetched, but the sha256 lines could not be rewritten — record them by hand",
			e->r.name);
	printf("  accepted %s -> %s\n", e->r.name, cand);
	/* The fetch downloads the NEW tarball; nothing here ever deletes the
	 * old one. Both are LFS-tracked and often tens of megabytes, so the
	 * maintainer needs to know it is still sitting in the port directory —
	 * but deciding what leaves the repository is their call, not this
	 * tool's, so this is a note and never an rm. */
	if (!no_fetch)
		printf("    note: the old v%s tarball is still in %s — remove it by hand if it is no longer wanted\n",
		       old_ver, e->r.portdir);
	return 0;
}

/* Offered once, accepted for every member or none — including at the fetch
 * step. Rewriting all members first (cheap, local) and only then fetching
 * each in turn means a failure on member K reverts every member 1..K, not
 * just the one that failed: the alternative is exactly the half-bumped
 * epoch this feature exists to prevent, just arrived at one fetch later.
 *
 * Reverting a group is itself a loop over N recipe writes, so it can fail
 * partway exactly like the single-port case can — and here that means SOME
 * members land back on their old version while others stay bumped with
 * nothing fetched. Every member whose OWN revert fails still has to be
 * attempted and still has to be named: stopping at the first broken revert
 * would silently leave the rest un-reverted too, which is strictly worse. */
static int accept_group(const char *repo_root, PortEntry **m, int n, int no_fetch)
{
	if (n > GROUP_MAX)
		n = GROUP_MAX;
	char old_ver[GROUP_MAX][PU_MAX_VER];
	char cand[GROUP_MAX][PU_MAX_VER];

	for (int i = 0; i < n; i++) {
		kb_strlcpy(old_ver[i], m[i]->r.version, sizeof(old_ver[i]));
		kb_strlcpy(cand[i], m[i]->res.candidate, sizeof(cand[i]));
		if (pu_rewrite_version(m[i]->r.portdir, cand[i]) != 0) {
			kb_warn("%s: could not rewrite the recipe; reverting the group",
				m[i]->r.name);
			int broken = 0;
			for (int k = 0; k < i; k++) {
				if (pu_rewrite_version(m[k]->r.portdir, old_ver[k]) != 0) {
					warn_revert_failed(m[k], cand[k], old_ver[k]);
					broken = 1;
				}
			}
			return broken ? -2 : -1;
		}
	}
	if (!no_fetch) {
		for (int i = 0; i < n; i++) {
			if (run_ports_fetch(repo_root, m[i]->r.name) != 0) {
				kb_warn("%s: fetch failed, reverting the whole group",
					m[i]->r.name);
				int broken = 0;
				for (int k = 0; k < n; k++) {
					if (pu_rewrite_version(m[k]->r.portdir, old_ver[k]) != 0) {
						warn_revert_failed(m[k], cand[k], old_ver[k]);
						broken = 1;
					}
				}
				return broken ? -2 : -1;
			}
		}
	}
	for (int i = 0; i < n; i++) {
		printf("  accepted %s -> %s\n", m[i]->r.name, cand[i]);
		/* Same note as accept_one, per member — a group bump leaves one
		 * superseded tarball behind for every port it touched. */
		if (!no_fetch)
			printf("    note: the old v%s tarball is still in %s — remove it by hand if it is no longer wanted\n",
			       old_ver[i], m[i]->r.portdir);
	}
	return 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * The interactive prompt
 * ──────────────────────────────────────────────────────────────────────── */

/* First non-space character of a line, lower-cased; 'q' on EOF so a closed
 * or empty stdin (a script piping nothing in) ends the review instead of
 * spinning. */
static int read_cmd(void)
{
	/* Initialised because musl's fortified fgets is declared
	 * access(read_write), which makes the compiler treat the buffer as an
	 * input it may read: an uninitialised one is then a diagnostic on the
	 * libc this distro actually ships. */
	char buf[64] = { 0 };
	if (!fgets(buf, sizeof(buf), stdin))
		return 'q';
	for (char *p = buf; *p; p++)
		if (!isspace((unsigned char)*p))
			return tolower((unsigned char)*p);
	return 0;
}

static void prompt_line(void)
{
	printf("  > ");
	fflush(stdout);
}

/* `*unrecoverable` latches to 1 the moment any accept returns -2 (a revert
 * that itself failed) and is never cleared — one such port is enough to make
 * the whole run's exit status say so, no matter how many others went fine. */
static void one_port_review(PortEntry *e, const Opts *o, const char *repo_root,
			    int *auto_accept, int *quit, int *unrecoverable)
{
	int prompting = !*auto_accept;
	print_port_line(e, prompting);
	if (!prompting) {
		if (accept_one(repo_root, e, o->no_fetch) == -2)
			*unrecoverable = 1;
		return;
	}

	for (;;) {
		prompt_line();
		int c = read_cmd();
		if (c == 'y') {
			if (accept_one(repo_root, e, o->no_fetch) == -2)
				*unrecoverable = 1;
			return;
		}
		if (c == 'a') {
			*auto_accept = 1;
			if (accept_one(repo_root, e, o->no_fetch) == -2)
				*unrecoverable = 1;
			return;
		}
		if (c == 'n')
			return;
		if (c == 'q') {
			*quit = 1;
			return;
		}
		if (c == 'd') {
			print_recipe_diff(e);
			continue;
		}
		printf("  unrecognised: %c (expected one of y n d a q)\n", c);
	}
}

static void group_review(PortEntry **m, int n, const Opts *o,
			 const char *repo_root, int *auto_accept, int *quit,
			 int *unrecoverable)
{
	int prompting = !*auto_accept;
	print_group_line(m, n, prompting);
	if (!prompting) {
		if (accept_group(repo_root, m, n, o->no_fetch) == -2)
			*unrecoverable = 1;
		return;
	}

	for (;;) {
		prompt_line();
		int c = read_cmd();
		if (c == 'y') {
			if (accept_group(repo_root, m, n, o->no_fetch) == -2)
				*unrecoverable = 1;
			return;
		}
		if (c == 'a') {
			*auto_accept = 1;
			if (accept_group(repo_root, m, n, o->no_fetch) == -2)
				*unrecoverable = 1;
			return;
		}
		if (c == 'n')
			return;
		if (c == 'q') {
			*quit = 1;
			return;
		}
		if (c == 'd') {
			for (int i = 0; i < n; i++)
				print_recipe_diff(m[i]);
			continue;
		}
		printf("  unrecognised: %c (expected one of y n d a q)\n", c);
	}
}

/* Human-readable pass: every port gets a line (progress already happened
 * while checking); only a NEWER port or a complete group gets a decision,
 * and only when prompting is enabled at all (`interactive`). A complete
 * group's members are skipped individually — the group line stands in for
 * all of them, first-seen-in-tree-order, so the output stays in the same
 * order the ports tree lists them. */
static void review(PortEntry *pe, int n, const Opts *o, const char *repo_root,
		   int interactive, int *unrecoverable)
{
	int auto_accept = 0, quit = 0;
	char printed[GROUP_MAX][64];
	int nprinted = 0;

	for (int i = 0; i < n && !quit; i++) {
		PortEntry *e = &pe[i];

		if (e->in_complete_group) {
			int already = 0;
			for (int k = 0; k < nprinted && !already; k++)
				already = !strcmp(printed[k], e->group);
			if (already)
				continue;
			if (nprinted < GROUP_MAX)
				kb_strlcpy(printed[nprinted++], e->group,
					   sizeof(printed[0]));

			PortEntry *members[GROUP_MAX];
			int nm = 0;
			for (int j = 0; j < n && nm < GROUP_MAX; j++)
				if (pe[j].in_complete_group &&
				    !strcmp(pe[j].group, e->group))
					members[nm++] = &pe[j];

			if (interactive)
				group_review(members, nm, o, repo_root,
					     &auto_accept, &quit, unrecoverable);
			else
				print_group_line(members, nm, 0);
			continue;
		}

		if (interactive && e->res.state == PU_NEWER) {
			one_port_review(e, o, repo_root, &auto_accept, &quit,
					unrecoverable);
		} else {
			print_port_line(e, 0);
		}
	}
}

/* ────────────────────────────────────────────────────────────────────────
 * --json
 * ──────────────────────────────────────────────────────────────────────── */

static void print_json(PortEntry *pe, int n)
{
	KbBuf b = {0};
	kb_buf_str(&b, "[\n");
	for (int i = 0; i < n; i++) {
		PortEntry *e = &pe[i];
		kb_buf_str(&b, "  {\"name\": ");
		json_escape(&b, e->r.name);
		kb_buf_str(&b, ", \"version\": ");
		json_escape(&b, e->r.version);
		kb_buf_str(&b, ", \"state\": ");
		json_escape(&b, state_name(e->res.state));
		kb_buf_str(&b, ", \"candidate\": ");
		json_escape(&b, e->res.candidate);
		kb_buf_str(&b, ", \"url\": ");
		json_escape(&b, e->res.url);
		kb_buf_str(&b, ", \"reason\": ");
		json_escape(&b, e->res.reason);
		kb_buf_printf(&b, ", \"low_confidence\": %s",
			      e->res.low_confidence ? "true" : "false");
		kb_buf_str(&b, ", \"vendoring\": ");
		json_escape(&b, e->r.vendoring);
		kb_buf_printf(&b, ", \"patches\": %d", e->r.npatches);
		kb_buf_str(&b, ", \"group\": ");
		json_escape(&b, e->in_complete_group ? e->group : "");
		kb_buf_str(&b, "}");
		kb_buf_str(&b, i + 1 < n ? ",\n" : "\n");
	}
	kb_buf_str(&b, "]\n");
	fputs(b.p ? b.p : "[]\n", stdout);
	kb_buf_free(&b);
}

/* ────────────────────────────────────────────────────────────────────────
 * The discovery pass
 * ──────────────────────────────────────────────────────────────────────── */

static PortEntry *g_entries;
static int g_nentries;

/* Every candidate name, in tree order. When `want` is non-empty only those
 * names are checked (and each has to resolve, or the run fails loudly rather
 * than silently checking fewer ports than asked). */
static int gather_names(const KpConf *conf, char **want, int nwant,
			char names[][64])
{
	if (nwant == 0) {
		int count = 0;
		char **all = kp_all_ports(conf, &count);
		int n = 0;
		for (int i = 0; i < count && n < MAX_PORTS; i++)
			kb_strlcpy(names[n++], all[i], 64);
		kb_strv_free(all);
		return n;
	}

	int n = 0;
	for (int i = 0; i < nwant && n < MAX_PORTS; i++) {
		char *dir = kp_port_dir(conf, want[i]);
		if (!dir) {
			kb_warn("%s: no such port", want[i]);
			continue;
		}
		free(dir);
		kb_strlcpy(names[n++], want[i], 64);
	}
	return n;
}

/*
 * `--cve`: ask repology whether the version each port PINS is flagged.
 *
 * Separate pass, separate output, and never cached: the update cache stores
 * "what is the newest version" and this stores "is ours known-bad", which age
 * differently — a version that was clean this morning is not clean this evening
 * because someone filed a CVE, and a stale yes/no is worse than another minute
 * of requests.
 *
 * One request per port at repology's documented one per second, which is six
 * and a half minutes over the whole tree. That cost is why this is a flag and
 * why `kdos cve` — offline, instant, and vendored — is the everyday answer.
 */
static void report_vulnerable(const KpConf *conf, const char *kpkg_bin,
			      char names[][64], int nnames)
{
	int flagged = 0, unknown = 0, checked = 0;

	fprintf(stderr, "\nasking repology about %d pinned versions "
			"(one request per second)...\n", nnames);
	printf("\nrepology's vulnerability flag\n");
	for (int i = 0; i < nnames; i++) {
		char *dir = kp_port_dir(conf, names[i]);
		if (!dir)
			continue;
		PuRecipe r;
		int ok = pu_recipe_read(kpkg_bin, dir, &r) == 0;
		free(dir);
		if (!ok || !r.source[0])
			continue;	/* ours: nothing upstream to ask about */

		fprintf(stderr, "[%d/%d] %s\n", i + 1, nnames, names[i]);
		checked++;
		int v = pu_repology_vuln(&r);
		if (v == PU_VULN_YES) {
			flagged++;
			printf("  %-24s %-16s FLAGGED\n", r.name, r.version);
		} else if (v == PU_VULN_UNKNOWN) {
			unknown++;
		}
	}
	printf("\n  %d asked, %d flagged, %d repology could not answer for\n",
	       checked, flagged, unknown);
	printf("  this is the ONLINE cross-check; `kdos cve` answers offline "
	       "from the vendored table\n");
}

/* Checks every named port (cache first, unless --refresh), storing a
 * PortEntry for each one that has an upstream source at all — the ports
 * that are ours (kdos-*) are dropped here, silently, exactly once,
 * rather than at every later call site that would otherwise have to know
 * the same rule. Progress goes to stderr: pu_check is network-bound and can
 * take minutes over the whole tree, and stderr keeps --json's stdout clean
 * while still telling a human something is happening. */
static int discover(const KpConf *conf, const char *kpkg_bin, char names[][64],
		    int nnames, Cache *cache, int refresh, int *any_newer)
{
	long long now = (long long)time(NULL);
	int n = 0;

	for (int i = 0; i < nnames; i++) {
		fprintf(stderr, "[%d/%d] %s\n", i + 1, nnames, names[i]);

		char *dir = kp_port_dir(conf, names[i]);
		if (!dir) {
			kb_warn("%s: no such port", names[i]);
			continue;
		}

		PuRecipe r;
		if (pu_recipe_read(kpkg_bin, dir, &r) != 0) {
			kb_warn("%s: could not read the recipe", names[i]);
			free(dir);
			continue;
		}
		free(dir);

		if (!r.source[0])
			continue;	/* ours: no upstream to check */

		PuResult res;
		if (!try_cache(cache, &r, refresh, now, &res)) {
			pu_check(kpkg_bin, &r, &res);
			cache_upsert(cache, r.name, r.version, &res, now);
		}
		if (res.state == PU_NEWER)
			*any_newer = 1;

		if (n >= MAX_PORTS)
			continue;
		PortEntry *e = &g_entries[n++];
		e->r = r;
		e->res = res;
		e->has_group = pu_group_key(&r, e->group, sizeof(e->group));
	}
	return n;
}

/* ────────────────────────────────────────────────────────────────────────
 * --selftest — pure, offline assertions over this file's own logic
 *
 * kp_vercmp/pu_shape/pu_extract already have a table in src/libs/selftest.c,
 * built against the pipeline's earlier stages. Everything new in THIS file
 * is the line layout, the grouping decision and the cache's round trip —
 * none of it touches the network, so all three get exercised here with no
 * fixture required.
 * ──────────────────────────────────────────────────────────────────────── */

static int st_checks, st_failed;

static void st_ok(int cond, const char *what)
{
	st_checks++;
	if (!cond) {
		st_failed++;
		printf("  NOT OK: %s\n", what);
	}
}

static void selftest_columns(void)
{
	/* append_name_version must land the version's last character at
	 * column 26 regardless of how the name and version lengths trade
	 * off against each other — that is the whole layout rule, measured
	 * off the spec's worked examples. */
	struct { const char *name, *version; } cases[] = {
		{ "curl", "8.17.0" },
		{ "imagemagick", "7.1.2.21" },
		{ "pop-os epoch (17)", "1.4.0" },
		{ "aalib", "1.4rc5" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		KbBuf b = {0};
		append_name_version(&b, cases[i].name, cases[i].version);
		st_ok(b.n == 26, "version ends at column 26");
		st_ok(!strncmp(b.p + b.n - strlen(cases[i].version),
			       cases[i].version, strlen(cases[i].version)),
		      "version is the last thing on the line");
		kb_buf_free(&b);
	}

	KbBuf b = {0};
	append_arrow_and_candidate(&b, PU_NEWER, "8.21.0");
	/* 6 columns of arrow ("  ->  ") plus an 11-wide candidate field. */
	st_ok(b.n == 17, "candidate field is 11 columns wide");
	kb_buf_free(&b);

	KbBuf b2 = {0};
	append_arrow_and_candidate(&b2, PU_CURRENT, "");
	st_ok(!strcmp(b2.p, "      current"), "current has no arrow and no pad");
	kb_buf_free(&b2);
}

static void selftest_grouping(void)
{
	PortEntry pe[3] = {0};
	kb_strlcpy(pe[0].r.name, "gst-plugins-base", sizeof(pe[0].r.name));
	kb_strlcpy(pe[0].r.version, "1.4.0", sizeof(pe[0].r.version));
	kb_strlcpy(pe[0].group, "gstreamer@1.4.0", sizeof(pe[0].group));
	pe[0].has_group = 1;
	pe[0].res.state = PU_NEWER;
	kb_strlcpy(pe[0].res.candidate, "1.5.0", sizeof(pe[0].res.candidate));

	pe[1] = pe[0];
	kb_strlcpy(pe[1].r.name, "gst-plugins-good", sizeof(pe[1].r.name));

	pe[2] = pe[0];
	kb_strlcpy(pe[2].r.name, "gst-plugins-bad", sizeof(pe[2].r.name));
	pe[2].res.state = PU_CURRENT;	/* breaks completeness */

	compute_groups(pe, 3);
	st_ok(!pe[0].in_complete_group && !pe[1].in_complete_group,
	      "a group with one member not-yet-newer is incomplete");

	pe[2].res.state = PU_NEWER;
	kb_strlcpy(pe[2].res.candidate, "1.5.0", sizeof(pe[2].res.candidate));
	compute_groups(pe, 3);
	st_ok(pe[0].in_complete_group && pe[1].in_complete_group &&
	      pe[2].in_complete_group,
	      "all-newer, same-candidate, 3 members -> complete");

	kb_strlcpy(pe[2].res.candidate, "1.5.1", sizeof(pe[2].res.candidate));
	compute_groups(pe, 3);
	st_ok(!pe[0].in_complete_group,
	      "a mismatched candidate breaks completeness even if all are newer");

	/* A group key shared by exactly one checked port is not a group —
	 * most single-repo GitHub ports have a unique org key by construction. */
	PortEntry solo[1] = {0};
	kb_strlcpy(solo[0].group, "curl@8.17.0", sizeof(solo[0].group));
	solo[0].has_group = 1;
	solo[0].res.state = PU_NEWER;
	kb_strlcpy(solo[0].res.candidate, "8.21.0", sizeof(solo[0].res.candidate));
	compute_groups(solo, 1);
	st_ok(!solo[0].in_complete_group, "a lone member is never offered as a group");
}

static void selftest_cache(void)
{
	char tmpl[] = "/tmp/portup-selftest.XXXXXX";
	int fd = mkstemp(tmpl);
	st_ok(fd >= 0, "created a scratch cache file");
	if (fd >= 0)
		close(fd);

	Cache c = {0};
	PuResult r = {0};
	r.state = PU_NEWER;
	kb_strlcpy(r.candidate, "8.21.0", sizeof(r.candidate));
	kb_strlcpy(r.url, "https://example.invalid/curl-8.21.0.tar.xz",
		   sizeof(r.url));
	r.low_confidence = 1;
	cache_upsert(&c, "curl", "8.17.0", &r, 1000);

	PuResult weird = {0};
	weird.state = PU_UNKNOWN;
	kb_strlcpy(weird.reason, "a \"quoted\" host\\path\nwith control bytes",
		   sizeof(weird.reason));
	cache_upsert(&c, "weird\"name", "1.0", &weird, 1000);

	cache_save(tmpl, &c);

	Cache back = {0};
	cache_load(tmpl, &back);
	st_ok(back.n == 2, "round trip preserves entry count");

	CacheEntry *ce = cache_find(&back, "curl");
	st_ok(ce != NULL, "round trip preserves the name");
	if (ce) {
		st_ok(!strcmp(ce->version, "8.17.0"), "round trip preserves version");
		st_ok(ce->state == PU_NEWER, "round trip preserves state");
		st_ok(!strcmp(ce->candidate, "8.21.0"), "round trip preserves candidate");
		st_ok(ce->low_confidence == 1, "round trip preserves low_confidence");
		st_ok(ce->checked == 1000, "round trip preserves the timestamp");
	}

	CacheEntry *cw = cache_find(&back, "weird\"name");
	st_ok(cw != NULL, "an escaped key round-trips");
	if (cw)
		st_ok(!strcmp(cw->reason, weird.reason),
		      "quotes, backslashes and control bytes round-trip");

	PuResult hit;
	st_ok(try_cache(&c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			0, 1000 + CACHE_TTL - 1, &hit) == 1,
	      "a fresh, version-matched entry is a hit");
	st_ok(try_cache(&c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			0, 1000 + CACHE_TTL + 1, &hit) == 0,
	      "an entry past its TTL is a miss");
	st_ok(try_cache(&c, &(PuRecipe){ .name = "curl", .version = "8.18.0" },
			0, 1000, &hit) == 0,
	      "a version mismatch (the recipe moved) is a miss");
	st_ok(try_cache(&c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			1, 1000, &hit) == 0,
	      "--refresh always misses");

	unlink(tmpl);

	/* A corrupt cache reads as an empty one, never as a partial one. */
	char tmpl2[] = "/tmp/portup-selftest.XXXXXX";
	int fd2 = mkstemp(tmpl2);
	if (fd2 >= 0) {
		ssize_t junk_written = write(fd2, "{not json", 9);
		st_ok(junk_written == 9, "wrote the corrupt fixture");
		close(fd2);
	}
	Cache corrupt = {0};
	cache_load(tmpl2, &corrupt);
	st_ok(corrupt.n == 0, "a cache that does not parse whole reads as absent");
	unlink(tmpl2);
}

/*
 * The bump's second half. A version rewrite that does not move the hashes with
 * it leaves the recipe naming tarballs that are no longer there, and the fetch
 * then downloads the new ones with no verification at all.
 */
static void selftest_rehash(void)
{
	char dir[] = "/tmp/kdos-portup-rehash.XXXXXX";
	if (!mkdtemp(dir))
		return;
	char path[600], f1[600], f2[600];

	/* Two hashed sources, as a rust port has: the tarball and the vendor
	 * bundle. Both carry the version in their names. */
	snprintf(path, sizeof(path), "%s/kpkgbuild", dir);
	kb_write_file(path,
		      "name        = demo\n"
		      "version     = 1.0\n"
		      "sha256      = " "0000000000000000000000000000000000000000000000000000000000000000"
		      "  demo-1.0.tar.gz\n"
		      "sha256      = " "1111111111111111111111111111111111111111111111111111111111111111"
		      "  demo-vendor-1.0.tar.xz\n"
		      "description = a port\n");
	snprintf(f1, sizeof(f1), "%s/demo-2.0.tar.gz", dir);
	snprintf(f2, sizeof(f2), "%s/demo-vendor-2.0.tar.xz", dir);
	kb_write_file(f1, "new tarball\n");
	kb_write_file(f2, "new vendor\n");

	st_ok(pu_rewrite_sha256(dir, "1.0", "2.0") == 0, "the rehash runs");

	size_t n = 0;
	char *out = kb_read_all(path, &n);
	if (out) {
		char want1[65], want2[65];
		kb_sha256_file(f1, want1);
		kb_sha256_file(f2, want2);
		st_ok(strstr(out, "demo-2.0.tar.gz") != NULL,
		      "the tarball's line names the new version");
		st_ok(strstr(out, "demo-vendor-2.0.tar.xz") != NULL,
		      "and so does the vendor bundle's — both move with a bump");
		st_ok(strstr(out, want1) && strstr(out, want2),
		      "each line carries the hash of the file that is now there");
		st_ok(strstr(out, "0000000000000000") == NULL,
		      "and no stale hash survives");
		st_ok(strstr(out, "description = a port") != NULL,
		      "every other line is untouched");
		free(out);
	}
	unlink(path); unlink(f1); unlink(f2); rmdir(dir);
}

static int run_selftest(void)
{
	printf("kdos-portup --selftest\n");
	selftest_columns();
	selftest_grouping();
	selftest_cache();
	selftest_rehash();
	printf("\n%d checks, %d failed\n", st_checks, st_failed);
	return st_failed ? 1 : 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * main
 * ──────────────────────────────────────────────────────────────────────── */

static void usage_die(const char *arg)
{
	kb_die("unknown option: %s", arg);
}

int main(int argc, char **argv)
{
	kb_set_progname("kdos-portup");

	Opts o = {0};
	char *want[MAX_PORTS];
	int nwant = 0;
	int selftest = 0;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--check"))
			o.check = 1;
		else if (!strcmp(a, "--json"))
			o.json = 1;
		else if (!strcmp(a, "--no-fetch"))
			o.no_fetch = 1;
		else if (!strcmp(a, "--refresh"))
			o.refresh = 1;
		else if (!strcmp(a, "--cve"))
			o.cve = 1;
		else if (!strcmp(a, "--selftest"))
			selftest = 1;
		else if (!strcmp(a, "--fixture")) {
			if (i + 1 >= argc)
				kb_die("--fixture needs a directory");
			o.fixture = argv[++i];
		} else if (a[0] == '-' && a[1]) {
			usage_die(a);
		} else if (nwant < MAX_PORTS) {
			want[nwant++] = argv[i];
		}
	}

	if (o.fixture)
		pu_http_set_fixture_dir(o.fixture);

	if (selftest)
		return run_selftest();

	char repo_root[1024];
	find_repo_root(repo_root, sizeof(repo_root));

	char kpkg_bin[1664];
	ensure_kpkg_bin(repo_root, kpkg_bin, sizeof(kpkg_bin));

	/* The same PORT_REPO the build uses: ports/core for every upstream
	 * port plus src/packages for the handful this tool has to be able to
	 * SEE (so it can filter them out by their empty `source`) without
	 * ever trying to check them. */
	char port_repo[4096];
	snprintf(port_repo, sizeof(port_repo), "%s/ports/core %s/src/packages",
		 repo_root, repo_root);
	setenv("PORT_REPO", port_repo, 1);

	KpConf conf;
	kp_conf_load(&conf);

	char (*names)[64] = kb_calloc(MAX_PORTS, sizeof(*names));
	int nnames = gather_names(&conf, want, nwant, names);
	if (nnames == 0)
		kb_die("no ports to check");

	char cache_path[1536];
	snprintf(cache_path, sizeof(cache_path), "%s/ports/.update-cache.json",
		 repo_root);
	Cache cache;
	memset(&cache, 0, sizeof(cache));
	/* --fixture makes pu_http_head answer 200 for any URL that has a
	 * recorded response, which is what lets the offline selftest exercise
	 * the proof step at all — but that 200 was never real. Loading or
	 * saving the cache under fixture mode would let a fixture-proved
	 * `newer` outlive the process: a genuine interactive run within the
	 * cache's 24h TTL would then offer a bump that was never actually
	 * checked against upstream. Fixture results are not evidence about
	 * the real world, so they must never touch the file a real run trusts. */
	if (!o.fixture)
		cache_load(cache_path, &cache);

	g_entries = kb_calloc(MAX_PORTS, sizeof(*g_entries));
	int any_newer = 0;
	g_nentries = discover(&conf, kpkg_bin, names, nnames, &cache, o.refresh,
			      &any_newer);


	if (!o.fixture)
		cache_save(cache_path, &cache);

	compute_groups(g_entries, g_nentries);

	int unrecoverable = 0;
	if (o.json) {
		print_json(g_entries, g_nentries);
	} else {
		int interactive = !o.check;
		review(g_entries, g_nentries, &o, repo_root, interactive,
		       &unrecoverable);
	}

	/* After the review, so the two answers do not interleave: freshness
	 * first, then the security cross-check. `names` is still alive here
	 * precisely for this. */
	if (o.cve)
		report_vulnerable(&conf, kpkg_bin, names, nnames);
	free(names);
	free(g_entries);

	/* Exit status ranks the three outcomes a script needs to tell apart:
	 * 2 means at least one port is now in the state this whole feature
	 * exists to prevent (a bump with no tarball fetched) and needs a
	 * human's attention before the next build, which outranks the merely
	 * informational "updates exist" of --check. */
	if (unrecoverable)
		return 2;
	if (o.check && any_newer)
		return 1;
	return 0;
}

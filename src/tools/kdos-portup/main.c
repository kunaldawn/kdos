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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kbuild.h"
#include "kpkg.h"
#include "portup.h"

/*
 * How many ports one run carries. It is this tool's own ceiling and nothing
 * else's — kp_all_ports() grows its answer — so it is set well above the tree
 * rather than at it: a run that silently stopped at the cut reported every
 * port after it as absent, which reads as a repo that lost them.
 */
#define MAX_PORTS  4096
#define GROUP_MAX  64		/* generous headroom over the 17-member max */
#define CACHE_TTL  86400	/* 24h, per the design                      */
/* The checker logic a cached verdict came from. An entry carrying any other
 * value, or none, is a miss: a verdict is only as good as the discovery and
 * filters that reached it, and a changed checker serving its predecessor's
 * answers for a day reports exactly what the change was made to stop. Raise
 * it with every change to what a port's answer can be. */
#define CACHE_LOGIC 5

/* ────────────────────────────────────────────────────────────────────────
 * CLI options
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int check;
	int cve;
	int json;
	int no_fetch;
	int refresh;
	int jobs;
	const char *fixture;
} Opts;

/* Checks in flight at once. A check is almost all waiting on a remote host,
 * so the run is bounded by the slowest ports rather than by their sum; eight
 * keeps any one host at a handful of concurrent requests, which every forge
 * and mirror in the tree serves without complaint. repology's one request a
 * second is enforced across all of them (probe.c). */
#define JOBS_DEFAULT 8
#define JOBS_MAX     32

/* ────────────────────────────────────────────────────────────────────────
 * Repo location and the on-demand kpkg-meta build
 *
 * `ports/update` (the wrapper) always runs this binary from
 * <repo_root>/ports/.portup, so walking two directories up from
 * /proc/self/exe finds repo_root without the caller ever having to say so —
 * the same trick ports/srclib.sh's src_kpkg_ensure relies on via
 * $SRCLIB_ROOT. KDOS_PORTUP_REPO is a maintenance escape hatch, not part of
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
 * fields (see portup.h). Building it here mirrors ports/srclib.sh's
 * src_kpkg_ensure, which builds ports/.kpkgbin/kpkg — same sources, same
 * flags — into a directory of this tool's own. The OUTPUT NAME is
 * load-bearing: kdos-kpkg dispatches on argv[0]'s basename (main.c's
 * `self`), falling back to argv[1] only when self itself is not one of the
 * five tool names, so a binary named anything other than exactly "kpkg"
 * has no way to reach the "meta" subcommand by invoking `<bin> meta <dir>`
 * the way pu_recipe_read's run_meta() and ports/srclib.sh's
 * `"$KPKG" meta .` both do — it prints "no tool named '<bin>'". */
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
	 * in ports/srclib.sh (src_kpkg_ensure) and testing/selftest.sh; all three must list the same
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
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	/* Everything from here on is this function's own allocation: KbArgv
	 * holds pointers and frees none of them. */
	int own = a.n;
	kb_argv_addf(&a, "-I%s", kdir);
	kb_argv_addf(&a, "-I%s", lbase);
	kb_argv_addf(&a, "-I%s", lpkg);
	kb_argv_addf(&a, "-I%s", lsig);
	add_c_files(&a, kdir);
	add_c_files(&a, lbase);
	add_c_files(&a, lpkg);
	add_c_files(&a, lsig);
	add_c_files(&a, lmono);
	kb_argv_end(&a);

	int rc = kb_run_tty(&a);
	for (int i = own; i < a.n; i++)
		free((void *)a.v[i]);
	if (rc != 0)
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
	int logic;		/* CACHE_LOGIC when written */
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
		ce->logic = (int)kj_num(e, "logic", 0);
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
	ce->logic = CACHE_LOGIC;
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
		kb_buf_printf(&b, ", \"low_confidence\": %s, \"checked\": %lld, \"logic\": %d}",
			      e->low_confidence ? "true" : "false", e->checked,
			      e->logic);
		kb_buf_str(&b, i + 1 < c->n ? ",\n" : "\n");
	}
	kb_buf_str(&b, "}\n");
	kb_write_all(path, b.p ? b.p : "{}\n", b.p ? b.n : 3);
	kb_buf_free(&b);
}

/* A hit needs four things to agree: the port was checked before, by this
 * checker's logic (CACHE_LOGIC), the recipe's version has not moved since (an
 * accepted bump invalidates a cached "current" for the OLD version), and the
 * entry is inside its 24h window. Any of those failing is an ordinary miss,
 * not an error. */
static int try_cache(Cache *c, const PuRecipe *r, int refresh, long long now,
		     PuResult *out)
{
	if (refresh)
		return 0;
	CacheEntry *e = cache_find(c, r->name);
	if (!e || e->logic != CACHE_LOGIC || strcmp(e->version, r->version))
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
		if (e->res.reason[0]) {
			kb_buf_printf(&b, "%s(%s)", had_flags ? " " : "",
				      e->res.reason);
			had_flags = 1;
		}
		if (prompt)
			kb_buf_str(&b, had_flags ? " [y/n/d/a/q]" : "[y/n/d/a/q]");
	} else if (e->res.state == PU_UNKNOWN) {
		kb_buf_printf(&b, "unknown: %s", e->res.reason);
	} else if (e->res.reason[0]) {
		/* A current port held to a series, or pinned to a branch
		 * tip, says so. */
		kb_buf_printf(&b, "  (%s)", e->res.reason);
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
	/* The fetch places the NEW tarball; nothing here ever deletes the old
	 * one. It is gitignored and a hard link into ports/.srccache, but it is
	 * still a file in the port directory that no recipe line names, and
	 * testing/preflight.sh reports exactly that — so the maintainer is told,
	 * and deciding what to remove is their call, never this tool's.
	 *
	 * THE NEW SOURCE EXISTS ONLY HERE UNTIL IT IS PUBLISHED. The recipe now
	 * names a hash the kunaldawn/kdos-sources archive does not hold, so
	 * every other clone's `make fetch` falls back to upstream and the
	 * pre-push hook refuses the commit, until `ports/publish` uploads it. */
	if (!no_fetch) {
		printf("    note: the old v%s tarball is still in %s — remove it by hand if it is no longer wanted\n",
		       old_ver, e->r.portdir);
		printf("    next: ports/publish %s — before pushing the bump\n",
		       e->r.name);
	}
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
		/* Same notes as accept_one, per member — a group bump leaves one
		 * superseded tarball behind, and one unpublished source, for every
		 * port it touched. */
		if (!no_fetch) {
			printf("    note: the old v%s tarball is still in %s — remove it by hand if it is no longer wanted\n",
			       old_ver[i], m[i]->r.portdir);
			printf("    next: ports/publish %s — before pushing the bump\n",
			       m[i]->r.name);
		}
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
		if (count > MAX_PORTS)
			kb_warn("%d ports in the tree, %d carried: raise MAX_PORTS",
				count, MAX_PORTS);
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

/* One pu_check in a child process, its PuResult written whole to `fd`. The
 * struct is plain data and smaller than a pipe's buffer, so the write never
 * blocks on a parent that has not started reading yet. */
static void check_in_child(const char *kpkg_bin, const PuRecipe *r, int fd)
{
	PuResult res;
	pu_check(kpkg_bin, r, &res);
	const char *p = (const char *)&res;
	size_t left = sizeof(res);
	while (left) {
		ssize_t w = write(fd, p, left);
		if (w <= 0)
			break;
		p += w;
		left -= (size_t)w;
	}
	_exit(0);
}

/* Runs pu_check for every entry in `todo`, `jobs` at a time, each in its own
 * process. A child that dies before writing its whole result leaves that
 * port `unknown` with a reason — never `current`, and never a guess from
 * half a struct. With jobs == 1 nothing forks. */
static void run_checks(const char *kpkg_bin, PortEntry *pe, const int *todo,
		       int ntodo, int jobs)
{
	if (jobs <= 1) {
		for (int i = 0; i < ntodo; i++) {
			fprintf(stderr, "[%d/%d] %s\n", i + 1, ntodo,
				pe[todo[i]].r.name);
			pu_check(kpkg_bin, &pe[todo[i]].r, &pe[todo[i]].res);
		}
		return;
	}

	struct { pid_t pid; int fd, idx; } w[JOBS_MAX];
	int active = 0, next = 0, done = 0;

	/* A child inherits the parent's unwritten stdio buffers, and a child
	 * that exits through kb_die flushes them a second time. */
	fflush(stdout);
	fflush(stderr);

	while (done < ntodo) {
		while (active < jobs && next < ntodo) {
			int idx = todo[next++];
			fprintf(stderr, "[%d/%d] %s\n", next, ntodo, pe[idx].r.name);
			int fds[2];
			if (pipe(fds) != 0) {
				pu_check(kpkg_bin, &pe[idx].r, &pe[idx].res);
				done++;
				continue;
			}
			pid_t pid = fork();
			if (pid == 0) {
				close(fds[0]);
				check_in_child(kpkg_bin, &pe[idx].r, fds[1]);
			}
			close(fds[1]);
			if (pid < 0) {
				close(fds[0]);
				pu_check(kpkg_bin, &pe[idx].r, &pe[idx].res);
				done++;
				continue;
			}
			w[active].pid = pid;
			w[active].fd = fds[0];
			w[active].idx = idx;
			active++;
		}
		if (!active)
			break;

		int st;
		pid_t pid = waitpid(-1, &st, 0);
		if (pid < 0)
			break;
		for (int i = 0; i < active; i++) {
			if (w[i].pid != pid)
				continue;
			PuResult res;
			char *p = (char *)&res;
			size_t got = 0;
			while (got < sizeof(res)) {
				ssize_t r = read(w[i].fd, p + got, sizeof(res) - got);
				if (r <= 0)
					break;
				got += (size_t)r;
			}
			close(w[i].fd);
			PuResult *out = &pe[w[i].idx].res;
			if (got == sizeof(res)) {
				*out = res;
			} else {
				memset(out, 0, sizeof(*out));
				out->state = PU_UNKNOWN;
				snprintf(out->reason, sizeof(out->reason),
					 "the check did not finish");
			}
			w[i] = w[--active];
			done++;
			break;
		}
	}
}

/* Checks every named port (cache first, unless --refresh), storing a
 * PortEntry for each one that has an upstream source at all — the ports
 * that are ours (kdos-*) are dropped here, silently, exactly once,
 * rather than at every later call site that would otherwise have to know
 * the same rule. Recipes are read and the cache consulted first, in tree
 * order; only the misses go to the network, `jobs` at a time. Progress goes
 * to stderr: checking is network-bound and can take minutes over the whole
 * tree, and stderr keeps --json's stdout clean while still telling a human
 * something is happening. */
static int discover(const KpConf *conf, const char *kpkg_bin, char names[][64],
		    int nnames, Cache *cache, int refresh, int jobs,
		    int *any_newer)
{
	long long now = (long long)time(NULL);
	int n = 0;
	int *todo = kb_calloc(MAX_PORTS, sizeof(int));
	int ntodo = 0;

	for (int i = 0; i < nnames && n < MAX_PORTS; i++) {
		char *dir = kp_port_dir(conf, names[i]);
		if (!dir) {
			kb_warn("%s: no such port", names[i]);
			continue;
		}

		PortEntry *e = &g_entries[n];
		memset(e, 0, sizeof(*e));
		if (pu_recipe_read(kpkg_bin, dir, &e->r) != 0) {
			kb_warn("%s: could not read the recipe", names[i]);
			free(dir);
			continue;
		}
		free(dir);

		if (!e->r.source[0])
			continue;	/* ours: no upstream to check */

		if (!try_cache(cache, &e->r, refresh, now, &e->res)) {
			/* What a port reads as until its check reports back. */
			e->res.state = PU_UNKNOWN;
			snprintf(e->res.reason, sizeof(e->res.reason),
				 "the check did not finish");
			todo[ntodo++] = n;
		}
		n++;
	}

	run_checks(kpkg_bin, g_entries, todo, ntodo, jobs);
	for (int i = 0; i < ntodo; i++) {
		PortEntry *e = &g_entries[todo[i]];
		cache_upsert(cache, e->r.name, e->r.version, &e->res, now);
	}
	free(todo);

	for (int i = 0; i < n; i++) {
		PortEntry *e = &g_entries[i];
		if (e->res.state == PU_NEWER)
			*any_newer = 1;
		e->has_group = pu_group_key(&e->r, e->group, sizeof(e->group));
	}
	return n;
}

/* ────────────────────────────────────────────────────────────────────────
 * --selftest — offline assertions
 *
 * kp_vercmp/kp_vershape/pu_extract have a table in src/libs/selftest.c. This
 * one covers the rest: the line layout, the grouping decision, the cache's
 * round trip and the rehash, which need nothing; the anchors and filters of
 * match.c, which are pure; and every discovery adapter, replayed from the
 * responses recorded under testing/fixtures/portup, so no check here makes a
 * network request.
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

	/* On the heap, for the reason main's is: one entry per port, each
	 * carrying a URL, is megabytes. */
	Cache *c = kb_calloc(1, sizeof(*c));
	PuResult r = {0};
	r.state = PU_NEWER;
	kb_strlcpy(r.candidate, "8.21.0", sizeof(r.candidate));
	kb_strlcpy(r.url, "https://example.invalid/curl-8.21.0.tar.xz",
		   sizeof(r.url));
	r.low_confidence = 1;
	cache_upsert(c, "curl", "8.17.0", &r, 1000);

	PuResult weird = {0};
	weird.state = PU_UNKNOWN;
	kb_strlcpy(weird.reason, "a \"quoted\" host\\path\nwith control bytes",
		   sizeof(weird.reason));
	cache_upsert(c, "weird\"name", "1.0", &weird, 1000);

	cache_save(tmpl, c);

	/* On the heap: a Cache holds MAX_PORTS entries and is measured in
	 * megabytes, which is more than a thread's stack. */
	Cache *back = kb_calloc(1, sizeof(*back));

	if (!back)
		return;
	cache_load(tmpl, back);
	st_ok(back->n == 2, "round trip preserves entry count");

	CacheEntry *ce = cache_find(back, "curl");
	st_ok(ce != NULL, "round trip preserves the name");
	if (ce) {
		st_ok(!strcmp(ce->version, "8.17.0"), "round trip preserves version");
		st_ok(ce->state == PU_NEWER, "round trip preserves state");
		st_ok(!strcmp(ce->candidate, "8.21.0"), "round trip preserves candidate");
		st_ok(ce->low_confidence == 1, "round trip preserves low_confidence");
		st_ok(ce->checked == 1000, "round trip preserves the timestamp");
		st_ok(ce->logic == CACHE_LOGIC, "round trip preserves the logic stamp");
	}

	CacheEntry *cw = cache_find(back, "weird\"name");
	st_ok(cw != NULL, "an escaped key round-trips");
	if (cw)
		st_ok(!strcmp(cw->reason, weird.reason),
		      "quotes, backslashes and control bytes round-trip");

	PuResult hit;
	st_ok(try_cache(c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			0, 1000 + CACHE_TTL - 1, &hit) == 1,
	      "a fresh, version-matched entry is a hit");
	st_ok(try_cache(c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			0, 1000 + CACHE_TTL + 1, &hit) == 0,
	      "an entry past its TTL is a miss");
	st_ok(try_cache(c, &(PuRecipe){ .name = "curl", .version = "8.18.0" },
			0, 1000, &hit) == 0,
	      "a version mismatch (the recipe moved) is a miss");
	st_ok(try_cache(c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			1, 1000, &hit) == 0,
	      "--refresh always misses");
	CacheEntry *stale = cache_find(c, "curl");
	stale->logic = CACHE_LOGIC - 1;
	st_ok(try_cache(c, &(PuRecipe){ .name = "curl", .version = "8.17.0" },
			0, 1000, &hit) == 0,
	      "a verdict from another checker's logic is a miss");

	unlink(tmpl);

	/* A corrupt cache reads as an empty one, never as a partial one. */
	char tmpl2[] = "/tmp/portup-selftest.XXXXXX";
	int fd2 = mkstemp(tmpl2);
	if (fd2 >= 0) {
		ssize_t junk_written = write(fd2, "{not json", 9);
		st_ok(junk_written == 9, "wrote the corrupt fixture");
		close(fd2);
	}
	Cache *corrupt = kb_calloc(1, sizeof(*corrupt));

	cache_load(tmpl2, corrupt);
	st_ok(corrupt->n == 0, "a cache that does not parse whole reads as absent");
	unlink(tmpl2);
	free(corrupt);
	free(back);
	free(c);
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

static int anchor_reads(const char *name, const char *version, int archive,
			const char *raw, const char *want)
{
	PuAnchor a;
	char v[PU_MAX_VER];
	if (!pu_anchor_from(name, version, archive, &a))
		return 0;
	if (pu_anchor_match(&a, raw, v, sizeof(v)))
		return want == NULL;
	return want && !strcmp(v, want);
}

static void selftest_match(void)
{
	st_ok(!strcmp(pu_source_url("tokei-14.0.0.tar.gz::https://github.com/x/y/v14.tar.gz"),
		      "https://github.com/x/y/v14.tar.gz"),
	      "a cache name is not part of the URL");
	st_ok(!strcmp(pu_source_url("https://zlib.net/zlib-1.3.1.tar.gz"),
		      "https://zlib.net/zlib-1.3.1.tar.gz"),
	      "a plain source is its own URL");

	/* A shared directory: only this port's own files are read. */
	st_ok(anchor_reads("xcb-util-0.4.1.tar.xz", "0.4.1", 1,
			   "xcb-util-0.4.2.tar.gz", "0.4.2"),
	      "any archive suffix is read");
	st_ok(anchor_reads("xcb-util-0.4.1.tar.xz", "0.4.1", 1,
			   "xcb-util-cursor-0.1.6.tar.xz", NULL),
	      "a sibling project sharing the prefix is not read");
	st_ok(anchor_reads("xcb-util-0.4.1.tar.xz", "0.4.1", 1,
			   "xcb-util-0.4.2.tar.xz.sig", NULL),
	      "a signature is not an archive");
	st_ok(anchor_reads("gcc-15.2.0.tar.xz", "15.2.0", 1,
			   "https://ftp.gnu.org/gnu/gcc/gcc-16.1.0/gcc-16.1.0.tar.xz",
			   "16.1.0"),
	      "a URL is read by its last segment");
	st_ok(anchor_reads("boost_1_89_0.tar.bz2", "1.89.0", 1,
			   "boost_1_90_0.tar.bz2", "1.90.0"),
	      "an underscore spelling reads back as dots");
	st_ok(anchor_reads("llvmorg-21.1.8", "21.1.8", 0, "llvmorg-23.1.2",
			   "23.1.2"),
	      "a tag prefix is literal");
	st_ok(anchor_reads("gopls/v0.20.0", "0.20.0", 0, "gopls/v0.21.1",
			   "0.21.1"),
	      "a tag spanning a slash is read whole");
	st_ok(anchor_reads("gopls/v0.20.0", "0.20.0", 0, "v0.21.1", NULL),
	      "and the repository's other tag family is not");
	PuAnchor bare;
	char got[PU_MAX_VER];
	st_ok(pu_anchor_from("0.196", "0.196", 0, &bare) &&
	      (bare.digits_only = 1) &&
	      pu_anchor_match(&bare, "5.1K", got, sizeof(got)) != 0 &&
	      !pu_anchor_match(&bare, "0.197/", got, sizeof(got)) &&
	      !strcmp(got, "0.197"),
	      "a bare directory name reads digits only, not a file size");
	st_ok(anchor_reads("passt-2026_07_28.f8df3f1.tar.xz", "2026_07_28.f8df3f1",
			   1, "passt-2026_08_02.a1b2c3d.tar.xz",
			   "2026_08_02.a1b2c3d"),
	      "a version carrying a commit id");
	st_ok(pu_same_class("2026_08_02.a1b2c3d", "2026_07_28.f8df3f1"),
	      "whose commit ids differ in shape");
	st_ok(!pu_same_class("600.0132", "26.2.4"),
	      "a zero-padded component is another numbering");
	st_ok(pu_same_class("26.01", "25.10"), "unless the pin pads to the same width");
	st_ok(!pu_same_class("5.0-post1", "5.0.9"),
	      "a post-release of an older base is older");
	st_ok(!pu_same_class("5.1.22_dict", "5.1.21"), "a variant file is no release");
	st_ok(!pu_same_class("3.14.7-win32", "3.14.2"), "nor is a platform build");
	st_ok(pu_same_class("3.6a", "3.5"), "a single-letter release is one");
	st_ok(!pu_same_class("3.8.13-w64", "3.8.12"), "a Windows build is no release");
	st_ok(!pu_same_class("1.8.1.3.patch", "1.8.1.2"), "nor a patch beside one");
	st_ok(!pu_same_class("56.7z", "7.1.2.31"), "nor an archive suffix");
	st_ok(pu_same_class("1.9.0.jumbo2", "1.9.0.jumbo1"),
	      "a word the pin carries is part of the numbering");
	st_ok(pu_same_class("2026e", "2026d") && pu_same_class("1.9.17p3", "1.9.17p2") &&
	      pu_same_class("0.5.4+git20240101", "0.5.3+git20230121"),
	      "tzdata's letter, sudo's p-level, a git date the pin carries");
	st_ok(pu_in_series("21.1.8", "21") && !pu_in_series("210.1", "21") &&
	      !pu_in_series("5.5.0", "5.4") && pu_in_series("1.0.199", "1.0.199") &&
	      pu_in_series("4.5", ""),
	      "a series holds whole components");
	st_ok(anchor_reads("ghostscript-10.07.1.tar.xz", "10.07.1", 1,
			   "ghostscript-10.08.0.tar.xz", "10.08.0"),
	      "a zero-padded part reads as written");
	st_ok(anchor_reads("gs10071", "10.07.1", 0, "gs10080", "10.08.0"),
	      "a squashed spelling is dotted back at the pin's widths");
	st_ok(anchor_reads("unzip60.tar.gz", "6.0", 1, "unzip610.tar.gz", NULL),
	      "and one with more digits than the pin's parts is not read");
	char longv[PU_MAX_VER + 8];
	memset(longv, '1', sizeof(longv) - 1);
	longv[sizeof(longv) - 1] = 0;
	longv[1] = '.';
	st_ok(!pu_anchor_from("x-1234", longv, 0, &bare),
	      "a version longer than any buffer anchors nothing");
	st_ok(anchor_reads("libevent-2.1.12-stable.tar.gz", "2.1.12", 1,
			   "libevent-2.2.1-alpha.tar.gz", NULL),
	      "a literal suffix after the version must be there");
	st_ok(anchor_reads("ImageMagick-7.1.2-31.tar.xz", "7.1.2.31", 1,
			   "ImageMagick-7.1.2-32.tar.xz", "7.1.2.32"),
	      "a version spelled with mixed separators reads back dotted");
	st_ok(anchor_reads("ImageMagick-7.1.2-31.tar.xz", "7.1.2.31", 1,
			   "ImageMagick-6.9.13-56.7z", "6.9.13.56"),
	      "and a .7z beside it is an archive, not a version's tail");
	st_ok(anchor_reads("cacert-2026-08-13.pem", "20260813", 1,
			   "/ca/cacert-2026-09-01.pem", "20260901"),
	      "a date pin reads through the source's separators");
	st_ok(anchor_reads("cacert-2026-08-13.pem", "20260813", 1,
			   "cacert-2026-9-1.pem", NULL),
	      "but only at the pin's group widths");
	char ex[PU_MAX_CAND][PU_MAX_VER];
	int nex = pu_extract("ImageMagick-6.9.13-56.7z", ex, PU_MAX_CAND);
	int has7z = 0;
	for (int i = 0; i < nex; i++)
		has7z |= strstr(ex[i], "7z") != NULL;
	st_ok(nex > 0 && !has7z, "the extractor trims a .7z suffix");

	char pre[PU_MAX_VER];
	st_ok(!pu_version_prefix("4.2.8", 2, pre, sizeof(pre)) &&
	      !strcmp(pre, "4.2"), "a two-component series of 4.2.8");
	st_ok(pu_version_prefix("4", 2, pre, sizeof(pre)) != 0,
	      "no two-component series of 4");

	/* Classes. */
	st_ok(pu_same_class("2.47", "2.45.1"), "binutils' x.y after x.y.z");
	st_ok(pu_same_class("4.0", "3.10.2"), "nettle's new major");
	st_ok(pu_same_class("25.07.1", "25.07"), "helix's point release");
	st_ok(pu_same_class("10.3p1", "10.2p1"), "OpenSSH's p-suffix");
	st_ok(pu_same_class("1.5.8.pl02", "1.5.6"), "libburnia's .plNN");
	st_ok(!pu_same_class("20260101", "1.2"), "a date is not a counter");
	st_ok(!pu_same_class("2026.7.22", "84.0.0"), "a dotted date is not one either");
	st_ok(!pu_same_class("22-init", "21.1.8"), "llvm's branch tag is no release");

	/* Pre-releases. */
	st_ok(pu_prerelease("1.26-rc1", "1.25"), "an rc");
	st_ok(pu_prerelease("3.15.0b2", "3.14.7"), "a PEP 440 beta");
	st_ok(pu_prerelease("2.9.0dev.12", "2.8.9"), "a dev build");
	st_ok(pu_prerelease("2.0.0-b9", "1.8.1.1"), "a separated beta");
	st_ok(!pu_prerelease("3.6a", "3.5"), "tmux's letter release");
	st_ok(!pu_prerelease("2025_02_17.a1e48a0", "2025_01_21.4f2c8e7") &&
	      !pu_prerelease("2026_01_20.386b5f5", "2025_01_21.4f2c8e7"),
	      "a commit id holding a1 or b5 is no beta");

	/* Pretests, judged against the rest of the list. */
	char gnome[][PU_MAX_VER] = { "1.25.91", "1.25.90", "1.25.0", "1.24.0" };
	char ctr[][PU_MAX_VER] = { "1.0.92", "1.0.91", "1.0.89", "1.0.88" };
	st_ok(pu_pretest("1.25.91", "1.24.0", gnome, 4),
	      "a GNOME/freedesktop x.y.9x");
	st_ok(pu_pretest("4.4.0.90", "4.4.1", gnome, 0), "a GNU pretest");
	st_ok(pu_pretest("26.0.99.902", "24.1.13", gnome, 0), "an X.Org x.y.99.z");
	st_ok(!pu_pretest("1.94.100", "1.94.9", gnome, 0),
	      "a micro counter past 99 is not a pretest");
	st_ok(!pu_pretest("2.4.134", "2.4.131", gnome, 0),
	      "not when the project's micro numbers are already that high");
	st_ok(!pu_pretest("1.0.92", "1.0.60", ctr, 4),
	      "nor when the list walks up through the eighties");
	st_ok(!pu_pretest("1.0.92", "1.0.85", ctr, 1),
	      "nor when the pin itself is in the eighties");
	st_ok(!pu_prerelease("1.4rc6", "1.4rc5"),
	      "a port that pins a pre-release follows that line");

	/* Development series. */
	const char *gst = "https://gstreamer.freedesktop.org/src/gst-libav/x.tar.xz";
	const char *cpan = "https://www.cpan.org/src/5.0/perl-5.44.0.tar.gz";
	st_ok(pu_devseries("1.29.2", "1.28.7", "", gst), "GStreamer's odd minor");
	st_ok(!pu_devseries("5.45.2", "5.44.0", "", cpan),
	      "no convention is assumed where none is declared");
	st_ok(pu_devseries("5.45.2", "5.44.0", "odd-minor", cpan),
	      "the recipe's devseries key declares one");
	st_ok(pu_devseries("1.90.0", "1.58.2", "preview-minor", cpan),
	      "preview-minor: Pango 1.90 is Pango 2");
	st_ok(!pu_devseries("1.29.2", "1.29.1", "odd-minor", gst),
	      "a port pinned in a development series follows it");
}

/* One adapter, replayed: a recipe as pu_recipe_read would leave it, and the
 * newest candidate discovery must hand the decision engine. `absent`, when
 * set, is a version that must NOT be among the candidates; `other`, when set,
 * is what the tag list names under another prefix. */
static void st_found_in(const char *what, const char *version, const char *url,
			const char *homepage, const char *devseries,
			const char *via, const char *top, const char *absent,
			const char *other)
{
	PuRecipe r;
	memset(&r, 0, sizeof(r));
	kb_strlcpy(r.name, "selftest", sizeof(r.name));
	kb_strlcpy(r.version, version, sizeof(r.version));
	kb_strlcpy(r.source, url, sizeof(r.source));
	kb_strlcpy(r.first_source, url, sizeof(r.first_source));
	kb_strlcpy(r.homepage, homepage, sizeof(r.homepage));
	kb_strlcpy(r.devseries, devseries, sizeof(r.devseries));

	PuFound f = { 0 };
	f.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	pu_discover(&r, &f);

	int ok = f.n > 0 && !strcmp(f.cand[0], top) && !strcmp(f.via, via) &&
		 !strcmp(f.other_prefix, other ? other : "");
	for (int i = 0; ok && absent && i < f.n; i++)
		ok = strcmp(f.cand[i], absent) != 0;
	st_checks++;
	if (!ok) {
		st_failed++;
		printf("  NOT OK: %s (via %s, %d candidates, newest %s, other %s)\n",
		       what, f.via[0] ? f.via : "-", f.n, f.n ? f.cand[0] : "-",
		       f.other_prefix[0] ? f.other_prefix : "-");
	}
	free(f.cand);
}

static void st_recipe(PuRecipe *r, const char *version, const char *url)
{
	memset(r, 0, sizeof(*r));
	kb_strlcpy(r->name, "selftest", sizeof(r->name));
	kb_strlcpy(r->version, version, sizeof(r->version));
	kb_strlcpy(r->source, url, sizeof(r->source));
	kb_strlcpy(r->first_source, url, sizeof(r->first_source));
}

static void st_found(const char *what, const char *version, const char *url,
		     const char *devseries, const char *via, const char *top,
		     const char *absent)
{
	st_found_in(what, version, url, "", devseries, via, top, absent, NULL);
}

static void selftest_adapters(void)
{
	st_found("PyPI, from a hashed file URL", "4.0.2",
		 "https://files.pythonhosted.org/packages/46/ef/0f1e/flit_core-4.0.2.tar.gz",
		 "", "registry", "4.1.0", NULL);
	st_found("PyPI, from a packages/source URL", "4.0.2",
		 "https://files.pythonhosted.org/packages/source/f/flit_core/flit_core-4.0.2.tar.gz",
		 "", "registry", "4.1.0", NULL);
	st_found("crates.io", "1.0.10",
		 "https://static.crates.io/crates/itoa/itoa-1.0.10.crate",
		 "", "registry", "1.0.18", NULL);
	st_found("MetaCPAN", "5.34",
		 "https://cpan.metacpan.org/authors/id/O/OA/OALDERS/URI-5.34.tar.gz",
		 "", "registry", "5.37", NULL);
	st_found("SourceForge's file feed", "3.100",
		 "https://downloads.sourceforge.net/lame/lame-3.100.tar.gz",
		 "", "sourceforge", "4.0", NULL);
	st_found("cgit snapshot, tags from git", "1.7.0",
		 "https://git.kernel.org/pub/scm/utils/dtc/dtc.git/snapshot/dtc-1.7.0.tar.gz",
		 "", "forge", "1.8.1", NULL);
	st_found("GitHub release asset under a tag prefix", "3.18.2",
		 "https://github.com/libfuse/libfuse/releases/download/fuse-3.18.2/fuse-3.18.2.tar.gz",
		 "", "forge", "3.18.3", NULL);
	st_found("raw.githubusercontent is GitHub", "2025.01",
		 "https://raw.githubusercontent.com/hugsy/gef/2025.01/gef.py",
		 "", "forge", "2026.01", NULL);
	st_found("GitLab's API when git has no answer", "1.24.0",
		 "https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.24.0/downloads/wayland-1.24.0.tar.xz",
		 "", "forge", "1.26.0", "1.25.91");
	st_found("a version-named directory's siblings", "15.2.0",
		 "https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.xz",
		 "", "directory", "16.2.0", NULL);
	st_found("a series directory's newer siblings", "4.2.1",
		 "https://cmake.org/files/v4.2/cmake-4.2.1.tar.gz",
		 "", "directory", "4.4.3", "4.4.0-rc3");
	st_found("a directory every suckless tool shares", "2.0",
		 "https://dl.suckless.org/tools/ii-2.0.tar.gz",
		 "", "directory", "2.0", NULL);
	st_found("a release candidate beside its release", "1.25",
		 "https://download.savannah.gnu.org/releases/lzip/lzip-1.25.tar.gz",
		 "", "directory", "1.26", "1.26-rc1");
	st_found("GStreamer's odd-minor development series", "1.28.0",
		 "https://gstreamer.freedesktop.org/src/gst-libav/gst-libav-1.28.0.tar.xz",
		 "", "directory", "1.28.7", "1.29.2");
	st_found("a typo tag on an older release's commit", "2.5.12",
		 "https://github.com/intel/thermal_daemon/archive/refs/tags/v2.5.12.tar.gz",
		 "", "forge", "2.5.13", "2.15.10");
	st_found("a release tagged on the previous one's commit", "0.45",
		 "https://github.com/YosysHQ/sby/archive/refs/tags/yosys-0.45.tar.gz",
		 "", "forge", "0.47", NULL);
	st_found_in("newer tags under another prefix", "0.47",
		    "https://github.com/YosysHQ/sby/archive/refs/tags/yosys-0.47.tar.gz",
		    "", "", "forge", "0.47", NULL, "0.69");
	st_found("a series tag on its newest release's commit", "0.5.2",
		 "https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.5.2.tar.gz",
		 "", "forge", "0.6.1", NULL);
	st_found_in("another module's tags in the same repository", "0.23.0",
		    "https://github.com/golang/tools/archive/refs/tags/gopls/v0.23.0.tar.gz",
		    "", "", "forge", "0.23.0", NULL, NULL);
	st_found_in("a download page linked from the homepage", "1.3.1",
		    "https://www.netfilter.org/pub/libnftnl/libnftnl-1.3.1.tar.xz",
		    "https://netfilter.org/projects/libnftnl/downloads.html",
		    "", "homepage", "1.3.2", NULL, NULL);

	st_found("a meta refresh to the page that lists, a date in groups", "20251202",
		 "https://curl.se/ca/cacert-2025-12-02.pem", "", "directory",
		 "20260813", NULL);
	st_found("an iframe standing in for the listing", "3.6.7",
		 "https://www.nethack.org/download/3.6.7/nethack-367-src.tgz",
		 "", "directory", "5.0.0", NULL);
	st_found("a download page one hop from the directory's parent", "1.4",
		 "https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2",
		 "", "directory", "1.5", NULL);
	st_found("a download page linking back with ../", "1.8.33",
		 "https://marlam.de/msmtp/releases/msmtp-1.8.33.tar.xz",
		 "", "directory", "1.8.34", NULL);
	st_found("a page that links only its current release", "4.2.1",
		 "https://www.mpfr.org/mpfr-4.2.1/mpfr-4.2.1.tar.xz",
		 "", "directory", "4.2.2", NULL);
	st_found("an S3 bucket lists under ?prefix=", "0.18",
		 "https://s3.amazonaws.com/json-c_releases/releases/json-c-0.18.tar.gz",
		 "", "directory", "0.19", NULL);
	st_found("Bitbucket is a git forge", "4.1",
		 "https://bitbucket.org/multicoreware/x265_git/downloads/x265_4.1.tar.gz",
		 "", "forge", "4.2", NULL);
	st_found("a page whose links lead elsewhere is no listing", "0.50.2",
		 "http://launchpad.net/intltool/trunk/0.50.2/+download/intltool-0.50.2.tar.gz",
		 "", "directory", "0.51.0", "14.09");
	st_found("a patch beside a release is no version", "1.8.1.2",
		 "http://www.dest-unreach.org/socat/download/socat-1.8.1.2.tar.gz",
		 "", "directory", "1.8.1.3", "1.8.1.3.patch");
	st_found("a Windows build beside a release is no version", "3.8.12",
		 "https://www.gnupg.org/ftp/gcrypt/gnutls/v3.8/gnutls-3.8.12.tar.xz",
		 "", "directory", "3.8.13", "3.8.13-w64");

	PuRecipe x;
	PuFound xf = { 0 };
	xf.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);

	/* A link in the page chrome is no sibling directory: the frameworks
	 * index's footer names linkedin.com/company/29561/. */
	st_recipe(&x, "6.29.0",
		  "https://download.kde.org/stable/frameworks/6.29/extra-cmake-modules-6.29.0.tar.xz");
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "6.30.0") && !xf.truncated,
	      "an off-listing href is not read as a later series");

	st_recipe(&x, "0.5.2",
		  "https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.5.2.tar.gz");
	kb_strlcpy(x.series, "0.5", sizeof(x.series));
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "0.5.2") && !strcmp(xf.outside, "0.6.1"),
	      "a series key holds the line, and remembers what is past it");

	/* A tag past the newest GitHub release may be an engineering drop
	 * (intel) or a release whose release object is late (bindgen): it
	 * stays a candidate, marked, never dropped into a current. */
	st_recipe(&x, "26.2.3",
		  "https://github.com/intel/media-driver/archive/refs/tags/intel-media-26.2.3.tar.gz");
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "26.3.5") &&
	      !strcmp(xf.unreleased, "26.3.5") && xf.low_confidence,
	      "a tag GitHub has no release for stays, marked low confidence");
	st_recipe(&x, "0.72.1",
		  "https://github.com/rust-lang/rust-bindgen/archive/refs/tags/v0.72.1.tar.gz");
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "0.73.2") &&
	      !strcmp(xf.unreleased, "0.73.2") && xf.low_confidence,
	      "a release whose GitHub release is late is still a candidate");
	st_recipe(&x, "0.71.1",
		  "https://github.com/rust-lang/rust-bindgen/archive/refs/tags/v0.71.1.tar.gz");
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "0.73.2") && xf.low_confidence,
	      "and so is one past a released one");

	/* A SourceForge feed that ends at the pin is not proof of current:
	 * the project may have left, and SourceForge or repology says so. */
	PuResult xr;
	st_recipe(&x, "3.0.18",
		  "http://downloads.sourceforge.net/gnu-efi/gnu-efi-3.0.18.tar.bz2");
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_UNKNOWN && strstr(xr.reason, "4.0.4 is tagged at") &&
	      strstr(xr.reason, "github.com/ncroxon/gnu-efi"),
	      "a SourceForge project moved to later tags is unknown at its last file");
	st_recipe(&x, "23.7",
		  "https://sourceforge.net/projects/psmisc/files/psmisc/psmisc-23.7.tar.xz");
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_CURRENT,
	      "and one whose new repository tags nothing past the pin is current");
	st_recipe(&x, "7.5",
		  "https://downloads.sourceforge.net/smartmontools/smartmontools-7.5.tar.gz");
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_CURRENT,
	      "and so is one whose tags spell the version with underscores");
	st_recipe(&x, "0.15.1b",
		  "https://downloads.sourceforge.net/mad/libid3tag-0.15.1b.tar.gz");
	kb_strlcpy(x.name, "libid3tag", sizeof(x.name));
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_UNKNOWN && strstr(xr.reason, "repology names 0.16.3"),
	      "and so is one whose feed ends where repology knows later");
	st_recipe(&x, "4.0", "https://downloads.sourceforge.net/lame/lame-4.0.tar.gz");
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_CURRENT,
	      "an active project whose feed ends at the pin is current");

	/* A release tagged on GitHub whose tarball upstream uploads only to
	 * its own site: the homepage's download page links it. */
	st_recipe(&x, "1.18.2",
		  "https://github.com/hpjansson/chafa/releases/download/1.18.2/chafa-1.18.2.tar.xz");
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_UNKNOWN && !xr.url[0] &&
	      strstr(xr.reason, "none at the recipe's URL (newest 1.18.3)"),
	      "a tag whose file is at no candidate URL is unknown");
	kb_strlcpy(x.homepage, "https://hpjansson.org/chafa/", sizeof(x.homepage));
	pu_check("/nonexistent/kpkg", &x, &xr);
	st_ok(xr.state == PU_UNKNOWN &&
	      !strcmp(xr.url, "https://hpjansson.org/chafa/releases/chafa-1.18.3.tar.xz") &&
	      !strcmp(xr.reason, "newest 1.18.3 is at hpjansson.org, not at the recipe's URL"),
	      "and names upstream's own copy when its homepage links one");

	st_recipe(&x, "4.9.3",
		  "https://downloads.unidata.ucar.edu/netcdf-c/4.9.3/netcdf-c-4.9.3.tar.gz");
	kb_strlcpy(x.watch, "https://downloads.unidata.ucar.edu/netcdf-c/release_info.json",
		   sizeof(x.watch));
	pu_discover(&x, &xf);
	st_ok(xf.n && !strcmp(xf.cand[0], "4.10.1") && !strcmp(xf.via, "watch"),
	      "a watch key names the release list");

	st_recipe(&x, "0.24",
		  "https://github.com/newsboat/stfl/archive/bbb2404580e845df2556560112c8aefa27494d66.tar.gz");
	pu_discover(&x, &xf);
	st_ok(!xf.n && xf.pinned && !strcmp(xf.head, "master"),
	      "a pinned commit at a branch tip is found there");
	st_recipe(&x, "0.23",
		  "https://github.com/newsboat/stfl/archive/0123456789abcdef0123456789abcdef01234567.tar.gz");
	pu_discover(&x, &xf);
	st_ok(!xf.n && xf.pinned && !xf.head[0],
	      "and one no branch has moved past is not");
	free(xf.cand);

	/* files/v4.3/ was never recorded: a later series than the pin's that
	 * could not be listed leaves the walk incomplete. */
	PuRecipe cm;
	memset(&cm, 0, sizeof(cm));
	kb_strlcpy(cm.name, "selftest", sizeof(cm.name));
	kb_strlcpy(cm.version, "4.2.1", sizeof(cm.version));
	kb_strlcpy(cm.first_source, "https://cmake.org/files/v4.2/cmake-4.2.1.tar.gz",
		   sizeof(cm.first_source));
	PuFound cf = { 0 };
	cf.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	pu_discover(&cm, &cf);
	st_ok(cf.truncated, "an unread later series marks the walk incomplete");
	free(cf.cand);

	st_ok(pu_http_verdict(18, 200) == 0,
	      "a 200 whose body was cut is no answer");
	st_ok(pu_http_verdict(7, 301) == 0,
	      "a redirect whose next hop failed is no answer");
	st_ok(pu_http_verdict(28, 404) == 404, "a 404 is an answer however it ended");
	st_ok(pu_http_verdict(0, 200) == 200, "a whole 200 is one");
}

static int run_selftest(void)
{
	printf("kdos-portup --selftest\n");
	selftest_columns();
	selftest_grouping();
	selftest_cache();
	selftest_rehash();
	selftest_match();
	selftest_adapters();
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
	o.jobs = JOBS_DEFAULT;
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
		else if (!strcmp(a, "--jobs")) {
			if (i + 1 >= argc)
				kb_die("--jobs needs a number");
			o.jobs = atoi(argv[++i]);
			if (o.jobs < 1 || o.jobs > JOBS_MAX)
				kb_die("--jobs takes 1 to %d", JOBS_MAX);
		}
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

	/* git asks for credentials when a repository has moved or gone
	 * private, on the terminal or through whatever helper the desktop
	 * set. A batch check must hear "no" instead. */
	setenv("GIT_TERMINAL_PROMPT", "0", 1);
	unsetenv("GIT_ASKPASS");
	unsetenv("SSH_ASKPASS");

	char repo_root[1024];
	find_repo_root(repo_root, sizeof(repo_root));

	/* --selftest replays the recorded corpus whether or not --fixture
	 * names it: its adapter checks are meaningless against the network. */
	char fixture_default[1200];
	if (selftest && !o.fixture) {
		snprintf(fixture_default, sizeof(fixture_default),
			 "%s/testing/fixtures/portup", repo_root);
		o.fixture = fixture_default;
	}
	if (o.fixture)
		pu_http_set_fixture_dir(o.fixture);

	if (selftest)
		return run_selftest();

	char kpkg_bin[1664];
	ensure_kpkg_bin(repo_root, kpkg_bin, sizeof(kpkg_bin));

	/* The same PORT_REPO the build uses: ports/core for every upstream
	 * port plus src/packages for the handful this tool has to be able to
	 * SEE (so it can filter them out by their empty `source`) without
	 * ever trying to check them. */
	char port_repo[4096];
	snprintf(port_repo, sizeof(port_repo), "%s/ports/core %s/src/packages",
		 repo_root, repo_root);
	/* A fixture corpus carries the recipes its responses were recorded
	 * against. Replaying it against the live tree's recipes would stop
	 * reproducing anything the moment one of them is bumped. */
	char fixture_ports[1200];
	if (o.fixture) {
		snprintf(fixture_ports, sizeof(fixture_ports), "%s/ports",
			 o.fixture);
		if (kb_is_dir(fixture_ports))
			kb_strlcpy(port_repo, fixture_ports, sizeof(port_repo));
	}
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
	/* On the heap: the table is one entry per port and an entry carries a
	 * URL, so at the tree's size it is megabytes and a stack frame is not
	 * where megabytes go. */
	Cache *cache = kb_calloc(1, sizeof(*cache));
	/* --fixture makes pu_http_head answer 200 for any URL that has a
	 * recorded response, which is what lets the offline selftest exercise
	 * the proof step at all — but that 200 was never real. Loading or
	 * saving the cache under fixture mode would let a fixture-proved
	 * `newer` outlive the process: a genuine interactive run within the
	 * cache's 24h TTL would then offer a bump that was never actually
	 * checked against upstream. Fixture results are not evidence about
	 * the real world, so they must never touch the file a real run trusts. */
	if (!o.fixture)
		cache_load(cache_path, cache);

	g_entries = kb_calloc(MAX_PORTS, sizeof(*g_entries));
	int any_newer = 0;
	g_nentries = discover(&conf, kpkg_bin, names, nnames, cache, o.refresh,
			      o.jobs, &any_newer);


	if (!o.fixture)
		cache_save(cache_path, cache);

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
	free(cache);

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

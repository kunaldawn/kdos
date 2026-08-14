/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — upstream version checker for the ports tree
 *
 * Host-only. Nothing here ships on the ISO: build/fs/ports is an empty
 * bind-mount point, so there is no ports tree on the target to update.
 * ---------------------------------
 */

#ifndef PORTUP_H
#define PORTUP_H

#include <stddef.h>

#include "kbase.h"
#include "kpkg.h"

#define PU_MAX_VER   64
#define PU_MAX_RAW   128
#define PU_MAX_CAND  256

/* -1 if a < b, 0 if equal, 1 if a > b. */
/* The comparator is libkpkg's (kp_vercmp): `kdos cve` asks the same question
 * about the same strings, and two implementations would drift. */
/* Digit-runs collapse to 'N', letter-runs to 'a', separators are kept. A
 * string that is ONE digit run records its length ("N8") so a date cannot
 * compare equal in shape to a bare "1". */
void pu_shape(const char *v, char *out, size_t cap);
/* Extract version candidates from `raw` (filename or git tag). A generous,
 * shape-filter-ready extractor that handles diverse upstream naming conventions.
 * Uses a closure/fixpoint algorithm: iteratively trim archive suffixes
 * (tar/gz/xz/bz2/lz/lzma/zst/tgz/txz/tbz2/zip/pem/src/orig) and apply three
 * transformations (strip leading letter-prefix groups, strip letter immediately
 * followed by digit, emit hyphen-separated digit-containing segments) until no
 * new candidates are generated. A run longer than PU_MAX_VER (64 bytes) is
 * dropped (truncating a version would invent one). Returns count. */
int pu_extract(const char *raw, char out[][PU_MAX_VER], int max);

typedef struct {
	char name[64];
	char version[PU_MAX_VER];
	char source[1024];	/* fully expanded, as kpkg meta prints it */
	/* `source` is every tarball on one space-separated line — a port with
	 * vendored deps prints crates.io URLs after its own. The first token
	 * is always the port's own tarball (mesa has 7; the other 6 are
	 * vendor bundles), and it is the only one an updater probes, so it
	 * gets its own field rather than making every caller re-split. */
	char first_source[1024];
	char portdir[512];
	char vendoring[16];	/* "" when the recipe declares none        */
	int  npatches;
	char group[64];		/* explicit `group =` key, or ""           */
} PuRecipe;

/* Reads a recipe through `kpkg meta`, which prints the FULLY EXPANDED fields.
 * One parser, shared with the build — reimplementing helper expansion here is
 * how a maintenance tool and its build silently disagree. 0 on success. */
int pu_recipe_read(const char *kpkg_bin, const char *portdir, PuRecipe *r);

/* The URL the build WOULD download if this port were at `candidate`. The
 * recipe is copied to a temp dir with the version substituted and `kpkg meta`
 * expands it, so helper chains work for free — ca-certificates turns 20260115
 * into cacert-2026-01-15.pem with no special case — and the real tree is
 * never touched by a probe. 0 on success. */
int pu_render_candidate(const char *kpkg_bin, const PuRecipe *r,
			const char *candidate, char *url, size_t cap);

/* Rewrites `version =` in place, preserving the recipe's column alignment.
 * Returns 0 on success. Pass the old version to put it back. The write is
 * atomic (temp file + rename in the recipe's own directory), so an
 * interrupted write can never leave a truncated kpkgbuild — load-bearing
 * for the revert path, which calls this same function to put the old
 * version back after a failed fetch. */
int pu_rewrite_version(const char *portdir, const char *newv);

/* True when `line` is the recipe's `version =` key line — the one line
 * pu_rewrite_version is allowed to touch. Exported so a read-only preview
 * (main.c's `d` command) can recognise the same line pu_rewrite_version
 * would, rather than carrying its own copy of the same rule. */
int pu_is_version_line(const char *line);

/* HTTP status, or 0 when curl could not complete the request at all. The
 * distinction matters: 404 means "no such version", 0 means "we do not know",
 * and those are different outcomes to the caller. */
int pu_http_head(const char *url);
int pu_http_get(const char *url, KbBuf *out);

/* Fixture mode: when set, every GET reads testing/fixtures/portup/<slug>
 * instead of the network, which is what makes the pipeline testable offline. */
void pu_http_set_fixture_dir(const char *dir);

/* Raw upstream strings — tag names or file names — for this port. Sets
 * *low_confidence when the answer came from repology rather than upstream.
 * Sets *truncated when the directory adapter's listing held more matching
 * entries than `max` and had to drop some — the kept entries are the
 * listing-order TAIL (newest-last is the convention this relies on, not a
 * guarantee), but a truncated listing means the true newest release may
 * still be missing, so the caller must not report `current` on the
 * strength of it. Either pointer may be NULL. Returns the count, or -1 when
 * nothing could be reached at all (which the caller must report as
 * `unknown`, never as `current`). */
int pu_list_upstream(const PuRecipe *r, char out[][PU_MAX_RAW], int max,
		     int *low_confidence, int *truncated);

/* ────────────────────────────────────────────────────────────────────────
 * The decision engine
 * ──────────────────────────────────────────────────────────────────────── */

enum { PU_CURRENT = 0, PU_NEWER, PU_UNKNOWN };

/* repology's per-version `vulnerable` flag, asked only under `--cve`. Tri-state
 * because "not in repology" is not "not vulnerable". */
enum { PU_VULN_UNKNOWN = -1, PU_VULN_NO = 0, PU_VULN_YES = 1 };
int pu_repology_vuln(const PuRecipe *r);

typedef struct {
	int  state;
	char candidate[PU_MAX_VER];
	char url[1024];		/* the URL that was proved                  */
	char reason[128];	/* why, when state is PU_UNKNOWN            */
	int  low_confidence;
} PuResult;

/* Pipeline: pu_list_upstream -> pu_extract every raw string -> keep the
 * candidates whose pu_shape matches the CURRENT version's -> kp_vercmp,
 * walking from the highest match down -> pu_render_candidate ->
 * pu_http_head. Only a 200 proves PU_NEWER; a 404 means that particular
 * candidate does not exist there and the next-highest is tried rather than
 * giving up outright. Three outcomes, never two: an adapter that could not
 * be reached, a listing with no candidates, and a candidate whose URL never
 * resolves are all PU_UNKNOWN with a reason — folding any of those into
 * PU_CURRENT would report "up to date" for a check that never actually
 * happened, which is worse than not checking at all. Always returns 0
 * unless the arguments themselves are unusable. */
int pu_check(const char *kpkg_bin, const PuRecipe *r, PuResult *out);

/* "<forge-org>@<current-version>" — a whole upstream epoch under one key.
 * Chosen over a name prefix for a measured reason, and the case that proved
 * it is worth keeping even though those ports are gone: the 17 pop-os ports
 * at 1.4.0 included `pop-launcher`, whose repo is pop-os/launcher, so a
 * `cosmic-*` name rule would have left the seventeenth member behind and
 * shipped exactly the half-bumped epoch grouping exists to prevent. Any
 * family released in lockstep behaves the same way.
 * An explicit `group =` key in the recipe overrides this. Returns 0 when the
 * port has no group (out left empty), 1 when `out` was written. */
int pu_group_key(const PuRecipe *r, char *out, size_t cap);

#endif /* PORTUP_H */

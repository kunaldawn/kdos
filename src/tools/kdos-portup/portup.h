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
#define PU_MAX_RAW   256
#define PU_MAX_CAND  256

/* The comparator is libkpkg's (kp_vercmp): `kdos cve` asks the same question
 * about the same strings, and two implementations would drift. kp_vershape,
 * beside it, is the exact shape pu_same_class falls back to. */

typedef struct {
	char name[64];
	char version[PU_MAX_VER];
	char source[1024];	/* fully expanded, as kpkg meta prints it */
	/* The URL of the port's own tarball: the first token of `source`,
	 * with any `cachename::` prefix removed. `source` is every tarball on
	 * one line — a port with vendored deps prints crates.io URLs after its
	 * own — and only the first is ever probed. The cache name is where the
	 * archive lands on disk, not where it comes from: left on, every host
	 * test below reads the cache name as the host and every candidate
	 * request goes to a URL that cannot exist. */
	char first_source[1024];
	char portdir[512];
	char vendoring[16];	/* "" when the recipe declares none        */
	int  npatches;
	char group[64];		/* explicit `group =` key, or ""           */
	char devseries[64];	/* explicit `devseries =` key, or ""       */
	char series[64];	/* explicit `series =` key, or ""          */
	char watch[512];	/* explicit `watch =` key, or ""           */
	char homepage[512];	/* the recipe's `homepage =`, or ""         */
} PuRecipe;

/* Reads a recipe through `kpkg meta`, which prints the FULLY EXPANDED fields.
 * One parser, shared with the build — reimplementing helper expansion here is
 * how a maintenance tool and its build silently disagree. 0 on success. */
int pu_recipe_read(const char *kpkg_bin, const char *portdir, PuRecipe *r);

/* The URL half of a `source` token: everything after `::` when the token
 * carries a cache name, the token itself otherwise. Points into `src`. */
const char *pu_source_url(const char *src);

/* The URL the build WOULD download if this port were at `candidate`, with
 * any cache-name prefix removed. The recipe is copied to a temp dir with the
 * version substituted and `kpkg meta` expands it, so helper chains work for
 * free — ca-certificates turns 20260115 into cacert-2026-01-15.pem with no
 * special case — and the real tree is never touched by a probe. 0 on
 * success. */
int pu_render_candidate(const char *kpkg_bin, const PuRecipe *r,
			const char *candidate, char *url, size_t cap);

/* Rewrites `version =` in place, preserving the recipe's column alignment.
 * Returns 0 on success. Pass the old version to put it back. The write is
 * atomic (temp file + rename in the recipe's own directory), so an
 * interrupted write can never leave a truncated kpkgbuild — load-bearing
 * for the revert path, which calls this same function to put the old
 * version back after a failed fetch. */
int pu_rewrite_version(const char *portdir, const char *newv);

/* Re-hash every sha256 line whose filename carried the old version, after the
 * fetch has put the new files on disk. Without it a bump leaves the tree with
 * an archive nothing verifies. Returns 0 when nothing needed changing too. */
int pu_rewrite_sha256(const char *portdir, const char *oldv, const char *newv);

/* True when `line` is the recipe's `version =` key line — the one line
 * pu_rewrite_version is allowed to touch. Exported so a read-only preview
 * (main.c's `d` command) can recognise the same line pu_rewrite_version
 * would, rather than carrying its own copy of the same rule. */
int pu_is_version_line(const char *line);

/* ────────────────────────────────────────────────────────────────────────
 * HTTP and git (http.c)
 *
 * An HTTP request is tried three times, pausing two and then five seconds,
 * when the answer is one a busy host gives and a missing file does not: no
 * response at all, 429, or 5xx. A 404 is an answer and is never retried. A
 * git tag listing has no status to read and is tried once more, after two
 * seconds, on any failure.
 * ──────────────────────────────────────────────────────────────────────── */

/* The HTTP status (an FTP reply code for ftp://), or 0 when curl could not
 * complete the request at all. The distinction matters: 404 means "no such
 * version", 0 means "we do not know", and those are different outcomes to the
 * caller. A 403, 405 or 501 to HEAD is asked again as a one-byte ranged GET,
 * because a host that refuses HEAD — or walls it off from scripts — still
 * serves the file, and "refused the method" is not "no such file". */
int pu_http_head(const char *url);

/* The body of `url` into `out`, returning its status as pu_http_head does.
 * A non-2xx body is an error page: callers read `out` only behind a 2xx. */
int pu_http_get(const char *url, KbBuf *out);

/* pu_http_get with no retry, for a caller that paces its own attempts —
 * repology, whose one-request-a-second limit a retry must also wait for. */
int pu_http_get_once(const char *url, KbBuf *out);

/* The status a GET reports, from curl's exit and the status it wrote: a 2xx
 * whose body stopped part-way is 0, "could not complete", because a cut
 * listing is missing its newest entries and would read as complete; so is a
 * redirect whose next hop failed. pu_http_head applies the redirect half of
 * the rule. */
int pu_http_verdict(int curl_exit, int code);

/* `git ls-remote --tags` against `repo`, the raw output into `out`, peeled
 * lines (<tag>^{}) included.
 * 0 when git answered, -1 otherwise. Every git forge serves the smart HTTP
 * protocol without authentication or an API rate limit, and it names EVERY
 * tag — a feed carries the newest ten, which a project that tags each of its
 * crates, or maintains two majors at once, fills with the wrong ones. */
int pu_git_tags(const char *repo, KbBuf *out);

/* `git ls-remote` of `repo`'s HEAD and branch heads, as pu_git_tags. */
int pu_git_heads(const char *repo, KbBuf *out);

/* Fixture mode: when set, every GET reads <dir>/<slug of the URL> instead of
 * the network, git reads <dir>/<slug of "git+" and the repository URL> (its
 * branch heads "git-heads+"), and a missing file answers 404. That is what
 * makes the pipeline testable offline. */
void pu_http_set_fixture_dir(const char *dir);
const char *pu_http_fixture_dir(void);

/* ────────────────────────────────────────────────────────────────────────
 * Reading a version out of an upstream name (match.c)
 *
 * The recipe's own URL says where its version sits: `gcc-15.2.0.tar.xz` is
 * "gcc-" + version + an archive suffix, a tag `llvmorg-21.1.8` is "llvmorg-"
 * + version. An anchor records the literal text on either side, so a name in
 * a listing is read only when it has the same surroundings — a directory that
 * holds every X library, every suckless tool or every GNU pretest then
 * yields this port's versions and nobody else's.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	char pre[160];	/* literal text before the version              */
	char suf[64];	/* literal text after it (before any archive)  */
	char sep;	/* how the source spells the version's dots:
			   '.', '_' / '-' for boost_1_89_0 or R_2_7_3,
			   '*' for a mix of them (7.1.2-31 for 7.1.2.31),
			   or 0 for none at all (gs10071, unzip60)       */
	char seps[8];	/* with '*', the separator at each dot         */
	unsigned char widths[8];	/* with sep 0 or `joined`, each part's
					   digit count                     */
	int  nwidths;
	int  joined;	/* the pin is digits alone and the source writes
			   them in groups (20260813 as 2026-08-13):
			   `sep` joins parts of `widths`, and a match
			   reads back with the separators dropped      */
	int  archive;	/* the name ends in an archive suffix, and any
			   archive suffix is accepted in its place       */
	int  digit_lead;	/* the version starts with a digit, so a
				   match must too: xcb-util- must not read
				   xcb-util-cursor-0.1.6 as a version    */
	int  digits_only;	/* a match may hold no letters: set for a
				   directory named as a bare version, where
				   anything starting with a digit would
				   otherwise read (a file size, 5.1K)     */
} PuAnchor;

/* The length of the archive suffix `s` (`n` long) ends in — the longest of
 * .tar.gz, .tar.xz, .zip, .7z, .crate, .pem and the rest — or 0 when it ends
 * in none: a signature, a checksum, a page. */
size_t pu_archive_len(const char *s, size_t n);

/* Builds an anchor from `name` (a file name, a tag or a directory segment)
 * around the first bounded occurrence of `version` spelled with '.', '_' or
 * '-', or failing those with a mix of them, with a pin of digits alone
 * written in groups, or with its dots dropped. `archive` asks for the
 * file-name form: the text after the version is split into a literal part and
 * an archive suffix. 0 when the version is not in `name`. */
int pu_anchor_from(const char *name, const char *version, int archive,
		   PuAnchor *a);

/* The version `raw` carries under anchor `a`, spelled with dots, into `v`.
 * `raw` may be a path or URL; its last segment is what is read (a trailing
 * '/' of a directory entry and SourceForge's "/download" are dropped). 0 on a
 * match. */
int pu_anchor_match(const PuAnchor *a, const char *raw, char *v, size_t cap);

/* The first `ncomp` dot-separated components of `v` ("4.2" from 4.2.8). 0 on
 * success, -1 when `v` has fewer. */
int pu_version_prefix(const char *v, int ncomp, char *out, size_t cap);

/* Can `cand` be a later release of the numbering `cur` uses? Any two dotted
 * numeric versions are one class whatever their length — binutils went from
 * 2.45.1 to 2.47 — with a pre- or post-release marker (1.4rc5, 10.2p1,
 * 1.5.8.pl02) and a trailing commit id ignored; a leading year (2026.7.22) is
 * a class of its own so a date never passes for a counter, and so is a
 * zero-padded part the pin does not pad (600.0132 beside 26.2.4); a marker
 * never lifts a version past a later base (5.0-post1 is not after 5.0.9); a
 * word that is no marker and not in the pin makes it a file beside a release
 * (5.1.22_dict, 3.8.13-w64, 1.8.1.3.patch), never one; everything else falls
 * back to kp_vershape's exact shape. */
int pu_same_class(const char *cand, const char *cur);

/* Is `v` inside the line a recipe's `series` key holds it to? `series` is a
 * version prefix matched on whole dotted components: 21 holds 21.1.8 and not
 * 210.1, 5.4 holds 5.4.9 and not 5.5.0. Always true for an empty `series`. */
int pu_in_series(const char *v, const char *series);

/* A pre-release: an rc, alpha, beta, pre, preview, dev, snapshot, wip, test,
 * nightly, unstable, trunk or cr marker, or a PEP 440 a1/b2; a trailing
 * commit id is not read for one (passt's 2025_02_17.a1e48a0). Never when
 * `cur` is itself a pre-release: a port that pins one follows that line. */
int pu_prerelease(const char *cand, const char *cur);

/* A pretest numbered 90-99 in the third place or later, where the current
 * version has less there: GNOME, freedesktop and GNU number one x.y.9x, X.Org
 * x.y.99.z. `all` is every version the same listing named; when one of them,
 * or `cur`, shares the candidate's leading parts and has 80-89 in that place,
 * the project's counter walked up into the nineties and 1.0.92 is a release
 * (anyhow, proc-macro2). 100 and up is always a counter (libfprint's
 * 1.94.100). */
int pu_pretest(const char *cand, const char *cur, char all[][PU_MAX_VER],
	       int n);

/* A development series under `conventions` (the recipe's `devseries` key,
 * space-separated) plus the host's built-in one: `odd-minor` makes an odd
 * second component a development series (GStreamer, GLib, Perl), and
 * `preview-minor` makes a second component of 90 or more a preview of the
 * next major (Pango 1.90 is Pango 2). Never when `cur` is in such a series
 * itself. */
int pu_devseries(const char *cand, const char *cur, const char *conventions,
		 const char *url);

/* Extract version candidates from `raw` (filename or git tag). A generous,
 * shape-filter-ready extractor that handles diverse upstream naming conventions.
 * Uses a closure/fixpoint algorithm: iteratively trim archive suffixes
 * (tar/gz/xz/bz2/lz/lzma/zst/tgz/txz/tbz2/zip/pem/src/orig) and apply three
 * transformations (strip leading letter-prefix groups, strip letter immediately
 * followed by digit, emit hyphen-separated digit-containing segments) until no
 * new candidates are generated. A run longer than PU_MAX_VER (64 bytes) is
 * dropped (truncating a version would invent one). Returns count. It is the
 * reader of last resort, for a source whose file name does not carry the
 * version as written — the anchors above are tried first. */
int pu_extract(const char *raw, char out[][PU_MAX_VER], int max);

/* ────────────────────────────────────────────────────────────────────────
 * Discovery and the decision engine (probe.c)
 * ──────────────────────────────────────────────────────────────────────── */

enum { PU_CURRENT = 0, PU_NEWER, PU_UNKNOWN };

/* repology's per-version `vulnerable` flag, asked only under `--cve`. Tri-state
 * because "not in repology" is not "not vulnerable". */
enum { PU_VULN_UNKNOWN = -1, PU_VULN_NO = 0, PU_VULN_YES = 1 };
int pu_repology_vuln(const PuRecipe *r);

/* What discovery found for one port: every version upstream names that could
 * be a later release of this port, newest first, already past the class,
 * pre-release, pretest and development-series filters. */
typedef struct {
	char (*cand)[PU_MAX_VER];	/* PU_MATCH_MAX entries, caller-owned */
	int  n;
	int  reached;		/* some adapter completed a request            */
	int  truncated;		/* a listing was cut, or a later series
				   directory not read, so the newest may be
				   gone                                        */
	int  low_confidence;	/* the answer came from repology, or names a
				   GitHub tag with no release               */
	char via[32];		/* the adapter that answered                   */
	/* The newest version, later than the pin, that the same tag list
	 * names under a default prefix (v, or none) when the recipe's own
	 * prefix names nothing later and that family starts after the
	 * recipe's ends: sby's tags went from yosys-0.47 to v0.48. The
	 * template cannot prove it, and "current" would be wrong, so it makes
	 * the answer unknown. "" when there is none. */
	char other_prefix[PU_MAX_VER];
	/* The newest release, later than the pin, that the recipe's `series`
	 * key kept out. "" when there is none. */
	char outside[PU_MAX_VER];
	/* The source names a commit rather than a release, and the forge
	 * tags none: `pinned` is set, and `head` names the branch whose tip
	 * is that commit, or is "" when no branch is at it. */
	int  pinned;
	char head[64];
	int  unanchored;	/* the recipe's version is nowhere in its URL */
	/* The newest tag, later than the pin and past the newest release,
	 * that a GitHub project which publishes releases selectively has no
	 * release for. It stays a candidate. "" when none. */
	char unreleased[PU_MAX_VER];
	/* A SourceForge feed that ends at the pin, of a project SourceForge
	 * says has moved to a repository that does not tag the pin as its
	 * newest (`moved`, where to; `elsewhere`, the later tag, if read), or
	 * one repology knows a later release of (`elsewhere`). Either makes
	 * the answer unknown. "" when none. */
	char moved[128];
	char elsewhere[PU_MAX_VER];
} PuFound;

#define PU_MATCH_MAX 512

/* Adapters in order, the first to yield a candidate wins: the recipe's `watch`
 * key, a package registry (PyPI, crates.io, MetaCPAN), a git forge's tags
 * (GitHub, GitLab on any host, Codeberg, sr.ht, Bitbucket, cgit) or, for a
 * pinned commit none of them tags, its branch heads, SourceForge's file feed,
 * the directory listing — climbing past every path segment the version is
 * written into, and one hop on to a download page — the homepage, and
 * repology last. 0, or -1 on unusable arguments. */
int pu_discover(const PuRecipe *r, PuFound *f);

typedef struct {
	int  state;
	char candidate[PU_MAX_VER];
	char url[1024];		/* the URL that was proved; with PU_UNKNOWN,
				   upstream's own copy of the newest file
				   when no candidate URL had it, if any     */
	char reason[128];	/* why, when state is PU_UNKNOWN; with
				   PU_NEWER, the newer version upstream that
				   the recipe's URL could not reach, or the
				   newest tag with no GitHub release, if any;
				   with PU_CURRENT, the release past a held
				   series or the branch a pinned commit tips,
				   if any */
	int  low_confidence;
} PuResult;

/* Pipeline: pu_discover -> the candidates that beat CURRENT, highest first ->
 * pu_render_candidate -> pu_http_head. Only a 200 proves PU_NEWER; a 404
 * means that particular candidate does not exist there and the next-highest
 * is tried rather than giving up outright. A rendered URL identical to the
 * recipe's own proves nothing — the version is not in it — and is skipped.
 * Three outcomes, never two: an adapter that could not be reached, a listing
 * with no candidates, and a candidate whose URL never resolves are all
 * PU_UNKNOWN with a reason — folding any of those into PU_CURRENT would
 * report "up to date" for a check that never actually happened, which is
 * worse than not checking at all. A pinned commit that is still a release
 * branch's tip is PU_CURRENT with nothing to render. Always returns 0 unless
 * the arguments themselves are unusable. */
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

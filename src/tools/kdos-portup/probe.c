/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — discovery: what does upstream actually offer
 *
 * Three adapters, tried in order, each cheaper to be wrong about than the
 * last: a forge's tag feed (no auth, no rate limit worth worrying about), a
 * bare directory listing (works for anything Apache/nginx autoindexes), and
 * repology (rate-limited, and never upstream itself, hence *low_confidence).
 * Every parser here is fed bytes a remote host chose, not bytes this program
 * chose — every copy into out[] is bounded by PU_MAX_RAW and by `max`, and
 * every scan terminates on its own (a missing closing quote or tag stops
 * that one scan rather than reading past the end of the buffer).
 * ---------------------------------
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kbase.h"
#include "kbuild.h"
#include "portup.h"

/* ────────────────────────────────────────────────────────────────────────
 * small shared helper
 * ──────────────────────────────────────────────────────────────────────── */

/* Bounded append: truncates rather than overflows, and is silently a no-op
 * once `max` is reached so every caller can just keep calling it in a loop
 * without its own bounds check. */
static void add_raw(char out[][PU_MAX_RAW], int *n, int max,
		     const char *s, size_t len)
{
	if (*n >= max || !len)
		return;
	if (len >= PU_MAX_RAW)
		len = PU_MAX_RAW - 1;
	memcpy(out[*n], s, len);
	out[*n][len] = 0;
	(*n)++;
}

/* ────────────────────────────────────────────────────────────────────────
 * The forge adapter — GitHub, GitLab, Codeberg, sr.ht tag feeds
 *
 * GitHub's REST API allows 60 unauthenticated requests an hour, which does
 * not cover one run over ~156 forge-hosted ports. The Atom/RSS tag feeds
 * have no such limit and need no token, which is the only reason this tool
 * can check a forge at all.
 * ──────────────────────────────────────────────────────────────────────── */

/* Builds the tag-feed URL for a recognised forge, or returns -1 for
 * anything else. Confidence over guessing: GitHub, Codeberg (Gitea) and
 * sr.ht each have one fixed feed shape that was checked against a live
 * response before being written here; GitLab's shape was checked against
 * three different instances (gitlab.com, gitlab.freedesktop.org,
 * gitlab.gnome.org) and generalises because every GitLab source URL in this
 * tree routes through the literal "/-/" that separates a project from
 * whatever is being fetched from it. Anything else — a plain git.kernel.org
 * cgit, a bespoke forge — returns -1 rather than a feed URL nobody has ever
 * seen the shape of. */
static int forge_feed_url(const char *src, char *out, size_t cap)
{
	const char *host = strstr(src, "://");
	if (!host)
		return -1;
	host += 3;

	char owner[128] = "", repo[128] = "";
	if (!strncmp(host, "github.com/", 11)) {
		if (sscanf(host + 11, "%127[^/]/%127[^/]", owner, repo) != 2)
			return -1;
		snprintf(out, cap, "https://github.com/%s/%s/tags.atom",
			 owner, repo);
		return 0;
	}
	if (!strncmp(host, "codeberg.org/", 13)) {
		if (sscanf(host + 13, "%127[^/]/%127[^/]", owner, repo) != 2)
			return -1;
		snprintf(out, cap, "https://codeberg.org/%s/%s/tags.rss",
			 owner, repo);
		return 0;
	}
	if (!strncmp(host, "git.sr.ht/", 10)) {
		/* The owner segment is "~name" — %127[^/] takes the '~' along
		 * with it, which is exactly what the feed path needs back. */
		if (sscanf(host + 10, "%127[^/]/%127[^/]", owner, repo) != 2)
			return -1;
		snprintf(out, cap, "https://git.sr.ht/%s/%s/refs/rss.xml",
			 owner, repo);
		return 0;
	}
	if (!strncmp(host, "gitlab.", 7)) {
		/* GitLab groups nest arbitrarily (xorg/font/util is three
		 * levels), so the project path is not "the first two path
		 * segments" — it is everything between the hostname and the
		 * literal "/-/" every gitlab source URL in this tree uses to
		 * separate the project from the object fetched from it
		 * (archive, releases, raw). */
		const char *slash = strchr(host, '/');
		if (!slash)
			return -1;
		const char *proj = slash + 1;
		const char *marker = strstr(proj, "/-/");
		if (!marker)
			return -1;

		size_t hlen = (size_t)(slash - host);
		size_t plen = (size_t)(marker - proj);
		char hostname[128], project[256];
		if (hlen >= sizeof(hostname) || plen >= sizeof(project))
			return -1;
		memcpy(hostname, host, hlen);
		hostname[hlen] = 0;
		memcpy(project, proj, plen);
		project[plen] = 0;

		snprintf(out, cap, "https://%s/%s/-/tags?format=atom",
			 hostname, project);
		return 0;
	}
	return -1;
}

/* Extracts every <title>...</title> in document order and drops the first
 * one, which on every forge checked (GitHub, GitLab, Codeberg, sr.ht) is the
 * feed's own header ("Tags from curl", "mesa tags", the channel title) and
 * never a tag — RSS and Atom both put it before any entry/item. A feed with
 * no closing </title> for some entry stops there rather than scanning past
 * the end of the buffer, so a truncated response yields whatever it managed
 * to parse rather than crashing. */
static int parse_feed_titles(const char *body, char out[][PU_MAX_RAW], int max)
{
	int n = 0;
	int skipped_header = 0;
	const char *p = body;

	while (*p && n < max) {
		const char *open = strstr(p, "<title>");
		if (!open)
			break;
		open += 7;
		const char *close = strstr(open, "</title>");
		if (!close)
			break;

		if (!skipped_header) {
			skipped_header = 1;
			p = close + 8;
			continue;
		}

		const char *s = open;
		size_t len = (size_t)(close - open);
		while (len && isspace((unsigned char)*s)) {
			s++;
			len--;
		}
		while (len && isspace((unsigned char)s[len - 1]))
			len--;
		add_raw(out, &n, max, s, len);

		p = close + 8;
	}
	return n;
}

static int forge_list(const PuRecipe *r, char out[][PU_MAX_RAW], int max,
		       int *reached)
{
	*reached = 0;

	char feed[700];
	if (forge_feed_url(r->first_source, feed, sizeof(feed)) != 0)
		return 0;

	KbBuf b = {0};
	if (pu_http_get(feed, &b) != 0) {
		kb_buf_free(&b);
		return 0;
	}
	*reached = 1;
	int n = b.p ? parse_feed_titles(b.p, out, max) : 0;
	kb_buf_free(&b);
	return n;
}

/* ────────────────────────────────────────────────────────────────────────
 * The directory adapter — Apache/nginx-style listings
 *
 * Strips the last path component off the port's own tarball URL to get the
 * parent directory, fetches it, and scrapes filenames out of the listing.
 * Covers GNU, x.org, kernel.org, GNOME, gstreamer.freedesktop.org and the
 * rest of the plain-index tail with one implementation.
 * ──────────────────────────────────────────────────────────────────────── */

static int dir_parent_url(const char *src, char *out, size_t cap)
{
	const char *host = strstr(src, "://");
	if (!host)
		return -1;
	const char *last_slash = strrchr(host + 3, '/');
	if (!last_slash)
		return -1;		/* no path component to strip */
	size_t len = (size_t)(last_slash - src) + 1;	/* keep the slash */
	if (len >= cap)
		return -1;
	memcpy(out, src, len);
	out[len] = 0;
	return 0;
}

static int is_tok_char(char c)
{
	return isalnum((unsigned char)c) || c == '.' || c == '-' ||
	       c == '_' || c == '~' || c == '+';
}

static int has_digit(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++)
		if (isdigit((unsigned char)s[i]))
			return 1;
	return 0;
}

/* A bounded sink that can also discard a caller-chosen number of LEADING
 * matches. Both directory-listing scans below run twice through the same
 * body with the same sink: once with `max=0` (so add_raw can never write)
 * purely to learn `seen` — the true total — and once for real with `skip`
 * set from that total, so the two passes can never disagree about what
 * counts as a match. */
typedef struct {
	char (*out)[PU_MAX_RAW];
	int max;	/* 0 during the counting pass: nothing is ever written */
	int n;		/* items written so far, this pass */
	int seen;	/* items matched so far, written or not */
	int skip;	/* leading matches to discard, for tail-keeping */
} RawSink;

static void sink_emit(RawSink *s, const char *str, size_t len)
{
	if (s->seen >= s->skip)
		add_raw(s->out, &s->n, s->max, str, len);
	s->seen++;
}

/* A directory listing's filenames arrive two ways depending on the server:
 * inside href="..." (Apache, nginx autoindex — mesa, gstreamer, libuv all
 * look like this) or, for whatever oddly-formatted index does not use
 * anchors at all, as plain text. This pass covers the first. A real archive
 * directory (mesa's has 649 entries, gstreamer's over 1600) also has sort
 * links (href="?C=N;O=D") and parent/sibling directory links before any
 * tarball — none of those can contain a version, so they are skipped rather
 * than spent out of a caller's `max` budget ahead of the entries that
 * matter; a version-shaped href always has a digit somewhere, so "no digit
 * at all" is a safe, cheap rejection with no risk of dropping a real one.
 * Bounded two ways an adversarial listing can defeat a naive scanner: an
 * href value longer than PU_MAX_RAW is truncated rather than overflowing
 * out[], and an href with NO closing quote before EOF stops the whole scan
 * (there is nothing left in the buffer that could still be a valid
 * attribute), rather than strchr running off the end — which it cannot,
 * since the buffer is NUL-terminated, but the point is the scan does not
 * spin re-reading a ten-thousand-byte unterminated tail one byte at a
 * time either. */
static void href_scan(const char *body, RawSink *sink)
{
	const char *p = body;

	while (*p) {
		if ((p[0] == 'h' || p[0] == 'H') && (p[1] == 'r' || p[1] == 'R') &&
		    (p[2] == 'e' || p[2] == 'E') && (p[3] == 'f' || p[3] == 'F')) {
			const char *q = p + 4;
			while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
				q++;
			if (*q == '=') {
				q++;
				while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
					q++;
				char quote = (*q == '"' || *q == '\'') ? *q : 0;
				if (quote)
					q++;
				const char *vstart = q;
				const char *vend;
				if (quote) {
					vend = strchr(q, quote);
				} else {
					vend = q;
					while (*vend && *vend != '>' &&
					       *vend != ' ' && *vend != '\t' &&
					       *vend != '\n')
						vend++;
				}
				if (!vend)
					break;	/* unterminated: nothing more to find */
				size_t vlen = (size_t)(vend - vstart);
				if (has_digit(vstart, vlen))
					sink_emit(sink, vstart, vlen);
				p = vend + (quote ? 1 : 0);
				continue;
			}
		}
		p++;
	}
}

/* The second pass, for the visible text a listing shows outside any markup —
 * some indexes print the filename a second time as anchor text, others (or
 * a plain-text index with no anchors at all) show it with no href in sight.
 * "<...>" spans are skipped rather than tokenised; what remains is
 * tokenised into filename-character runs, and only runs that look like a
 * real filename (a digit — a version — AND a dot — an extension) are kept,
 * so column headers like "Last modified" or "Description" never make it
 * into the output. */
static void token_scan(const char *body, RawSink *sink)
{
	const char *p = body;
	int in_tag = 0;

	while (*p) {
		if (in_tag) {
			if (*p == '>')
				in_tag = 0;
			p++;
			continue;
		}
		if (*p == '<') {
			in_tag = 1;
			p++;
			continue;
		}
		if (isalnum((unsigned char)*p)) {
			const char *start = p;
			while (*p && is_tok_char(*p))
				p++;
			size_t len = (size_t)(p - start);
			int has_dot = 0;
			for (size_t i = 0; i < len; i++) {
				if (start[i] == '.') {
					has_dot = 1;
					break;
				}
			}
			if (has_dot && has_digit(start, len))
				sink_emit(sink, start, len);
			continue;
		}
		p++;
	}
}

/* Runs `scan` twice and returns the count written. When `scan` finds more
 * matches than `max` can hold, keeps the TAIL, not the head. Archive indexes
 * sort ascending essentially everywhere, so the newest entries are last:
 * mesa's index carries 328 matching hrefs and its newest tarball is the
 * 159th match, so taking the first N silently yields nothing but ancient
 * versions and the port reports `current` when an update exists. That is
 * the one answer this tool must never give. Ordering is a convention, not a
 * guarantee — hence the truncation signal below, so a caller who cannot
 * trust listing order to have put the newest release within reach can fall
 * back to `unknown` instead of trusting a wrong `current`. */
static int tail_scan(void (*scan)(const char *, RawSink *), const char *body,
		      char out[][PU_MAX_RAW], int max, int *truncated)
{
	RawSink counting = { out, 0, 0, 0, INT_MAX };
	scan(body, &counting);
	int total = counting.seen;

	int skip = total > max ? total - max : 0;
	if (skip && truncated)
		*truncated = 1;

	RawSink writing = { out, max, 0, 0, skip };
	scan(body, &writing);
	return writing.n;
}

static int find_hrefs(const char *body, char out[][PU_MAX_RAW], int max,
		       int *truncated)
{
	return tail_scan(href_scan, body, out, max, truncated);
}

static int find_bare_tokens(const char *body, char out[][PU_MAX_RAW], int max,
			     int *truncated)
{
	return tail_scan(token_scan, body, out, max, truncated);
}

static int dir_list(const PuRecipe *r, char out[][PU_MAX_RAW], int max,
		     int *reached, int *truncated)
{
	*reached = 0;

	char parent[700];
	if (dir_parent_url(r->first_source, parent, sizeof(parent)) != 0)
		return 0;

	/* Cost a debug cycle on imagemagick: its upstream directory was
	 * retired (imagemagick.org/archive/releases/ now 404s, redirected
	 * off /archive/ entirely) but still answers with a normal HTML error
	 * page, not an empty body. pu_http_get has no -f, on purpose (a 404
	 * still has to count as "the host answered" — see pu_http_head's own
	 * comment on the same point) so an unchecked GET here scraped stray
	 * digit-shaped text out of the error page's own markup as if it were
	 * real filenames. Worse than the junk itself: a non-zero return stops
	 * pu_list_upstream from ever falling through to repology, which does
	 * carry imagemagick's real current version. HEAD the page first and
	 * only trust the body behind a 200.
	 *
	 * Trade-off worth knowing about: a server that answers HEAD with 405
	 * or 501 while serving the same URL fine over GET loses its listing
	 * here too — not observed on any port tested live, but the first
	 * suspect if a previously-working port goes UNKNOWN with no other
	 * explanation. */
	int code = pu_http_head(parent);
	if (!code)
		return 0;		/* host unreachable: *reached stays 0 */
	*reached = 1;
	if (code != 200)
		return 0;		/* reached, but nothing there to scrape */

	KbBuf b = {0};
	if (pu_http_get(parent, &b) != 0) {
		kb_buf_free(&b);
		return 0;
	}

	int n = 0;
	if (b.p) {
		n = find_hrefs(b.p, out, max, truncated);
		if (n < max)
			n += find_bare_tokens(b.p, out + n, max - n, truncated);
	}
	kb_buf_free(&b);
	return n;
}

/* ────────────────────────────────────────────────────────────────────────
 * The repology fallback
 *
 * Only reached when both adapters above genuinely found nothing — repology
 * is never upstream itself, so every result from here sets *low_confidence.
 * ──────────────────────────────────────────────────────────────────────── */

static double s_last_repology_req;

/* Serialised to one request per second — repology's documented limit, and a
 * maintenance tool has no business being the reason a free service
 * rate-limits the next person. */
static void repology_throttle(void)
{
	if (s_last_repology_req > 0) {
		double wait = 1.0 - (kb_now_s() - s_last_repology_req);
		if (wait > 0) {
			struct timespec ts;
			ts.tv_sec = (time_t)wait;
			ts.tv_nsec = (long)((wait - (double)ts.tv_sec) * 1e9);
			nanosleep(&ts, NULL);
		}
	}
	s_last_repology_req = kb_now_s();
}

static int repology_list(const PuRecipe *r, char out[][PU_MAX_RAW], int max,
			  int *reached)
{
	*reached = 0;
	if (!r->name[0])
		return 0;

	char url[300];
	snprintf(url, sizeof(url), "https://repology.org/api/v1/project/%s",
		 r->name);

	repology_throttle();

	KbBuf b = {0};
	if (pu_http_get(url, &b) != 0) {
		kb_buf_free(&b);
		return 0;
	}
	*reached = 1;

	int n = 0;
	if (b.p) {
		/* kj_parse refuses anything that does not parse whole, which is
		 * exactly right here: a half-read or error-page response must
		 * read as "no candidates", never as a confident wrong one. */
		KjNode *root = kj_parse(b.p);
		if (root && root->type == KJ_ARR) {
			for (const KjNode *e = root->child; e && n < max; e = e->next) {
				/* Cost a debug cycle on aalib: repology's own
				 * curators mark a per-repo version "incorrect"
				 * when their parser mis-splits a distro revision
				 * out of the upstream string — aalib's Debian-
				 * derived "1.4p5-48+apertis1" comes back as
				 * version "1.4p5", which this tool's vercmp then
				 * ranks ABOVE the real "1.4rc5" release because
				 * 'p' is a non-prerelease suffix (OpenSSH's own
				 * convention, and correct for OpenSSH). repology
				 * says outright not to trust that entry;
				 * "untrusted" is its stronger per-repo version of
				 * the same flag. */
				const char *status = kj_str(e, "status", "");
				if (!strcmp(status, "incorrect") ||
				    !strcmp(status, "untrusted"))
					continue;
				const char *v = kj_str(e, "version", "");
				if (v[0])
					add_raw(out, &n, max, v, strlen(v));
			}
		}
		kj_free(root);
	}
	kb_buf_free(&b);
	return n;
}

/* ────────────────────────────────────────────────────────────────────────
 * The entry point
 * ──────────────────────────────────────────────────────────────────────── */

int pu_list_upstream(const PuRecipe *r, char out[][PU_MAX_RAW], int max,
		     int *low_confidence, int *truncated)
{
	if (low_confidence)
		*low_confidence = 0;
	if (truncated)
		*truncated = 0;
	if (max <= 0)
		return 0;

	int reached, any_reached = 0;

	int n = forge_list(r, out, max, &reached);
	any_reached |= reached;
	if (n > 0)
		return n;

	n = dir_list(r, out, max, &reached, truncated);
	any_reached |= reached;
	if (n > 0)
		return n;

	n = repology_list(r, out, max, &reached);
	any_reached |= reached;
	if (n > 0) {
		if (low_confidence)
			*low_confidence = 1;
		return n;
	}

	/* Every adapter either did not apply or reached its host and found
	 * nothing: a confident, documented zero. Only when NOT ONE of them
	 * could even complete a request is this "we do not know" — the
	 * caller turns that into `unknown`, never into `current`. */
	return any_reached ? 0 : -1;
}

/* ────────────────────────────────────────────────────────────────────────
 * The decision engine
 *
 * Everything upstream of this point is discovery: a bag of raw strings the
 * adapters scraped. This turns that bag into one of three verdicts for one
 * port. `PU_LIST_MAX` clears the max>=2048 floor the earlier live probes
 * measured (curl's directory listing is 1229 entries, mesa's 637) with
 * headroom to spare.
 * ──────────────────────────────────────────────────────────────────────── */

#define PU_LIST_MAX  3000
/* Candidates one raw string can explode into via pu_extract's closure —
 * generous; a real filename yields a handful. */
#define PU_EX_MAX    32
/* Distinct shape-matching CANDIDATE VERSIONS, not raw filenames: the same
 * release shows up as .tar.gz/.tar.xz/.sig/etc, which collapse to one entry
 * here, so a project's entire release history rarely gets close to this. */
#define PU_MATCH_MAX 512

static void strip_seps(const char *s, char *out, size_t cap)
{
	size_t o = 0;
	for (; *s && o + 1 < cap; s++)
		if (isalnum((unsigned char)*s))
			out[o++] = *s;
	out[o] = 0;
}

static void seps_to_dot(const char *s, char *out, size_t cap)
{
	size_t o = 0;
	for (; *s && o + 1 < cap; s++)
		out[o++] = isalnum((unsigned char)*s) ? *s : '.';
	out[o] = 0;
}

static int already_have(char cands[][PU_MAX_VER], int n, const char *s)
{
	for (int i = 0; i < n; i++)
		if (!strcmp(cands[i], s))
			return 1;
	return 0;
}

static int add_if_matching_shape(char cands[][PU_MAX_VER], int n, int max,
				  const char *cand, const char *want_shape)
{
	if (n >= max || !cand[0])
		return n;
	char shape[PU_MAX_VER];
	pu_shape(cand, shape, sizeof(shape));
	if (strcmp(shape, want_shape) || already_have(cands, n, cand))
		return n;
	kb_strlcpy(cands[n], cand, PU_MAX_VER);
	return n + 1;
}

/* Scans every raw upstream string, extracts every version-shaped run out of
 * it, and keeps the ones sharing CURRENT's shape. `normalize` is the fallback
 * for the ports whose recipe version and URL version are the same release
 * spelled with different separators:
 *
 *     ca-certificates  recipe 20251202   url .../cacert-2025-12-02.pem
 *     imagemagick      recipe 7.1.2.21   url .../ImageMagick-7.1.2-21.tar.xz
 *
 * The raw extracted candidate ("2025-12-02", "7.1.2-21") never shares the
 * recipe's shape, so without this these two ports report `unknown` forever.
 * Rather than teach this tool either port's helper chain, every extracted
 * candidate is also tried with its separators stripped (2026-01-15 ->
 * 20260115, matching ca-certificates' all-digits shape) and with its
 * separators normalised to '.' (7.1.2-21 -> 7.1.2.21, matching imagemagick's
 * four-dotted-numbers shape) — one of the two guesses lands, the other is
 * just a shape mismatch that gets dropped here for free, and a WRONG guess
 * that survives the shape filter costs nothing worse than one HEAD request
 * that 404s. */
static int collect_matches(char raws[][PU_MAX_RAW], int nraw,
			    const char *want_shape, int normalize,
			    char out[][PU_MAX_VER], int max)
{
	int n = 0;
	for (int i = 0; i < nraw && n < max; i++) {
		char ex[PU_EX_MAX][PU_MAX_VER];
		int ne = pu_extract(raws[i], ex, PU_EX_MAX);
		for (int j = 0; j < ne && n < max; j++) {
			if (!normalize) {
				n = add_if_matching_shape(out, n, max, ex[j],
							   want_shape);
				continue;
			}
			char a[PU_MAX_VER], b[PU_MAX_VER];
			strip_seps(ex[j], a, sizeof(a));
			seps_to_dot(ex[j], b, sizeof(b));
			n = add_if_matching_shape(out, n, max, a, want_shape);
			n = add_if_matching_shape(out, n, max, b, want_shape);
		}
	}
	return n;
}

static int vercmp_desc(const void *a, const void *b)
{
	return pu_vercmp((const char *)b, (const char *)a);
}

int pu_check(const char *kpkg_bin, const PuRecipe *r, PuResult *out)
{
	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->state = PU_UNKNOWN;
	if (!kpkg_bin || !r)
		return -1;

	char (*raws)[PU_MAX_RAW] = kb_calloc(PU_LIST_MAX, PU_MAX_RAW);
	int low_confidence = 0, truncated = 0;
	int nraw = pu_list_upstream(r, raws, PU_LIST_MAX, &low_confidence,
				     &truncated);
	out->low_confidence = low_confidence;

	if (nraw < 0) {
		/* Requirement, not a guess: -1 means not one adapter could
		 * even complete a request, so there is nothing to report but
		 * ignorance. */
		snprintf(out->reason, sizeof(out->reason),
			 "no adapter could reach a host for this port");
		free(raws);
		return 0;
	}
	if (nraw == 0) {
		snprintf(out->reason, sizeof(out->reason),
			 "upstream listing had no entries");
		free(raws);
		return 0;
	}

	char cur_shape[PU_MAX_VER];
	pu_shape(r->version, cur_shape, sizeof(cur_shape));

	char (*matches)[PU_MAX_VER] = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	int nm = collect_matches(raws, nraw, cur_shape, 0, matches, PU_MATCH_MAX);
	if (nm == 0)
		nm = collect_matches(raws, nraw, cur_shape, 1, matches,
				      PU_MATCH_MAX);
	free(raws);

	/* The same invariant as `truncated` above, one level down: filling
	 * PU_MATCH_MAX means candidates that never got looked at (raws past
	 * whichever one filled the array) might have included the true
	 * newest, and this array is built in raw-string order — ascending
	 * for a directory listing — so a silent stop here would drop exactly
	 * the entries a `current` verdict needs to have seen. One flag, two
	 * places it can be raised; the `truncated -> PU_UNKNOWN` logic below
	 * does not care which one fired. */
	if (nm == PU_MATCH_MAX)
		truncated = 1;

	if (nm == 0) {
		snprintf(out->reason, sizeof(out->reason),
			 "no upstream entry matched the recipe's version shape");
		free(matches);
		return 0;
	}

	qsort(matches, (size_t)nm, PU_MAX_VER, vercmp_desc);

	/* matches is sorted descending, so the candidates that beat CURRENT
	 * are exactly the leading run. */
	int ncand = 0;
	while (ncand < nm && pu_vercmp(matches[ncand], r->version) > 0)
		ncand++;

	if (ncand == 0) {
		/* Nothing in the listing beat CURRENT. That is what `current`
		 * means everywhere else, but not when the listing itself was
		 * incomplete: `truncated` relies on archive indexes sorting
		 * ascending, which curl.se's does NOT (its tail is
		 * download/archeology/...), so a truncated listing may
		 * genuinely be missing the one entry that would have beaten
		 * CURRENT. Reporting `current` from it would be a confident
		 * wrong answer — the one outcome this tool must never give. */
		if (truncated) {
			snprintf(out->reason, sizeof(out->reason),
				 "upstream listing was truncated; cannot rule out a newer release");
		} else {
			out->state = PU_CURRENT;
			kb_strlcpy(out->candidate, r->version,
				   sizeof(out->candidate));
		}
		free(matches);
		return 0;
	}

	/* code 0 (curl could not complete the request) is a legitimate HTTP
	 * code slot, so "did we break early" needs its own flag rather than
	 * testing last_code for truth. */
	int broke_early = 0, last_code = 0;
	for (int i = 0; i < ncand; i++) {
		char url[1024];
		if (pu_render_candidate(kpkg_bin, r, matches[i], url,
					 sizeof(url)) != 0)
			continue;
		int code = pu_http_head(url);
		if (code == 200) {
			out->state = PU_NEWER;
			kb_strlcpy(out->candidate, matches[i],
				   sizeof(out->candidate));
			kb_strlcpy(out->url, url, sizeof(out->url));
			free(matches);
			return 0;
		}
		/* A listing can name a version whose tarball is not where the
		 * recipe's template expects it: drop it and try the
		 * next-highest rather than giving up on the whole check. */
		if (code == 404)
			continue;
		last_code = code;
		broke_early = 1;
		break;
	}
	free(matches);

	if (broke_early) {
		if (last_code)
			snprintf(out->reason, sizeof(out->reason),
				 "candidate URL returned HTTP %d", last_code);
		else
			snprintf(out->reason, sizeof(out->reason),
				 "could not reach the candidate URL's host");
	} else {
		snprintf(out->reason, sizeof(out->reason),
			 "%d candidate version(s) named upstream, none confirmed",
			 ncand);
	}
	return 0;
}

/* The forge "org" a source URL belongs to. A cut-down twin of
 * forge_feed_url's host parsing above — that function's job is building a
 * feed URL and bailing on anything it does not recognise the shape of;
 * this one only needs "who owns this repo", which is a different enough
 * question (and a much smaller one) that sharing the code would mean
 * threading a second purpose through every branch of the other. */
static int forge_org(const char *src, char *org, size_t cap)
{
	const char *host = strstr(src, "://");
	if (!host)
		return -1;
	host += 3;

	char owner[128];
	if (!strncmp(host, "github.com/", 11)) {
		if (sscanf(host + 11, "%127[^/]", owner) != 1)
			return -1;
	} else if (!strncmp(host, "codeberg.org/", 13)) {
		if (sscanf(host + 13, "%127[^/]", owner) != 1)
			return -1;
	} else if (!strncmp(host, "git.sr.ht/", 10)) {
		/* Keeps the '~' — git.sr.ht owners are spelled ~name. */
		if (sscanf(host + 10, "%127[^/]", owner) != 1)
			return -1;
	} else if (!strncmp(host, "gitlab.", 7)) {
		/* First path segment only, matching the other forges. Grouping
		 * two unrelated projects that happen to share an org and a
		 * version is harmless: a group is only ever OFFERED when every
		 * member has the same newer version available, and unrelated
		 * projects do not move in lockstep. Under-grouping is the
		 * dangerous direction — that is what ships a half-bumped
		 * epoch.
		 *
		 * This used to take everything up to the literal "/-/" that
		 * separates a GitLab project from the object fetched from it —
		 * the project, not the org, since GitLab groups nest
		 * arbitrarily (xorg/font/util is three levels). That made
		 * xorg/font/encodings and xorg/font/util, siblings under one
		 * group and both in ports/core, produce different keys: no
		 * two gitlab-hosted ports could ever share a group. */
		const char *slash = strchr(host, '/');
		if (!slash || sscanf(slash + 1, "%127[^/]", owner) != 1)
			return -1;
	} else {
		return -1;
	}
	kb_strlcpy(org, owner, cap);
	return 0;
}

int pu_group_key(const PuRecipe *r, char *out, size_t cap)
{
	if (cap)
		out[0] = 0;

	if (r->group[0]) {
		kb_strlcpy(out, r->group, cap);
		return 1;
	}

	char org[128];
	if (forge_org(r->first_source, org, sizeof(org)) != 0)
		return 0;

	snprintf(out, cap, "%s@%s", org, r->version);
	return 1;
}

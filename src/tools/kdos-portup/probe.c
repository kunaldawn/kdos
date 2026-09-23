/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-portup — discovery: what does upstream actually offer
 *
 * Adapters, tried in order, each cheaper to be wrong about than the last: the
 * release list a recipe's `watch` key names, a package registry's own index,
 * a git forge's tags, SourceForge's file feed, a bare directory listing
 * (anything Apache/nginx/FTP/S3 indexes, walked up past the directories a
 * version is written into, and one hop on to a download page), the project's
 * homepage, and repology (rate-limited, and never upstream itself, hence
 * low_confidence).
 * Every parser here is fed bytes a remote host chose, not bytes this program
 * chose — every copy is bounded by PU_MAX_RAW, PU_MAX_VER and the caller's
 * `max`, and every scan terminates on its own (a missing closing quote or tag
 * stops that one scan rather than reading past the end of the buffer).
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kbuild.h"
#include "portup.h"

/* Raw strings one listing may carry. Clears the largest index measured (curl's
 * directory listing is 1229 entries, mesa's 637) with headroom; a listing
 * read through an anchor only spends it on this port's own entries. */
#define PU_LIST_MAX  3000
/* Candidates one raw string can explode into via pu_extract's closure —
 * generous; a real filename yields a handful. */
#define PU_EX_MAX    32
/* Candidates proved per port. A listing read through an anchor names this
 * port's releases only, so the newest twenty cover any real gap between a
 * pin and upstream; more is a port whose URL template no candidate fits,
 * and every further request is spent learning the same thing. */
#define PU_TRY_MAX   20
/* Requests one directory walk may make: the parent listing plus a handful of
 * series directories. */
#define PU_WALK_GETS 8
/* Newest series directories a walk descends into, beside the current one. */
#define PU_WALK_SERIES 3
/* An object id as ls-remote prints it (SHA-1's 40 hex digits; a SHA-256 id is
 * cut to the same length, which still tells two commits apart), and its NUL. */
#define PU_OID 41

/* ────────────────────────────────────────────────────────────────────────
 * small shared helpers
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

static int has_digit(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++)
		if (isdigit((unsigned char)s[i]))
			return 1;
	return 0;
}

static int ok2xx(int code)
{
	return code >= 200 && code < 300;
}

/* The host (no port) and the path (from its leading '/') of `url`. -1 when
 * `url` has no scheme. */
static int split_url(const char *url, char *host, size_t hcap,
		     const char **path)
{
	const char *h = strstr(url, "://");
	if (!h)
		return -1;
	h += 3;
	size_t n = strcspn(h, "/:?#");
	if (n >= hcap)
		return -1;
	memcpy(host, h, n);
	host[n] = 0;
	const char *p = h + n;
	if (*p == ':')
		p += strcspn(p, "/");
	*path = p;
	return 0;
}

/* Up to `max` '/'-separated segments of `path`, each bounded to `cap`. */
static int path_segments(const char *path, char seg[][160], int max)
{
	int n = 0;
	for (const char *p = path; *p && *p != '?' && *p != '#' && n < max;) {
		while (*p == '/')
			p++;
		if (!*p || *p == '?' || *p == '#')
			break;
		size_t l = strcspn(p, "/?#");
		size_t c = l < 159 ? l : 159;
		memcpy(seg[n], p, c);
		seg[n][c] = 0;
		n++;
		p += l;
	}
	return n;
}

static void url_encode_path(const char *in, char *out, size_t cap)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;
	for (const unsigned char *p = (const unsigned char *)in;
	     *p && o + 4 < cap; p++) {
		if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' ||
		    *p == '~') {
			out[o++] = (char)*p;
		} else {
			out[o++] = '%';
			out[o++] = hex[*p >> 4];
			out[o++] = hex[*p & 15];
		}
	}
	out[o] = 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * What the recipe's URL says: host, forge, registry, anchors
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	REG_NONE, REG_PYPI, REG_CRATES, REG_CPAN
};

typedef struct {
	const PuRecipe *r;
	const char *url;		/* r->first_source                    */
	char host[256];
	const char *path;
	char seg[24][160];		/* the path's segments, file included */
	int nseg;
	char base[160];			/* the file's own name               */

	PuAnchor file;			/* the version inside `base`          */
	int has_file;
	PuAnchor tag[4];		/* the version inside a tag name      */
	int ntag;

	char repo[768];			/* a git URL for ls-remote, or ""     */
	char feed[2][1024];		/* fallbacks when git cannot answer   */
	int feed_kind;			/* FEED_*                             */
	int nfeed;

	int reg;			/* REG_*                              */
	char reg_name[160];

	char sf_rss[768];		/* SourceForge's file feed, or ""     */
	char sf_proj[160];		/* its project, or ""                 */
	int no_index;			/* a host that serves no listing      */
} Probe;

enum { FEED_NONE, FEED_TITLES, FEED_GITHUB, FEED_GITLAB, FEED_CGIT };

/* Default anchors for a tag list when the URL names no tag: the tag is the
 * version as written, with a v in front, or with the archive's own name in
 * front (cgit's snapshot names follow the tag). */
static void default_tag_anchors(Probe *p)
{
	const char *pres[3] = { "v", "", p->has_file ? p->file.pre : NULL };
	for (int i = 0; i < 3 && p->ntag < 4; i++) {
		if (!pres[i] || (i == 2 && (!pres[i][0] || !strcmp(pres[i], "v"))))
			continue;
		PuAnchor *a = &p->tag[p->ntag++];
		memset(a, 0, sizeof(*a));
		kb_strlcpy(a->pre, pres[i], sizeof(a->pre));
		a->sep = '.';
		a->digit_lead = isdigit((unsigned char)p->r->version[0]) != 0;
	}
}

/* The tag named by `t` (percent-decoded, without an archive suffix when
 * `strip_ext`), anchored on the recipe's version. */
static void tag_anchor(Probe *p, const char *t, size_t n, int strip_ext)
{
	char tag[256];
	if (n >= sizeof(tag))
		return;
	memcpy(tag, t, n);
	tag[n] = 0;
	if (strip_ext) {
		static const char *const ext[] = { ".tar.gz", ".tar.bz2",
			".tar.xz", ".tar.zst", ".tgz", ".zip", NULL };
		for (int i = 0; ext[i]; i++) {
			size_t l = strlen(ext[i]);
			if (n > l && !strcmp(tag + n - l, ext[i])) {
				tag[n - l] = 0;
				break;
			}
		}
	}
	/* The URL form of a tag: v1.0%2Bgit is v1.0+git. */
	char dec[256];
	size_t o = 0;
	for (const char *q = tag; *q && o + 1 < sizeof(dec); q++) {
		if (q[0] == '%' && isxdigit((unsigned char)q[1]) &&
		    isxdigit((unsigned char)q[2])) {
			char h[3] = { q[1], q[2], 0 };
			dec[o++] = (char)strtol(h, NULL, 16);
			q += 2;
		} else {
			dec[o++] = *q;
		}
	}
	dec[o] = 0;
	/* Text after the version that crosses a '/' is a path, not the rest of
	 * a tag name. */
	if (p->ntag < 4 && pu_anchor_from(dec, p->r->version, 0, &p->tag[p->ntag]) &&
	    !strchr(p->tag[p->ntag].suf, '/'))
		p->ntag++;
}

/* The segment after `key` in the path, as a pointer and length. */
static const char *seg_after(const char *path, const char *key, size_t *len)
{
	const char *k = strstr(path, key);
	if (!k)
		return NULL;
	k += strlen(key);
	*len = strcspn(k, "/?#");
	return *len ? k : NULL;
}

/* GitHub, Codeberg, sr.ht and raw.githubusercontent: owner/repo are the first
 * two segments (after the owner's '~' on sr.ht), and the tag is wherever that
 * forge's archive or release URL puts it. */
static void classify_forge(Probe *p)
{
	const char *h = p->host;
	int gh = !strcmp(h, "github.com"), raw = !strcmp(h, "raw.githubusercontent.com");
	int cb = !strcmp(h, "codeberg.org"), srht = !strcmp(h, "git.sr.ht");
	int bb = !strcmp(h, "bitbucket.org");

	if ((gh || raw || cb || srht || bb) && p->nseg >= 2) {
		const char *o = p->seg[0], *r = p->seg[1];
		char repo[160];
		kb_strlcpy(repo, r, sizeof(repo));
		size_t rl = strlen(repo);
		if (rl > 4 && !strcmp(repo + rl - 4, ".git"))
			repo[rl - 4] = 0;
		const char *fh = raw ? "github.com" : h;
		snprintf(p->repo, sizeof(p->repo), "https://%s/%s/%s%s", fh, o, repo,
			 bb ? ".git" : "");
		p->no_index = 1;
		/* Bitbucket's downloads page is a script and its feeds need an
		 * account; git alone answers. */
		if (bb)
			return;

		const char *t;
		size_t n;
		if (raw && p->nseg >= 3) {
			tag_anchor(p, p->seg[2], strlen(p->seg[2]), 0);
		} else if ((t = strstr(p->path, "/archive/refs/tags/"))) {
			t += 19;
			tag_anchor(p, t, strcspn(t, "?#"), 1);
		} else if ((t = seg_after(p->path, "/releases/download/", &n)) ||
			   (t = seg_after(p->path, "/refs/download/", &n))) {
			tag_anchor(p, t, n, 0);
		} else if ((t = strstr(p->path, "/archive/"))) {
			/* archive/<tag>.tar.gz, or archive/<tag>/<any name>
			 * — GitHub serves both. */
			t += 9;
			tag_anchor(p, t, strcspn(t, "?#"), 1);
			tag_anchor(p, t, strcspn(t, "/?#"), 1);
		}

		if (gh || raw) {
			snprintf(p->feed[0], sizeof(p->feed[0]),
				 "https://github.com/%s/%s/tags.atom", o, repo);
			snprintf(p->feed[1], sizeof(p->feed[1]),
				 "https://github.com/%s/%s/releases.atom", o, repo);
			p->nfeed = 2;
			p->feed_kind = FEED_GITHUB;
		} else if (cb) {
			snprintf(p->feed[0], sizeof(p->feed[0]),
				 "https://codeberg.org/%s/%s/tags.rss", o, repo);
			p->nfeed = 1;
			p->feed_kind = FEED_TITLES;
		} else {
			snprintf(p->feed[0], sizeof(p->feed[0]),
				 "https://git.sr.ht/%s/%s/refs/rss.xml", o, repo);
			p->nfeed = 1;
			p->feed_kind = FEED_TITLES;
		}
		return;
	}

	/* GitLab on any host: its project paths nest arbitrarily (xorg/font/util
	 * is three levels), so the project is everything before the literal
	 * "/-/" that separates it from whatever is fetched from it — the same
	 * separator on gitlab.com, gitlab.freedesktop.org, code.videolan.org
	 * and salsa.debian.org. */
	const char *m = strstr(p->path, "/-/");
	if (m && m > p->path + 1) {
		char project[240];
		size_t pl = (size_t)(m - (p->path + 1));
		if (pl >= sizeof(project))
			return;
		memcpy(project, p->path + 1, pl);
		project[pl] = 0;
		snprintf(p->repo, sizeof(p->repo), "https://%s/%s.git", h, project);
		char enc[480];
		url_encode_path(project, enc, sizeof(enc));
		/* The API rather than the project's HTML: gitlab.freedesktop.org
		 * walls its pages off from scripts and not its API. */
		snprintf(p->feed[0], sizeof(p->feed[0]),
			 "https://%s/api/v4/projects/%s/repository/tags?per_page=100&order_by=version",
			 h, enc);
		p->nfeed = 1;
		p->feed_kind = FEED_GITLAB;
		p->no_index = 1;

		const char *t;
		size_t n;
		if ((t = seg_after(m, "/-/archive/", &n)) ||
		    (t = seg_after(m, "/-/releases/", &n)) ||
		    (t = seg_after(m, "/-/raw/", &n)))
			tag_anchor(p, t, n, 0);
		return;
	}

	/* cgit: a snapshot is <repo>/snapshot/<name>-<tag>.<ext>, and the
	 * repository clones from the same base. */
	const char *s = strstr(p->path, "/snapshot/");
	if (s) {
		size_t bl = (size_t)(s - p->url);
		const char *u = p->url;
		if (bl + 1 >= sizeof(p->repo))
			return;
		memcpy(p->repo, u, bl);
		p->repo[bl] = 0;
		snprintf(p->feed[0], sizeof(p->feed[0]), "%s/refs/tags", p->repo);
		p->nfeed = 1;
		p->feed_kind = FEED_CGIT;
	}
}

/* Package registries: the project's name is in the URL, and the registry's
 * own index names every release. */
static void classify_registry(Probe *p)
{
	const char *h = p->host;
	if (!strcmp(h, "files.pythonhosted.org") || !strcmp(h, "pypi.io") ||
	    !strcmp(h, "pypi.org") || !strcmp(h, "pypi.python.org")) {
		p->no_index = 1;
		/* /packages/source/<l>/<project>/<file> names the project;
		 * /packages/<xx>/<yy>/<hash>/<file> only the file does, and an
		 * sdist is always <project>-<version>. */
		if (p->nseg >= 5 && !strcmp(p->seg[0], "packages") &&
		    !strcmp(p->seg[1], "source")) {
			kb_strlcpy(p->reg_name, p->seg[3], sizeof(p->reg_name));
		} else if (p->has_file && p->file.pre[0]) {
			kb_strlcpy(p->reg_name, p->file.pre, sizeof(p->reg_name));
			size_t l = strlen(p->reg_name);
			if (l && (p->reg_name[l - 1] == '-' || p->reg_name[l - 1] == '_'))
				p->reg_name[l - 1] = 0;
		}
		if (p->reg_name[0])
			p->reg = REG_PYPI;
		return;
	}
	if ((!strcmp(h, "static.crates.io") && p->nseg >= 2 &&
	     !strcmp(p->seg[0], "crates")) ||
	    (!strcmp(h, "crates.io") && p->nseg >= 4 &&
	     !strcmp(p->seg[0], "api") && !strcmp(p->seg[2], "crates"))) {
		p->no_index = 1;
		kb_strlcpy(p->reg_name, p->seg[h[0] == 's' ? 1 : 3],
			   sizeof(p->reg_name));
		p->reg = REG_CRATES;
		return;
	}
	if (strstr(p->path, "/authors/id/") && strstr(h, "cpan") &&
	    p->has_file && p->file.pre[0]) {
		kb_strlcpy(p->reg_name, p->file.pre, sizeof(p->reg_name));
		size_t l = strlen(p->reg_name);
		if (l && p->reg_name[l - 1] == '-')
			p->reg_name[l - 1] = 0;
		p->reg = REG_CPAN;
	}
}

/* SourceForge's File Release System: the RSS feed of a project, narrowed to
 * the path above the first directory the version is written into, lists the
 * newest hundred files under it. */
static void classify_sourceforge(Probe *p)
{
	const char *h = p->host;
	const char *proj = NULL;
	char web_proj[128];
	int first = 0;			/* first segment of the in-project path */

	if (!strcmp(h, "downloads.sourceforge.net") ||
	    !strcmp(h, "download.sourceforge.net")) {
		if (p->nseg >= 3 && !strcmp(p->seg[0], "project")) {
			proj = p->seg[1];
			first = 2;
		} else if (p->nseg >= 2) {
			proj = p->seg[0];
			first = 1;
		}
	} else if ((!strcmp(h, "sourceforge.net") ||
		    !strcmp(h, "www.sourceforge.net")) && p->nseg >= 4 &&
		   !strcmp(p->seg[0], "projects") && !strcmp(p->seg[2], "files")) {
		proj = p->seg[1];
		first = 3;
	} else {
		size_t hl = strlen(h);
		static const char *const web[] = { ".sourceforge.net",
						   ".sourceforge.io", NULL };
		for (int i = 0; web[i] && !proj; i++) {
			size_t l = strlen(web[i]);
			if (hl > l && !strcmp(h + hl - l, web[i]) &&
			    strncmp(h, "downloads.", 10)) {
				size_t sl = hl - l < sizeof(web_proj) - 1 ?
					    hl - l : sizeof(web_proj) - 1;
				memcpy(web_proj, h, sl);
				web_proj[sl] = 0;
				proj = web_proj;
				first = p->nseg;	/* project web: the whole feed */
			}
		}
	}
	if (!proj)
		return;

	char sub[512] = "";
	size_t o = 0;
	/* Directories only — the last segment is the file (or "download"). */
	int last = p->nseg - 1;
	if (last >= 0 && !strcmp(p->seg[last], "download"))
		last--;
	for (int i = first; i < last; i++) {
		PuAnchor a;
		char pref[PU_MAX_VER];
		int versioned = 0;
		for (int k = 1; k <= 4 && !versioned; k++)
			if (!pu_version_prefix(p->r->version, k, pref, sizeof(pref)) &&
			    pu_anchor_from(p->seg[i], pref, 0, &a))
				versioned = 1;
		if (versioned)
			break;
		char enc[200];
		url_encode_path(p->seg[i], enc, sizeof(enc));
		int w = snprintf(sub + o, sizeof(sub) - o, "/%s", enc);
		if (w < 0 || (size_t)w >= sizeof(sub) - o)
			break;
		o += (size_t)w;
	}
	kb_strlcpy(p->sf_proj, proj, sizeof(p->sf_proj));
	snprintf(p->sf_rss, sizeof(p->sf_rss),
		 "https://sourceforge.net/projects/%s/rss?path=%s", proj,
		 sub[0] ? sub : "/");
	p->no_index = 1;
}

static int probe_init(const PuRecipe *r, Probe *p)
{
	memset(p, 0, sizeof(*p));
	p->r = r;
	p->url = r->first_source;
	if (split_url(p->url, p->host, sizeof(p->host), &p->path) != 0)
		return -1;
	p->nseg = path_segments(p->path, p->seg, 24);

	/* The file's own name: the last segment, or the one before
	 * SourceForge's trailing "download". */
	if (p->nseg) {
		int b = p->nseg - 1;
		if (b > 0 && !strcmp(p->seg[b], "download"))
			b--;
		kb_strlcpy(p->base, p->seg[b], sizeof(p->base));
		p->has_file = pu_anchor_from(p->base, r->version, 1, &p->file);
	}

	classify_registry(p);
	if (!p->reg)
		classify_forge(p);
	if (!p->reg && !p->repo[0])
		classify_sourceforge(p);
	if (p->repo[0] && !p->ntag)
		default_tag_anchors(p);
	return 0;
}

/* ────────────────────────────────────────────────────────────────────────
 * Candidates: every version a listing names, filtered once, on the way in
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	const Probe *p;
	PuFound *f;
	int raw_seen;		/* the listing had entries at all */
	int exact_shape;	/* the strings are not upstream's own */
} Sink;

/* Could `v` be a later release of this port's numbering at all? */
static void note_outside(Sink *s, const char *v)
{
	char *o = s->f->outside;
	if (kp_vercmp(v, s->p->r->version) > 0 && (!o[0] || kp_vercmp(v, o) > 0))
		kb_strlcpy(o, v, sizeof(s->f->outside));
}

static int eligible(Sink *s, const char *v)
{
	const PuRecipe *r = s->p->r;
	if (!v[0] || strlen(v) >= PU_MAX_VER)
		return 0;
	if (s->exact_shape) {
		/* repology's strings are distros' spellings of upstream's —
		 * aalib's 1.4rc5 is 1.4.0 in some of them — so only the exact
		 * shape of the pin counts as the same numbering. */
		char a[PU_MAX_VER], b[PU_MAX_VER];
		kp_vershape(v, a, sizeof(a));
		kp_vershape(r->version, b, sizeof(b));
		if (strcmp(a, b))
			return 0;
	} else if (!pu_same_class(v, r->version)) {
		return 0;
	}
	if (pu_prerelease(v, r->version) ||
	    pu_devseries(v, r->version, r->devseries, s->p->url))
		return 0;
	/* A release past the line the recipe holds to is upstream's, not
	 * this port's: remembered, so the answer can name it. */
	if (!pu_in_series(v, r->series)) {
		note_outside(s, v);
		return 0;
	}
	return 1;
}

static void add_cand(Sink *s, const char *v)
{
	PuFound *f = s->f;
	if (!eligible(s, v))
		return;
	for (int i = 0; i < f->n; i++)
		if (!strcmp(f->cand[i], v))
			return;
	if (f->n < PU_MATCH_MAX) {
		kb_strlcpy(f->cand[f->n++], v, PU_MAX_VER);
		return;
	}
	/* Full: the oldest makes way. Only the newest candidates are ever
	 * proved or compared, so a project with more releases than the table
	 * (samba, setuptools) loses nothing a verdict reads. */
	int low = 0;
	for (int i = 1; i < f->n; i++)
		if (kp_vercmp(f->cand[i], f->cand[low]) < 0)
			low = i;
	if (kp_vercmp(v, f->cand[low]) > 0)
		kb_strlcpy(f->cand[low], v, PU_MAX_VER);
}

static int anchored(Sink *s, const PuAnchor *a, int na, const char *raw)
{
	char v[PU_MAX_VER];
	for (int i = 0; i < na; i++) {
		if (!pu_anchor_match(&a[i], raw, v, sizeof(v))) {
			add_cand(s, v);
			return 1;
		}
	}
	return 0;
}

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

/* The reader of last resort, for a source whose name does not carry the
 * version as written. `normalize` is the fallback for the ports whose recipe
 * version and URL version are the same release spelled with different
 * separators:
 *
 *     ca-certificates  recipe 20251202   url .../cacert-2025-12-02.pem
 *     imagemagick      recipe 7.1.2.21   url .../ImageMagick-7.1.2-21.tar.xz
 *
 * Every extracted candidate is also tried with its separators stripped
 * (2026-01-15 -> 20260115) and with its separators normalised to '.'
 * (7.1.2-21 -> 7.1.2.21) — one of the two guesses lands, the other is a
 * class mismatch dropped for free, and a WRONG guess that survives the class
 * filter costs nothing worse than one request that 404s. */
static void generic(Sink *s, char raws[][PU_MAX_RAW], int n)
{
	int before = s->f->n;
	for (int pass = 0; pass < 2 && s->f->n == before; pass++) {
		for (int i = 0; i < n; i++) {
			char ex[PU_EX_MAX][PU_MAX_VER];
			int ne = pu_extract(raws[i], ex, PU_EX_MAX);
			for (int j = 0; j < ne; j++) {
				if (!pass) {
					add_cand(s, ex[j]);
					continue;
				}
				char a[PU_MAX_VER], b[PU_MAX_VER];
				strip_seps(ex[j], a, sizeof(a));
				seps_to_dot(ex[j], b, sizeof(b));
				add_cand(s, a);
				add_cand(s, b);
			}
		}
	}
}

/* Tag names or file names, read through the anchors when the recipe's URL
 * gave some and through the generic extractor when it gave none. */
static void harvest(Sink *s, const PuAnchor *a, int na, char raws[][PU_MAX_RAW],
		    int n)
{
	if (n > 0)
		s->raw_seen = 1;
	if (!na) {
		generic(s, raws, n);
		return;
	}
	for (int i = 0; i < n; i++)
		anchored(s, a, na, raws[i]);
}

/* A tag list, read as harvest() reads one, less the tags that alias an older
 * release. A tag on the commit of a lower release is the same code under a
 * second name; when a release with other code sits between the two, the
 * higher name cannot be a later release and is a mistake. thermald's v2.15.10 is on
 * v2.5.10's commit, a typo for it, with 2.5.11-2.5.13 between: read as a
 * release it offers a downgrade under a number past the pin. A tag on the
 * commit of the release just before it is kept — sby tags every yosys
 * version, and 0.47 on 0.46's commit is a release with nothing changed. Only
 * versions that could be releases are compared, so v1.3.0 on the commit of
 * its own v1.3.0-rc2 is untouched. */
static void harvest_tags(Sink *s, const PuAnchor *a, int na,
			 char raws[][PU_MAX_RAW], char ids[][PU_OID], int n)
{
	if (n > 0)
		s->raw_seen = 1;
	if (!na || n <= 0) {
		generic(s, raws, n);
		return;
	}
	char (*ver)[PU_MAX_VER] = kb_calloc((size_t)n, PU_MAX_VER);
	for (int i = 0; i < n; i++)
		for (int k = 0; k < na && !ver[i][0]; k++)
			if (pu_anchor_match(&a[k], raws[i], ver[i], PU_MAX_VER) ||
			    !eligible(s, ver[i]))
				ver[i][0] = 0;
	for (int i = 0; i < n; i++) {
		if (!ver[i][0])
			continue;
		int alias = 0;
		for (int j = 0; j < n && !alias && ids[i][0]; j++) {
			size_t jl = strlen(ver[j]);
			if (j == i || !ver[j][0] || strcmp(ids[i], ids[j]) ||
			    kp_vercmp(ver[j], ver[i]) >= 0)
				continue;
			/* A series tag that follows its newest release —
			 * corrosion's v0.6 on v0.6.1's commit — is lower and
			 * on the same commit, but names no release. */
			if (!strncmp(ver[j], ver[i], jl) && ver[i][jl] == '.')
				continue;
			for (int m = 0; m < n && !alias; m++)
				alias = ver[m][0] && strcmp(ids[m], ids[i]) &&
					kp_vercmp(ver[m], ver[j]) > 0 &&
					kp_vercmp(ver[m], ver[i]) < 0;
		}
		if (!alias)
			add_cand(s, ver[i]);
	}
	free(ver);
}

/* ────────────────────────────────────────────────────────────────────────
 * Feed and listing parsers
 * ──────────────────────────────────────────────────────────────────────── */

/* Extracts every <title>...</title> in document order and drops the first
 * one, which on every forge checked (GitHub, GitLab, Codeberg, sr.ht,
 * SourceForge) is the feed's own header and never an entry — RSS and Atom
 * both put it before any entry/item. A CDATA wrapper is removed. A feed with
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
		if (len >= 12 && !strncmp(s, "<![CDATA[", 9) &&
		    !strncmp(s + len - 3, "]]>", 3)) {
			s += 9;
			len -= 12;
		}
		add_raw(out, &n, max, s, len);

		p = close + 8;
	}
	return n;
}

/* GitHub's tag and release feeds give every entry a link to
 * .../releases/tag/<tag>. The link, not the title: a release's title is
 * whatever its author typed ("Little CMS 2.19.1", a codename), while the link
 * always carries the tag. */
static int parse_github_feed(const char *body, char out[][PU_MAX_RAW], int max)
{
	int n = 0;
	static const char key[] = "/releases/tag/";
	for (const char *p = body; n < max && (p = strstr(p, key));) {
		p += sizeof(key) - 1;
		size_t l = strcspn(p, "\"'<> \n");
		add_raw(out, &n, max, p, l);
		p += l;
	}
	return n;
}

/* `git ls-remote --tags`: "<id>\trefs/tags/<tag>" per line, and for an
 * annotated tag a second line "<commit>\trefs/tags/<tag>^{}". ids[i] ends up
 * holding the commit tag i names — the peeled id where there is one, since
 * an annotated tag's own id is its tag object's and two of them on one commit
 * still differ. A line past `max` is not stored; the caller reads n == max
 * as a cut list. */
static int parse_ls_remote(const char *body, char out[][PU_MAX_RAW],
			   char ids[][PU_OID], int max)
{
	int n = 0;
	static const char key[] = "\trefs/tags/";
	const size_t kl = sizeof(key) - 1;
	for (const char *p = body; *p;) {
		size_t ll = strcspn(p, "\r\n");
		const char *tab = memchr(p, '\t', ll);
		if (tab && (size_t)(p + ll - tab) > kl && !strncmp(tab, key, kl)) {
			const char *name = tab + kl;
			size_t nl = (size_t)(p + ll - name);
			size_t il = (size_t)(tab - p);
			if (il >= PU_OID)
				il = PU_OID - 1;
			if (nl > 3 && !memcmp(name + nl - 3, "^{}", 3)) {
				nl -= 3;
				if (nl >= PU_MAX_RAW)
					nl = PU_MAX_RAW - 1;
				for (int i = n - 1; i >= 0; i--) {
					if (strlen(out[i]) == nl &&
					    !memcmp(out[i], name, nl)) {
						memcpy(ids[i], p, il);
						ids[i][il] = 0;
						break;
					}
				}
			} else if (n < max) {
				memcpy(ids[n], p, il);
				ids[n][il] = 0;
				add_raw(out, &n, max, name, nl);
			}
		}
		p += ll;
		while (*p == '\r' || *p == '\n')
			p++;
	}
	return n;
}

/* A bounded sink that can also discard a caller-chosen number of LEADING
 * matches. Both directory-listing scans below run twice through the same
 * body with the same sink: once with `max=0` (so add_raw can never write)
 * purely to learn `seen` — the true total — and once for real with `skip`
 * set from that total, so the two passes can never disagree about what
 * counts as a match. With an anchor set, only names it reads count, and what
 * is stored is the version it read. */
typedef struct {
	char (*out)[PU_MAX_RAW];
	int max;	/* 0 during the counting pass: nothing is ever written */
	int n;		/* items written so far, this pass */
	int seen;	/* items matched so far, written or not */
	int skip;	/* leading matches to discard, for tail-keeping */
	const PuAnchor *filter;	/* NULL: every candidate string counts */
	/* With `within`, an href counts only when, resolved against `page`,
	 * its path is below `within`: a listing's own entries are its
	 * children, and a link in its page chrome (a footer's
	 * linkedin.com/company/29561/) is no sibling directory. */
	const char *page;
	const char *within;
} RawSink;

static int resolve_url(const char *base, const char *href, size_t hl,
		       char *out, size_t cap);

/* The path of `url`, from its first '/' after the host. */
static const char *url_path(const char *url)
{
	const char *h = strstr(url, "://");
	if (!h)
		return url;
	h += 3;
	return h + strcspn(h, "/?#");
}

static int below(const RawSink *s, const char *str, size_t len)
{
	char abs[1024];
	if (resolve_url(s->page, str, len, abs, sizeof(abs)) != 0)
		return 0;
	const char *path = url_path(abs);
	size_t wl = strlen(s->within);
	return !strncmp(path, s->within, wl) && path[wl] && path[wl] != '?';
}

static void sink_emit(RawSink *s, const char *str, size_t len, int href)
{
	if (href && s->within && !below(s, str, len))
		return;
	if (s->filter) {
		char raw[PU_MAX_RAW], v[PU_MAX_VER];
		size_t c = len < sizeof(raw) - 1 ? len : sizeof(raw) - 1;
		memcpy(raw, str, c);
		raw[c] = 0;
		if (pu_anchor_match(s->filter, raw, v, sizeof(v)))
			return;
		str = v;
		len = strlen(v);
		if (s->seen >= s->skip)
			add_raw(s->out, &s->n, s->max, str, len);
		s->seen++;
		return;
	}
	if (s->seen >= s->skip)
		add_raw(s->out, &s->n, s->max, str, len);
	s->seen++;
}

static int is_tok_char(char c)
{
	return isalnum((unsigned char)c) || c == '.' || c == '-' ||
	       c == '_' || c == '~' || c == '+';
}

/* The next href="..." at or after `p`: its value in `*v`, `*vl` long, and
 * where scanning resumes. NULL at the end, or at an href with no closing
 * quote before the end of the buffer — nothing after it can still be a whole
 * attribute. */
static const char *next_href(const char *p, const char **v, size_t *vl)
{
	for (; *p; p++) {
		if (strncasecmp(p, "href", 4))
			continue;
		const char *q = p + 4;
		while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
			q++;
		if (*q != '=')
			continue;
		q++;
		while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
			q++;
		char quote = (*q == '"' || *q == '\'') ? *q : 0;
		if (quote)
			q++;
		const char *end;
		if (quote) {
			end = strchr(q, quote);
			if (!end)
				return NULL;
		} else {
			end = q;
			while (*end && *end != '>' && *end != ' ' &&
			       *end != '\t' && *end != '\n')
				end++;
		}
		*v = q;
		*vl = (size_t)(end - q);
		return end + (quote ? 1 : 0);
	}
	return NULL;
}

/* A directory listing's filenames arrive two ways depending on the server:
 * inside href="..." (Apache, nginx autoindex — mesa, gstreamer, libuv all
 * look like this) or, for an FTP listing or an index that does not use
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
 * attribute), so the scan never spins re-reading an unterminated tail. */
static void href_scan(const char *body, RawSink *sink)
{
	const char *v;
	size_t vl;
	for (const char *p = body; (p = next_href(p, &v, &vl));)
		if (has_digit(v, vl))
			sink_emit(sink, v, vl, 1);
}

/* The second pass, for the visible text a listing shows outside any markup —
 * some indexes print the filename a second time as anchor text, others (an
 * FTP LIST, or a plain-text index with no anchors at all) show it with no
 * href in sight. "<...>" spans are skipped rather than tokenised; what
 * remains is tokenised into filename-character runs, and only runs that look
 * like a real filename (a digit — a version — AND a dot — an extension) are
 * kept, so column headers like "Last modified" never make it into the
 * output. */
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
			int has_dot = memchr(start, '.', len) != NULL;
			if (has_dot && has_digit(start, len))
				sink_emit(sink, start, len, 0);
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
 * guarantee — hence the truncation signal, so a caller who cannot trust
 * listing order to have put the newest release within reach can fall back
 * to `unknown` instead of trusting a wrong `current`. */
static int tail_scan(void (*scan)(const char *, RawSink *), const char *body,
		      const PuAnchor *filter, const char *page,
		      const char *within, char out[][PU_MAX_RAW], int max,
		      int *truncated)
{
	RawSink counting = { out, 0, 0, 0, INT_MAX, filter, page, within };
	scan(body, &counting);
	int total = counting.seen;

	int skip = total > max ? total - max : 0;
	if (skip && truncated)
		*truncated = 1;

	RawSink writing = { out, max, 0, 0, skip, filter, page, within };
	scan(body, &writing);
	return writing.n;
}

/* One listing body: its links, or — only when it has none that count — its
 * visible text. A page with links also has prose around them, and prose
 * carries numbers that are no release's (the server's own version in an
 * Apache footer, a file size of 5.1K). With `filter`, out[] holds
 * versions. */
static int scan_listing_in(const char *body, const PuAnchor *filter,
			   const char *page, const char *within,
			   char out[][PU_MAX_RAW], int max, int *truncated)
{
	int n = tail_scan(href_scan, body, filter, page, within, out, max,
			  truncated);
	if (n)
		return n;
	/* A page whose versioned links all lead elsewhere is a page, not a
	 * listing, and its prose names other things' versions (launchpad's
	 * "Ubuntu RTM 14.09"). */
	if (within) {
		const char *v;
		size_t vl;
		for (const char *p = body; (p = next_href(p, &v, &vl));)
			if (has_digit(v, vl))
				return 0;
	}
	return tail_scan(token_scan, body, filter, NULL, NULL, out, max,
			 truncated);
}

static int scan_listing(const char *body, const PuAnchor *filter,
			char out[][PU_MAX_RAW], int max, int *truncated)
{
	return scan_listing_in(body, filter, NULL, NULL, out, max, truncated);
}

/* ────────────────────────────────────────────────────────────────────────
 * The registry adapter — PyPI, crates.io, MetaCPAN
 * ──────────────────────────────────────────────────────────────────────── */

static int registry_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	char url[600];
	switch (p->reg) {
	case REG_PYPI:
		snprintf(url, sizeof(url), "https://pypi.org/pypi/%s/json",
			 p->reg_name);
		break;
	case REG_CRATES:
		snprintf(url, sizeof(url), "https://crates.io/api/v1/crates/%s",
			 p->reg_name);
		break;
	case REG_CPAN:
		snprintf(url, sizeof(url),
			 "https://fastapi.metacpan.org/v1/release/%s", p->reg_name);
		break;
	default:
		return 0;
	}

	KbBuf b = {0};
	int code = pu_http_get(url, &b);
	if (code)
		s->f->reached = 1;
	if (!ok2xx(code) || !b.p) {
		kb_buf_free(&b);
		return 0;
	}
	/* kj_parse refuses anything that does not parse whole: a half-read or
	 * error-page response reads as "no candidates", never as a confident
	 * wrong one. */
	KjNode *root = kj_parse(b.p);
	kb_buf_free(&b);
	if (!root || root->type != KJ_OBJ) {
		kj_free(root);
		return 0;
	}

	int before = s->f->n;
	if (p->reg == REG_PYPI) {
		/* A release every file of which was yanked is withdrawn, and one
		 * with no files at all was never published. */
		const KjNode *rel = kj_get(root, "releases");
		for (const KjNode *e = rel && rel->type == KJ_OBJ ? rel->child : NULL;
		     e; e = e->next) {
			int live = 0;
			if (e->type == KJ_ARR)
				for (const KjNode *f = e->child; f && !live; f = f->next)
					live = f->type == KJ_OBJ && !kj_bool(f, "yanked", 0);
			if (live && e->key) {
				s->raw_seen = 1;
				add_cand(s, e->key);
			}
		}
	} else if (p->reg == REG_CRATES) {
		const KjNode *vs = kj_get(root, "versions");
		for (const KjNode *e = vs && vs->type == KJ_ARR ? vs->child : NULL;
		     e; e = e->next) {
			if (e->type != KJ_OBJ || kj_bool(e, "yanked", 0))
				continue;
			const char *v = kj_str(e, "num", "");
			s->raw_seen = 1;
			add_cand(s, v);
		}
	} else {
		/* The newest release only — MetaCPAN's `archive` is its file
		 * name, read through the recipe's own anchor. */
		int n = 0;
		const char *a = kj_str(root, "archive", "");
		add_raw(raws, &n, 1, a, strlen(a));
		harvest(s, p->has_file ? &p->file : NULL, p->has_file, raws, n);
	}
	kj_free(root);
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The forge adapter — every tag, from git itself
 *
 * `git ls-remote` is the one interface every forge serves unauthenticated and
 * unmetered, and it names every tag. The forge's own feed or API is asked
 * only when git cannot answer: GitHub's REST API allows 60 unauthenticated
 * requests an hour, which does not cover one run, and its feeds carry the
 * newest ten entries.
 * ──────────────────────────────────────────────────────────────────────── */

static int feed_parse(int kind, const char *body, char raws[][PU_MAX_RAW],
		      int max)
{
	switch (kind) {
	case FEED_GITHUB:
		return parse_github_feed(body, raws, max);
	case FEED_TITLES:
		return parse_feed_titles(body, raws, max);
	case FEED_GITLAB: {
		int n = 0;
		KjNode *root = kj_parse(body);
		if (root && root->type == KJ_ARR)
			for (const KjNode *e = root->child; e && n < max; e = e->next) {
				const char *t = kj_str(e, "name", "");
				add_raw(raws, &n, max, t, strlen(t));
			}
		kj_free(root);
		return n;
	}
	default:
		return 0;
	}
}

/* The recipe's tag prefix named nothing later than the pin: read the same
 * list through the default prefixes, and keep the newest later release they
 * name in f->other_prefix. sby tagged yosys-0.40 to yosys-0.47 and then
 * v0.48 onwards, so its own prefix alone reports "current" two dozen
 * releases behind. Only a family that starts after the recipe's own ends is
 * a change of scheme: one whose numbers run alongside it is another
 * project's in the same repository, as golang/tools' v0.50.0 is beside
 * gopls/v0.23.0. */
static void other_prefix(const Probe *p, Sink *s, char raws[][PU_MAX_RAW],
			 char ids[][PU_OID], int n)
{
	const char *pin = p->r->version;
	for (int i = 0; i < s->f->n; i++)
		if (kp_vercmp(s->f->cand[i], pin) > 0)
			return;
	Probe *d = kb_calloc(1, sizeof(*d));
	*d = *p;
	d->ntag = 0;
	d->has_file = 0;	/* v and none; the file's own name is the recipe's */
	default_tag_anchors(d);
	PuFound tf = { 0 };
	tf.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	Sink ts = { d, &tf, 0, 0 };
	harvest_tags(&ts, d->tag, d->ntag, raws, ids, n);
	int best = -1, low = -1, own = -1;
	for (int i = 0; i < tf.n; i++) {
		if (kp_vercmp(tf.cand[i], pin) > 0 &&
		    (best < 0 || kp_vercmp(tf.cand[i], tf.cand[best]) > 0))
			best = i;
		if (low < 0 || kp_vercmp(tf.cand[i], tf.cand[low]) < 0)
			low = i;
	}
	for (int i = 0; i < s->f->n; i++)
		if (own < 0 || kp_vercmp(s->f->cand[i], s->f->cand[own]) > 0)
			own = i;
	if (best >= 0 && (own < 0 || kp_vercmp(tf.cand[low], s->f->cand[own]) > 0))
		kb_strlcpy(s->f->other_prefix, tf.cand[best],
			   sizeof(s->f->other_prefix));
	free(tf.cand);
	free(d);
}

/* Every tag of `repo` through `a`, and then — when nothing later than the pin
 * came of it — through the default prefixes into f->other_prefix. 0 when git
 * answered. */
static int git_list(const Probe *p, Sink *s, const char *repo,
		    const PuAnchor *a, int na, char raws[][PU_MAX_RAW])
{
	KbBuf b = {0};
	if (pu_git_tags(repo, &b) != 0) {
		kb_buf_free(&b);
		return -1;
	}
	s->f->reached = 1;
	char (*ids)[PU_OID] = kb_calloc(PU_LIST_MAX, PU_OID);
	int n = b.p ? parse_ls_remote(b.p, raws, ids, PU_LIST_MAX) : 0;
	kb_buf_free(&b);
	if (n == PU_LIST_MAX)
		s->f->truncated = 1;
	harvest_tags(s, a, na, raws, ids, n);
	other_prefix(p, s, raws, ids, n);
	free(ids);
	return 0;
}

/* The 40-hex commit id `url`'s path names, into `out`; 0 when it names none. */
static int commit_in(const char *url, char *out)
{
	for (const char *p = url_path(url); *p; p++) {
		if (!isxdigit((unsigned char)*p) ||
		    (p > url && isalnum((unsigned char)p[-1])))
			continue;
		size_t n = 0;
		while (isxdigit((unsigned char)p[n]))
			n++;
		if (n == 40 && !isalnum((unsigned char)p[n])) {
			memcpy(out, p, 40);
			out[40] = 0;
			return 1;
		}
		p += n ? n - 1 : 0;
	}
	return 0;
}

/* A source that names a commit, in a repository that tags no release it could
 * be read against: the one question left is whether the commit is still the
 * tip of a branch upstream develops on — the default branch, or one named for
 * releases (x264 pins its `stable`). A tip of a feature branch says nothing
 * about what upstream ships. */
static void pinned_head(Probe *p, Sink *s)
{
	char id[PU_OID];
	if (!commit_in(p->url, id))
		return;
	s->f->pinned = 1;
	KbBuf b = {0};
	if (pu_git_heads(p->repo, &b) != 0 || !b.p) {
		kb_buf_free(&b);
		return;
	}
	s->f->reached = 1;
	static const char *const branches[] = { "master", "main", "stable",
						"trunk", "release", NULL };
	for (const char *l = b.p; *l;) {
		size_t ll = strcspn(l, "\r\n");
		const char *tab = memchr(l, '\t', ll);
		if (tab && (size_t)(tab - l) == 40 && !strncasecmp(l, id, 40)) {
			const char *ref = tab + 1;
			size_t rl = (size_t)(l + ll - ref);
			const char *br = NULL;
			size_t bl = 0;
			if (rl == 4 && !strncmp(ref, "HEAD", 4)) {
				br = ref;
				bl = 4;
			} else if (rl > 11 && !strncmp(ref, "refs/heads/", 11)) {
				br = ref + 11;
				bl = rl - 11;
				int named = 0;
				for (int i = 0; branches[i] && !named; i++) {
					size_t n = strlen(branches[i]);
					named = bl >= n && !strncmp(br, branches[i], n) &&
						(bl == n || strchr("-/._", br[n]));
				}
				if (!named)
					br = NULL;
			}
			/* A branch's name says more than HEAD's. */
			if (br && (!s->f->head[0] || !strcmp(s->f->head, "HEAD")) &&
			    bl < sizeof(s->f->head)) {
				memcpy(s->f->head, br, bl);
				s->f->head[bl] = 0;
			}
		}
		l += ll;
		while (*l == '\r' || *l == '\n')
			l++;
	}
	kb_buf_free(&b);
}

/* A GitHub project that publishes releases for some of its tags and not
 * others may be tagging engineering drops past its newest release —
 * intel/media-driver tags 26.2.1 to 26.2.4 and publishes 26.2.4, and its
 * 26.3.x tags are the next quarter's — or may be late creating the release
 * objects for real ones: bindgen's 0.73.x were on crates.io with no GitHub
 * release. Nothing in the tags tells the two apart, so the later tags stay
 * candidates, the newest of them is remembered and the answer is marked low
 * confidence; dropping them would turn a late release into a false current.
 * Read from the releases API — whose sixty requests an hour are spent only on
 * a port that already has a later tag — and applied only when a tag between
 * the oldest and newest release has none, and the pin is no later than the
 * newest release. When the API refuses, the tags stand unmarked, so the
 * verdict is the same either way. */
static void github_unreleased(Probe *p, Sink *s)
{
	const char *pin = p->r->version;
	int later = 0;
	for (int i = 0; i < s->f->n && !later; i++)
		later = kp_vercmp(s->f->cand[i], pin) > 0;
	if (p->feed_kind != FEED_GITHUB || !later || !p->ntag)
		return;
	char url[900];
	snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases?per_page=100",
		 p->repo + strlen("https://github.com/"));
	KbBuf b = {0};
	int code = pu_http_get(url, &b);
	KjNode *root = ok2xx(code) && b.p ? kj_parse(b.p) : NULL;
	kb_buf_free(&b);
	if (!root || root->type != KJ_ARR) {
		kj_free(root);
		return;
	}
	char (*rel)[PU_MAX_VER] = kb_calloc(128, PU_MAX_VER);
	int nrel = 0;
	for (const KjNode *e = root->child; e && nrel < 128; e = e->next) {
		if (e->type != KJ_OBJ || kj_bool(e, "draft", 0) ||
		    kj_bool(e, "prerelease", 0))
			continue;
		const char *t = kj_str(e, "tag_name", "");
		/* Only releases of the pin's own numbering bound the range:
		 * intel's old 600.0132 would span every tag there is. */
		for (int k = 0; k < p->ntag; k++)
			if (!pu_anchor_match(&p->tag[k], t, rel[nrel], PU_MAX_VER)) {
				if (pu_same_class(rel[nrel], pin))
					nrel++;
				break;
			}
	}
	kj_free(root);
	int hi = -1, lo = -1;
	for (int i = 0; i < nrel; i++) {
		if (hi < 0 || kp_vercmp(rel[i], rel[hi]) > 0)
			hi = i;
		if (lo < 0 || kp_vercmp(rel[i], rel[lo]) < 0)
			lo = i;
	}
	if (hi < 0 || kp_vercmp(pin, rel[hi]) > 0) {
		free(rel);
		return;
	}
	int selective = 0;
	for (int i = 0; i < s->f->n && !selective; i++) {
		const char *c = s->f->cand[i];
		if (kp_vercmp(c, rel[lo]) <= 0 || kp_vercmp(c, rel[hi]) >= 0)
			continue;
		selective = 1;
		for (int j = 0; j < nrel && selective; j++)
			selective = strcmp(c, rel[j]) != 0;
	}
	for (int i = 0; selective && i < s->f->n; i++) {
		const char *c = s->f->cand[i];
		int released = kp_vercmp(c, rel[hi]) <= 0;
		for (int j = 0; j < nrel && !released; j++)
			released = !strcmp(c, rel[j]);
		if (!released && (!s->f->unreleased[0] ||
				  kp_vercmp(c, s->f->unreleased) > 0))
			kb_strlcpy(s->f->unreleased, c, sizeof(s->f->unreleased));
	}
	if (s->f->unreleased[0])
		s->f->low_confidence = 1;
	free(rel);
}

static int forge_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	if (!p->repo[0])
		return 0;
	int before = s->f->n;

	git_list(p, s, p->repo, p->tag, p->ntag, raws);
	if (s->f->n > before) {
		github_unreleased(p, s);
		return s->f->n - before;
	}

	for (int i = 0; i < p->nfeed; i++) {
		KbBuf fb = {0};
		int code = pu_http_get(p->feed[i], &fb);
		if (code)
			s->f->reached = 1;
		if (ok2xx(code) && fb.p) {
			if (p->feed_kind == FEED_CGIT) {
				/* cgit's tag page links every snapshot by
				 * its file name. */
				int n = scan_listing(fb.p, NULL, raws,
						     PU_LIST_MAX, &s->f->truncated);
				harvest(s, p->has_file ? &p->file : NULL,
					p->has_file, raws, n);
			} else {
				int n = feed_parse(p->feed_kind, fb.p, raws,
						   PU_LIST_MAX);
				harvest(s, p->tag, p->ntag, raws, n);
			}
		}
		kb_buf_free(&fb);
	}
	if (s->f->n == before)
		pinned_head(p, s);
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The SourceForge adapter
 * ──────────────────────────────────────────────────────────────────────── */

static int repology_get(const char *url, KbBuf *b);

/* The newest version repology itself calls upstream's newest ("newest", not
 * a distro's own "outdated", "legacy" or "devel" line) that is later than the
 * pin and could be a release of the recipe's line, into `out`; "" when none
 * or when repology cannot be asked. Only ever turns a current into an
 * unknown, so a distro spelling of another shape (0.16.3 against 0.15.1b)
 * counts. */
static void repology_newest(const Probe *p, char *out, size_t cap)
{
	const PuRecipe *r = p->r;
	out[0] = 0;
	char url[300];
	snprintf(url, sizeof(url), "https://repology.org/api/v1/project/%s",
		 r->name);
	KbBuf b = {0};
	int code = repology_get(url, &b);
	KjNode *root = ok2xx(code) && b.p ? kj_parse(b.p) : NULL;
	kb_buf_free(&b);
	if (root && root->type == KJ_ARR) {
		for (const KjNode *e = root->child; e; e = e->next) {
			const char *v = kj_str(e, "version", "");
			if (strcmp(kj_str(e, "status", ""), "newest") ||
			    !v[0] || strlen(v) >= cap ||
			    kp_vercmp(v, r->version) <= 0 ||
			    (out[0] && kp_vercmp(v, out) <= 0) ||
			    pu_prerelease(v, r->version) ||
			    pu_devseries(v, r->version, r->devseries, p->url) ||
			    !pu_in_series(v, r->series))
				continue;
			kb_strlcpy(out, v, cap);
		}
	}
	kj_free(root);
}

/* The tags of the repository a SourceForge project moved to, read through
 * the default prefixes and the file's own, and failing those as a word and
 * then nothing but numbers and separators, read with every separator a dot
 * (smartmontools tags RELEASE_7_5; its X86_64_LINUX_OK is no version).
 * 1 when they name the pin and nothing later: the project still uploads its
 * releases to SourceForge (procps-ng, psmisc). Otherwise 0, and `later` the newest later release
 * they name, or "". A repository that does not answer is 0. */
static int moved_tags(const Probe *p, const char *repo, char *later, size_t cap)
{
	later[0] = 0;
	if (strncmp(repo, "https://", 8))
		return 0;
	KbBuf b = {0};
	if (pu_git_tags(repo, &b) != 0 || !b.p) {
		kb_buf_free(&b);
		return 0;
	}
	char (*raws)[PU_MAX_RAW] = kb_calloc(PU_LIST_MAX, PU_MAX_RAW);
	char (*ids)[PU_OID] = kb_calloc(PU_LIST_MAX, PU_OID);
	int n = parse_ls_remote(b.p, raws, ids, PU_LIST_MAX);
	kb_buf_free(&b);
	Probe *d = kb_calloc(1, sizeof(*d));
	*d = *p;
	d->ntag = 0;
	default_tag_anchors(d);
	PuFound tf = { 0 };
	tf.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	int at_pin = 0;
	for (int pass = 0; pass < 2 && !at_pin; pass++) {
		Sink ts = { d, &tf, 0, 0 };
		tf.n = 0;
		if (!pass) {
			harvest_tags(&ts, d->tag, d->ntag, raws, ids, n);
		} else {
			for (int i = 0; i < n; i++) {
				const char *t = raws[i];
				size_t w = strspn(t, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						     "abcdefghijklmnopqrstuvwxyz_-.");
				if (!isdigit((unsigned char)t[w]) ||
				    t[w + strspn(t + w, "0123456789_-.")])
					continue;
				char v[PU_MAX_VER];
				seps_to_dot(t + w, v, sizeof(v));
				add_cand(&ts, v);
			}
		}
		for (int i = 0; i < tf.n; i++)
			at_pin |= !kp_vercmp(tf.cand[i], p->r->version);
	}
	for (int i = 0; at_pin && i < tf.n; i++)
		if (kp_vercmp(tf.cand[i], p->r->version) > 0 &&
		    !pu_pretest(tf.cand[i], p->r->version, tf.cand, tf.n) &&
		    (!later[0] || kp_vercmp(tf.cand[i], later) > 0))
			kb_strlcpy(later, tf.cand[i], cap);
	int current = at_pin && !later[0] && n < PU_LIST_MAX;
	free(tf.cand);
	free(d);
	free(ids);
	free(raws);
	return current;
}

/* SourceForge's mirrors keep every file a project ever uploaded after the
 * project leaves, so a feed whose newest file is the pin may be a project that
 * now releases elsewhere: gnu-efi went to GitHub and libjpeg-turbo to its own
 * site, their feeds still ending at 3.0.18 and 3.0.1. Only then is more asked:
 * SourceForge's own record of the project, whose status says "moved", and
 * failing that repology's newest. A move makes the answer unknown unless the
 * repository moved to tags the pin and nothing later; a newer repology
 * version makes it unknown. Neither is a current nobody checked. */
static void sf_left(Probe *p, Sink *s)
{
	const char *pin = p->r->version;
	int at_pin = 0;
	for (int i = 0; i < s->f->n; i++) {
		int c = kp_vercmp(s->f->cand[i], pin);
		if (c > 0 && !pu_pretest(s->f->cand[i], pin, s->f->cand, s->f->n))
			return;
		at_pin |= c == 0;
	}
	if (!at_pin)
		return;
	char url[300];
	snprintf(url, sizeof(url), "https://sourceforge.net/rest/p/%s", p->sf_proj);
	KbBuf b = {0};
	int code = pu_http_get(url, &b);
	KjNode *root = ok2xx(code) && b.p ? kj_parse(b.p) : NULL;
	kb_buf_free(&b);
	if (root && root->type == KJ_OBJ &&
	    !strcmp(kj_str(root, "status", ""), "moved")) {
		const char *to = kj_str(root, "moved_to_url", "");
		snprintf(s->f->moved, sizeof(s->f->moved), "%.120s",
			 to[0] ? to : "another host");
	}
	kj_free(root);
	if (!s->f->moved[0])
		repology_newest(p, s->f->elsewhere, sizeof(s->f->elsewhere));
	else if (moved_tags(p, s->f->moved, s->f->elsewhere,
			    sizeof(s->f->elsewhere)))
		s->f->moved[0] = 0;
}

static int sf_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	if (!p->sf_rss[0])
		return 0;
	int before = s->f->n;
	KbBuf b = {0};
	int code = pu_http_get(p->sf_rss, &b);
	if (code)
		s->f->reached = 1;
	if (ok2xx(code) && b.p) {
		int n = parse_feed_titles(b.p, raws, PU_LIST_MAX);
		harvest(s, p->has_file ? &p->file : NULL, p->has_file, raws, n);
	}
	kb_buf_free(&b);
	if (s->f->n > before && p->sf_proj[0])
		sf_left(p, s);
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The directory adapter — Apache/nginx/FTP listings, and the walk
 *
 * A tarball's own directory holds the newest release only when the version
 * is not in the path. When it is — gnu/gcc/gcc-15.2.0/, ftp/python/3.14.2/,
 * sources/pango/1.57/, dist/v8/, kernel/v7.x/ — the newer releases are in
 * SIBLING directories, so the walk lists the parent of the outermost such
 * segment and reads its siblings through that segment's own anchor. A
 * sibling named with a whole version is a candidate as it stands; one named
 * with a series (1.58, v9) is descended into, newest first, to read the
 * files.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int idx;		/* index into Probe.seg */
	PuAnchor a;
	int full;		/* names the whole version, not a series */
	char cur[PU_MAX_VER];	/* the series (or version) it names now */
} VSeg;

/* The directory segments the recipe's version is written into, outermost
 * first. A segment counts when it holds a leading run of the version's
 * components with nothing but a name, a '-', '_' or '.', or a single letter
 * (v7.x, r2) in front: gtk3 is a name that ends in a digit, not the 3 of a
 * version 3.24. */
static int version_segments(const Probe *p, VSeg *out, int max)
{
	int n = 0;
	int last = p->nseg - 1;		/* the file itself is not a directory */
	for (int i = 0; i < last && n < max; i++) {
		for (int k = 6; k >= 1; k--) {
			char pref[PU_MAX_VER];
			if (pu_version_prefix(p->r->version, k, pref, sizeof(pref)))
				continue;
			PuAnchor a;
			if (!pu_anchor_from(p->seg[i], pref, 0, &a))
				continue;
			size_t pl = strlen(a.pre);
			char lastc = pl ? a.pre[pl - 1] : 0;
			int pre_ok = !pl || lastc == '-' || lastc == '_' ||
				     lastc == '.' ||
				     (pl == 1 && isalpha((unsigned char)lastc));
			int suf_ok = strlen(a.suf) <= 8 &&
				     !has_digit(a.suf, strlen(a.suf));
			if (!pre_ok || !suf_ok)
				continue;
			/* A directory named as nothing but the version reads
			 * nothing but digits: its listing's prose starts with
			 * digits too. */
			a.digits_only = !pl && !a.suf[0];
			VSeg *v = &out[n++];
			v->idx = i;
			v->a = a;
			v->full = !strcmp(pref, p->r->version);
			kb_strlcpy(v->cur, pref, sizeof(v->cur));
			break;
		}
	}
	return n;
}

static int vercmp_desc(const void *a, const void *b)
{
	return kp_vercmp((const char *)b, (const char *)a);
}

/* `base` with `seg` (sibling spelling) and the literal segments after it up
 * to, not including, segment `upto`, each followed by '/'. */
static int join_dir(char *out, size_t cap, const char *base, const char *seg,
		    const Probe *p, int from, int upto)
{
	int w = snprintf(out, cap, "%s%s/", base, seg);
	if (w < 0 || (size_t)w >= cap)
		return -1;
	size_t o = (size_t)w;
	for (int i = from; i < upto; i++) {
		w = snprintf(out + o, cap - o, "%s/", p->seg[i]);
		if (w < 0 || (size_t)w >= cap - o)
			return -1;
		o += (size_t)w;
	}
	return 0;
}

/* The name a sibling directory has for series `s` under `a`. */
static void sibling_name(const PuAnchor *a, const char *s, char *out, size_t cap)
{
	char sp[PU_MAX_VER];
	size_t o = 0;
	int d = 0;
	for (size_t i = 0; s[i] && o + 1 < sizeof(sp); i++) {
		char c = s[i];
		if (c == '.' && a->sep == '*')
			c = d < a->nwidths ? a->seps[d++] : '.';
		else if (c == '.')
			c = a->sep;
		if (c)			/* squashed: the dot is dropped */
			sp[o++] = c;
	}
	sp[o] = 0;
	snprintf(out, cap, "%s%s%s", a->pre, sp, a->suf);
}

/* `href`, `hl` long, as an absolute URL against the page `base` it was read
 * from, without its fragment and with its "." and ".." segments resolved. -1
 * for a link that is not a location (mailto:, javascript:) or does not fit. */
static int resolve_url(const char *base, const char *href, size_t hl,
		       char *out, size_t cap)
{
	char h[1024];
	if (!base || hl >= sizeof(h))
		return -1;
	memcpy(h, href, hl);
	h[hl] = 0;
	h[strcspn(h, "#")] = 0;
	const char *bs = strstr(base, "://");
	if (!bs)
		return -1;
	const char *bhost = bs + 3;
	size_t origin = (size_t)(bhost - base) + strcspn(bhost, "/?#");

	char path[1024];
	size_t colon = strcspn(h, ":/?");
	if (h[colon] == ':') {
		if (strncmp(h + colon, "://", 3))
			return -1;
		const char *hh = h + colon + 3;
		size_t ho = (size_t)(hh - h) + strcspn(hh, "/?#");
		if (ho >= cap)
			return -1;
		memcpy(out, h, ho);
		out[ho] = 0;
		kb_strlcpy(path, h[ho] ? h + ho : "/", sizeof(path));
	} else if (h[0] == '/' && h[1] == '/') {
		size_t sl = (size_t)(bs - base);
		const char *hh = h + 2;
		size_t hn = strcspn(hh, "/?#");
		if (sl + 3 + hn >= cap)
			return -1;
		snprintf(out, cap, "%.*s://%.*s", (int)sl, base, (int)hn, hh);
		kb_strlcpy(path, hh[hn] ? hh + hn : "/", sizeof(path));
	} else {
		if (origin >= cap)
			return -1;
		memcpy(out, base, origin);
		out[origin] = 0;
		if (h[0] == '/') {
			kb_strlcpy(path, h, sizeof(path));
		} else {
			const char *bp = base + origin;
			size_t bl = strcspn(bp, "?#");
			while (bl && bp[bl - 1] != '/')
				bl--;
			int w = snprintf(path, sizeof(path), "%.*s%s",
					 (int)bl, bl ? bp : "/", h);
			if (w < 0 || (size_t)w >= sizeof(path))
				return -1;
		}
	}

	/* Segments, with "." dropped and ".." taking the one before it. */
	char q[1024] = "";
	char *query = strchr(path, '?');
	if (query) {
		kb_strlcpy(q, query, sizeof(q));
		*query = 0;
	}
	char norm[1024];
	size_t o = 0;
	int trail = 0;
	for (char *seg = path; *seg;) {
		while (*seg == '/')
			seg++;
		size_t l = strcspn(seg, "/");
		if (!l)
			break;
		trail = seg[l] == '/' || (l == 1 && seg[0] == '.') ||
			(l == 2 && !strncmp(seg, "..", 2));
		if (l == 1 && seg[0] == '.') {
		} else if (l == 2 && !strncmp(seg, "..", 2)) {
			while (o && norm[o - 1] != '/')
				o--;
			if (o)
				o--;
		} else {
			if (o + l + 2 >= sizeof(norm))
				return -1;
			norm[o++] = '/';
			memcpy(norm + o, seg, l);
			o += l;
		}
		seg += l;
	}
	if (!o || trail)
		norm[o++] = '/';
	norm[o] = 0;
	size_t ol = strlen(out);
	int w = snprintf(out + ol, cap - ol, "%s%s", norm, q);
	return w < 0 || (size_t)w >= cap - ol ? -1 : 0;
}

/* The URL that lists the directory `dir` names. An Amazon S3 bucket answers a
 * directory path 403 and lists its keys at the bucket root under ?prefix=,
 * path-style (s3.amazonaws.com/<bucket>/<prefix>/) or virtual-hosted
 * (<bucket>.s3[.<region>].amazonaws.com/<prefix>/); every other host lists a
 * directory at its own URL. */
static void listing_url(const char *dir, char *out, size_t cap)
{
	char host[256];
	const char *path;
	kb_strlcpy(out, dir, cap);
	if (split_url(dir, host, sizeof(host), &path) != 0)
		return;
	size_t hl = strlen(host);
	static const char tail[] = ".amazonaws.com";
	size_t tl = sizeof(tail) - 1;
	if (hl <= tl || strcmp(host + hl - tl, tail))
		return;
	const char *pfx = path + (*path == '/');
	if (!strncmp(host, "s3.", 3) || !strncmp(host, "s3-", 3)) {
		size_t bl = strcspn(pfx, "/");
		if (!bl || !pfx[bl])
			return;
		snprintf(out, cap, "https://%s/%.*s?prefix=%s", host, (int)bl,
			 pfx, pfx + bl + 1);
	} else if (strstr(host, ".s3.") || strstr(host, ".s3-")) {
		snprintf(out, cap, "https://%s/?prefix=%s", host, pfx);
	}
}

/* A page that stands in for another: a meta refresh (curl.se/ca/ sends a
 * browser on to /docs/caextract.html), or an iframe that is the whole of a
 * page with no link of its own to anything versioned (nethack.org's
 * download/ frames /common/dnldindex.html). The target, resolved against
 * `page`, into `out`; 0 when there is none. */
static int stand_in(const char *body, const char *page, char *out, size_t cap)
{
	const char *m = strcasestr(body, "http-equiv");
	for (; m; m = strcasestr(m + 1, "http-equiv")) {
		const char *end = strchr(m, '>');
		if (!end)
			break;
		const char *r = strcasestr(m, "refresh");
		if (!r || r > end)
			continue;
		const char *u = strcasestr(m, "url=");
		if (!u || u > end)
			continue;
		u += 4;
		if (*u == '\'' || *u == '"')
			u++;
		size_t l = strcspn(u, "'\"> ");
		return resolve_url(page, u, l, out, cap) == 0;
	}

	const char *v;
	size_t vl;
	for (const char *p = body; (p = next_href(p, &v, &vl));)
		if (has_digit(v, vl))
			return 0;
	const char *f = strcasestr(body, "<iframe");
	if (!f)
		return 0;
	const char *end = strchr(f, '>');
	const char *src = strcasestr(f, "src=");
	if (!end || !src || src > end)
		return 0;
	src += 4;
	char q = (*src == '"' || *src == '\'') ? *src++ : 0;
	size_t l = q ? strcspn(src, q == '"' ? "\"" : "'") : strcspn(src, " >");
	return resolve_url(page, src, l, out, cap) == 0;
}

/* GET a listing or a page: through listing_url, and on to the page a stand-in
 * names. The URL the body is read against lands in `page`. A body that says
 * it is a cut listing (S3's IsTruncated) marks the walk truncated. */
static int get_page(Sink *s, const char *url, KbBuf *b, char *page, size_t pcap)
{
	char lurl[1200];
	listing_url(url, lurl, sizeof(lurl));
	kb_strlcpy(page, url, pcap);
	int code = pu_http_get(lurl, b);
	if (code)
		s->f->reached = 1;
	if (!ok2xx(code) || !b->p)
		return code;
	if (strstr(b->p, "<IsTruncated>true</IsTruncated>"))
		s->f->truncated = 1;
	char next[1024];
	if (stand_in(b->p, page, next, sizeof(next)) && strcmp(next, page)) {
		KbBuf nb = {0};
		int c2 = pu_http_get(next, &nb);
		if (ok2xx(c2) && nb.p) {
			kb_buf_free(b);
			*b = nb;
			kb_strlcpy(page, next, pcap);
			return c2;
		}
		kb_buf_free(&nb);
	}
	return code;
}

/* Pages one hop from `page` that may list the releases a page does not: a
 * link on the same host whose last segment names a download (download.html,
 * downloads.html, download.php, download/, TestDisk_Download), releases
 * (releases.html) or the current or latest release (mpfr-current/). Links
 * below the page's own directory come first — intra2net's download.php
 * before the site-wide /en/download/ — then by that order of words. */
#define PU_HOPS 3
static int hop_links(const char *page, const char *body, char out[][1024],
		     int max)
{
	static const char *const words[] = { "download", "release", "current",
					     "latest", NULL };
	char host[256], dir[1024];
	const char *path;
	if (split_url(page, host, sizeof(host), &path) != 0)
		return 0;
	kb_strlcpy(dir, path, sizeof(dir));
	dir[strcspn(dir, "?#")] = 0;
	char *slash = strrchr(dir, '/');
	if (slash)
		slash[1] = 0;

	char best[PU_HOPS][1024];
	int rank[PU_HOPS], n = 0;
	const char *v;
	size_t vl;
	for (const char *p = body; (p = next_href(p, &v, &vl));) {
		char abs[1024], ah[256];
		const char *ap;
		if (resolve_url(page, v, vl, abs, sizeof(abs)) != 0 ||
		    split_url(abs, ah, sizeof(ah), &ap) != 0 ||
		    strcasecmp(ah, host) || !strcmp(abs, page) ||
		    strchr(ap, '?'))
			continue;
		char last[256];
		size_t al = strlen(ap);
		while (al && ap[al - 1] == '/')
			al--;
		size_t ls = al;
		while (ls && ap[ls - 1] != '/')
			ls--;
		if (al - ls >= sizeof(last) || pu_archive_len(ap + ls, al - ls))
			continue;
		memcpy(last, ap + ls, al - ls);
		last[al - ls] = 0;
		int r = -1;
		for (int w = 0; words[w] && r < 0; w++)
			if (strcasestr(last, words[w]))
				r = w;
		if (r < 0)
			continue;
		if (strncmp(ap, dir, strlen(dir)))
			r += 8;
		int dup = 0;
		for (int i = 0; i < n && !dup; i++)
			dup = !strcmp(best[i], abs);
		if (dup)
			continue;
		/* Keep the best PU_HOPS, in rank order, first seen first. */
		int at = n;
		while (at > 0 && rank[at - 1] > r)
			at--;
		if (at >= PU_HOPS)
			continue;
		int last_i = n < PU_HOPS ? n : PU_HOPS - 1;
		for (int i = last_i; i > at; i--) {
			kb_strlcpy(best[i], best[i - 1], sizeof(best[i]));
			rank[i] = rank[i - 1];
		}
		kb_strlcpy(best[at], abs, sizeof(best[at]));
		rank[at] = r;
		if (n < PU_HOPS)
			n++;
	}
	int k = n < max ? n : max;
	for (int i = 0; i < k; i++)
		kb_strlcpy(out[i], best[i], 1024);
	return k;
}

static int read_page(Probe *p, Sink *s, const char *url, int hops,
		     char raws[][PU_MAX_RAW], int *budget);

/* This port's files one hop from `page`: each hop link in turn, until one of
 * them names any. */
static int hop_scan(Probe *p, Sink *s, const char *page, const char *body,
		    int hops, char raws[][PU_MAX_RAW], int *budget)
{
	char (*links)[1024] = kb_calloc(PU_HOPS, 1024);
	int nl = hop_links(page, body, links, PU_HOPS);
	int got = 0;
	for (int i = 0; i < nl && got <= 0 && *budget > 0; i++)
		got = read_page(p, s, links[i], hops - 1, raws, budget);
	free(links);
	return got > 0 ? got : 0;
}

/* Reads `url` as a page of this port's files, through the file anchor: a page
 * is prose, and prose is full of numbers. When nothing on it is this port's
 * and `hops` allows, the pages hop_links finds on it are read the same way.
 * The number of this port's names read; -1 when the page itself could not be
 * read or the budget is spent. */
static int read_page(Probe *p, Sink *s, const char *url, int hops,
		     char raws[][PU_MAX_RAW], int *budget)
{
	if (*budget <= 0 || !p->has_file)
		return -1;
	(*budget)--;
	KbBuf b = {0};
	char page[1024];
	int code = get_page(s, url, &b, page, sizeof(page));
	if (!ok2xx(code) || !b.p) {
		kb_buf_free(&b);
		return -1;
	}
	int n = scan_listing(b.p, &p->file, raws, PU_LIST_MAX, &s->f->truncated);
	if (n)
		s->raw_seen = 1;
	for (int i = 0; i < n; i++)
		add_cand(s, raws[i]);
	if (!n && hops > 0)
		n = hop_scan(p, s, page, b.p, hops, raws, budget);
	kb_buf_free(&b);
	return n;
}

/* Lists `dir` and feeds this port's files, read through the file anchor, into
 * the sink. Returns how many of the listing's names were this port's, before
 * any filter: a directory that holds only 3.15.0a1 files answered, and its
 * own name must not then stand in for a 3.15.0 it does not contain. -1 when
 * the listing was not read at all, the request failing or the walk's budget
 * spent. */
static int list_files(Probe *p, Sink *s, const char *dir,
		      char raws[][PU_MAX_RAW], int *budget)
{
	if (*budget <= 0)
		return -1;
	(*budget)--;
	int n = -1;
	KbBuf b = {0};
	char page[1024];
	int code = get_page(s, dir, &b, page, sizeof(page));
	if (ok2xx(code) && b.p) {
		if (p->has_file) {
			n = scan_listing(b.p, &p->file, raws, PU_LIST_MAX,
					 &s->f->truncated);
			if (n)
				s->raw_seen = 1;
			for (int i = 0; i < n; i++)
				add_cand(s, raws[i]);
		} else {
			n = scan_listing(b.p, NULL, raws, PU_LIST_MAX,
					 &s->f->truncated);
			harvest(s, NULL, 0, raws, n);
		}
	}
	kb_buf_free(&b);
	return n;
}

/* One level of the walk. -1 when `base` itself could not be listed. */
static int walk_level(Probe *p, Sink *s, const char *base, VSeg *vs, int nvs,
		      int k, char raws[][PU_MAX_RAW], int *budget)
{
	if (*budget <= 0)
		return -1;
	(*budget)--;
	KbBuf b = {0};
	char page[1024];
	int code = get_page(s, base, &b, page, sizeof(page));
	if (!ok2xx(code) || !b.p) {
		kb_buf_free(&b);
		return -1;
	}
	int n = scan_listing_in(b.p, &vs[k].a, page, url_path(base), raws,
				PU_LIST_MAX, &s->f->truncated);
	/* No sibling at all, not even the current one: this is a page, not
	 * the listing (mpfr.org links only mpfr-current/). Its download pages
	 * may still name this port's files. */
	if (!n && k == 0) {
		hop_scan(p, s, page, b.p, 1, raws, budget);
		kb_buf_free(&b);
		return 0;
	}
	kb_buf_free(&b);
	if (n)
		s->raw_seen = 1;

	/* Siblings at or after the current one, newest first, without the
	 * development series and pre-releases the port does not follow. */
	char (*sib)[PU_MAX_VER] = kb_calloc((size_t)n + 1, PU_MAX_VER);
	int ns = 0, have_cur = 0;
	for (int i = 0; i < n; i++) {
		const char *v = raws[i];
		if (strlen(v) >= PU_MAX_VER || kp_vercmp(v, vs[k].cur) < 0)
			continue;
		if (pu_prerelease(v, vs[k].cur) ||
		    pu_devseries(v, vs[k].cur, p->r->devseries, p->url))
			continue;
		/* Outside the recipe's series, and holding none of it. */
		if (!pu_in_series(v, p->r->series) &&
		    !pu_in_series(p->r->series, v)) {
			if (vs[k].full && pu_same_class(v, p->r->version))
				note_outside(s, v);
			continue;
		}
		int dup = 0;
		for (int j = 0; j < ns && !dup; j++)
			dup = !strcmp(sib[j], v);
		if (!dup)
			kb_strlcpy(sib[ns++], v, PU_MAX_VER);
	}
	qsort(sib, (size_t)ns, PU_MAX_VER, vercmp_desc);

	/* A whole-version sibling at the current version holds only the
	 * current release: it is a candidate, not a directory to read. A
	 * series one holds the current series' later patch releases too, so
	 * it is always read. */
	if (vs[k].full) {
		for (int i = 0; i < ns; i++)
			if (!strcmp(sib[i], vs[k].cur)) {
				add_cand(s, sib[i]);
				memmove(sib[i], sib[i + 1],
					(size_t)(ns - i - 1) * PU_MAX_VER);
				ns--;
				break;
			}
	} else {
		for (int i = 0; i < ns && i < PU_WALK_SERIES; i++)
			have_cur |= !strcmp(sib[i], vs[k].cur);
		if (!have_cur) {
			int at = ns < PU_WALK_SERIES ? ns : PU_WALK_SERIES;
			kb_strlcpy(sib[at], vs[k].cur, PU_MAX_VER);
			if (at >= ns)
				ns = at + 1;
		}
	}

	int upto = k + 1 < nvs ? vs[k + 1].idx : p->nseg - 1;
	int limit = vs[k].full ? PU_WALK_SERIES
			       : (ns < PU_WALK_SERIES + 1 ? ns : PU_WALK_SERIES + 1);
	for (int i = 0; i < ns; i++) {
		int read = 0;
		char name[200], next[1024];
		int listed = 0;
		if (i < limit && *budget > 0) {
			sibling_name(&vs[k].a, sib[i], name, sizeof(name));
			if (!join_dir(next, sizeof(next), base, name, p,
				      vs[k].idx + 1, upto)) {
				int before = s->f->n;
				if (k + 1 < nvs) {
					listed = walk_level(p, s, next, vs, nvs,
							    k + 1, raws,
							    budget) == 0;
					read = s->f->n > before;
				} else {
					int got = list_files(p, s, next, raws,
							     budget);
					listed = got >= 0;
					/* Without a file anchor no name in
					 * the listing is known to be this
					 * port's: a src-x86_64.tgz beside an
					 * unversioned src.tgz is not a
					 * pre-release, and taking it for one
					 * would drop the directory's version
					 * and answer "current". */
					read = p->has_file ? got > 0
							   : s->f->n > before;
				}
			}
		}
		/* A whole-version name no listing could be read under is
		 * still a version upstream published; the proof step decides
		 * whether its file is where the recipe looks. A later series
		 * that was not listed is a gap nothing else fills: the
		 * current series alone would answer "current". */
		if (vs[k].full && !read)
			add_cand(s, sib[i]);
		else if (!vs[k].full && !listed && kp_vercmp(sib[i], vs[k].cur) > 0)
			s->f->truncated = 1;
	}
	free(sib);
	return 0;
}

static int dir_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	if (p->no_index)
		return 0;
	int before = s->f->n;
	int budget = PU_WALK_GETS;

	VSeg vs[4];
	int nvs = version_segments(p, vs, 4);
	if (nvs) {
		/* The URL up to and including the '/' before the outermost
		 * versioned segment. */
		char base[1024];
		size_t o = (size_t)(p->path - p->url);
		if (o >= sizeof(base))
			return 0;
		memcpy(base, p->url, o);
		base[o] = 0;
		for (int i = 0; i < vs[0].idx; i++) {
			int w = snprintf(base + o, sizeof(base) - o, "/%s", p->seg[i]);
			if (w < 0 || (size_t)w >= sizeof(base) - o)
				return 0;
			o += (size_t)w;
		}
		if (o + 1 >= sizeof(base))
			return 0;
		base[o++] = '/';
		base[o] = 0;
		walk_level(p, s, base, vs, nvs, 0, raws, &budget);
		return s->f->n - before;
	}

	/* No version in the path: the tarball's own directory. A directory
	 * that is no listing (403, 404, or a page) often has one hop away:
	 * its own download page, or the one its parent — the project's page
	 * — links (netfilter's files/ beside downloads.html, musl's
	 * releases/ beside releases.html). */
	char parent[1024];
	const char *slash = strrchr(p->path, '/');
	if (!slash)
		return 0;
	size_t len = (size_t)(slash - p->url) + 1;
	if (len >= sizeof(parent))
		return 0;
	memcpy(parent, p->url, len);
	parent[len] = 0;
	if (!p->has_file) {
		list_files(p, s, parent, raws, &budget);
		return s->f->n - before;
	}
	if (read_page(p, s, parent, 1, raws, &budget) < 0) {
		size_t pl = strlen(parent);
		size_t root = (size_t)(p->path - p->url) + 1;
		while (pl > root && parent[pl - 1] == '/')
			pl--;
		while (pl > root && parent[pl - 1] != '/')
			pl--;
		if (pl >= root && pl < strlen(parent)) {
			parent[pl] = 0;
			read_page(p, s, parent, 1, raws, &budget);
		}
	}
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * Forge repositories named outside the source URL
 * ──────────────────────────────────────────────────────────────────────── */

/* The git repository a forge URL names — a project's page on GitHub,
 * Codeberg, sr.ht, Bitbucket or a GitLab — into `out`. 0 when `url` is on no
 * forge. A GitLab project path nests (xorg/lib/libxcb), so there it is the
 * whole path up to any "/-/". */
static int forge_repo_of(const char *url, char *out, size_t cap)
{
	char host[256];
	const char *path;
	if (split_url(url, host, sizeof(host), &path) != 0)
		return 0;
	char seg[8][160];
	int nseg = path_segments(path, seg, 8);
	if (nseg < 2)
		return 0;
	if (!strncmp(host, "gitlab.", 7) || strstr(path, "/-/")) {
		size_t pl = strcspn(path, "?#");
		const char *m = strstr(path, "/-/");
		if (m && (size_t)(m - path) < pl)
			pl = (size_t)(m - path);
		while (pl && path[pl - 1] == '/')
			pl--;
		if (pl > 4 && !strncmp(path + pl - 4, ".git", 4))
			pl -= 4;
		snprintf(out, cap, "https://%s%.*s.git", host, (int)pl, path);
		return 1;
	}
	if (strcmp(host, "github.com") && strcmp(host, "codeberg.org") &&
	    strcmp(host, "git.sr.ht") && strcmp(host, "bitbucket.org"))
		return 0;
	size_t rl = strlen(seg[1]);
	if (rl > 4 && !strcmp(seg[1] + rl - 4, ".git"))
		seg[1][rl - 4] = 0;
	snprintf(out, cap, "https://%s/%s/%s%s", host, seg[0], seg[1],
		 !strcmp(host, "bitbucket.org") ? ".git" : "");
	return 1;
}

/* Every tag of the forge repository `repo`, read through the default tag
 * anchors: the version as written, after a v, or after the file's own name. */
static int forge_tags_of(Probe *p, Sink *s, const char *repo,
			 char raws[][PU_MAX_RAW])
{
	int before = s->f->n;
	Probe *fp = kb_calloc(1, sizeof(*fp));
	*fp = *p;
	fp->ntag = 0;
	kb_strlcpy(fp->repo, repo, sizeof(fp->repo));
	default_tag_anchors(fp);
	git_list(fp, s, repo, fp->tag, fp->ntag, raws);
	free(fp);
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The homepage adapter
 *
 * A download directory that answers 403 (netfilter, marlam.de, intra2net)
 * usually has a download page that links every file, and the homepage is
 * where the recipe says to look. Read only through the file anchor — a page
 * is prose, and prose is full of numbers — and one hop further when the page
 * itself names none of this port's files. A homepage on a forge is asked for
 * its tags instead.
 * ──────────────────────────────────────────────────────────────────────── */

static int homepage_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	const char *hp = p->r->homepage;
	if (!hp[0] || !p->has_file || p->repo[0] || p->reg)
		return 0;
	int before = s->f->n;
	char repo[768];
	if (forge_repo_of(hp, repo, sizeof(repo)))
		return forge_tags_of(p, s, repo, raws);
	int budget = PU_WALK_GETS;
	read_page(p, s, hp, 1, raws, &budget);
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The watch adapter
 *
 * A recipe's `watch` key names where upstream lists its releases when no
 * rule above can find it from the source URL: a forge repository, whose tags
 * are read, or a page or listing — a JSON release index, a channel manifest,
 * a browsable mirror of the source's own host — read through the file
 * anchor. Tried first, since the recipe named it.
 * ──────────────────────────────────────────────────────────────────────── */

static int watch_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	const char *w = p->r->watch;
	if (!w[0])
		return 0;
	int before = s->f->n;
	char repo[768];
	if (forge_repo_of(w, repo, sizeof(repo)))
		return forge_tags_of(p, s, repo, raws);
	int budget = PU_WALK_GETS;
	if (p->has_file) {
		read_page(p, s, w, 1, raws, &budget);
	} else {
		/* Nothing to read the page through: a listing's names, by
		 * the extractor. */
		list_files(p, s, w, raws, &budget);
	}
	return s->f->n - before;
}

/* ────────────────────────────────────────────────────────────────────────
 * The repology fallback
 *
 * Only reached when every adapter above genuinely found nothing — repology
 * is never upstream itself, so every result from here sets low_confidence.
 * ──────────────────────────────────────────────────────────────────────── */

static void sleep_s(double s)
{
	struct timespec ts;
	ts.tv_sec = (time_t)s;
	ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
		;
}

/* Serialised to one request per second across the whole run — repology's
 * documented limit, and a maintenance tool has no business being the reason a
 * free service rate-limits the next person. Checks run in several processes
 * at once, so the last request's time lives in a locked file every one of
 * them reads, not in a variable each has its own copy of.
 *
 * The file is in $XDG_RUNTIME_DIR, private to the user, and in /tmp only
 * where that is unset. It is opened without following a link and used only
 * when it is a regular file of this user's: in a shared /tmp another user
 * could otherwise point the name at a file of ours to be truncated, or hold a
 * lock of theirs forever. The lock is waited for a bounded time, and a lock
 * that cannot be had costs a plain one-second pause instead. A fixture run
 * requests nothing and waits for nothing. */
static void repology_throttle(void)
{
	if (pu_http_fixture_dir())
		return;
	char path[512];
	const char *rt = getenv("XDG_RUNTIME_DIR");
	snprintf(path, sizeof(path), "%s/kdos-portup-repology-%d.lock",
		 rt && rt[0] == '/' ? rt : "/tmp", (int)getuid());
	int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
	struct stat st;
	int locked = 0;
	if (fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
	    st.st_uid == getuid()) {
		/* Eight workers each holding it under a second: thirty
		 * seconds is a lock nobody is going to release. */
		for (int t = 0; t < 600 && !locked; t++) {
			if (flock(fd, LOCK_EX | LOCK_NB) == 0)
				locked = 1;
			else
				sleep_s(0.05);
		}
	}
	if (!locked) {
		if (fd >= 0)
			close(fd);
		sleep_s(1.0);
		return;
	}
	char buf[64] = { 0 };
	ssize_t got = pread(fd, buf, sizeof(buf) - 1, 0);
	double last = got > 0 ? strtod(buf, NULL) : 0;
	double wait = last > 0 ? 1.0 - (kb_now_s() - last) : 0;
	if (wait > 1.0)
		wait = 1.0;		/* a clock that stepped back */
	if (wait > 0)
		sleep_s(wait);
	int w = snprintf(buf, sizeof(buf), "%.3f\n", kb_now_s());
	if (w > 0 && pwrite(fd, buf, (size_t)w, 0) == w) {
		int t = ftruncate(fd, w);
		(void)t;		/* a longer stale tail parses the same */
	}
	close(fd);			/* releases the lock */
}

/* A repology GET: every attempt, a retry included, waits its turn under the
 * throttle. A 429 is repology asking for less, so it waits ten seconds before
 * that turn rather than the generic two. */
static int repology_get(const char *url, KbBuf *b)
{
	static const double pause[] = { 2, 5 };
	int code = 0;
	for (int t = 0; t < 3; t++) {
		if (t)
			sleep_s(code == 429 ? 10 : pause[t - 1]);
		repology_throttle();
		code = pu_http_get_once(url, b);
		if (code && code != 429 && code < 500)
			break;
	}
	return code;
}

static int repology_list(Probe *p, Sink *s, char raws[][PU_MAX_RAW])
{
	const PuRecipe *r = p->r;
	if (!r->name[0])
		return 0;
	int before = s->f->n;

	char url[300];
	snprintf(url, sizeof(url), "https://repology.org/api/v1/project/%s",
		 r->name);

	KbBuf b = {0};
	int code = repology_get(url, &b);
	if (code)
		s->f->reached = 1;
	int n = 0;
	if (ok2xx(code) && b.p) {
		KjNode *root = kj_parse(b.p);
		if (root && root->type == KJ_ARR) {
			for (const KjNode *e = root->child; e && n < PU_LIST_MAX;
			     e = e->next) {
				/* repology's own curators mark a per-repo
				 * version "incorrect" when their parser
				 * mis-splits a distro revision out of the
				 * upstream string — aalib's Debian-derived
				 * "1.4p5-48+apertis1" comes back as "1.4p5",
				 * which kp_vercmp ranks ABOVE the real
				 * "1.4rc5" because 'p' is a non-prerelease
				 * suffix (OpenSSH's convention, and correct for
				 * OpenSSH). "untrusted" is its stronger
				 * per-repo version of the same flag. */
				const char *status = kj_str(e, "status", "");
				if (!strcmp(status, "incorrect") ||
				    !strcmp(status, "untrusted"))
					continue;
				const char *v = kj_str(e, "version", "");
				if (v[0])
					add_raw(raws, &n, PU_LIST_MAX, v, strlen(v));
			}
		}
		kj_free(root);
	}
	kb_buf_free(&b);
	s->exact_shape = 1;
	harvest(s, NULL, 0, raws, n);
	s->exact_shape = 0;
	if (s->f->n > before)
		s->f->low_confidence = 1;
	return s->f->n - before;
}

/*
 * Does repology flag the version we PIN as vulnerable?
 *
 * A different question from every other one in this file, and the only one that
 * is about security rather than freshness: repology carries a per-entry
 * `vulnerable` flag, aggregated from the distros that track CVEs, and a hit on
 * our exact version string is a real signal that something is known about it.
 *
 * Deliberately NOT part of the normal update check. It costs one request per
 * port at repology's one-per-second limit — six and a half minutes over the
 * whole tree — so it lives behind `--cve` and the offline `kdos cve` is the
 * everyday answer. This is the cross-check on that answer, not a replacement:
 * repology sees distros KDOS does not, and misses the version comparison the
 * secdb table makes.
 *
 * Tri-state on purpose. "repology has never heard of this project" and "this
 * version is not flagged" are different, and only the second is good news.
 */
int pu_repology_vuln(const PuRecipe *r)
{
	if (!r->name[0] || !r->version[0])
		return PU_VULN_UNKNOWN;

	char url[300];
	snprintf(url, sizeof(url), "https://repology.org/api/v1/project/%s",
		 r->name);
	KbBuf b = {0};
	if (!ok2xx(repology_get(url, &b))) {
		kb_buf_free(&b);
		return PU_VULN_UNKNOWN;
	}

	int seen = 0, flagged = 0;
	if (b.p) {
		KjNode *root = kj_parse(b.p);
		if (root && root->type == KJ_ARR) {
			for (const KjNode *e = root->child; e; e = e->next) {
				const char *v = kj_str(e, "version", "");
				if (strcmp(v, r->version))
					continue;
				seen = 1;
				if (kj_bool(e, "vulnerable", 0))
					flagged = 1;
			}
		}
		kj_free(root);
	}
	kb_buf_free(&b);
	return !seen ? PU_VULN_UNKNOWN : flagged ? PU_VULN_YES : PU_VULN_NO;
}

/* ────────────────────────────────────────────────────────────────────────
 * The entry points
 * ──────────────────────────────────────────────────────────────────────── */

int pu_discover(const PuRecipe *r, PuFound *f)
{
	if (!r || !f || !f->cand)
		return -1;
	f->n = 0;
	f->reached = f->truncated = f->low_confidence = 0;
	f->pinned = f->unanchored = 0;
	f->via[0] = f->other_prefix[0] = f->outside[0] = f->head[0] = 0;
	f->unreleased[0] = f->moved[0] = f->elsewhere[0] = 0;

	Probe *p = kb_calloc(1, sizeof(*p));
	if (probe_init(r, p) != 0) {
		free(p);
		return 0;
	}

	static const struct {
		const char *name;
		int (*fn)(Probe *, Sink *, char[][PU_MAX_RAW]);
	} adapters[] = {
		{ "watch", watch_list },
		{ "registry", registry_list },
		{ "forge", forge_list },
		{ "sourceforge", sf_list },
		{ "directory", dir_list },
		{ "homepage", homepage_list },
		{ "repology", repology_list },
	};

	char (*raws)[PU_MAX_RAW] = kb_calloc(PU_LIST_MAX, PU_MAX_RAW);
	Sink s = { p, f, 0, 0 };
	for (size_t i = 0; i < sizeof(adapters) / sizeof(adapters[0]); i++) {
		/* A cut listing from an adapter that then found nothing says
		 * nothing about the next one's answer. */
		int trunc = f->truncated;
		if (adapters[i].fn(p, &s, raws) > 0) {
			kb_strlcpy(f->via, adapters[i].name, sizeof(f->via));
			break;
		}
		f->truncated = trunc;
		/* A pinned commit is answered by the forge or by nobody: a
		 * version from anywhere else cannot be rendered into its URL. */
		if (f->pinned)
			break;
	}
	/* The recipe's version appears nowhere in its URL, so no upstream
	 * name can be read against it. */
	PuAnchor any;
	f->unanchored = !pu_anchor_from(p->url, r->version, 0, &any);
	if (!f->n && s.raw_seen)
		kb_strlcpy(f->via, "listing", sizeof(f->via));
	free(raws);
	free(p);

	/* Pretests are judged against the whole list, which only exists once
	 * an adapter has finished. A registry marks its own pre-releases, so
	 * a 1.0.92 there is always a release. */
	if (strcmp(f->via, "registry")) {
		int keep = 0;
		for (int i = 0; i < f->n; i++)
			if (!pu_pretest(f->cand[i], r->version, f->cand, f->n))
				kb_strlcpy(f->cand[keep++], f->cand[i], PU_MAX_VER);
		f->n = keep;
	}

	qsort(f->cand, (size_t)f->n, PU_MAX_VER, vercmp_desc);
	return 0;
}

int pu_check(const char *kpkg_bin, const PuRecipe *r, PuResult *out)
{
	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->state = PU_UNKNOWN;
	if (!kpkg_bin || !r)
		return -1;

	PuFound f = { 0 };
	f.cand = kb_calloc(PU_MATCH_MAX, PU_MAX_VER);
	pu_discover(r, &f);
	out->low_confidence = f.low_confidence;

	if (!f.n && f.pinned && f.head[0]) {
		/* The commit the recipe pins is still where upstream's branch
		 * is: there is nothing later to have. */
		out->state = PU_CURRENT;
		kb_strlcpy(out->candidate, r->version, sizeof(out->candidate));
		snprintf(out->reason, sizeof(out->reason),
			 "pinned commit is the tip of %.40s; upstream tags no release",
			 f.head);
		free(f.cand);
		return 0;
	}
	if (!f.n) {
		/* Requirement, not a guess: not one adapter completing a request
		 * leaves nothing to report but ignorance. */
		snprintf(out->reason, sizeof(out->reason), "%s",
			 !strstr(r->first_source, "://") ?
				"the source is no URL; there is no upstream to ask" :
			 f.pinned && f.reached ?
				"pinned commit is no branch tip upstream, and upstream tags no release" :
			 !f.reached ? "no adapter could reach a host for this port" :
			 f.unanchored ?
				"the recipe's version is not written in its source URL, so no upstream name reads against it" :
			 f.via[0] ? "no upstream entry matched the source's name and the recipe's version" :
				    "upstream listing had no entries");
		free(f.cand);
		return 0;
	}

	/* cand is sorted descending, so the ones that beat CURRENT are exactly
	 * the leading run. */
	int ncand = 0;
	while (ncand < f.n && kp_vercmp(f.cand[ncand], r->version) > 0)
		ncand++;

	if (ncand == 0) {
		/* Nothing beat CURRENT. That is what `current` means everywhere
		 * else, but not when the listing itself was incomplete:
		 * `truncated` relies on archive indexes sorting ascending, which
		 * curl.se's does NOT (its tail is download/archeology/...), so a
		 * truncated listing may genuinely be missing the one entry that
		 * would have beaten CURRENT. Reporting `current` from it would be
		 * a confident wrong answer — the one outcome this tool must never
		 * give. */
		if (f.other_prefix[0]) {
			snprintf(out->reason, sizeof(out->reason),
				 "the tags name %.32s under another prefix than the source's",
				 f.other_prefix);
		} else if (f.truncated) {
			snprintf(out->reason, sizeof(out->reason),
				 "an upstream listing was cut short or not read; cannot rule out a newer release");
		} else if (f.moved[0] && f.elsewhere[0]) {
			snprintf(out->reason, sizeof(out->reason),
				 "SourceForge ends at the pin; %.24s is tagged at %.56s",
				 f.elsewhere, f.moved);
		} else if (f.moved[0]) {
			snprintf(out->reason, sizeof(out->reason),
				 "SourceForge lists nothing past the pin; the project moved to %.60s",
				 f.moved);
		} else if (f.elsewhere[0]) {
			snprintf(out->reason, sizeof(out->reason),
				 "SourceForge lists nothing past the pin; repology names %.32s",
				 f.elsewhere);
		} else {
			out->state = PU_CURRENT;
			kb_strlcpy(out->candidate, r->version,
				   sizeof(out->candidate));
			if (f.outside[0])
				snprintf(out->reason, sizeof(out->reason),
					 "held to series %.32s; newest upstream %.32s",
					 r->series, f.outside);
		}
		free(f.cand);
		return 0;
	}

	/* code 0 (curl could not complete the request) is a legitimate HTTP
	 * code slot, so "did we break early" needs its own flag rather than
	 * testing last_code for truth. */
	char newest[PU_MAX_VER];
	kb_strlcpy(newest, f.cand[0], sizeof(newest));
	int broke_early = 0, last_code = 0, unprovable = 0, tried = 0;
	/* Three misses in a row within a major newer than the pin's and the
	 * rest of that major is skipped: a template that cannot fetch a major's
	 * three newest releases cannot fetch that major at all
	 * (SDL2-<v>.tar.gz under SDL 3's tags, LLVM's per-component tarballs,
	 * which stopped at 21), and the proof budget belongs to the next major
	 * down. The pin's own major is never skipped — its template is known
	 * to work there. */
	char miss_major[16] = "", skip_major[16] = "", own_major[16];
	int misses = 0;
	size_t oml = strspn(r->version, "0123456789");
	if (oml >= sizeof(own_major))
		oml = sizeof(own_major) - 1;
	memcpy(own_major, r->version, oml);
	own_major[oml] = 0;
	for (int i = 0; i < ncand && tried < PU_TRY_MAX; i++) {
		char major[16];
		size_t ml = strspn(f.cand[i], "0123456789");
		if (ml >= sizeof(major))
			ml = sizeof(major) - 1;
		memcpy(major, f.cand[i], ml);
		major[ml] = 0;
		if (skip_major[0] && !strcmp(major, skip_major))
			continue;

		char url[1024];
		if (pu_render_candidate(kpkg_bin, r, f.cand[i], url,
					 sizeof(url)) != 0)
			continue;
		/* A URL the version does not reach (a pinned commit, an
		 * unversioned file name) resolves for every candidate and
		 * proves none of them. */
		if (!strcmp(url, r->first_source)) {
			unprovable++;
			continue;
		}
		tried++;
		int code = pu_http_head(url);
		if (code == 200) {
			out->state = PU_NEWER;
			kb_strlcpy(out->candidate, f.cand[i],
				   sizeof(out->candidate));
			kb_strlcpy(out->url, url, sizeof(out->url));
			/* A newer version the template could not render or
			 * fetch is still upstream's newest, and the review
			 * says so: a series directory written ${version%.*}
			 * turns 0.21.8.2 into a 0.21.8/ upstream never made. */
			if (i > 0)
				snprintf(out->reason, sizeof(out->reason),
					 "newest upstream %.32s is not at the recipe's URL",
					 newest);
			else if (f.unreleased[0])
				snprintf(out->reason, sizeof(out->reason),
					 "no GitHub release for tags up to %.32s yet",
					 f.unreleased);
			free(f.cand);
			return 0;
		}
		/* A listing can name a version whose tarball is not where the
		 * recipe's template expects it: drop it and try the
		 * next-highest rather than giving up on the whole check. */
		if (code == 404 || code == 410) {
			if (strcmp(major, miss_major)) {
				kb_strlcpy(miss_major, major, sizeof(miss_major));
				misses = 0;
			}
			if (++misses >= 3 && strcmp(major, own_major))
				kb_strlcpy(skip_major, major, sizeof(skip_major));
			continue;
		}
		last_code = code;
		broke_early = 1;
		break;
	}
	free(f.cand);

	if (broke_early) {
		if (last_code)
			snprintf(out->reason, sizeof(out->reason),
				 "candidate URL returned HTTP %d", last_code);
		else
			snprintf(out->reason, sizeof(out->reason),
				 "could not reach the candidate URL's host");
	} else if (unprovable && !tried) {
		snprintf(out->reason, sizeof(out->reason),
			 "the source URL has no version in it to change; newest upstream %.32s, unproved",
			 newest);
	} else {
		snprintf(out->reason, sizeof(out->reason),
			 "%d newer version(s) upstream, none at the recipe's URL (newest %.32s)",
			 ncand, newest);
	}
	return 0;
}

/* The forge "org" a source URL belongs to: the first path segment on GitHub,
 * Codeberg and sr.ht (keeping sr.ht's '~'), and on a GitLab — any host whose
 * URL carries the "/-/" project separator — too. Grouping two unrelated
 * projects that happen to share an org and a version is harmless: a group is
 * only ever OFFERED when every member has the same newer version available,
 * and unrelated projects do not move in lockstep. Under-grouping is the
 * dangerous direction — that is what ships a half-bumped epoch — and keying a
 * GitLab project on its full path would give xorg/font/encodings and
 * xorg/font/util, siblings under one group, different keys. */
static int forge_org(const char *src, char *org, size_t cap)
{
	char host[256];
	const char *path;
	if (split_url(src, host, sizeof(host), &path) != 0)
		return -1;
	int known = !strcmp(host, "github.com") || !strcmp(host, "codeberg.org") ||
		    !strcmp(host, "git.sr.ht") || strstr(path, "/-/") != NULL;
	if (!known)
		return -1;
	char seg[1][160];
	if (path_segments(path, seg, 1) != 1)
		return -1;
	kb_strlcpy(org, seg[0], cap);
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

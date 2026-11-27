/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the release index — where every file of an update is, and its hash
 * ---------------------------------
 *
 * A PUBLISHED UPDATE IS A SIGNED INDEX AND THE FILES IT NAMES. The binhost
 * (PACKAGES, every package and its sidecar, the deltas, the AppStream
 * catalogue) and the slot image (its manifest, parts and deltas) are kept on
 * GitHub as flat asset names in releases of at most 1,000 assets each, so no
 * client can spell a file's URL from its name alone. `kdos-update.idx`,
 * beside the ports tree of the system release, says where each one is:
 *
 *   # kdos-update-index 1
 *   version <release>
 *   releases <URL that release names are joined to>
 *   <sha256> <size> <release> <asset> <path>
 *
 * One line per file, sorted by <path>, the name a client asks for:
 * `binhost/PACKAGES`, `binhost/swcatalog/xml/kdos-binhost.xml`,
 * `image/kdos-image.manifest`. <release> is the GitHub release holding the
 * asset (`binhost-<version>-NN`, `image-<version>`), or `-` for a file beside
 * the index itself, which is how a directory a build wrote — build/binhost,
 * a release image directory, a stick — is laid out; `releases` is then
 * absent. Written by `ports/publish --release` for a release and by the
 * producers (010_binhost.sh, `kdos image make`) for their own directory.
 *
 * NOTHING IS FETCHED BY A NAME THE INDEX DOES NOT GIVE, AND NOTHING IT GIVES
 * IS KEPT WITHOUT ITS HASH. The index is verified against the host keyring
 * before one line is read; every file is downloaded to a temporary name,
 * checked for the size and SHA-256 its line names and only then renamed into
 * place. An unsigned index, one no trusted key signed, a line that is not
 * the shape it must be, and a file of other bytes are each refused.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "kdos-tools.h"
#include "kbase.h"
#include "ksig.h"

#define RI_MARK "# kdos-update-index 1"

typedef struct {
	char sha[65];
	long long size;
	char release[48];
	char asset[256];
	char path[256];
} RiEnt;

struct KdtRelIdx {
	RiEnt *ent;
	int n;
	char version[48];
	char releases[512];
	char origin[1024];	/* the place the index was read from */
	char who[KSIG_ID_HEX];
	char keyname[64];
};

/* The alphabet a GitHub tag or asset of ours is spelled with, no leading
 * dot and no leading dash. */
static int ri_chars_ok(const char *s, size_t max)
{
	size_t n = strlen(s);
	if (!n || n >= max || *s == '.' || *s == '-')
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '.' || *c == '+' ||
		      *c == '-' || *c == '_' || *c == '~'))
			return 0;
	return 1;
}

/* A release: a tag, or `-` for a file beside the index. */
static int ri_release_ok(const char *s, size_t max)
{
	return !strcmp(s, "-") || ri_chars_ok(s, max);
}

/* An asset name is flat: no `/`, nothing a URL or a shell would read
 * twice. */
static int ri_asset_ok(const char *s)
{
	return ri_chars_ok(s, 256);
}

/* A path is `/`-separated names of the asset alphabet, none empty, none
 * starting with a dot, so `..` cannot be spelled. */
static int ri_path_ok(const char *s)
{
	size_t n = strlen(s);
	if (!n || n >= 256 || *s == '/' || s[n - 1] == '/')
		return 0;
	for (const char *seg = s; *seg;) {
		size_t l = strcspn(seg, "/");
		char b[256];
		memcpy(b, seg, l);
		b[l] = 0;
		if (!ri_asset_ok(b))
			return 0;
		seg += l + (seg[l] == '/');
	}
	return 1;
}

static int ri_hex64(const char *s)
{
	if (strlen(s) != 64)
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f')))
			return 0;
	return 1;
}

void kdt_relidx_free(KdtRelIdx *ix)
{
	if (!ix)
		return;
	free(ix->ent);
	free(ix);
}

const char *kdt_relidx_version(const KdtRelIdx *ix) { return ix->version; }
const char *kdt_relidx_origin(const KdtRelIdx *ix) { return ix->origin; }
const char *kdt_relidx_signer(const KdtRelIdx *ix) { return ix->who; }
int kdt_relidx_count(const KdtRelIdx *ix) { return ix->n; }

/*
 * The index's text, checked line by line: one line that is not the shape it
 * must be refuses the whole index, because every field becomes part of a URL
 * or a path. NULL with the reason in `why`.
 */
KdtRelIdx *kdt_relidx_parse(const char *text, char *why, size_t wcap)
{
	KdtRelIdx *ix = kb_calloc(1, sizeof(*ix));
	int cap = 0, lineno = 0;
	const char *l = text;

	if (strncmp(text, RI_MARK "\n", sizeof(RI_MARK))) {
		snprintf(why, wcap, "not a KDOS release index");
		goto bad;
	}
	while (l && *l) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		char b[1200], f[5][520];
		int nf;

		lineno++;
		if (len >= sizeof(b)) {
			snprintf(why, wcap, "release index line %d is too long",
				 lineno);
			goto bad;
		}
		memcpy(b, l, len);
		b[len] = 0;
		l = nl ? nl + 1 : NULL;
		if (!b[0] || b[0] == '#')
			continue;
		if (!strncmp(b, "version ", 8)) {
			if (!ri_chars_ok(b + 8, sizeof(ix->version))) {
				snprintf(why, wcap, "release index line %d: a bad "
					 "version", lineno);
				goto bad;
			}
			kb_strlcpy(ix->version, b + 8, sizeof(ix->version));
			continue;
		}
		if (!strncmp(b, "releases ", 9)) {
			const char *u = b + 9;
			if (strlen(u) >= sizeof(ix->releases) ||
			    (strncmp(u, "https://", 8) && strncmp(u, "file://", 7)) ||
			    strpbrk(u, " \t\"'\\")) {
				snprintf(why, wcap, "release index line %d: a bad "
					 "releases URL", lineno);
				goto bad;
			}
			kb_strlcpy(ix->releases, u, sizeof(ix->releases));
			continue;
		}
		nf = sscanf(b, "%519s %519s %519s %519s %519s", f[0], f[1], f[2],
			    f[3], f[4]);
		char *end = NULL;
		long long sz = nf == 5 ? strtoll(f[1], &end, 10) : -1;
		if (nf != 5 || !ri_hex64(f[0]) || !end || *end || sz < 0 ||
		    !ri_release_ok(f[2], sizeof(ix->ent->release)) ||
		    !ri_asset_ok(f[3]) || !ri_path_ok(f[4])) {
			snprintf(why, wcap, "release index line %d is not an "
				 "entry", lineno);
			goto bad;
		}
		if (ix->n && strcmp(ix->ent[ix->n - 1].path, f[4]) >= 0) {
			snprintf(why, wcap, "release index line %d: out of order",
				 lineno);
			goto bad;
		}
		if (ix->n + 1 > cap) {
			cap = cap ? cap * 2 : 256;
			ix->ent = kb_realloc(ix->ent, (size_t)cap * sizeof(*ix->ent));
		}
		RiEnt *e = &ix->ent[ix->n++];
		memcpy(e->sha, f[0], 65);
		e->size = sz;
		kb_strlcpy(e->release, f[2], sizeof(e->release));
		kb_strlcpy(e->asset, f[3], sizeof(e->asset));
		kb_strlcpy(e->path, f[4], sizeof(e->path));
		if (strcmp(e->release, "-") && !ix->releases[0]) {
			snprintf(why, wcap, "release index line %d names release "
				 "%s before any `releases` line", lineno,
				 e->release);
			goto bad;
		}
	}
	if (!ix->version[0]) {
		snprintf(why, wcap, "the release index names no version");
		goto bad;
	}
	return ix;
bad:
	kdt_relidx_free(ix);
	return NULL;
}

static const RiEnt *ri_find(const KdtRelIdx *ix, const char *path)
{
	int lo = 0, hi = ix->n - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		int c = strcmp(ix->ent[mid].path, path);
		if (!c)
			return &ix->ent[mid];
		if (c < 0)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/* The URL of `path`, with its hash and size; -1 when the index lacks it. */
int kdt_relidx_url(const KdtRelIdx *ix, const char *path, char *url,
		   size_t cap, char sha[65], long long *size)
{
	const RiEnt *e = ri_find(ix, path);
	if (!e)
		return -1;
	if (!strcmp(e->release, "-"))
		snprintf(url, cap, "%s/%s", ix->origin, e->asset);
	else
		snprintf(url, cap, "%s/%s/%s", ix->releases, e->release,
			 e->asset);
	if (sha)
		memcpy(sha, e->sha, 65);
	if (size)
		*size = e->size;
	return 0;
}

/* The index's text and its signature, verified against `keys`. */
static KdtRelIdx *ri_verify(const char *ip, const char *sp, const char *keys,
			    char *why, size_t wcap)
{
	size_t len = 0;
	KsigRing ring;
	char who[KSIG_ID_HEX];
	char *data = kb_read_all(ip, &len);

	if (!data) {
		snprintf(why, wcap, "no release index at %s", ip);
		return NULL;
	}
	if (ksig_ring_load(&ring, keys) <= 0) {
		free(data);
		snprintf(why, wcap, "no trusted key in %s can verify the release "
			 "index", keys);
		return NULL;
	}
	if (!kb_path_exists(sp) ||
	    ksig_verify_file(&ring, sp, data, len, who) != 0) {
		free(data);
		snprintf(why, wcap, "the release index's signature does not "
			 "verify against %s", keys);
		return NULL;
	}
	if (memchr(data, 0, len)) {
		free(data);
		snprintf(why, wcap, "the release index is not text");
		return NULL;
	}
	KdtRelIdx *ix = kdt_relidx_parse(data, why, wcap);
	free(data);
	if (!ix)
		return NULL;
	memcpy(ix->who, who, sizeof(ix->who));
	for (int k = 0; k < ring.n; k++)
		if (!strcmp(ring.key[k].id, who))
			kb_strlcpy(ix->keyname, ring.key[k].name,
				   sizeof(ix->keyname));
	return ix;
}

/* The verified index cached in `dir` (INDEX, INDEX.sig, ORIGIN). */
KdtRelIdx *kdt_relidx_load(const char *dir, const char *keys, char *why,
			   size_t wcap)
{
	char ip[1100], sp[1100], op[1100];
	snprintf(ip, sizeof(ip), "%s/INDEX", dir);
	snprintf(sp, sizeof(sp), "%s/INDEX.sig", dir);
	snprintf(op, sizeof(op), "%s/ORIGIN", dir);
	if (!kb_path_exists(ip)) {
		snprintf(why, wcap, "no signed release index is on this machine");
		return NULL;
	}
	KdtRelIdx *ix = ri_verify(ip, sp, keys, why, wcap);
	if (!ix)
		return NULL;
	char *o = kb_read_all(op, NULL);
	if (o) {
		o[strcspn(o, "\r\n")] = 0;
		kb_strlcpy(ix->origin, o, sizeof(ix->origin));
		free(o);
	}
	return ix;
}

/* 1 when one line of a /proc/net routing table is a usable default route:
 * destination and prefix all zero, the route up (RTF_UP) and not a
 * rejecting one (RTF_REJECT), on an interface other than lo. The IPv6
 * table always holds an unreachable default on lo, which is no way out. */
static int ri_default_line(const char *dst, const char *plen, unsigned flags,
			   const char *dev)
{
	return strspn(dst, "0") == strlen(dst) && strspn(plen, "0") ==
	       strlen(plen) && (flags & 0x1) && !(flags & 0x200) &&
	       strcmp(dev, "lo");
}

int kdt_have_route(void)
{
	char line[512], dev[64], dst[64], plen[16], gw[64], mask[64];
	unsigned flags;
	int have = 0;
	FILE *f = fopen("/proc/net/route", "r");

	/* Iface Destination Gateway Flags RefCnt Use Metric Mask … */
	if (f) {
		while (!have && fgets(line, sizeof(line), f))
			if (sscanf(line, "%63s %63s %63s %x %*s %*s %*s %63s",
				   dev, dst, gw, &flags, mask) == 5)
				have = ri_default_line(dst, mask, flags, dev);
		fclose(f);
	}
	/* dest plen src splen nexthop metric refcnt use flags iface */
	f = have ? NULL : fopen("/proc/net/ipv6_route", "r");
	if (f) {
		while (!have && fgets(line, sizeof(line), f))
			if (sscanf(line, "%63s %15s %*s %*s %*s %*s %*s %*s %x "
					 "%63s", dst, plen, &flags, dev) == 4)
				have = ri_default_line(dst, plen, flags, dev);
		fclose(f);
	}
	return have;
}

/* What a curl exit status says about the network. A name that did not
 * resolve (5, 6) is OFFLINE; a connection refused (7) or a timeout (28) is
 * OFFLINE only on a machine with no default route, and otherwise a server
 * that did not answer, which no network setting changes. An answer that the
 * file is not there is ABSENT. */
static int ri_class(int rc)
{
	if (rc == 0)
		return KDT_GOT_OK;
	if (rc == 5 || rc == 6)
		return KDT_GOT_OFFLINE;
	if ((rc == 7 || rc == 28) && !kdt_have_route())
		return KDT_GOT_OFFLINE;
	return rc == 22 || rc == 37 ? KDT_GOT_ABSENT : KDT_GOT_FAILED;
}

/* `the server at <host> did not answer` into `why` for a FAILED refusal or
 * timeout, with the host and port of `url` and no scheme or path, or `the
 * download from <host> stalled` for a timeout from a server that had sent
 * its answer (`answered`: an HTTP status arrived); 0 when curl's `rc` is
 * not one of those, and `why` is left alone. */
static int ri_unanswered(const char *url, int rc, int answered, char *why,
			 size_t wcap)
{
	if ((rc != 7 && rc != 28) || ri_class(rc) != KDT_GOT_FAILED)
		return 0;
	const char *h = strstr(url, "://");
	h = h ? h + 3 : url;
	size_t n = strcspn(h, "/?#");
	const char *at = memchr(h, '@', n);
	if (at) {
		n -= (size_t)(at + 1 - h);
		h = at + 1;
	}
	if (rc == 28 && answered)
		snprintf(why, wcap, "the download from %.*s stalled: less "
			 "than " KDT_STALL_BYTES " bytes a second arrived for "
			 KDT_STALL_SECS " seconds", (int)n, h);
	else
		snprintf(why, wcap, "the server at %.*s did not answer",
			 (int)n, h);
	return 1;
}

/* curl into `out`, continuing a partial `out` when `resume` is set (`-C -`,
 * a Range request from its size). The exit status in `*rc`, and in
 * `*answered` whether the server sent an HTTP status, which is what tells a
 * stalled transfer from a server that never answered when both end in a
 * timeout; `out` is left as curl left it. */
static int ri_curl(const char *url, const char *out, int resume, int *rc,
		   int *answered)
{
	KbArgv a = {0};
	char code[64];
	kb_argv_add(&a, "curl");
	kb_argv_add(&a, "-fsSL");
	kb_argv_add(&a, "--proto");
	kb_argv_add(&a, "=https,file");
	kb_argv_add(&a, "--proto-redir");
	kb_argv_add(&a, "=https");
	kb_argv_add(&a, "--retry");
	kb_argv_add(&a, "2");
	kb_argv_add(&a, "--connect-timeout");
	kb_argv_add(&a, "30");
	kb_argv_add(&a, "--speed-limit");
	kb_argv_add(&a, KDT_STALL_BYTES);
	kb_argv_add(&a, "--speed-time");
	kb_argv_add(&a, KDT_STALL_SECS);
	kb_argv_add(&a, "-w");
	kb_argv_add(&a, "%{http_code}");
	if (resume) {
		kb_argv_add(&a, "-C");
		kb_argv_add(&a, "-");
	}
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	kb_argv_add(&a, "--");
	kb_argv_add(&a, url);
	kb_argv_end(&a);
	*rc = kb_run_capture(&a, code, sizeof(code));
	/* A retried transfer writes one status per try; the last is the one
	 * that ended it. */
	size_t cl = strlen(code);
	*answered = cl >= 3 && atoi(code + cl - 3) > 0;
	return ri_class(*rc);
}

/* curl, HTTPS or a local file:// path and nothing else, into `out`, which
 * a failure removes: KDT_GOT_*, as ri_class() reads the exit status. */
int kdt_relidx_curl(const char *url, const char *out)
{
	int rc, ans;
	int got = ri_curl(url, out, 0, &rc, &ans);
	if (got != KDT_GOT_OK)
		unlink(out);
	return got;
}

/*
 * The signed index at `place` (a URL or a file:// directory) into `dir`: the
 * pair is downloaded beside the cached one, verified against `keys`, and
 * only then renamed over it, with `place` written to ORIGIN. NULL with the
 * reason in `why` and in `how` (KDT_GOT_*); a refused index leaves the
 * cached one as it was.
 */
KdtRelIdx *kdt_relidx_fetch(const char *place, const char *dir,
			    const char *keys, int *how, char *why, size_t wcap)
{
	char url[1300], ip[1100], sp[1100], in[1110], sn[1110], op[1100];

	*how = KDT_GOT_FAILED;
	if (kb_mkdir_p(dir) != 0) {
		snprintf(why, wcap, "cannot create %s", dir);
		return NULL;
	}
	snprintf(ip, sizeof(ip), "%s/INDEX", dir);
	snprintf(sp, sizeof(sp), "%s/INDEX.sig", dir);
	snprintf(in, sizeof(in), "%s.new", ip);
	snprintf(sn, sizeof(sn), "%s.new", sp);
	snprintf(op, sizeof(op), "%s/ORIGIN", dir);
	snprintf(url, sizeof(url), "%s/" KDT_RELIDX_FILE ".sig", place);
	int rc, ans;
	int got = ri_curl(url, sn, 0, &rc, &ans);
	if (got == KDT_GOT_OK) {
		snprintf(url, sizeof(url), "%s/" KDT_RELIDX_FILE, place);
		got = ri_curl(url, in, 0, &rc, &ans);
	}
	if (got != KDT_GOT_OK) {
		unlink(in);
		unlink(sn);
		*how = got;
		if (!ri_unanswered(url, rc, ans, why, wcap))
			snprintf(why, wcap, got == KDT_GOT_ABSENT
				 ? "no release index is published at %s"
				 : got == KDT_GOT_OFFLINE
				 ? "the network could not be reached for the "
				   "release index at %s"
				 : "the release index at %s could not be "
				   "downloaded", place);
		return NULL;
	}
	KdtRelIdx *ix = ri_verify(in, sn, keys, why, wcap);
	if (!ix) {
		unlink(in);
		unlink(sn);
		*how = KDT_GOT_UNVERIFIED;
		return NULL;
	}
	if (rename(in, ip) != 0 || rename(sn, sp) != 0) {
		snprintf(why, wcap, "cannot keep the release index in %s", dir);
		kdt_relidx_free(ix);
		return NULL;
	}
	KbBuf b = {0};
	kb_buf_printf(&b, "%s\n", place);
	kb_write_file_atomic(op, b.p);
	kb_buf_free(&b);
	kb_strlcpy(ix->origin, place, sizeof(ix->origin));
	return ix;
}

/* 1 when `file` is `size` bytes with the SHA-256 `sha`. */
static int ri_matches(const char *file, const char *sha, long long size)
{
	struct stat st;
	char got[65] = "";
	return stat(file, &st) == 0 && S_ISREG(st.st_mode) &&
	       (long long)st.st_size == size && kb_sha256_file(file, got) == 0 &&
	       !strcmp(got, sha);
}

/* The size of `file`, or 0 when there is none. */
static long long ri_size(const char *file)
{
	struct stat st;
	return stat(file, &st) == 0 && S_ISREG(st.st_mode) ? st.st_size : 0;
}

/*
 * `path` from where the index says it is, into `out`: downloaded as
 * `<out>.part`, checked for the size and hash its line names, then renamed.
 * KDT_GOT_OK, or the failure with the reason in `why`: ABSENT when the index
 * names no such file, UNVERIFIED when what arrived is other bytes.
 *
 * With `keep`, A CUT DOWNLOAD IS CONTINUED, NOT STARTED AGAIN. A failed curl
 * leaves the bytes that arrived in `<out>.part`, `*cut` says how many, and
 * the next take of the same path asks for the rest with a Range request
 * (`-C -`) when that part is smaller than the index's size; a server that
 * cannot serve a range (curl exit 33) has the part discarded and the file
 * downloaded whole. The hash check covers the joined file, so a part from
 * other bytes is refused there and deleted, never kept for another try. A
 * part of the full size or larger is no prefix of the file and is discarded
 * first. Only a caller whose `out` is a path named for one version of one
 * file (the package cache) may keep: a part under a fixed name would be
 * resumed with another release's bytes, and one under a staging directory
 * the next run removes would never be resumed. Without `keep`, a part left
 * at `<out>.part` is discarded before the download and on every failure.
 *
 * `why` for a cut download states the cut and no next step: the caller
 * knows whether running a command again continues it.
 */
static int ri_take(const KdtRelIdx *ix, const char *path, const char *out,
		   int keep, long long *cutp, char *why, size_t wcap)
{
	char url[1600], sha[65], part[1100];
	long long size;
	int rc, ans;

	if (kdt_relidx_url(ix, path, url, sizeof(url), sha, &size) != 0) {
		snprintf(why, wcap, "the signed release index names no %s", path);
		return KDT_GOT_ABSENT;
	}
	if (cutp)
		*cutp = 0;
	snprintf(part, sizeof(part), "%s.part", out);
	if (!keep)
		unlink(part);
	long long had = ri_size(part);
	if (had >= size) {
		unlink(part);
		had = 0;
	}
	int got = ri_curl(url, part, had > 0, &rc, &ans);
	if (got != KDT_GOT_OK && had > 0 && rc == 33) {
		unlink(part);
		got = ri_curl(url, part, 0, &rc, &ans);
	}
	if (got != KDT_GOT_OK) {
		/* A full disk fails curl the way a broken link does; the room
		 * left beside `out` tells the two apart. */
		struct statvfs vs;
		char dir[1100];
		kb_strlcpy(dir, out, sizeof(dir));
		char *sl = strrchr(dir, '/');
		if (sl)
			*sl = 0;
		long long cut = ri_size(part);
		if (got != KDT_GOT_OFFLINE && sl &&
		    statvfs(dir[0] ? dir : "/", &vs) == 0 &&
		    (long long)(vs.f_bavail * vs.f_frsize) < size - cut) {
			unlink(part);
			snprintf(why, wcap, "no room for %s: it is %lld bytes "
				 "and %s has %lld free", path, size,
				 dir[0] ? dir : "/",
				 (long long)(vs.f_bavail * vs.f_frsize));
			return KDT_GOT_FAILED;
		}
		if (!keep || got == KDT_GOT_ABSENT || cut <= 0 ||
		    cut >= size) {
			unlink(part);
			cut = 0;
		}
		if (cutp)
			*cutp = cut;
		if (cut > 0 && rc == 28)
			snprintf(why, wcap, "%s stalled at %lld of %lld bytes: "
				 "less than " KDT_STALL_BYTES " bytes a second "
				 "arrived for " KDT_STALL_SECS " seconds", out,
				 cut, size);
		else if (cut > 0)
			snprintf(why, wcap, "%s was cut off at %lld of %lld "
				 "bytes (curl exit %d)", out, cut, size, rc);
		else if (!ri_unanswered(url, rc, ans, why, wcap))
			snprintf(why, wcap, got == KDT_GOT_OFFLINE
				 ? "the network could not be reached for %s"
				 : "%s could not be downloaded", path);
		/* A file the index names and the release lacks is a broken
		 * release, not one that publishes nothing; and bytes that
		 * arrived came over a network that was there. */
		return got == KDT_GOT_ABSENT ||
		       (cut > 0 && got == KDT_GOT_OFFLINE) ? KDT_GOT_FAILED
							   : got;
	}
	if (!ri_matches(part, sha, size)) {
		unlink(part);
		snprintf(why, wcap, "%s does not match the signed release index",
			 path);
		return KDT_GOT_UNVERIFIED;
	}
	if (rename(part, out) != 0) {
		unlink(part);
		snprintf(why, wcap, "cannot keep %s: %s", out, strerror(errno));
		return KDT_GOT_FAILED;
	}
	return KDT_GOT_OK;
}

int kdt_relidx_take(const KdtRelIdx *ix, const char *path, const char *out,
		    char *why, size_t wcap)
{
	return ri_take(ix, path, out, 0, NULL, why, wcap);
}

int kdt_relidx_take_resume(const KdtRelIdx *ix, const char *path,
			   const char *out, long long *cut, char *why,
			   size_t wcap)
{
	return ri_take(ix, path, out, 1, cut, why, wcap);
}

/* 1 when the local `file` is what the index names for `path`. */
int kdt_relidx_holds(const KdtRelIdx *ix, const char *path, const char *file)
{
	const RiEnt *e = ri_find(ix, path);
	return e && ri_matches(file, e->sha, e->size);
}

/*
 * A producer's own index: `kdos-update.idx` in `dir`, one line for each of
 * `files` (names in `dir`) at `<lane>/<name>`, every asset beside the index,
 * signed with `key` when one is given. 0, or -1 with the reason in `why`.
 */
static int ri_cmp(const void *a, const void *b)
{
	return strcmp(((const RiEnt *)a)->path, ((const RiEnt *)b)->path);
}

int kdt_relidx_write(const char *dir, const char *lane, const char *version,
		     char *const *files, int n, const char *key, char *why,
		     size_t wcap)
{
	RiEnt *e = kb_calloc((size_t)n + 1, sizeof(*e));
	KbBuf b = {0};
	char ip[1100], sp[1110];
	int rc = -1;

	for (int i = 0; i < n; i++) {
		char p[1400];
		struct stat st;
		snprintf(p, sizeof(p), "%s/%s", dir, files[i]);
		snprintf(e[i].path, sizeof(e[i].path), "%s/%s", lane, files[i]);
		kb_strlcpy(e[i].asset, files[i], sizeof(e[i].asset));
		kb_strlcpy(e[i].release, "-", sizeof(e[i].release));
		if (!ri_asset_ok(files[i]) || !ri_path_ok(e[i].path) ||
		    stat(p, &st) != 0 || !S_ISREG(st.st_mode) ||
		    kb_sha256_file(p, e[i].sha) != 0) {
			snprintf(why, wcap, "%s cannot be indexed", p);
			goto out;
		}
		e[i].size = (long long)st.st_size;
	}
	qsort(e, (size_t)n, sizeof(*e), ri_cmp);
	kb_buf_printf(&b, RI_MARK "\nversion %s\n", version);
	for (int i = 0; i < n; i++)
		kb_buf_printf(&b, "%s %lld - %s %s\n", e[i].sha, e[i].size,
			      e[i].asset, e[i].path);
	snprintf(ip, sizeof(ip), "%s/" KDT_RELIDX_FILE, dir);
	snprintf(sp, sizeof(sp), "%s.sig", ip);
	if (kb_write_file_atomic(ip, b.p) != 0) {
		snprintf(why, wcap, "cannot write %s", ip);
		goto out;
	}
	unlink(sp);
	if (key) {
		uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
		if (ksig_read_secret(key, seed, pub) != 0 ||
		    ksig_sig_append(sp, seed, pub, b.p, b.n) != 0) {
			snprintf(why, wcap, "cannot sign %s with %s", ip, key);
			goto out;
		}
	}
	rc = 0;
out:
	kb_buf_free(&b);
	free(e);
	return rc;
}

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

/* curl, HTTPS or a local file:// path and nothing else, into `out`. One of
 * KDT_GOT_*: a name that did not resolve, a connection refused or a timeout
 * is OFFLINE; an answer that the file is not there is ABSENT. */
int kdt_relidx_curl(const char *url, const char *out)
{
	KbArgv a = {0};
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
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, out);
	kb_argv_add(&a, "--");
	kb_argv_add(&a, url);
	kb_argv_end(&a);
	int rc = kb_run(&a);
	if (rc == 0)
		return KDT_GOT_OK;
	unlink(out);
	if (rc == 5 || rc == 6 || rc == 7 || rc == 28)
		return KDT_GOT_OFFLINE;
	return rc == 22 || rc == 37 ? KDT_GOT_ABSENT : KDT_GOT_FAILED;
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
	int got = kdt_relidx_curl(url, sn);
	if (got == KDT_GOT_OK) {
		snprintf(url, sizeof(url), "%s/" KDT_RELIDX_FILE, place);
		got = kdt_relidx_curl(url, in);
	}
	if (got != KDT_GOT_OK) {
		unlink(in);
		unlink(sn);
		*how = got;
		snprintf(why, wcap, got == KDT_GOT_ABSENT
			 ? "no release index is published at %s"
			 : got == KDT_GOT_OFFLINE
			 ? "the network could not be reached for the release "
			   "index at %s"
			 : "the release index at %s could not be downloaded",
			 place);
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

/*
 * `path` from where the index says it is, into `out`: downloaded as
 * `<out>.part`, checked for the size and hash its line names, then renamed.
 * KDT_GOT_OK, or the failure with the reason in `why`: ABSENT when the index
 * names no such file, UNVERIFIED when what arrived is other bytes.
 */
int kdt_relidx_take(const KdtRelIdx *ix, const char *path, const char *out,
		    char *why, size_t wcap)
{
	char url[1600], sha[65], part[1100];
	long long size;

	if (kdt_relidx_url(ix, path, url, sizeof(url), sha, &size) != 0) {
		snprintf(why, wcap, "the signed release index names no %s", path);
		return KDT_GOT_ABSENT;
	}
	snprintf(part, sizeof(part), "%s.part", out);
	int got = kdt_relidx_curl(url, part);
	if (got != KDT_GOT_OK) {
		snprintf(why, wcap, got == KDT_GOT_OFFLINE
			 ? "the network could not be reached for %s"
			 : "%s could not be downloaded", path);
		/* A file the index names and the release lacks is a broken
		 * release, not one that publishes nothing. */
		return got == KDT_GOT_ABSENT ? KDT_GOT_FAILED : got;
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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos image — a whole root slot as one signed release, and its deltas
 *
 *   kdos image make ROOT OUTDIR --version V [--key FILE] [--prev DIR
 *                   [--prev-root DIR]]... [--exclude PATH]...
 *   kdos image verify DIR|URL [--keys DIR]
 *   kdos image unpack DIR|URL TARGET [--seed ROOT] [--keys DIR]
 *   kdos image carry RUNNING SLOT
 *
 * One file holds both halves, the release step that makes an image and the
 * machine that writes one into its inactive slot, so the format has one
 * reader and one writer and they cannot drift.
 *
 * ── The format ──
 *
 * An image is a MANIFEST and the objects it names. The manifest is text,
 * signed with libksig like the binhost index, and lists every directory,
 * regular file, symbolic link and hard link of the root with its mode,
 * owner, modification time and, for a file, its size and SHA-256. An object
 * is the content of a regular file, named by that hash.
 *
 * Objects travel in BUNDLES: one zstd stream of records, each a header line
 * and the bytes it announces. The full bundle holds every object once and is
 * cut into parts small enough for a release asset. A DELTA bundle, one per
 * earlier version, holds the objects that version lacks — whole, or as a
 * `zstd --patch-from` difference against the file the same path held in that
 * version — and every object under etc/ and var/, which a running machine
 * has usually edited (make_delta).
 *
 * NOTHING IN A BUNDLE IS TRUSTED. Every object, whole or patched, is hashed
 * after it is written and kept only when the hash is the one the signed
 * manifest gives; a bundle that was tampered with costs a download, never a
 * file. So, as with kpkg's deltas, a bundle carries no signature of its own.
 *
 * EVERY FILE IS FOUND THROUGH THE RELEASE INDEX (relidx.c). `make` writes
 * `kdos-update.idx` beside the manifest, signed with the same key, naming the
 * manifest, its signature, every part and every delta at `image/<name>`; a
 * reader fetches the index from a directory or URL first and takes the
 * manifest only when the index's hash for it matches, and every bundle by the
 * URL the index gives. `ports/publish --release` writes the published index,
 * which puts the same files in the release `image-<version>`.
 *
 * ── Why not one patch over the whole slot ──
 *
 * `zstd --patch-from` needs a window covering the whole reference, and its
 * window stops at 2 GiB; a slot with Plasma and the Gear applications is
 * several times that. Per-file differences stay inside the window, and the
 * manifest says which files changed at all, so most of a slot is copied
 * from the root the machine is running and only what changed is downloaded.
 *
 * ── Writing a slot ──
 *
 * The target is SYNCHRONISED, not formatted: a file the target already holds
 * with the right hash stays, one the running root (the SEED) holds is
 * copied, and only the rest comes from a bundle. Entries the manifest does
 * not name are removed, except `lost+found` and anything on another
 * filesystem. The slot keeps its filesystem and its UUID, whichever of the
 * four filesystems the installer gave it, and nothing here knows how to make
 * one.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>

#include "kdos-tools.h"
#include "kpkg.h"
#include "ksig.h"

#define IM_MAGIC	"kdos-image 1"
#define IM_STREAM	"kdos-objects 1"
#define IM_MANIFEST	"kdos-image.manifest"
#define IM_INSTALLED	"/var/lib/kdos/image/MANIFEST"
/* A release asset may be 2 GiB; a part stays under it. */
#define IM_PART_BYTES	(1900LL << 20)
/* Patch bases larger than this are shipped whole: zstd's window must cover
 * the base, and the client pays that window in memory. */
#define IM_PATCH_MAX	(1LL << 30)
#define IM_EMPTY_SHA	"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

/* ── the manifest ──────────────────────────────────────────────────────── */

typedef struct {
	char type;		/* d f l h */
	unsigned mode, uid, gid;
	long long mtime, size;
	char sha[65];
	char *path;
	char *target;		/* l: the link's text; h: the path it shares */
	dev_t dev;		/* make only: the inode, for hard links */
	ino_t ino;
	nlink_t nlink;
} ImEnt;

typedef struct {
	char file[200];
	long long size;
	char sha[65];
} ImPart;

typedef struct {
	char from[64];
	char file[200];
	long long size;
	char sha[65];
	long long objects;
} ImDelta;

typedef struct {
	int *slot;
	size_t cap;
} ImMap;

struct KdtImage {
	char version[64], arch[32], tree[64];
	long long time, objects, obytes;
	ImPart *part;
	int npart;
	ImDelta *delta;
	int ndelta;
	ImEnt *ent;
	int nent, ecap;
	char *raw;		/* the manifest's bytes, as verified */
	size_t rawlen;
	char *sig;		/* its signature file, when it had one */
	char who[KSIG_ID_HEX], keyname[64];
	ImMap bypath, bysha;
	int *same;		/* next entry with the same object, or -1 */
};

/* A byte a manifest line may hold as itself: printable, no space, and not the
 * two that begin an escape. Everything else is %XX, so a path is one field
 * whatever it contains. */
static int enc_plain(unsigned char c)
{
	return c > 0x20 && c < 0x7f && c != '%' && c != '\\';
}

static void enc(KbBuf *b, const char *s)
{
	char hex[4];
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		if (enc_plain(*p)) {
			kb_buf_add(b, (const char *)p, 1);
		} else {
			snprintf(hex, sizeof(hex), "%%%02X", *p);
			kb_buf_add(b, hex, 3);
		}
	}
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/* NULL on a malformed escape, a raw byte that should have been escaped, or
 * an escaped NUL, which no path or link holds. */
static char *dec(const char *s)
{
	size_t n = strlen(s);
	char *out = kb_calloc(n + 1, 1), *o = out;
	for (const char *p = s; *p; p++) {
		if (*p == '%') {
			int h = hexval(p[1]), l = h < 0 ? -1 : hexval(p[2]);
			if (h < 0 || l < 0 || (h == 0 && l == 0)) {
				free(out);
				return NULL;
			}
			*o++ = (char)(h * 16 + l);
			p += 2;
		} else if (!enc_plain((unsigned char)*p)) {
			free(out);
			return NULL;
		} else {
			*o++ = *p;
		}
	}
	*o = 0;
	return out;
}

/* A path relative to the root, as the manifest names one: `.` for the root
 * itself, otherwise components that are none of empty, `.` and `..`. */
static int path_ok(const char *p)
{
	if (!strcmp(p, "."))
		return 1;
	if (!*p || *p == '/')
		return 0;
	for (const char *c = p; *c;) {
		size_t n = strcspn(c, "/");
		if (n == 0 || (n == 1 && c[0] == '.') ||
		    (n == 2 && c[0] == '.' && c[1] == '.'))
			return 0;
		c += n;
		if (*c == '/')
			c++;
		if (!*c && c[-1] == '/')
			return 0;
	}
	return 1;
}

static int sha_ok(const char *s)
{
	if (strlen(s) != 64)
		return 0;
	for (const char *c = s; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f')))
			return 0;
	return 1;
}

/* A file name a manifest gives for a part or a delta: it is joined to a URL. */
static int fname_ok(const char *f)
{
	if (!*f || *f == '.' || strlen(f) >= 200)
		return 0;
	for (const char *c = f; *c; c++)
		if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
		      (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' ||
		      *c == '_' || *c == '+'))
			return 0;
	return 1;
}

static int ver_ok(const char *v)
{
	size_t n = strlen(v);
	if (!n || n > 40)
		return 0;
	for (const char *c = v; *c; c++)
		if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
		      (*c >= 'A' && *c <= 'Z') || *c == '.' || *c == '+' ||
		      *c == '-' || *c == '_' || *c == '~'))
			return 0;
	return 1;
}

/* ── an index of entries by path and by object ─────────────────────────── */

static uint32_t fnv(const char *s)
{
	uint32_t h = 2166136261u;
	for (; *s; s++)
		h = (h ^ (unsigned char)*s) * 16777619u;
	return h;
}

static const char *map_key(const KdtImage *m, int i, int by_sha)
{
	return by_sha ? m->ent[i].sha : m->ent[i].path;
}

static void map_build(KdtImage *m, ImMap *mp, int by_sha)
{
	size_t cap = 64;
	while (cap < (size_t)m->nent * 2 + 2)
		cap <<= 1;
	mp->cap = cap;
	mp->slot = kb_calloc(cap, sizeof(int));
	for (size_t i = 0; i < cap; i++)
		mp->slot[i] = -1;
	for (int i = 0; i < m->nent; i++) {
		if (by_sha && m->ent[i].type != 'f')
			continue;
		const char *k = map_key(m, i, by_sha);
		size_t s = fnv(k) & (cap - 1);
		for (; mp->slot[s] >= 0; s = (s + 1) & (cap - 1))
			if (!strcmp(map_key(m, mp->slot[s], by_sha), k))
				break;
		if (mp->slot[s] < 0)
			mp->slot[s] = i;
		else if (by_sha) {
			/* Chain the later entry behind the first. */
			int j = mp->slot[s];
			while (m->same[j] >= 0)
				j = m->same[j];
			m->same[j] = i;
		}
	}
}

static int map_get(const KdtImage *m, const ImMap *mp, const char *k,
		   int by_sha)
{
	if (!mp->slot)
		return -1;
	for (size_t s = fnv(k) & (mp->cap - 1); mp->slot[s] >= 0;
	     s = (s + 1) & (mp->cap - 1))
		if (!strcmp(map_key(m, mp->slot[s], by_sha), k))
			return mp->slot[s];
	return -1;
}

void kdt_image_free(KdtImage *m)
{
	if (!m)
		return;
	for (int i = 0; i < m->nent; i++) {
		free(m->ent[i].path);
		free(m->ent[i].target);
	}
	free(m->ent);
	free(m->part);
	free(m->delta);
	free(m->raw);
	free(m->sig);
	free(m->bypath.slot);
	free(m->bysha.slot);
	free(m->same);
	free(m);
}

const char *kdt_image_version(const KdtImage *m) { return m->version; }
const char *kdt_image_arch(const KdtImage *m) { return m->arch; }
const char *kdt_image_tree(const KdtImage *m) { return m->tree; }
long long kdt_image_time(const KdtImage *m) { return m->time; }
const char *kdt_image_signer(const KdtImage *m) { return m->who; }
const char *kdt_image_signer_name(const KdtImage *m) { return m->keyname; }

/* What a machine running `from` downloads: its delta when the manifest has
 * one, otherwise every part of the full bundle. */
long long kdt_image_download_size(const KdtImage *m, const char *from)
{
	long long sz = 0;
	for (int i = 0; from && *from && i < m->ndelta; i++)
		if (!strcmp(m->delta[i].from, from))
			return m->delta[i].size;
	for (int i = 0; i < m->npart; i++)
		sz += m->part[i].size;
	return sz;
}

static ImEnt *ent_new(KdtImage *m)
{
	if (m->nent == m->ecap) {
		m->ecap = m->ecap ? m->ecap * 2 : 1024;
		m->ent = kb_realloc(m->ent, (size_t)m->ecap * sizeof(*m->ent));
	}
	ImEnt *e = &m->ent[m->nent++];
	memset(e, 0, sizeof(*e));
	return e;
}

/* Split `line` on single spaces into at most `max` fields. */
static int fields(char *line, char **f, int max)
{
	int n = 0;
	for (char *p = line; p && *p && n < max;) {
		f[n++] = p;
		p = strchr(p, ' ');
		if (p)
			*p++ = 0;
	}
	return n;
}

static int num_ok(const char *s, int octal)
{
	if (!*s)
		return 0;
	for (const char *c = s; *c; c++)
		if (*c < '0' || *c > (octal ? '7' : '9'))
			return 0;
	return 1;
}

#define BAD(...) do { snprintf(why, wcap, __VA_ARGS__); goto bad; } while (0)

/*
 * The manifest's text into `m`, checked line by line: a field that is not
 * the shape it must be refuses the whole manifest, because every one of
 * them becomes a path, a mode or an owner on the target.
 *
 * The ORDER is part of the format: `.` first, then every path in strictly
 * increasing byte order, which puts each directory before what it holds and
 * rules out a path named twice. Every entry's parent must be a directory
 * entry, and a hard link must name a regular file listed before it, so a
 * write into the target never goes through a symbolic link.
 */
static int im_parse(KdtImage *m, char *why, size_t wcap)
{
	char *data = kb_calloc(m->rawlen + 1, 1);
	int in_body = 0, lineno = 0;
	char *f[9];

	memcpy(data, m->raw, m->rawlen);
	if (strncmp(data, IM_MAGIC "\n", sizeof(IM_MAGIC))) {
		free(data);
		snprintf(why, wcap, "not a KDOS image manifest");
		return -1;
	}
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		lineno++;
		if (lineno == 1)
			goto next;
		if (!*l) {
			in_body = 1;
			goto next;
		}
		if (!in_body) {
			int n = fields(l, f, 7);
			if (!strcmp(f[0], "version") && n == 2 && ver_ok(f[1]))
				kb_strlcpy(m->version, f[1], sizeof(m->version));
			else if (!strcmp(f[0], "arch") && n == 2)
				kb_strlcpy(m->arch, f[1], sizeof(m->arch));
			else if (!strcmp(f[0], "tree") && n == 2)
				kb_strlcpy(m->tree, f[1], sizeof(m->tree));
			else if (!strcmp(f[0], "time") && n == 2 &&
				 num_ok(f[1], 0))
				m->time = atoll(f[1]);
			else if (!strcmp(f[0], "objects") && n == 3 &&
				 num_ok(f[1], 0) && num_ok(f[2], 0)) {
				m->objects = atoll(f[1]);
				m->obytes = atoll(f[2]);
			} else if (!strcmp(f[0], "part") && n == 4 &&
				   fname_ok(f[1]) && num_ok(f[2], 0) &&
				   sha_ok(f[3])) {
				m->part = kb_realloc(m->part, (size_t)(m->npart + 1)
						     * sizeof(*m->part));
				ImPart *p = &m->part[m->npart++];
				kb_strlcpy(p->file, f[1], sizeof(p->file));
				p->size = atoll(f[2]);
				kb_strlcpy(p->sha, f[3], sizeof(p->sha));
			} else if (!strcmp(f[0], "delta") && n == 6 &&
				   ver_ok(f[1]) && fname_ok(f[2]) &&
				   num_ok(f[3], 0) && sha_ok(f[4]) &&
				   num_ok(f[5], 0)) {
				m->delta = kb_realloc(m->delta, (size_t)
						      (m->ndelta + 1) *
						      sizeof(*m->delta));
				ImDelta *d = &m->delta[m->ndelta++];
				kb_strlcpy(d->from, f[1], sizeof(d->from));
				kb_strlcpy(d->file, f[2], sizeof(d->file));
				d->size = atoll(f[3]);
				kb_strlcpy(d->sha, f[4], sizeof(d->sha));
				d->objects = atoll(f[5]);
			} else {
				BAD("manifest line %d is not a header line",
				    lineno);
			}
			goto next;
		}
		{
			int n = fields(l, f, 9);
			ImEnt *e;
			const char *p = NULL;
			if (n < 2 || f[0][1])
				BAD("manifest line %d is not an entry", lineno);
			e = ent_new(m);
			e->type = f[0][0];
			switch (e->type) {
			case 'd':
				if (n != 6 || !num_ok(f[1], 1) ||
				    !num_ok(f[2], 0) || !num_ok(f[3], 0) ||
				    !num_ok(f[4], 0))
					BAD("manifest line %d: a bad directory",
					    lineno);
				p = f[5];
				break;
			case 'f':
				if (n != 8 || !num_ok(f[1], 1) ||
				    !num_ok(f[2], 0) || !num_ok(f[3], 0) ||
				    !num_ok(f[4], 0) || !num_ok(f[5], 0) ||
				    !sha_ok(f[6]))
					BAD("manifest line %d: a bad file",
					    lineno);
				e->size = atoll(f[5]);
				kb_strlcpy(e->sha, f[6], sizeof(e->sha));
				p = f[7];
				break;
			case 'l':
				if (n != 6 || !num_ok(f[1], 0) ||
				    !num_ok(f[2], 0) || !num_ok(f[3], 0) ||
				    !(e->target = dec(f[4])) || !*e->target)
					BAD("manifest line %d: a bad link",
					    lineno);
				e->uid = (unsigned)atol(f[1]);
				e->gid = (unsigned)atol(f[2]);
				e->mtime = atoll(f[3]);
				p = f[5];
				break;
			case 'h':
				if (n != 3 || !(e->target = dec(f[2])) ||
				    !path_ok(e->target))
					BAD("manifest line %d: a bad hard link",
					    lineno);
				p = f[1];
				break;
			default:
				BAD("manifest line %d: unknown entry type %c",
				    lineno, e->type);
			}
			if (e->type == 'd' || e->type == 'f') {
				e->mode = (unsigned)strtoul(f[1], NULL, 8) & 07777;
				e->uid = (unsigned)atol(f[2]);
				e->gid = (unsigned)atol(f[3]);
				e->mtime = atoll(f[4]);
			}
			if (!(e->path = dec(p)) || !path_ok(e->path))
				BAD("manifest line %d: a bad path", lineno);
			if (m->nent == 1 ? strcmp(e->path, ".") || e->type != 'd'
					 : !strcmp(e->path, ".") ||
					   (m->nent > 2 &&
					    strcmp(m->ent[m->nent - 2].path,
						   e->path) >= 0))
				BAD("manifest line %d: out of order", lineno);
		}
next:
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	data = NULL;
	if (!m->version[0] || !m->arch[0] || !m->nent)
		BAD("the manifest names no version, architecture or root");

	m->same = kb_calloc((size_t)m->nent + 1, sizeof(int));
	for (int i = 0; i < m->nent; i++)
		m->same[i] = -1;
	map_build(m, &m->bypath, 0);
	map_build(m, &m->bysha, 1);
	for (int i = 1; i < m->nent; i++) {
		ImEnt *e = &m->ent[i];
		char parent[4096];
		const char *sl = strrchr(e->path, '/');
		if (sl)
			snprintf(parent, sizeof(parent), "%.*s",
				 (int)(sl - e->path), e->path);
		else
			kb_strlcpy(parent, ".", sizeof(parent));
		int pi = map_get(m, &m->bypath, parent, 0);
		if (pi < 0 || m->ent[pi].type != 'd')
			BAD("%s has no directory entry for its parent",
			    e->path);
		if (e->type == 'h') {
			int t = map_get(m, &m->bypath, e->target, 0);
			if (t < 0 || t >= i || m->ent[t].type != 'f')
				BAD("hard link %s names no earlier file",
				    e->path);
		}
	}
	return 0;
bad:
	free(data);
	return -1;
}

/* The header of a manifest a root records, unverified: `kdos update` asks a
 * slot which image an acquire wrote into it. 0 when it has one. */
int kdt_image_peek(const char *path, char *ver, size_t vcap, char *tree,
		   size_t tcap, long long *t)
{
	char *d = kb_read_all(path, NULL);
	ver[0] = tree[0] = 0;
	*t = 0;
	if (!d || strncmp(d, IM_MAGIC "\n", sizeof(IM_MAGIC))) {
		free(d);
		return -1;
	}
	for (char *l = d; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!*l)
			break;
		if (!strncmp(l, "version ", 8) && ver_ok(l + 8))
			kb_strlcpy(ver, l + 8, vcap);
		else if (!strncmp(l, "tree ", 5))
			kb_strlcpy(tree, l + 5, tcap);
		else if (!strncmp(l, "time ", 5))
			*t = atoll(l + 5);
		l = nl ? nl + 1 : NULL;
	}
	free(d);
	return ver[0] ? 0 : -1;
}

/* Regular files the image holds: the `item` total a progress reader sees. */
int kdt_image_files(const KdtImage *m)
{
	int n = 0;
	for (int i = 0; i < m->nent; i++)
		n += m->ent[i].type == 'f';
	return n;
}

/* An unsigned manifest from disk, for a seed's hints and the producer's
 * previous release: everything taken from it is checked by hash again. */
static KdtImage *im_read_unverified(const char *path)
{
	char why[256];
	KdtImage *m = kb_calloc(1, sizeof(*m));
	if (!(m->raw = kb_read_all(path, &m->rawlen)) ||
	    im_parse(m, why, sizeof(why)) != 0) {
		kdt_image_free(m);
		return NULL;
	}
	return m;
}

/*
 * The manifest at `path`, VERIFIED against the keyring `keys` before one
 * line of it is parsed. NULL with the reason in `why` otherwise.
 */
KdtImage *kdt_image_load(const char *path, const char *sigpath,
			 const char *keys, char *why, size_t wcap)
{
	KsigRing ring;
	KdtImage *m = kb_calloc(1, sizeof(*m));

	why[0] = 0;
	if (!(m->raw = kb_read_all(path, &m->rawlen))) {
		snprintf(why, wcap, "no image manifest at %s", path);
		goto bad;
	}
	if (ksig_ring_load(&ring, keys) <= 0) {
		snprintf(why, wcap, "no trusted key in %s can verify the image "
			 "manifest", keys);
		goto bad;
	}
	if (ksig_verify_file(&ring, sigpath, m->raw, m->rawlen, m->who) != 0) {
		snprintf(why, wcap, "the image manifest's signature does not "
			 "verify against %s", keys);
		goto bad;
	}
	for (int k = 0; k < ring.n; k++)
		if (!strcmp(ring.key[k].id, m->who))
			kb_strlcpy(m->keyname, ring.key[k].name,
				   sizeof(m->keyname));
	m->sig = kb_read_all(sigpath, NULL);
	if (im_parse(m, why, wcap) != 0)
		goto bad;
	return m;
bad:
	kdt_image_free(m);
	return NULL;
}

/* ── processes ─────────────────────────────────────────────────────────── */

/* A child with stdin and stdout given; -1 for either is /dev/null. Every
 * descriptor this file opens is close-on-exec, so a child holds only its
 * own two ends and a pipe ends when the process at its far end does. */
static pid_t im_spawn(const KbArgv *a, int in, int out)
{
	pid_t pid = fork();
	if (pid != 0)
		return pid;
	int null = open("/dev/null", O_RDWR);
	dup2(in >= 0 ? in : null, 0);
	dup2(out >= 0 ? out : null, 1);
	if (!kb_proc_verbose)
		dup2(null, 2);
	execvp(a->v[0], (char *const *)a->v);
	_exit(127);
}

static int im_reap(pid_t pid)
{
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
}

/* curl's arguments for a release file: HTTPS, or a local file:// path, and
 * nothing else — a redirect to plain HTTP is refused, though everything
 * fetched is checked by hash anyway. */
static void im_curl(KbArgv *a)
{
	kb_argv_add(a, "curl");
	kb_argv_add(a, "-fsSL");
	kb_argv_add(a, "--proto");
	kb_argv_add(a, "=https,file");
	kb_argv_add(a, "--proto-redir");
	kb_argv_add(a, "=https");
	kb_argv_add(a, "--retry");
	kb_argv_add(a, "2");
	kb_argv_add(a, "--connect-timeout");
	kb_argv_add(a, "30");
}

/* A bundle being read: curl's output through zstd, into this process. */
typedef struct {
	FILE *f;
	pid_t curl, zstd;
	long long read;
} ImReader;

static int reader_open(ImReader *r, char *const *urls, int n)
{
	int p1[2], p2[2];
	KbArgv c = {0}, z = {0};

	memset(r, 0, sizeof(*r));
	if (pipe2(p1, O_CLOEXEC) != 0)
		return -1;
	if (pipe2(p2, O_CLOEXEC) != 0) {
		close(p1[0]);
		close(p1[1]);
		return -1;
	}
	im_curl(&c);
	kb_argv_add(&c, "--");
	for (int i = 0; i < n && c.n < KB_MAX_ARGV - 2; i++)
		kb_argv_add(&c, urls[i]);
	kb_argv_end(&c);
	kb_argv_add(&z, "zstd");
	kb_argv_add(&z, "-dcq");
	kb_argv_add(&z, "--memory=2048MB");
	kb_argv_end(&z);
	r->curl = im_spawn(&c, -1, p1[1]);
	r->zstd = im_spawn(&z, p1[0], p2[1]);
	close(p1[0]);
	close(p1[1]);
	close(p2[1]);
	r->f = fdopen(p2[0], "r");
	return r->f && r->curl > 0 && r->zstd > 0 ? 0 : -1;
}

/* 0 when both programs finished cleanly. `early` stops them first: every
 * object needed has arrived, and the rest of the bundle is not wanted. */
static int reader_close(ImReader *r, int early)
{
	if (early) {
		if (r->curl > 0)
			kill(r->curl, SIGTERM);
		if (r->zstd > 0)
			kill(r->zstd, SIGTERM);
	}
	if (r->f)
		fclose(r->f);
	int a = r->curl > 0 ? im_reap(r->curl) : -1;
	int b = r->zstd > 0 ? im_reap(r->zstd) : -1;
	return early || (a == 0 && b == 0) ? 0 : -1;
}

/* A bundle being written: this process into zstd, zstd into `path`. */
typedef struct {
	FILE *f;
	pid_t zstd;
} ImWriter;

static int writer_open(ImWriter *w, const char *path, int level)
{
	int p[2], out;
	KbArgv z = {0};

	memset(w, 0, sizeof(*w));
	out = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (out < 0)
		return -1;
	if (pipe2(p, O_CLOEXEC) != 0) {
		close(out);
		return -1;
	}
	kb_argv_add(&z, "zstd");
	kb_argv_add(&z, "-qc");
	kb_argv_add(&z, "-T0");
	kb_argv_addf(&z, "-%d", level);
	kb_argv_end(&z);
	w->zstd = im_spawn(&z, p[0], out);
	close(p[0]);
	close(out);
	w->f = fdopen(p[1], "w");
	return w->f && w->zstd > 0 ? 0 : -1;
}

static int writer_close(ImWriter *w)
{
	int bad = w->f ? fclose(w->f) != 0 : 1;
	int rc = w->zstd > 0 ? im_reap(w->zstd) : -1;
	return bad || rc != 0 ? -1 : 0;
}

/* ── files ─────────────────────────────────────────────────────────────── */

static void join(char *out, size_t cap, const char *root, const char *rel)
{
	if (!strcmp(rel, "."))
		kb_strlcpy(out, root, cap);
	else
		snprintf(out, cap, "%s%s%s", root,
			 root[0] && root[strlen(root) - 1] == '/' ? "" : "/", rel);
}

static int hash_file(const char *path, long long want_size, char out[65])
{
	char buf[1 << 16];
	KbSha256 s;
	struct stat st;
	ssize_t r;
	int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);

	if (fd < 0)
		return -1;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
	    (want_size >= 0 && st.st_size != want_size)) {
		close(fd);
		return -1;
	}
	kb_sha256_init(&s);
	while ((r = read(fd, buf, sizeof(buf))) > 0)
		kb_sha256_update(&s, buf, (size_t)r);
	close(fd);
	if (r < 0)
		return -1;
	kb_sha256_final(&s, out);
	return 0;
}

static int write_all(int fd, const void *p, size_t n)
{
	const char *c = p;
	while (n) {
		ssize_t w = write(fd, c, n);
		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0)
			return -1;
		c += w;
		n -= (size_t)w;
	}
	return 0;
}

/* The temporary a file is written to before it is renamed into place: in
 * the same directory, so the rename never crosses a filesystem. */
static void tmp_beside(char *out, size_t cap, const char *final, const char *tag)
{
	const char *sl = strrchr(final, '/');
	snprintf(out, cap, "%.*s/.kdos-image.%s.%ld", sl ? (int)(sl - final) : 1,
		 sl ? final : ".", tag, (long)getpid());
}

/* `rel` and everything under it, removed without crossing onto another
 * filesystem: a mount inside the target is left where it is. */
static void rm_samedev(const char *path, dev_t dev)
{
	struct stat st;
	if (lstat(path, &st) != 0 || st.st_dev != dev)
		return;
	if (!S_ISDIR(st.st_mode)) {
		unlink(path);
		return;
	}
	chmod(path, (st.st_mode & 07777) | 0700);
	int n = 0;
	char **names = kb_listdir(path, &n);
	for (int i = 0; names && i < n; i++) {
		char *c = kb_path_join(path, names[i]);
		rm_samedev(c, dev);
		free(c);
	}
	kb_strv_free(names);
	rmdir(path);
}

/*
 * `src` copied to `dst` through a temporary beside it, hashed on the way,
 * and renamed into place only when the hash and the size are the ones
 * given. 0 placed, -1 not (and nothing is left behind). `src` may be `dst`
 * itself: that breaks a hard link the manifest does not ask for.
 */
static int copy_verify(const char *src, const char *dst, const char *sha,
		       long long size)
{
	char buf[1 << 16], tmp[4200], got[65];
	KbSha256 s;
	struct stat st;
	ssize_t r = 0;
	int in = open(src, O_RDONLY | O_NOFOLLOW | O_CLOEXEC), out;

	if (in < 0)
		return -1;
	if (fstat(in, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != size) {
		close(in);
		return -1;
	}
	tmp_beside(tmp, sizeof(tmp), dst, "copy");
	out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
		   0600);
	if (out < 0) {
		close(in);
		return -1;
	}
	kb_sha256_init(&s);
	while ((r = read(in, buf, sizeof(buf))) > 0) {
		kb_sha256_update(&s, buf, (size_t)r);
		if (write_all(out, buf, (size_t)r) != 0) {
			r = -1;
			break;
		}
	}
	close(in);
	if (close(out) != 0)
		r = -1;
	kb_sha256_final(&s, got);
	if (r < 0 || strcmp(got, sha) || rename(tmp, dst) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

/* ── making an image ───────────────────────────────────────────────────── */

typedef struct {
	const char *root;
	char **excl;
	int nexcl;
	KdtImage *m;
	char why[4600];
} Walk;

/*
 * Contents left out of every image, the directories themselves kept:
 * filesystems the kernel provides, what runs only at run time, the shared
 * home and pack store (packs and boxes are never part of a slot image), and
 * the record of the image a root was written from, which the root cannot
 * contain before it exists.
 */
static const char *const IM_EXCLUDE[] = {
	"dev", "proc", "sys", "run", "tmp", "mnt", "media", "home",
	"var/lib/kdos/packs", "var/lib/kdos/image", NULL
};

/*
 * Trees whose directories are content and whose files are not: what a
 * running system writes there is its own, but packages own directories in
 * them (/var/log/cups, /var/cache/libvirt/qemu), which a program expects to
 * find and `kpkg check` reports missing — and a slot that fails the check is
 * never armed.
 */
static const char *const IM_DIRS_ONLY[] = {
	"var/tmp", "var/cache", "var/log", NULL
};

static int excluded(const Walk *w, const char *rel)
{
	for (int i = 0; IM_EXCLUDE[i]; i++)
		if (!strcmp(rel, IM_EXCLUDE[i]))
			return 1;
	for (int i = 0; i < w->nexcl; i++)
		if (!strcmp(rel, w->excl[i]))
			return 1;
	return 0;
}

/* 1 when `rel` lies inside one of IM_DIRS_ONLY. */
static int dirs_only(const char *rel)
{
	for (int i = 0; IM_DIRS_ONLY[i]; i++) {
		size_t n = strlen(IM_DIRS_ONLY[i]);
		if (!strncmp(rel, IM_DIRS_ONLY[i], n) && rel[n] == '/')
			return 1;
	}
	return 0;
}

/* An extended attribute the image cannot carry, which it refuses rather
 * than drops: a file capability or an ACL lost on the way would leave a
 * program that starts and then cannot do its job. */
static int xattr_refused(const char *path, char *name, size_t cap)
{
	char list[4096];
	ssize_t n = llistxattr(path, list, sizeof(list));
	for (ssize_t o = 0; n > 0 && o < n; o += (ssize_t)strlen(list + o) + 1)
		if (!strcmp(list + o, "security.capability") ||
		    !strncmp(list + o, "system.posix_acl", 16)) {
			kb_strlcpy(name, list + o, cap);
			return 1;
		}
	return 0;
}

static int walk(Walk *w, const char *rel)
{
	char full[4200];
	join(full, sizeof(full), w->root, rel);
	int n = 0;
	char **names = kb_listdir(full, &n);
	int rc = 0;

	for (int i = 0; names && i < n && rc == 0; i++) {
		char crel[4200], cfull[4200], xa[64];
		struct stat st;
		if (!strcmp(rel, "."))
			kb_strlcpy(crel, names[i], sizeof(crel));
		else
			snprintf(crel, sizeof(crel), "%s/%s", rel, names[i]);
		if (!strcmp(crel, "lost+found"))
			continue;
		join(cfull, sizeof(cfull), w->root, crel);
		if (lstat(cfull, &st) != 0) {
			snprintf(w->why, sizeof(w->why), "%s: %s", crel,
				 strerror(errno));
			rc = -1;
			break;
		}
		if (S_ISSOCK(st.st_mode))
			continue;	/* a run-time leftover, not content */
		if (!S_ISDIR(st.st_mode) && dirs_only(crel))
			continue;
		if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode) &&
		    !S_ISLNK(st.st_mode)) {
			snprintf(w->why, sizeof(w->why), "%s is a device or a "
				 "pipe, which an image does not carry", crel);
			rc = -1;
			break;
		}
		if (!S_ISLNK(st.st_mode) && xattr_refused(cfull, xa, sizeof(xa))) {
			snprintf(w->why, sizeof(w->why), "%s carries %s, which "
				 "an image does not carry", crel, xa);
			rc = -1;
			break;
		}
		ImEnt *e = ent_new(w->m);
		e->path = kb_strdup(crel);
		e->mode = (unsigned)st.st_mode & 07777;
		e->uid = (unsigned)st.st_uid;
		e->gid = (unsigned)st.st_gid;
		e->mtime = (long long)st.st_mtime;
		if (S_ISDIR(st.st_mode)) {
			e->type = 'd';
			if (!excluded(w, crel))
				rc = walk(w, crel);
		} else if (S_ISREG(st.st_mode)) {
			if (excluded(w, crel)) {
				free(e->path);
				w->m->nent--;
				continue;
			}
			e->type = 'f';
			e->size = (long long)st.st_size;
			e->dev = st.st_dev;
			e->ino = st.st_ino;
			e->nlink = st.st_nlink;
		} else {
			char t[4096];
			ssize_t tl = readlink(cfull, t, sizeof(t) - 1);
			if (tl <= 0) {
				snprintf(w->why, sizeof(w->why), "%s: unreadable "
					 "link", crel);
				rc = -1;
				break;
			}
			t[tl] = 0;
			e->type = 'l';
			e->target = kb_strdup(t);
		}
	}
	kb_strv_free(names);
	return rc;
}

static int ent_cmp(const void *a, const void *b)
{
	const ImEnt *x = a, *y = b;
	if (!strcmp(x->path, "."))
		return -1;
	if (!strcmp(y->path, "."))
		return 1;
	return strcmp(x->path, y->path);
}

static const KdtImage *g_ino_m;

static int ino_cmp(const void *a, const void *b)
{
	const ImEnt *x = &g_ino_m->ent[*(const int *)a];
	const ImEnt *y = &g_ino_m->ent[*(const int *)b];
	if (x->dev != y->dev)
		return x->dev < y->dev ? -1 : 1;
	if (x->ino != y->ino)
		return x->ino < y->ino ? -1 : 1;
	return *(const int *)a - *(const int *)b;
}

/* Files sharing an inode: the first in path order stays a file, every other
 * becomes an `h` entry naming it. */
static void find_hardlinks(KdtImage *m)
{
	int *v = kb_calloc((size_t)m->nent + 1, sizeof(int)), n = 0;
	for (int i = 0; i < m->nent; i++)
		if (m->ent[i].type == 'f' && m->ent[i].nlink > 1)
			v[n++] = i;
	g_ino_m = m;
	qsort(v, (size_t)n, sizeof(int), ino_cmp);
	for (int i = 1; i < n; i++) {
		ImEnt *a = &m->ent[v[i - 1]], *b = &m->ent[v[i]];
		int first = v[i - 1];
		if (a->dev != b->dev || a->ino != b->ino)
			continue;
		while (m->ent[first].type == 'h')
			first = map_get(m, &m->bypath, m->ent[first].target, 0);
		b->type = 'h';
		b->target = kb_strdup(m->ent[first].path);
	}
	free(v);
}

static void manifest_text(const KdtImage *m, KbBuf *b)
{
	kb_buf_printf(b, IM_MAGIC "\nversion %s\narch %s\ntree %s\ntime %lld\n"
		      "objects %lld %lld\n", m->version, m->arch,
		      m->tree[0] ? m->tree : "-", m->time, m->objects, m->obytes);
	for (int i = 0; i < m->npart; i++)
		kb_buf_printf(b, "part %s %lld %s\n", m->part[i].file,
			      m->part[i].size, m->part[i].sha);
	for (int i = 0; i < m->ndelta; i++)
		kb_buf_printf(b, "delta %s %s %lld %s %lld\n", m->delta[i].from,
			      m->delta[i].file, m->delta[i].size,
			      m->delta[i].sha, m->delta[i].objects);
	kb_buf_str(b, "\n");
	for (int i = 0; i < m->nent; i++) {
		const ImEnt *e = &m->ent[i];
		switch (e->type) {
		case 'd':
			kb_buf_printf(b, "d %04o %u %u %lld ", e->mode, e->uid,
				      e->gid, e->mtime);
			break;
		case 'f':
			kb_buf_printf(b, "f %04o %u %u %lld %lld %s ", e->mode,
				      e->uid, e->gid, e->mtime, e->size, e->sha);
			break;
		case 'l':
			kb_buf_printf(b, "l %u %u %lld ", e->uid, e->gid,
				      e->mtime);
			enc(b, e->target);
			kb_buf_str(b, " ");
			break;
		case 'h':
			kb_buf_str(b, "h ");
			enc(b, e->path);
			kb_buf_str(b, " ");
			enc(b, e->target);
			kb_buf_str(b, "\n");
			continue;
		}
		enc(b, e->path);
		kb_buf_str(b, "\n");
	}
}

/* One `O` record: the object's bytes, read again from the root and checked
 * against the hash taken a moment before, so a file that changed under the
 * release is a failed release and not a wrong image. */
static int emit_object(FILE *out, const char *root, const ImEnt *e)
{
	char full[4200], buf[1 << 16], got[65];
	KbSha256 s;
	ssize_t r;
	long long left = e->size;

	join(full, sizeof(full), root, e->path);
	int fd = open(full, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0)
		return -1;
	fprintf(out, "O %s %lld\n", e->sha, e->size);
	kb_sha256_init(&s);
	while (left > 0 && (r = read(fd, buf, sizeof(buf))) > 0) {
		if (r > left)
			r = (ssize_t)left;
		kb_sha256_update(&s, buf, (size_t)r);
		if (fwrite(buf, 1, (size_t)r, out) != (size_t)r) {
			close(fd);
			return -1;
		}
		left -= r;
	}
	close(fd);
	kb_sha256_final(&s, got);
	return left == 0 && !strcmp(got, e->sha) ? 0 : -1;
}

/* A `P` record against `base`, when zstd makes a difference worth sending:
 * under half the object. 1 written, 0 not worth it, -1 failed. */
static int emit_patch(FILE *out, const char *root, const ImEnt *e,
		      const char *base, const char *base_sha, const char *scratch)
{
	char full[4200], patch[4300];
	KbArgv a = {0};
	struct stat st;

	join(full, sizeof(full), root, e->path);
	snprintf(patch, sizeof(patch), "%s/.patch", scratch);
	kb_argv_add(&a, "zstd");
	kb_argv_add(&a, "-qf");
	kb_argv_add(&a, "-19");
	kb_argv_addf(&a, "--patch-from=%s", base);
	kb_argv_add(&a, full);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, patch);
	kb_argv_end(&a);
	if (kb_run(&a) != 0 || stat(patch, &st) != 0 ||
	    st.st_size >= e->size / 2) {
		unlink(patch);
		return 0;
	}
	size_t len = 0;
	char *data = kb_read_all(patch, &len);
	unlink(patch);
	if (!data)
		return -1;
	fprintf(out, "P %s %s %zu\n", e->sha, base_sha, len);
	int ok = fwrite(data, 1, len, out) == len;
	free(data);
	return ok ? 1 : -1;
}

/* The full bundle's stream, compressed into `tmp`, then cut into parts. */
static int make_full(KdtImage *m, const char *root, const char *outdir,
		     int level, long long part_bytes)
{
	char tmp[4200];
	ImWriter w;
	char *seen = kb_calloc((size_t)m->nent + 1, 1);
	int rc = 0;

	snprintf(tmp, sizeof(tmp), "%s/.kdos-image-full.zst", outdir);
	if (writer_open(&w, tmp, level) != 0) {
		free(seen);
		return -1;
	}
	fputs(IM_STREAM "\n", w.f);
	m->objects = m->obytes = 0;
	for (int i = 0; i < m->nent && rc == 0; i++) {
		if (m->ent[i].type != 'f' || seen[i])
			continue;
		for (int j = i; j >= 0; j = m->same[j])
			seen[j] = 1;
		m->objects++;
		m->obytes += m->ent[i].size;
		rc = emit_object(w.f, root, &m->ent[i]);
	}
	fputs("E\n", w.f);
	free(seen);
	if (writer_close(&w) != 0 || rc != 0) {
		unlink(tmp);
		return -1;
	}

	int in = open(tmp, O_RDONLY | O_CLOEXEC);
	char *buf = kb_calloc(1 << 20, 1);
	ssize_t r = 1;
	for (int k = 0; in >= 0 && r > 0; k++) {
		char name[200], path[4300];
		KbSha256 s;
		long long got = 0;
		snprintf(name, sizeof(name), "kdos-image-%s.full.%03d",
			 m->version, k);
		snprintf(path, sizeof(path), "%s/%s", outdir, name);
		int out = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
			       0644);
		if (out < 0) {
			rc = -1;
			break;
		}
		kb_sha256_init(&s);
		while (got < part_bytes) {
			size_t want = (size_t)(part_bytes - got < (1 << 20)
					       ? part_bytes - got : (1 << 20));
			r = read(in, buf, want);
			if (r <= 0)
				break;
			kb_sha256_update(&s, buf, (size_t)r);
			if (write_all(out, buf, (size_t)r) != 0) {
				rc = -1;
				break;
			}
			got += r;
		}
		close(out);
		if (got == 0 && k > 0) {
			unlink(path);
			break;
		}
		m->part = kb_realloc(m->part, (size_t)(m->npart + 1) *
				     sizeof(*m->part));
		ImPart *p = &m->part[m->npart++];
		kb_strlcpy(p->file, name, sizeof(p->file));
		p->size = got;
		kb_sha256_final(&s, p->sha);
		if (rc != 0 || r < 0)
			break;
	}
	free(buf);
	if (in < 0 || r < 0)
		rc = -1;
	if (in >= 0)
		close(in);
	unlink(tmp);
	return rc;
}

/* An object of a tree a running machine edits in place. */
static int local_tree(const ImEnt *e, const KdtImage *m)
{
	for (int j = map_get(m, &m->bysha, e->sha, 1); j >= 0; j = m->same[j])
		if (!strncmp(m->ent[j].path, "etc/", 4) ||
		    !strncmp(m->ent[j].path, "var/", 4))
			return 1;
	return 0;
}

/*
 * The delta from `prev`: every object `m` holds that `prev` does not, as a
 * patch against the file at the same path in `prev_root` when one is given,
 * holds the hash `prev` names and is small enough, whole otherwise.
 *
 * AND EVERY OBJECT UNDER etc/ AND var/, changed or not. Those are the trees
 * a running machine edits — a hostname, an account, a network — so the
 * running root seldom still holds the release's copy, and an object neither
 * the seed nor the delta holds sends the machine to the full image. They are
 * a few megabytes; the full image is gigabytes.
 */
static int make_delta(KdtImage *m, const char *root, const KdtImage *prev,
		      const char *prev_root, const char *outdir, int level)
{
	char name[200], path[4300], sum[65];
	ImWriter w;
	char *seen = kb_calloc((size_t)m->nent + 1, 1);
	long long count = 0;
	int rc = 0;

	snprintf(name, sizeof(name), "kdos-image-%s.from-%s.delta", m->version,
		 prev->version);
	snprintf(path, sizeof(path), "%s/%s", outdir, name);
	if (writer_open(&w, path, level) != 0) {
		free(seen);
		return -1;
	}
	fputs(IM_STREAM "\n", w.f);
	for (int i = 0; i < m->nent && rc == 0; i++) {
		const ImEnt *e = &m->ent[i];
		if (e->type != 'f' || seen[i])
			continue;
		for (int j = i; j >= 0; j = m->same[j])
			seen[j] = 1;
		if (map_get(prev, &prev->bysha, e->sha, 1) >= 0) {
			if (!local_tree(e, m))
				continue;
			count++;
			rc = emit_object(w.f, root, e);
			continue;
		}
		count++;
		int pi = map_get(prev, &prev->bypath, e->path, 0);
		int done = 0;
		if (prev_root && pi >= 0 && prev->ent[pi].type == 'f' &&
		    prev->ent[pi].size > 0 && prev->ent[pi].size <= IM_PATCH_MAX &&
		    e->size > 4096) {
			char base[4200];
			join(base, sizeof(base), prev_root, e->path);
			if (hash_file(base, prev->ent[pi].size, sum) == 0 &&
			    !strcmp(sum, prev->ent[pi].sha))
				done = emit_patch(w.f, root, e, base,
						  prev->ent[pi].sha, outdir);
		}
		if (done < 0)
			rc = -1;
		else if (!done)
			rc = emit_object(w.f, root, e);
	}
	fputs("E\n", w.f);
	free(seen);
	if (writer_close(&w) != 0 || rc != 0) {
		unlink(path);
		return -1;
	}
	struct stat st;
	if (stat(path, &st) != 0 || kb_sha256_file(path, sum) != 0)
		return -1;
	m->delta = kb_realloc(m->delta, (size_t)(m->ndelta + 1) *
			      sizeof(*m->delta));
	ImDelta *d = &m->delta[m->ndelta++];
	kb_strlcpy(d->from, prev->version, sizeof(d->from));
	kb_strlcpy(d->file, name, sizeof(d->file));
	d->size = (long long)st.st_size;
	kb_strlcpy(d->sha, sum, sizeof(d->sha));
	d->objects = count;
	return 0;
}

/* `<root>/ports/TREE`, for the commit and time an image is stamped with
 * when the command line does not say. */
static void tree_stamp(const char *root, char *commit, size_t cap,
		       long long *t, char *ver, size_t vcap)
{
	char p[4200];
	join(p, sizeof(p), root, "ports/TREE");
	char *d = kb_read_all(p, NULL);
	for (char *l = d; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		if (!strncmp(l, "commit ", 7) && !commit[0])
			kb_strlcpy(commit, l + 7, cap);
		else if (!strncmp(l, "time ", 5) && !*t)
			*t = atoll(l + 5);
		else if (!strncmp(l, "version ", 8) && !ver[0])
			kb_strlcpy(ver, l + 8, vcap);
		l = nl ? nl + 1 : NULL;
	}
	free(d);
}

static int cmd_make(int argc, char **argv)
{
	const char *root = NULL, *outdir = NULL, *key = NULL;
	char version[64] = "", tree[64] = "", arch[32] = "";
	long long t = 0, part_bytes = IM_PART_BYTES;
	int level = 12;
	const char *prev[8], *prev_root[8];
	int nprev = 0;
	Walk w;

	memset(&w, 0, sizeof(w));
	memset(prev_root, 0, sizeof(prev_root));
	w.excl = kb_calloc((size_t)argc + 1, sizeof(char *));
	for (int i = 0; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(a, "--version") && v)
			kb_strlcpy(version, argv[++i], sizeof(version));
		else if (!strcmp(a, "--tree") && v)
			kb_strlcpy(tree, argv[++i], sizeof(tree));
		else if (!strcmp(a, "--time") && v)
			t = atoll(argv[++i]);
		else if (!strcmp(a, "--arch") && v)
			kb_strlcpy(arch, argv[++i], sizeof(arch));
		else if (!strcmp(a, "--key") && v)
			key = argv[++i];
		else if (!strcmp(a, "--level") && v)
			level = atoi(argv[++i]);
		else if (!strcmp(a, "--part-mb") && v)
			part_bytes = atoll(argv[++i]) << 20;
		else if (!strcmp(a, "--exclude") && v) {
			const char *x = argv[++i];
			while (*x == '/')
				x++;
			w.excl[w.nexcl++] = (char *)x;
		} else if (!strcmp(a, "--prev") && v && nprev < 8)
			prev[nprev++] = argv[++i];
		else if (!strcmp(a, "--prev-root") && v && nprev > 0)
			prev_root[nprev - 1] = argv[++i];
		else if (a[0] != '-' && !root)
			root = a;
		else if (a[0] != '-' && !outdir)
			outdir = a;
		else {
			fprintf(stderr, "kdos image make: unexpected %s\n", a);
			free(w.excl);
			return 2;
		}
	}
	if (!root || !outdir || level < 1 || level > 19 || part_bytes <= 0) {
		fprintf(stderr, "usage: kdos image make ROOT OUTDIR --version V "
				"[--tree C] [--time T] [--key FILE]\n"
				"         [--prev DIR [--prev-root DIR]]... "
				"[--exclude PATH]... [--level N] [--part-mb N]\n");
		free(w.excl);
		return 2;
	}
	tree_stamp(root, tree, sizeof(tree), &t, version, sizeof(version));
	if (!ver_ok(version)) {
		fprintf(stderr, "kdos image make: no usable --version, and "
				"%s/ports/TREE names none\n", root);
		free(w.excl);
		return 2;
	}
	if (!arch[0])
		kp_arch(arch, sizeof(arch));
	signal(SIGPIPE, SIG_IGN);

	KdtImage *m = kb_calloc(1, sizeof(*m));
	kb_strlcpy(m->version, version, sizeof(m->version));
	kb_strlcpy(m->arch, arch, sizeof(m->arch));
	kb_strlcpy(m->tree, tree[0] ? tree : "-", sizeof(m->tree));
	m->time = t;

	struct stat rst;
	if (lstat(root, &rst) != 0 || !S_ISDIR(rst.st_mode)) {
		fprintf(stderr, "kdos image make: %s is not a directory\n", root);
		goto fail;
	}
	ImEnt *r0 = ent_new(m);
	r0->type = 'd';
	r0->path = kb_strdup(".");
	r0->mode = (unsigned)rst.st_mode & 07777;
	r0->uid = (unsigned)rst.st_uid;
	r0->gid = (unsigned)rst.st_gid;
	r0->mtime = (long long)rst.st_mtime;
	w.root = root;
	w.m = m;
	printf("==> reading %s\n", root);
	if (walk(&w, ".") != 0) {
		fprintf(stderr, "kdos image make: %s\n", w.why);
		goto fail;
	}
	qsort(m->ent, (size_t)m->nent, sizeof(*m->ent), ent_cmp);
	m->same = kb_calloc((size_t)m->nent + 1, sizeof(int));
	for (int i = 0; i < m->nent; i++)
		m->same[i] = -1;
	map_build(m, &m->bypath, 0);
	find_hardlinks(m);
	for (int i = 0; i < m->nent; i++) {
		ImEnt *e = &m->ent[i];
		char full[4200];
		if (e->type != 'f')
			continue;
		join(full, sizeof(full), root, e->path);
		if (hash_file(full, e->size, e->sha) != 0) {
			fprintf(stderr, "kdos image make: cannot read %s\n",
				e->path);
			goto fail;
		}
	}
	free(m->bysha.slot);
	memset(&m->bysha, 0, sizeof(m->bysha));
	map_build(m, &m->bysha, 1);

	if (kb_mkdir_p(outdir) != 0) {
		fprintf(stderr, "kdos image make: cannot create %s\n", outdir);
		goto fail;
	}
	printf("==> the full bundle\n");
	if (make_full(m, root, outdir, level, part_bytes) != 0) {
		fprintf(stderr, "kdos image make: the full bundle could not be "
				"written\n");
		goto fail;
	}
	for (int i = 0; i < nprev; i++) {
		char pp[4200];
		snprintf(pp, sizeof(pp), "%s/" IM_MANIFEST, prev[i]);
		KdtImage *p = im_read_unverified(pp);
		if (!p) {
			fprintf(stderr, "kdos image make: %s is not an image "
					"manifest\n", pp);
			goto fail;
		}
		printf("==> the delta from %s%s\n", p->version,
		       prev_root[i] ? ", with patches" : "");
		int rc = strcmp(p->version, m->version)
			 ? make_delta(m, root, p, prev_root[i], outdir, level)
			 : -1;
		kdt_image_free(p);
		if (rc != 0) {
			fprintf(stderr, "kdos image make: the delta from %s "
					"could not be written\n", prev[i]);
			goto fail;
		}
	}

	KbBuf b = {0};
	char mp[4200], sp[4300];
	manifest_text(m, &b);
	snprintf(mp, sizeof(mp), "%s/" IM_MANIFEST, outdir);
	snprintf(sp, sizeof(sp), "%s.sig", mp);
	if (kb_write_file_atomic(mp, b.p) != 0) {
		kb_buf_free(&b);
		fprintf(stderr, "kdos image make: cannot write %s\n", mp);
		goto fail;
	}
	unlink(sp);
	if (key) {
		uint8_t seed[KSIG_SEED_LEN], pub[KSIG_PUB_LEN];
		if (ksig_read_secret(key, seed, pub) != 0 ||
		    ksig_sig_append(sp, seed, pub, b.p, b.n) != 0) {
			kb_buf_free(&b);
			fprintf(stderr, "kdos image make: cannot sign with %s\n",
				key);
			goto fail;
		}
	}
	kb_buf_free(&b);
	{
		char **files = kb_calloc((size_t)(m->npart + m->ndelta) + 3,
					 sizeof(char *));
		char iwhy[512];
		int nf = 0;
		files[nf++] = IM_MANIFEST;
		if (key)
			files[nf++] = IM_MANIFEST ".sig";
		for (int i = 0; i < m->npart; i++)
			files[nf++] = m->part[i].file;
		for (int i = 0; i < m->ndelta; i++)
			files[nf++] = m->delta[i].file;
		int irc = kdt_relidx_write(outdir, "image", m->version, files, nf,
					   key, iwhy, sizeof(iwhy));
		free(files);
		if (irc != 0) {
			fprintf(stderr, "kdos image make: %s\n", iwhy);
			goto fail;
		}
	}
	printf("image %s: %d entries, %lld objects (%s), %d part%s, %d "
	       "delta%s%s\n", m->version, m->nent, m->objects,
	       kb_human_size((unsigned long long)m->obytes), m->npart,
	       m->npart == 1 ? "" : "s", m->ndelta, m->ndelta == 1 ? "" : "s",
	       key ? ", signed" : ", NOT signed");
	kdt_image_free(m);
	free(w.excl);
	return 0;
fail:
	kdt_image_free(m);
	free(w.excl);
	return 1;
}

/* ── writing an image into a root ──────────────────────────────────────── */

typedef struct {
	const KdtImage *m;
	const char *target, *seed;
	KdtImage *base;			/* the seed's own manifest, as hints */
	char *done;			/* per entry: in the target, verified */
	int nf, ndone, last;
	dev_t dev;
	KdtImageTick tick;
	void *tickd;
	volatile sig_atomic_t *stop;
	char *why;
	size_t wcap;
} Fill;

static void tick(Fill *F)
{
	int pct = F->nf ? (int)((long long)F->ndone * 100 / F->nf) : 100;
	if (F->tick && pct != F->last) {
		F->last = pct;
		F->tick(F->ndone, F->nf, F->tickd);
	}
}

static void mark(Fill *F, int i)
{
	if (!F->done[i]) {
		F->done[i] = 1;
		F->ndone++;
		tick(F);
	}
}

/* An entry of `sha` already in the target and verified, or -1. */
static int verified_copy(const Fill *F, const char *sha)
{
	for (int j = map_get(F->m, &F->m->bysha, sha, 1); j >= 0;
	     j = F->m->same[j])
		if (F->done[j])
			return j;
	return -1;
}

static void prune(Fill *F, const char *rel)
{
	char full[4200];
	join(full, sizeof(full), F->target, rel);
	int n = 0;
	char **names = kb_listdir(full, &n);

	for (int i = 0; names && i < n; i++) {
		char crel[4200], cfull[4200];
		struct stat st;
		if (!strcmp(rel, "."))
			kb_strlcpy(crel, names[i], sizeof(crel));
		else
			snprintf(crel, sizeof(crel), "%s/%s", rel, names[i]);
		if (!strcmp(crel, "lost+found"))
			continue;
		join(cfull, sizeof(cfull), F->target, crel);
		if (lstat(cfull, &st) != 0 || st.st_dev != F->dev)
			continue;
		int k = map_get(F->m, &F->m->bypath, crel, 0);
		char t = k >= 0 ? F->m->ent[k].type : 0;
		if (t == 'd' && S_ISDIR(st.st_mode))
			prune(F, crel);
		else if (!((t == 'f' || t == 'h') && S_ISREG(st.st_mode)) &&
			 !(t == 'l' && S_ISLNK(st.st_mode)))
			rm_samedev(cfull, F->dev);
	}
	kb_strv_free(names);
}

static int fail_at(Fill *F, const char *fmt, const char *arg)
{
	snprintf(F->why, F->wcap, fmt, arg);
	return -1;
}

/* Every directory, made where missing and left writable until the end. */
static int make_dirs(Fill *F)
{
	for (int i = 1; i < F->m->nent; i++) {
		const ImEnt *e = &F->m->ent[i];
		char full[4200];
		struct stat st;
		if (e->type != 'd')
			continue;
		join(full, sizeof(full), F->target, e->path);
		if (lstat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
			if ((st.st_mode & 0700) != 0700)
				chmod(full, (st.st_mode & 07777) | 0700);
			continue;
		}
		if (mkdir(full, 0755) != 0)
			return fail_at(F, "cannot create %s", e->path);
	}
	return 0;
}

/* The seed's file for entry `e`, when it holds one with the right hash:
 * the same path first, then any path the seed's own manifest gives for that
 * object. Copied and verified. */
static int from_seed(Fill *F, const ImEnt *e, const char *dst)
{
	char src[4200];
	if (!F->seed)
		return -1;
	join(src, sizeof(src), F->seed, e->path);
	if (copy_verify(src, dst, e->sha, e->size) == 0)
		return 0;
	if (!F->base)
		return -1;
	for (int k = map_get(F->base, &F->base->bysha, e->sha, 1); k >= 0;
	     k = F->base->same[k]) {
		join(src, sizeof(src), F->seed, F->base->ent[k].path);
		if (copy_verify(src, dst, e->sha, e->size) == 0)
			return 0;
	}
	return -1;
}

static int place_local(Fill *F)
{
	for (int i = 0; i < F->m->nent; i++) {
		const ImEnt *e = &F->m->ent[i];
		char dst[4200], got[65];
		struct stat st;
		if (e->type != 'f' || F->done[i])
			continue;
		if (F->stop && *F->stop)
			return fail_at(F, "%s", "cancelled");
		join(dst, sizeof(dst), F->target, e->path);
		if (e->size == 0) {
			if (strcmp(e->sha, IM_EMPTY_SHA))
				return fail_at(F, "%s: an empty file with a "
					       "content hash", e->path);
			int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC |
				      O_NOFOLLOW | O_CLOEXEC, 0600);
			if (fd < 0 || close(fd) != 0)
				return fail_at(F, "cannot write %s", e->path);
			mark(F, i);
			continue;
		}
		int ok = lstat(dst, &st) == 0 && S_ISREG(st.st_mode) &&
			 st.st_size == e->size &&
			 (st.st_nlink == 1
			  ? hash_file(dst, e->size, got) == 0 &&
			    !strcmp(got, e->sha)
			  : copy_verify(dst, dst, e->sha, e->size) == 0);
		if (!ok) {
			int j = verified_copy(F, e->sha);
			char src[4200];
			if (j >= 0) {
				join(src, sizeof(src), F->target,
				     F->m->ent[j].path);
				ok = copy_verify(src, dst, e->sha, e->size) == 0;
			}
			if (!ok)
				ok = from_seed(F, e, dst) == 0;
		}
		if (ok)
			mark(F, i);
	}
	return 0;
}

/* Every missing entry of `sha`, filled from the copy at entry `first`. */
static void spread(Fill *F, int first)
{
	const ImEnt *e = &F->m->ent[first];
	char src[4200], dst[4200];
	join(src, sizeof(src), F->target, e->path);
	for (int j = map_get(F->m, &F->m->bysha, e->sha, 1); j >= 0;
	     j = F->m->same[j]) {
		if (F->done[j])
			continue;
		join(dst, sizeof(dst), F->target, F->m->ent[j].path);
		if (copy_verify(src, dst, e->sha, e->size) == 0)
			mark(F, j);
	}
}

static int first_missing(const Fill *F, const char *sha)
{
	for (int j = map_get(F->m, &F->m->bysha, sha, 1); j >= 0;
	     j = F->m->same[j])
		if (!F->done[j])
			return j;
	return -1;
}

/* A base for a patch: a verified copy in the target, or a seed file the
 * seed's manifest names for that object, hashed before it is used. */
static int patch_base(const Fill *F, const char *sha, long long *size,
		      char *out, size_t cap)
{
	char got[65];
	for (int j = map_get(F->m, &F->m->bysha, sha, 1); j >= 0;
	     j = F->m->same[j])
		if (F->done[j]) {
			join(out, cap, F->target, F->m->ent[j].path);
			*size = F->m->ent[j].size;
			return 0;
		}
	if (!F->seed || !F->base)
		return -1;
	for (int k = map_get(F->base, &F->base->bysha, sha, 1); k >= 0;
	     k = F->base->same[k]) {
		join(out, cap, F->seed, F->base->ent[k].path);
		if (hash_file(out, F->base->ent[k].size, got) == 0 &&
		    !strcmp(got, sha)) {
			*size = F->base->ent[k].size;
			return 0;
		}
	}
	return -1;
}

/* `n` bytes of the stream into `fd` (or nowhere when fd < 0), hashed. */
static int stream_bytes(ImReader *r, long long n, int fd, KbSha256 *s)
{
	char buf[1 << 16];
	while (n > 0) {
		size_t want = n < (long long)sizeof(buf) ? (size_t)n : sizeof(buf);
		size_t got = fread(buf, 1, want, r->f);
		if (got != want)
			return -1;
		if (s)
			kb_sha256_update(s, buf, got);
		if (fd >= 0 && write_all(fd, buf, got) != 0)
			return -1;
		n -= (long long)got;
	}
	return 0;
}

static int read_line(ImReader *r, char *line, size_t cap)
{
	if (!fgets(line, (int)cap, r->f))
		return -1;
	size_t n = strlen(line);
	if (!n || line[n - 1] != '\n')
		return -1;
	line[n - 1] = 0;
	return 0;
}

static int remaining(const Fill *F)
{
	return F->nf - F->ndone;
}

/*
 * One bundle, read from `urls` in order, every record the target still
 * lacks written and verified, the rest read past. Stops reading once
 * nothing is missing. 0 when the stream was whole (or not needed to the
 * end), -1 when it broke off or was not a bundle; what it did deliver stays.
 */
static int apply_stream(Fill *F, char *const *urls, int n, int patches)
{
	ImReader r;
	char line[300];
	int early = 0, bad = 0;

	if (reader_open(&r, urls, n) != 0) {
		reader_close(&r, 1);
		return -1;
	}
	if (read_line(&r, line, sizeof(line)) != 0 || strcmp(line, IM_STREAM))
		bad = 1;
	while (!bad) {
		char *f[4];
		int nf;
		if (F->stop && *F->stop) {
			early = 1;
			break;
		}
		if (!remaining(F)) {
			early = 1;
			break;
		}
		if (read_line(&r, line, sizeof(line)) != 0) {
			bad = 1;
			break;
		}
		nf = fields(line, f, 4);
		if (nf == 1 && !strcmp(f[0], "E"))
			break;
		int is_patch = nf == 4 && !strcmp(f[0], "P");
		if (!(nf == 3 && !strcmp(f[0], "O")) && !is_patch) {
			bad = 1;
			break;
		}
		const char *sha = f[1];
		const char *len = f[nf - 1];
		if (!sha_ok(sha) || !num_ok(len, 0) ||
		    (is_patch && (!patches || !sha_ok(f[2])))) {
			bad = 1;
			break;
		}
		long long size = atoll(len);
		int i = first_missing(F, sha);
		if (i < 0) {
			if (stream_bytes(&r, size, -1, NULL) != 0)
				bad = 1;
			continue;
		}
		const ImEnt *e = &F->m->ent[i];
		char dst[4200], tmp[4300], got[65];
		join(dst, sizeof(dst), F->target, e->path);
		tmp_beside(tmp, sizeof(tmp), dst, is_patch ? "patch" : "obj");
		int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW |
			      O_CLOEXEC, 0600);
		if (fd < 0) {
			reader_close(&r, 1);
			return fail_at(F, "cannot write beside %s", e->path);
		}
		KbSha256 s;
		kb_sha256_init(&s);
		int rc = stream_bytes(&r, size, fd, &s);
		close(fd);
		if (rc != 0) {
			unlink(tmp);
			bad = 1;
			break;
		}
		if (!is_patch) {
			kb_sha256_final(&s, got);
			if (size == e->size && !strcmp(got, e->sha) &&
			    rename(tmp, dst) == 0) {
				mark(F, i);
				spread(F, i);
			} else {
				unlink(tmp);
			}
			continue;
		}
		/* A patch: its base, verified, then the result, verified. */
		char base[4200], out[4300];
		long long bsize = 0;
		tmp_beside(out, sizeof(out), dst, "out");
		if (patch_base(F, f[2], &bsize, base, sizeof(base)) == 0) {
			KbArgv a = {0};
			kb_argv_add(&a, "zstd");
			kb_argv_add(&a, "-dqf");
			kb_argv_add(&a, "--memory=2048MB");
			kb_argv_addf(&a, "--patch-from=%s", base);
			kb_argv_add(&a, tmp);
			kb_argv_add(&a, "-o");
			kb_argv_add(&a, out);
			kb_argv_end(&a);
			if (kb_run(&a) == 0 &&
			    hash_file(out, e->size, got) == 0 &&
			    !strcmp(got, e->sha) && rename(out, dst) == 0) {
				mark(F, i);
				spread(F, i);
			}
		}
		unlink(out);
		unlink(tmp);
	}
	int closed = reader_close(&r, early || bad);
	return bad || closed != 0 ? -1 : 0;
}

/* A bundle's URL, as the release index gives it; "" when the index does not
 * name it, which curl then fails to read like any missing file. */
static void url_of(char *out, size_t cap, const KdtRelIdx *ix, const char *file)
{
	char path[600];
	snprintf(path, sizeof(path), "image/%s", file);
	if (kdt_relidx_url(ix, path, out, cap, NULL, NULL) != 0)
		out[0] = 0;
}

/* Links, hard links, then every owner, mode and time, deepest first so a
 * directory's time is set after nothing more is written into it. */
static int finish(Fill *F)
{
	int root = geteuid() == 0;
	for (int i = 0; i < F->m->nent; i++) {
		const ImEnt *e = &F->m->ent[i];
		char p[4200], t[4096];
		join(p, sizeof(p), F->target, e->path);
		if (e->type == 'l') {
			ssize_t n = readlink(p, t, sizeof(t) - 1);
			if (n >= 0) {
				t[n] = 0;
				if (!strcmp(t, e->target))
					continue;
			}
			unlink(p);
			if (symlink(e->target, p) != 0)
				return fail_at(F, "cannot link %s", e->path);
		} else if (e->type == 'h') {
			char first[4200];
			join(first, sizeof(first), F->target, e->target);
			unlink(p);
			if (link(first, p) != 0)
				return fail_at(F, "cannot hard-link %s", e->path);
		}
	}
	for (int i = F->m->nent - 1; i >= 0; i--) {
		const ImEnt *e = &F->m->ent[i];
		char p[4200];
		struct timespec ts[2];
		if (e->type == 'h')
			continue;
		join(p, sizeof(p), F->target, e->path);
		if (root && lchown(p, (uid_t)e->uid, (gid_t)e->gid) != 0)
			return fail_at(F, "cannot set the owner of %s", e->path);
		if (e->type != 'l' && chmod(p, (mode_t)e->mode) != 0)
			return fail_at(F, "cannot set the mode of %s", e->path);
		ts[0].tv_sec = ts[1].tv_sec = (time_t)e->mtime;
		ts[0].tv_nsec = ts[1].tv_nsec = 0;
		utimensat(AT_FDCWD, p, ts, AT_SYMLINK_NOFOLLOW);
	}
	return 0;
}

/* The manifest a root was written from, inside it, for the next image's
 * seed hints and for `kdos update check`'s comparison. */
static int record(const KdtImage *m, const char *target)
{
	char p[4300], dir[4200];
	join(dir, sizeof(dir), target, "var/lib/kdos/image");
	if (kb_mkdir_p(dir) != 0)
		return -1;
	snprintf(p, sizeof(p), "%s/MANIFEST", dir);
	char *copy = kb_calloc(m->rawlen + 1, 1);
	memcpy(copy, m->raw, m->rawlen);
	int rc = kb_write_file_atomic(p, copy);
	free(copy);
	snprintf(p, sizeof(p), "%s/MANIFEST.sig", dir);
	if (rc == 0 && m->sig)
		rc = kb_write_file_atomic(p, m->sig);
	return rc;
}

/*
 * THE IMAGE INTO `target`. `ix` is the verified release index that says
 * where each bundle is; `seed` is a root to copy unchanged files
 * from, normally the one this machine runs. 0 when every entry is in place
 * and verified; -1 with the reason in `why`, the target left partly written
 * — never booted, because nothing arms a slot this did not finish.
 *
 * THE TARGET MUST NOT BE THE RUNNING ROOT, nor the seed: this removes every
 * entry the manifest does not name.
 */
static int is_running_root(const char *p)
{
	struct stat a, b;
	return stat(p, &a) != 0 || (stat("/", &b) == 0 && a.st_dev == b.st_dev &&
				    a.st_ino == b.st_ino);
}

int kdt_image_fill(const KdtImage *m, const KdtRelIdx *ix, const char *target,
		   const char *seed, KdtImageTick cb, void *cbd,
		   volatile sig_atomic_t *stop, char *why, size_t wcap)
{
	struct stat ts, rs, ss;
	Fill F;
	int rc = -1;

	memset(&F, 0, sizeof(F));
	F.m = m;
	F.target = target;
	F.seed = seed;
	F.tick = cb;
	F.tickd = cbd;
	F.stop = stop;
	F.why = why;
	F.wcap = wcap;
	F.last = -1;
	why[0] = 0;
	if (lstat(target, &ts) != 0 || !S_ISDIR(ts.st_mode))
		return fail_at(&F, "%s is not a directory", target);
	if (stat("/", &rs) == 0 && rs.st_dev == ts.st_dev &&
	    rs.st_ino == ts.st_ino)
		return fail_at(&F, "%s is the running root", target);
	if (seed && stat(seed, &ss) == 0 && ss.st_dev == ts.st_dev &&
	    ss.st_ino == ts.st_ino)
		return fail_at(&F, "%s is the root it would be filled from",
			       target);
	F.dev = ts.st_dev;
	if (seed) {
		char bp[4200];
		join(bp, sizeof(bp), seed, IM_INSTALLED + 1);
		F.base = im_read_unverified(bp);
	}
	F.done = kb_calloc((size_t)m->nent + 1, 1);
	for (int i = 0; i < m->nent; i++)
		if (m->ent[i].type == 'f')
			F.nf++;
	signal(SIGPIPE, SIG_IGN);

	prune(&F, ".");
	if (make_dirs(&F) != 0 || place_local(&F) != 0)
		goto out;

	printf("  %d of %d files already in place or copied from %s\n", F.ndone,
	       F.nf, seed ? seed : "nowhere");
	if (remaining(&F) && F.base) {
		for (int d = 0; d < m->ndelta; d++) {
			if (strcmp(m->delta[d].from, F.base->version))
				continue;
			char url[4400], *u = url;
			url_of(url, sizeof(url), ix, m->delta[d].file);
			printf("  the delta from %s, %s\n", m->delta[d].from,
			       kb_human_size((unsigned long long)m->delta[d].size));
			apply_stream(&F, &u, 1, 1);
			break;
		}
	}
	if (remaining(&F) && !(stop && *stop)) {
		printf("  %d file%s from the full image, %s\n", remaining(&F),
		       remaining(&F) == 1 ? "" : "s",
		       kb_human_size((unsigned long long)
				     kdt_image_download_size(m, NULL)));
		char **urls = kb_calloc((size_t)m->npart + 1, sizeof(char *));
		for (int p = 0; p < m->npart; p++) {
			urls[p] = kb_calloc(4400, 1);
			url_of(urls[p], 4400, ix, m->part[p].file);
		}
		int s = m->npart ? apply_stream(&F, urls, m->npart, 0) : -1;
		for (int p = 0; p < m->npart; p++)
			free(urls[p]);
		free(urls);
		if (s != 0 && remaining(&F) && !why[0])
			snprintf(why, wcap, "the image's bundle could not be "
				 "read from %s", kdt_relidx_origin(ix));
	}
	if (stop && *stop) {
		snprintf(why, wcap, "cancelled");
		goto out;
	}
	if (remaining(&F)) {
		if (!why[0])
			snprintf(why, wcap, "%d file%s of the image could not "
				 "be obtained", remaining(&F),
				 remaining(&F) == 1 ? "" : "s");
		goto out;
	}
	if (record(m, target) != 0) {
		snprintf(why, wcap, "cannot record the image in %s", target);
		goto out;
	}
	if (finish(&F) != 0)
		goto out;
	int fd = open(target, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd >= 0) {
		syncfs(fd);
		close(fd);
	}
	rc = 0;
out:
	kdt_image_free(F.base);
	free(F.done);
	return rc;
}

/* ── carrying a machine's own state into a slot ────────────────────────── */

/*
 * A SLOT IS A WHOLE ROOT, AND A MACHINE'S CONFIGURATION IS NOT IN THE
 * IMAGE. Only /home is shared between the two slots (and the pack store, a
 * bind from it); the accounts, the network, the host keys and every other
 * edit under /etc live in the root this machine runs. Before a slot is
 * armed they are carried into it, by one rule per file:
 *
 *   untouched   a file a package installed and nobody edited (its hash is
 *               the one the running root's package database recorded):
 *               the slot keeps ITS copy, which is the newer default
 *   edited or   a file somebody changed, or one no package owns (the
 *   unowned     hostname, fstab, machine-id, host keys, network profiles):
 *               the running root's copy wins, and a different copy the
 *               slot held is kept beside it as `<file>.kdos-new`
 *   accounts    passwd, group, shadow, gshadow, subuid, subgid: merged by
 *               name — every running entry, then every entry only the
 *               slot has, so an account made since the slot was filled
 *               and a system account a new version adds both survive
 *   fstab       the running root's, with the slot's own root line
 *
 * A few files are the slot's own and are never carried: its release
 * record, and caches regenerated from what the slot holds.
 *
 * Machine state under /var follows as whole trees (CARRY_STATE), and so do
 * the `carry =` paths of /etc/kdos/update.conf.
 */
static const char *const CARRY_SKIP[] = {
	"etc/kdos-release", "etc/mtab", "etc/ld.so.cache", "etc/udev/hwdb.bin",
	NULL
};

static const char *const CARRY_ACCOUNTS[] = {
	"etc/passwd", "etc/group", "etc/shadow", "etc/gshadow", "etc/subuid",
	"etc/subgid", NULL
};

static const char *const CARRY_STATE[] = {
	"var/lib/kdos/world", "var/lib/kdos/apps-pending", "var/lib/dbus/machine-id",
	"var/lib/bluetooth", "var/lib/AccountsService", "var/lib/sddm",
	"var/lib/alsa", "var/lib/NetworkManager", "var/spool/cron", "root",
	NULL
};

typedef struct {
	char **path, **sha;
	int n, cap;
} Pristine;

/* The running root's package database: every regular file under etc/ with
 * the hash it was installed with (`<db>/.files/<name>`). */
static void pristine_load(Pristine *p, const char *db)
{
	char dir[4200];
	int n = 0;
	snprintf(dir, sizeof(dir), "%s/.files", db);
	char **names = kb_listdir(dir, &n);
	memset(p, 0, sizeof(*p));
	for (int i = 0; names && i < n; i++) {
		char *fp = kb_path_join(dir, names[i]);
		char *d = kb_read_all(fp, NULL);
		free(fp);
		for (char *l = d; l && *l;) {
			char *nl = strchr(l, '\n');
			if (nl)
				*nl = 0;
			/* `<sha> <mode> ./etc/...` */
			char *sp = strchr(l, ' ');
			char *sp2 = sp ? strchr(sp + 1, ' ') : NULL;
			if (sp && sp2 && sp - l == 64 &&
			    !strncmp(sp2 + 1, "./etc/", 6)) {
				if (p->n == p->cap) {
					p->cap = p->cap ? p->cap * 2 : 256;
					p->path = kb_realloc(p->path, (size_t)p->cap
							     * sizeof(char *));
					p->sha = kb_realloc(p->sha, (size_t)p->cap
							    * sizeof(char *));
				}
				*sp = 0;
				p->sha[p->n] = kb_strdup(l);
				p->path[p->n] = kb_strdup(sp2 + 3);
				p->n++;
			}
			l = nl ? nl + 1 : NULL;
		}
		free(d);
	}
	kb_strv_free(names);
}

static void pristine_free(Pristine *p)
{
	for (int i = 0; i < p->n; i++) {
		free(p->path[i]);
		free(p->sha[i]);
	}
	free(p->path);
	free(p->sha);
}

/* 1 when `rel` holds the bytes its package installed. */
static int untouched(const Pristine *p, const char *full, const char *rel)
{
	char got[65];
	for (int i = 0; i < p->n; i++)
		if (!strcmp(p->path[i], rel))
			return hash_file(full, -1, got) == 0 &&
			       !strcmp(got, p->sha[i]);
	return 0;
}

static int in_list(const char *const *l, const char *s)
{
	for (; *l; l++)
		if (!strcmp(*l, s))
			return 1;
	return 0;
}

/* `src`'s mode, owner and times onto `dst`. */
static void same_meta(const struct stat *st, const char *dst)
{
	struct timespec ts[2] = { st->st_atim, st->st_mtim };
	/* An owner that cannot be set leaves root's, which is what a file
	 * root copies already has. */
	if (geteuid() == 0 && lchown(dst, st->st_uid, st->st_gid) != 0)
		errno = 0;
	if (!S_ISLNK(st->st_mode))
		chmod(dst, st->st_mode & 07777);
	utimensat(AT_FDCWD, dst, ts, AT_SYMLINK_NOFOLLOW);
}

static int copy_plain(const char *src, const char *dst, const struct stat *st)
{
	char tmp[4300], buf[1 << 16];
	ssize_t r;
	int in = open(src, O_RDONLY | O_NOFOLLOW | O_CLOEXEC), out;
	if (in < 0)
		return -1;
	tmp_beside(tmp, sizeof(tmp), dst, "carry");
	out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
		   0600);
	if (out < 0) {
		close(in);
		return -1;
	}
	while ((r = read(in, buf, sizeof(buf))) > 0)
		if (write_all(out, buf, (size_t)r) != 0) {
			r = -1;
			break;
		}
	close(in);
	if (close(out) != 0 || r < 0) {
		unlink(tmp);
		return -1;
	}
	same_meta(st, tmp);
	if (rename(tmp, dst) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

/* `src` (a file, link or tree) in place of whatever `dst` holds. */
static int copy_tree(const char *src, const char *dst)
{
	struct stat st;
	if (lstat(src, &st) != 0)
		return -1;
	if (S_ISREG(st.st_mode)) {
		struct stat ds;
		if (lstat(dst, &ds) == 0 && S_ISDIR(ds.st_mode))
			kb_rmtree(dst);
		return copy_plain(src, dst, &st);
	}
	if (S_ISLNK(st.st_mode)) {
		char t[4096];
		ssize_t n = readlink(src, t, sizeof(t) - 1);
		if (n < 0)
			return -1;
		t[n] = 0;
		kb_rmtree(dst);
		if (symlink(t, dst) != 0)
			return -1;
		same_meta(&st, dst);
		return 0;
	}
	if (!S_ISDIR(st.st_mode))
		return 0;
	struct stat ds;
	if (lstat(dst, &ds) != 0 || !S_ISDIR(ds.st_mode)) {
		kb_rmtree(dst);
		if (mkdir(dst, 0700) != 0)
			return -1;
	}
	int n = 0, rc = 0;
	char **names = kb_listdir(src, &n);
	for (int i = 0; names && i < n; i++) {
		char *s = kb_path_join(src, names[i]);
		char *d = kb_path_join(dst, names[i]);
		if (copy_tree(s, d) != 0)
			rc = -1;
		free(s);
		free(d);
	}
	kb_strv_free(names);
	/* What the slot's copy held and the running root's does not. */
	char **old = kb_listdir(dst, &n);
	for (int i = 0; old && i < n; i++) {
		char *s = kb_path_join(src, old[i]);
		struct stat x;
		if (lstat(s, &x) != 0) {
			char *d = kb_path_join(dst, old[i]);
			kb_rmtree(d);
			free(d);
		}
		free(s);
	}
	kb_strv_free(old);
	same_meta(&st, dst);
	return rc;
}

/* The parent directories of `rel` in the slot, made with the running root's
 * modes where the slot lacks them. */
static void parents(const char *run, const char *slot, const char *rel)
{
	char part[4200];
	for (const char *s = strchr(rel, '/'); s; s = strchr(s + 1, '/')) {
		char sp[4300], rp[4300];
		struct stat st;
		snprintf(part, sizeof(part), "%.*s", (int)(s - rel), rel);
		join(sp, sizeof(sp), slot, part);
		if (lstat(sp, &st) == 0)
			continue;
		join(rp, sizeof(rp), run, part);
		if (mkdir(sp, 0755) == 0 && lstat(rp, &st) == 0)
			same_meta(&st, sp);
	}
}

/* `data` into a new file only its owner can read, which same_meta then gives
 * the mode of the file it replaces: shadow's hashes are never readable, not
 * even under a temporary name, in a slot mounted where anyone can look. */
static int write_private(const char *path, const char *data, size_t n)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
		      0600);
	if (fd < 0)
		return -1;
	int rc = write_all(fd, data, n);
	if (close(fd) != 0)
		rc = -1;
	return rc;
}

/* Account lines merged by their first field. */
static int merge_accounts(const char *rp, const char *sp)
{
	struct stat st;
	char *r = kb_read_all(rp, NULL), *s = kb_read_all(sp, NULL);
	KbBuf b = {0};
	int rc = -1;

	if (!r || lstat(rp, &st) != 0)
		goto out;
	kb_buf_str(&b, r);
	if (b.n && b.p[b.n - 1] != '\n')
		kb_buf_str(&b, "\n");
	for (char *l = s; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		size_t kl = strcspn(l, ":");
		int have = 0;
		if (l[kl] == ':' && kl) {
			for (char *q = r; q && *q && !have;) {
				if (!strncmp(q, l, kl + 1))
					have = 1;
				q = strchr(q, '\n');
				if (q)
					q++;
			}
			if (!have)
				kb_buf_printf(&b, "%s\n", l);
		}
		l = nl ? nl + 1 : NULL;
	}
	char tmp[4300];
	tmp_beside(tmp, sizeof(tmp), sp, "accounts");
	if (write_private(tmp, b.p ? b.p : "", b.n) != 0) {
		unlink(tmp);
		goto out;
	}
	same_meta(&st, tmp);
	rc = rename(tmp, sp) == 0 ? 0 : -1;
	if (rc)
		unlink(tmp);
out:
	free(r);
	free(s);
	kb_buf_free(&b);
	return rc;
}

/* The first field of the fstab line whose mount point is `/`, or "". */
static void fstab_root(const char *data, char *out, size_t cap)
{
	out[0] = 0;
	for (const char *l = data; l && *l;) {
		const char *nl = strchr(l, '\n');
		size_t n = nl ? (size_t)(nl - l) : strlen(l);
		char line[1024], dev[512], mp[512];
		snprintf(line, sizeof(line), "%.*s", (int)n, l);
		if (line[0] != '#' && sscanf(line, "%511s %511s", dev, mp) == 2 &&
		    !strcmp(mp, "/")) {
			kb_strlcpy(out, dev, cap);
			return;
		}
		l = nl ? nl + 1 : NULL;
	}
}

/* The running root's fstab with the slot's own root device: `root_dev` when
 * the caller knows it (an image's fstab names no root), else the slot
 * fstab's. Left alone when neither says, or the running fstab has no root
 * line: a guess there is a slot that cannot boot. */
static int carry_fstab(const char *rp, const char *sp, const char *root_dev)
{
	char rdev[512], sdev[512];
	struct stat st;
	char *r = kb_read_all(rp, NULL), *s = kb_read_all(sp, NULL);
	int rc = 0;

	if (r && s && lstat(rp, &st) == 0) {
		fstab_root(r, rdev, sizeof(rdev));
		fstab_root(s, sdev, sizeof(sdev));
		if (root_dev && *root_dev)
			kb_strlcpy(sdev, root_dev, sizeof(sdev));
		if (rdev[0] && sdev[0]) {
			KbBuf b = {0};
			for (char *l = r; l && *l;) {
				char *nl = strchr(l, '\n');
				if (nl)
					*nl = 0;
				char dev[512], mp[512];
				if (l[0] != '#' &&
				    sscanf(l, "%511s %511s", dev, mp) == 2 &&
				    !strcmp(mp, "/") && !strcmp(dev, rdev)) {
					/* The device field where it is, so a
					 * line indented by hand keeps the rest
					 * of its fields. */
					const char *at = strstr(l, rdev);
					kb_buf_printf(&b, "%.*s%s%s\n",
						      (int)(at - l), l, sdev,
						      at + strlen(rdev));
				} else
					kb_buf_printf(&b, "%s\n", l);
				l = nl ? nl + 1 : NULL;
			}
			char tmp[4300];
			tmp_beside(tmp, sizeof(tmp), sp, "fstab");
			rc = kb_write_file(tmp, b.p) < 0 ? -1 : 0;
			if (rc == 0) {
				same_meta(&st, tmp);
				rc = rename(tmp, sp) == 0 ? 0 : -1;
			}
			if (rc)
				unlink(tmp);
			kb_buf_free(&b);
		}
	}
	free(r);
	free(s);
	return rc;
}

static int carry_etc(const char *run, const char *slot, const char *rel,
		     const Pristine *p, const char *root_dev, int *nnew)
{
	char rdir[4200];
	int n = 0, rc = 0;
	join(rdir, sizeof(rdir), run, rel);
	char **names = kb_listdir(rdir, &n);

	for (int i = 0; names && i < n; i++) {
		char crel[4200], rp[4300], sp[4300];
		struct stat st, ss;
		snprintf(crel, sizeof(crel), "%s/%s", rel, names[i]);
		join(rp, sizeof(rp), run, crel);
		join(sp, sizeof(sp), slot, crel);
		if (lstat(rp, &st) != 0 || in_list(CARRY_SKIP, crel))
			continue;
		size_t cl = strlen(crel);
		if (cl > 9 && !strcmp(crel + cl - 9, ".kdos-new"))
			continue;
		int have = lstat(sp, &ss) == 0;
		if (S_ISDIR(st.st_mode)) {
			if (have && !S_ISDIR(ss.st_mode))
				continue;	/* the slot made it a file */
			if (carry_etc(run, slot, crel, p, root_dev, nnew) != 0)
				rc = -1;
			continue;
		}
		if (in_list(CARRY_ACCOUNTS, crel)) {
			if (S_ISREG(st.st_mode) && have &&
			    merge_accounts(rp, sp) != 0)
				rc = -1;
			else if (!have && (parents(run, slot, crel),
					   copy_tree(rp, sp) != 0))
				rc = -1;
			continue;
		}
		if (!strcmp(crel, "etc/fstab")) {
			if (have ? carry_fstab(rp, sp, root_dev) != 0
				 : (parents(run, slot, crel),
				    copy_tree(rp, sp) != 0))
				rc = -1;
			continue;
		}
		if (S_ISLNK(st.st_mode)) {
			/* A link replaces only a link or nothing: where the
			 * slot now has a real file, the newer layout wins. */
			if (have && !S_ISLNK(ss.st_mode))
				continue;
			parents(run, slot, crel);
			if (copy_tree(rp, sp) != 0)
				rc = -1;
			continue;
		}
		if (!S_ISREG(st.st_mode) || untouched(p, rp, crel))
			continue;
		if (have && S_ISREG(ss.st_mode)) {
			char a[65], b[65];
			if (hash_file(rp, -1, a) == 0 &&
			    hash_file(sp, -1, b) == 0 && !strcmp(a, b))
				continue;
			char keep[4400];
			snprintf(keep, sizeof(keep), "%s.kdos-new", sp);
			if (rename(sp, keep) == 0)
				(*nnew)++;
		} else if (have) {
			continue;	/* the slot made it a directory */
		}
		parents(run, slot, crel);
		if (copy_tree(rp, sp) != 0)
			rc = -1;
	}
	kb_strv_free(names);
	return rc;
}

/* `carry = PATH` lines of the running root's update.conf, after the list
 * above. A path is relative to the root, never `..` and never /etc, which
 * the per-file rule already carries. */
static char **carry_extra(const char *run, int *n)
{
	char p[4200];
	char **v = NULL;
	int cap = 0;
	*n = 0;
	join(p, sizeof(p), run, "etc/kdos/update.conf");
	char *d = kb_read_all(p, NULL);
	for (char *l = d; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (!strncmp(l, "carry", 5) && (l[5] == ' ' || l[5] == '=' ||
						l[5] == '\t')) {
			char *x = l + 5;
			while (*x == ' ' || *x == '\t' || *x == '=')
				x++;
			x[strcspn(x, " \t\r#")] = 0;
			while (*x == '/')
				x++;
			if (*x && path_ok(x) && strncmp(x, "etc", 3)) {
				if (*n + 1 >= cap) {
					cap = cap ? cap * 2 : 8;
					v = kb_realloc(v, (size_t)cap *
						       sizeof(char *));
				}
				v[(*n)++] = kb_strdup(x);
				v[*n] = NULL;
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(d);
	return v;
}

/*
 * The running root's configuration and state into `slot`. `db` is the
 * running root's package database directory; `root_dev` the slot's own fstab
 * root device (`UUID=…`), or NULL to keep the one its fstab names. 0 done;
 * -1 with the first failure in `why` (the slot is then not armed).
 */
int kdt_image_carry(const char *run, const char *slot, const char *db,
		    const char *root_dev, char *why, size_t wcap)
{
	Pristine p;
	int nnew = 0, rc = 0, nx = 0;
	char rp[4200], sp[4200];
	struct stat st;

	why[0] = 0;
	join(rp, sizeof(rp), run, "etc");
	join(sp, sizeof(sp), slot, "etc");
	if (lstat(rp, &st) != 0 || !S_ISDIR(st.st_mode)) {
		snprintf(why, wcap, "%s has no /etc to carry", run);
		return -1;
	}
	if (lstat(sp, &st) != 0 || !S_ISDIR(st.st_mode)) {
		snprintf(why, wcap, "%s has no /etc to carry into", slot);
		return -1;
	}
	pristine_load(&p, db);
	if (carry_etc(run, slot, "etc", &p, root_dev, &nnew) != 0) {
		snprintf(why, wcap, "the configuration under /etc could not "
			 "all be carried into %s", slot);
		rc = -1;
	}
	pristine_free(&p);

	char **extra = carry_extra(run, &nx);
	for (int pass = 0; pass < 2; pass++) {
		const char *const *list = pass ? (const char *const *)extra
					       : CARRY_STATE;
		for (int i = 0; list && list[i]; i++) {
			join(rp, sizeof(rp), run, list[i]);
			join(sp, sizeof(sp), slot, list[i]);
			if (lstat(rp, &st) != 0) {
				/* Gone from the running root: a file goes from
				 * the slot too, a tree the slot holds stays. */
				struct stat ss;
				if (lstat(sp, &ss) == 0 && !S_ISDIR(ss.st_mode))
					unlink(sp);
				continue;
			}
			parents(run, slot, list[i]);
			if (copy_tree(rp, sp) != 0 && !rc) {
				snprintf(why, wcap, "/%s could not be carried "
					 "into %s", list[i], slot);
				rc = -1;
			}
		}
	}
	kb_strv_free(extra);
	if (nnew)
		printf("%d configuration file%s kept from this system; the "
		       "new default%s beside %s as .kdos-new\n", nnew,
		       nnew == 1 ? "" : "s", nnew == 1 ? " is" : "s are",
		       nnew == 1 ? "it" : "them");
	return rc;
}

/* ── the command ───────────────────────────────────────────────────────── */

static const char *im_keys(const char *flag)
{
	const char *e = getenv("KPKG_KEYRING");
	return flag ? flag : e && *e ? e : "/etc/kdos/keys";
}

static void as_url(const char *v, char *out, size_t cap)
{
	if (strstr(v, "://"))
		kb_strlcpy(out, v, cap);
	else if (v[0] == '/')
		snprintf(out, cap, "file://%s", v);
	else {
		char cwd[2048];
		snprintf(out, cap, "file://%s/%s",
			 getcwd(cwd, sizeof(cwd)) ? cwd : "", v);
	}
	size_t n = strlen(out);
	while (n > 1 && out[n - 1] == '/')
		out[--n] = 0;
}

/*
 * The signed manifest at `origin` (a URL or a directory), through the release
 * index there: the index is fetched into `dir` and verified against `keys`
 * (relidx.c), the manifest and its signature are downloaded where the index
 * says and kept only with the index's hashes, and the manifest is verified
 * against `keys` again and must name the index's version. The pair replaces
 * what `dir` held as MANIFEST and MANIFEST.sig only once all of that holds.
 * NULL with the reason in `why` and in `how`: KDT_IMAGE_OFFLINE, _ABSENT
 * (nothing published there), _UNVERIFIED, or _FAILED.
 */
KdtImage *kdt_image_fetch(const char *origin, const char *dir,
			  const char *keys, int *how, char *why,
			  size_t wcap)
{
	static const char *const keep[] = { "INDEX", "INDEX.sig", "ORIGIN",
					    "MANIFEST", "MANIFEST.sig", NULL };
	char nd[4200], mp[4300], sp[4300], url[4400];
	KdtImage *m = NULL;

	*how = KDT_IMAGE_FAILED;
	/* Everything lands in <dir>/new and moves up only once all of it
	 * holds, so a refused fetch leaves the index and manifest that were
	 * cached together. */
	snprintf(nd, sizeof(nd), "%s/new", dir);
	kb_rmtree(nd);
	KdtRelIdx *ix = kdt_relidx_fetch(origin, nd, keys, how, why, wcap);
	if (!ix) {
		if (*how == KDT_IMAGE_ABSENT)
			snprintf(why, wcap, "no slot image is published at %s",
				 origin);
		goto out;
	}
	snprintf(mp, sizeof(mp), "%s/MANIFEST", nd);
	snprintf(sp, sizeof(sp), "%s/MANIFEST.sig", nd);
	if (kdt_relidx_url(ix, "image/" IM_MANIFEST, url, sizeof(url), NULL,
			   NULL) != 0) {
		*how = KDT_IMAGE_ABSENT;
		snprintf(why, wcap, "no slot image is published at %s", origin);
		goto out;
	}
	int got = kdt_relidx_take(ix, "image/" IM_MANIFEST ".sig", sp, why,
				  wcap);
	if (got == KDT_GOT_OK)
		got = kdt_relidx_take(ix, "image/" IM_MANIFEST, mp, why, wcap);
	if (got != KDT_GOT_OK) {
		*how = got == KDT_GOT_ABSENT ? KDT_IMAGE_UNVERIFIED : got;
		goto out;
	}
	m = kdt_image_load(mp, sp, keys, why, wcap);
	if (m && strcmp(m->version, kdt_relidx_version(ix))) {
		snprintf(why, wcap, "the slot image is %s and the release index "
			 "is %s", m->version, kdt_relidx_version(ix));
		kdt_image_free(m);
		m = NULL;
	}
	if (!m) {
		*how = KDT_IMAGE_UNVERIFIED;
		goto out;
	}
	for (int i = 0; keep[i]; i++) {
		char from[4400], to[4400];
		snprintf(from, sizeof(from), "%s/%s", nd, keep[i]);
		snprintf(to, sizeof(to), "%s/%s", dir, keep[i]);
		if (rename(from, to) != 0) {
			snprintf(why, wcap, "cannot keep the manifest in %s", dir);
			kdt_image_free(m);
			m = NULL;
			break;
		}
	}
out:
	kdt_relidx_free(ix);
	kb_rmtree(nd);
	return m;
}

static void cli_tick(int done, int total, void *d)
{
	(void)d;
	printf("\r  %d of %d files", done, total);
	fflush(stdout);
}

static int cmd_unpack(int argc, char **argv)
{
	const char *src = NULL, *target = NULL, *seed = NULL, *keys = NULL;
	char origin[4200], why[512], scratch[4300];
	int how;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--seed") && i + 1 < argc)
			seed = argv[++i];
		else if (!strcmp(argv[i], "--keys") && i + 1 < argc)
			keys = argv[++i];
		else if (argv[i][0] != '-' && !src)
			src = argv[i];
		else if (argv[i][0] != '-' && !target)
			target = argv[i];
		else {
			fprintf(stderr, "usage: kdos image unpack DIR|URL TARGET "
					"[--seed ROOT] [--keys DIR]\n");
			return 2;
		}
	}
	if (!src || !target) {
		fprintf(stderr, "usage: kdos image unpack DIR|URL TARGET "
				"[--seed ROOT] [--keys DIR]\n");
		return 2;
	}
	as_url(src, origin, sizeof(origin));
	if (kb_mkdir_p(target) != 0 || is_running_root(target)) {
		fprintf(stderr, "kdos image: %s is not a root to write\n",
			target);
		return 2;
	}
	snprintf(scratch, sizeof(scratch), "%s/.kdos-image-manifest", target);
	KdtImage *m = kdt_image_fetch(origin, scratch, im_keys(keys), &how,
				      why, sizeof(why));
	if (!m) {
		kb_rmtree(scratch);
		fprintf(stderr, "kdos image: %s\n", why);
		return 3;
	}
	char arch[32];
	kp_arch(arch, sizeof(arch));
	if (strcmp(m->arch, arch)) {
		kb_rmtree(scratch);
		fprintf(stderr, "kdos image: the image is for %s, this machine "
				"is %s\n", m->arch, arch);
		kdt_image_free(m);
		return 3;
	}
	KdtRelIdx *ix = kdt_relidx_load(scratch, im_keys(keys), why,
					sizeof(why));
	if (!ix) {
		kb_rmtree(scratch);
		kdt_image_free(m);
		fprintf(stderr, "kdos image: %s\n", why);
		return 3;
	}
	printf("==> image %s, signed by %s (key %s), into %s\n", m->version,
	       m->keyname[0] ? m->keyname : "a trusted key", m->who, target);
	int rc = kdt_image_fill(m, ix, target, seed,
				isatty(STDOUT_FILENO) ? cli_tick : NULL, NULL,
				NULL, why, sizeof(why));
	kdt_relidx_free(ix);
	if (isatty(STDOUT_FILENO))
		putchar('\n');
	kb_rmtree(scratch);
	kdt_image_free(m);
	if (rc != 0) {
		fprintf(stderr, "kdos image: %s\n", why);
		return 1;
	}
	printf("%s holds the image\n", target);
	return 0;
}

static int cmd_verify(int argc, char **argv)
{
	const char *src = NULL, *keys = NULL;
	char why[512];
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--keys") && i + 1 < argc)
			keys = argv[++i];
		else if (!src && argv[i][0] != '-')
			src = argv[i];
		else
			src = NULL, i = argc;
	}
	if (!src || strstr(src, "://")) {
		fprintf(stderr, "usage: kdos image verify DIR [--keys DIR]\n");
		return 2;
	}
	char mp[4200], sp[4300];
	snprintf(mp, sizeof(mp), "%s/" IM_MANIFEST, src);
	snprintf(sp, sizeof(sp), "%s.sig", mp);
	KdtImage *m = kdt_image_load(mp, sp, im_keys(keys), why, sizeof(why));
	if (!m) {
		fprintf(stderr, "kdos image: %s\n", why);
		return 3;
	}
	int bad = 0;
	for (int i = 0; i < m->npart + m->ndelta; i++) {
		const char *f = i < m->npart ? m->part[i].file
					     : m->delta[i - m->npart].file;
		const char *want = i < m->npart ? m->part[i].sha
						: m->delta[i - m->npart].sha;
		char p[4400], got[65];
		snprintf(p, sizeof(p), "%s/%s", src, f);
		if (kb_sha256_file(p, got) != 0 || strcmp(got, want)) {
			fprintf(stderr, "kdos image: %s does not match the "
					"manifest\n", f);
			bad = 1;
		}
	}
	printf("image %s for %s, signed by %s (key %s): %d entries, %lld "
	       "objects, %d part%s, %d delta%s%s\n", m->version, m->arch,
	       m->keyname[0] ? m->keyname : "a trusted key", m->who, m->nent,
	       m->objects, m->npart, m->npart == 1 ? "" : "s", m->ndelta,
	       m->ndelta == 1 ? "" : "s", bad ? "; FILES DIFFER" : "");
	kdt_image_free(m);
	return bad ? 1 : 0;
}

static int cmd_carry(int argc, char **argv)
{
	char why[512];
	const char *dev = NULL;
	if (argc == 4 && !strcmp(argv[2], "--root-dev"))
		dev = argv[3];
	else if (argc != 2) {
		fprintf(stderr, "usage: kdos image carry RUNNING SLOT "
				"[--root-dev UUID=…]\n");
		return 2;
	}
	const char *db = getenv("PKGDB_DIR");
	char dbp[4200];
	if (db && *db)
		kb_strlcpy(dbp, db, sizeof(dbp));
	else
		join(dbp, sizeof(dbp), argv[0], "var/lib/kpkg/db");
	if (kdt_image_carry(argv[0], argv[1], dbp, dev, why, sizeof(why)) != 0) {
		fprintf(stderr, "kdos image: %s\n", why);
		return 1;
	}
	return 0;
}

int kdt_image_main(int argc, char **argv)
{
	const char *what = argc > 0 ? argv[0] : "";
	if (!strcmp(what, "make"))
		return cmd_make(argc - 1, argv + 1);
	if (!strcmp(what, "verify"))
		return cmd_verify(argc - 1, argv + 1);
	if (!strcmp(what, "unpack"))
		return cmd_unpack(argc - 1, argv + 1);
	if (!strcmp(what, "carry"))
		return cmd_carry(argc - 1, argv + 1);
	fprintf(stderr,
		"usage: kdos image {make|verify|unpack|carry}\n"
		"  make    ROOT OUTDIR --version V [--key FILE] [--prev DIR "
		"[--prev-root DIR]]...\n"
		"          a signed slot image of ROOT and its deltas from "
		"earlier images\n"
		"  verify  DIR [--keys DIR]   the signature and every file a "
		"manifest names\n"
		"  unpack  DIR|URL TARGET [--seed ROOT] [--keys DIR]\n"
		"          write an image into TARGET, copying what ROOT "
		"already holds\n"
		"  carry   RUNNING SLOT [--root-dev UUID=…]   a running "
		"root's configuration\n"
		"          and state into a slot\n");
	return 2;
}

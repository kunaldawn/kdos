/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkpack — the pack: one file that is a filesystem
 * ---------------------------------
 *
 * A pack is an EROFS image with KDOS parts APPENDED to it:
 *
 *   +----------------------------+  offset 0
 *   |  EROFS image (zstd)        |  mounted as-is
 *   +----------------------------+  erofs_len
 *   |  metadata blob             |  flat key = value
 *   +----------------------------+
 *   |  icon.png                  |  the app's own mark, untinted
 *   +----------------------------+
 *   |  signature block           |  ksig lines, or empty
 *   +----------------------------+  sig_off + sig_len
 *   |  footer (512 bytes)        |  magic, offsets, payload hash
 *   +----------------------------+  end of file
 *
 * APPENDED RATHER THAN PREPENDED IS THE WHOLE TRICK. EROFS records the image's
 * extent in its own superblock and never reads past it, so the file is a
 * mountable filesystem exactly as it sits — measured, with 64 KiB of junk on
 * the end (testing/notes/packs-w0.txt). Userspace seeks to `filesize - 512`
 * and finds everything else from there.
 *
 * THREE RULES THIS LIBRARY EXISTS TO KEEP:
 *
 *   - A PACK THAT DOES NOT PARSE WHOLE IS ABSENT, NEVER PARTIAL. A short
 *     footer, a wrong magic, an offset past the end of the file: each answers
 *     "there is no pack here" rather than handing back half a description.
 *     Half a manifest that looks complete is what loses a tree.
 *   - THE PAYLOAD HASH IS CHECKED BEFORE THE SIGNATURE MEANS ANYTHING. The
 *     signature is over `kpk_sig_subject()`, which is the id and the hash —
 *     small, so verification never holds a 400 MB pack in memory — and that
 *     binds it to the bytes only because the bytes were hashed first.
 *   - NOTHING HERE MOUNTS, EXECS OR WRITES OUTSIDE THE FILE IT WAS GIVEN. A
 *     root daemon links this library; every line of it is code running as
 *     root.
 *
 * Links libkbase + libksig + libkpkg and nothing else.
 */

#ifndef KPACK_H
#define KPACK_H

#include <stddef.h>
#include <stdint.h>

#include "ksig.h"

#define KPK_MAGIC        "KDOSPACK"
#define KPK_MAGIC_LEN    8
#define KPK_FOOTER_LEN   512
#define KPK_FORMAT       1

#define KPK_ID_MAX       64
#define KPK_LIST_MAX     32	/* desktop/mime/command/provides/needs entries */
#define KPK_REQ_MAX      32
#define KPK_GRAFT_MAX    8
#define KPK_ENV_MAX      8
#define KPK_PATH         256

/* An index of every pack a machine can see. 512 is four times the catalogue
 * and the cap exists so a malformed index cannot make the loader allocate. */
#define KPK_INDEX_MAX    512

/* ── the footer ────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t format;
	uint32_t flags;
	uint64_t erofs_len;
	uint64_t meta_off, meta_len;
	uint64_t icon_off, icon_len;
	uint64_t sig_off, sig_len;
	uint8_t payload_sha256[32];
} KpkFooter;

/* 0 when a whole, self-consistent footer was read; -1 for every other answer,
 * including a file too short to hold one. `fsize` is filled when non-NULL. */
int kpk_footer_read(const char *path, KpkFooter *f, uint64_t *fsize);

/* The 512 bytes, little-endian, as they go on disk. */
void kpk_footer_pack(const KpkFooter *f, uint8_t out[KPK_FOOTER_LEN]);
int kpk_footer_unpack(const uint8_t in[KPK_FOOTER_LEN], KpkFooter *f);

/* ── the metadata blob ─────────────────────────────────────────────────── */

typedef enum {
	KPK_KIND_UNKNOWN = 0,
	KPK_KIND_BASE,
	KPK_KIND_RUNTIME,
	KPK_KIND_APP,
	KPK_KIND_DATA,
} KpkKind;

typedef struct {
	char name[KPK_ID_MAX];
	char op[4];			/* "", ">=", ">", "=", "<=", "<"   */
	char ver[32];
} KpkReq;

typedef struct {
	char from[KPK_PATH];		/* a path inside the pack           */
	char to[KPK_PATH];		/* under /usr/share, or a box name  */
} KpkGraft;

typedef struct {
	char id[KPK_ID_MAX];
	KpkKind kind;
	char name[128];
	char version[64];
	char release[16];
	char arch[16];
	char summary[256];
	char description[1024];
	char category[64];
	char licence[64];

	KpkReq req[KPK_REQ_MAX];
	int nreq;
	char provides[KPK_LIST_MAX][KPK_ID_MAX];
	int nprov;
	char desktop[KPK_LIST_MAX][128];
	int ndesktop;
	char mime[KPK_LIST_MAX][128];
	int nmime;
	char command[KPK_LIST_MAX][KPK_ID_MAX];
	int ncmd;
	/* A pack whose application is useless without a dataset names it, and
	 * `kdos app show` prints it. A program whose database is empty is not
	 * shipped software, it is a broken menu entry. */
	char needs[KPK_LIST_MAX][KPK_ID_MAX];
	int nneeds;

	/* kind = data only. Declarations the daemon interprets — there is no
	 * script in a pack and nothing here is ever handed to a shell. */
	KpkGraft graft[KPK_GRAFT_MAX];
	int ngraft;
	KpkGraft boxgraft[KPK_GRAFT_MAX];
	int nboxgraft;
	char env[KPK_ENV_MAX][256];
	int nenv;

	unsigned long long size;	/* the pack file                    */
	unsigned long long installed;	/* the image unpacked               */
	int launch_cold;		/* ms, measured; 0 = never measured */
	int recommended;		/* preselected by the installer     */
} KpkMeta;

/*
 * Flat `key = value`, repeatable keys, `#` comments. An unterminated or
 * over-long value reads as EMPTY rather than running off the end of the blob,
 * and an unknown key is ignored — a pack from a newer KDOS must not become
 * unreadable because it says one more thing.
 *
 * Returns 0 on a blob that parsed, -1 on one that could not be read at all.
 */
int kpk_meta_parse(const char *text, size_t len, KpkMeta *m);

/* The canonical rendering: keys in a fixed order, repeats in the order they
 * were added. `kpk_meta_parse` of this yields the same struct, which is what
 * lets a pack round-trip through `kdos-pack extract-meta`. */
char *kpk_meta_render(const KpkMeta *m, size_t *len);

/* Is this metadata usable? Fills `err` with the first thing wrong. */
int kpk_meta_valid(const KpkMeta *m, char *err, size_t cap);

const char *kpk_kind_name(KpkKind k);
KpkKind kpk_kind_parse(const char *s);

/* ── a pack, opened ────────────────────────────────────────────────────── */

typedef struct {
	char path[KPK_PATH * 2];
	KpkFooter foot;
	KpkMeta meta;
	uint64_t fsize;
} KpkPack;

/*
 * Read the footer and the metadata. Does NOT hash the payload and does NOT
 * check a signature: opening is cheap and is what `list` does over a
 * directory, while verifying is what happens once, at the point of use.
 */
int kpk_open(const char *path, KpkPack *p);

/* The icon, into a freshly allocated buffer. NULL when the pack has none. */
void *kpk_icon_read(const KpkPack *p, size_t *len);

/* ── hashing, signing, verifying ───────────────────────────────────────── */

/* SHA-256 over bytes [0, sig_off) — the filesystem, the metadata and the icon,
 * which is everything a signature is meant to cover. Streamed. */
int kpk_payload_hash(const char *path, const KpkFooter *f, char out[65]);

/*
 * What a signature is actually over:
 *
 *     kdos-pack-1\n<id>\n<payload sha256 hex>\n
 *
 * Small, so signing and verification never buffer the pack; and it names the
 * pack as well as its bytes, so a signature cannot be lifted onto another
 * artefact with the same content under a different id.
 */
void kpk_sig_subject(const char *id, const char *hashhex, char out[192]);

typedef enum {
	KPK_SIG_NONE = 0,	/* no signature block — allowed, and said     */
	KPK_SIG_GOOD,		/* verified against a key in the ring         */
	KPK_SIG_BAD,		/* a block that no trusted key verifies       */
	KPK_SIG_HASH,		/* the payload does not hash to the footer    */
} KpkSigState;

/*
 * Hash the payload, compare it with the footer, then verify the block. THE
 * ORDER IS THE POINT: a signature over a subject naming a hash says nothing
 * until the bytes have been shown to hash to it.
 *
 * A BAD SIGNATURE IS NOT A MISSING ONE. The packs on a medium you booted are
 * signed by nobody, and shipping a key in the image would be asking you to
 * trust whoever built it — which is the question signing exists to let you
 * answer yourself. So KPK_SIG_NONE is a state a caller may accept, and
 * KPK_SIG_BAD never is.
 */
KpkSigState kpk_verify(const KpkPack *p, const KsigRing *ring,
		       char who[KSIG_ID_HEX]);

const char *kpk_sig_state_name(KpkSigState s);

/* Append a signature line to an existing pack, rewriting its footer. */
int kpk_sign(const char *path, const uint8_t seed[KSIG_SEED_LEN],
	     const uint8_t pub[KSIG_PUB_LEN]);

/* ── writing one ───────────────────────────────────────────────────────── */

/*
 * Assemble `out` from an EROFS image, a metadata struct and an optional icon.
 * The image is copied byte for byte: this function does not run mkfs.erofs and
 * has no opinion about what is in the filesystem.
 */
int kpk_write(const char *out, const char *erofs_image, const KpkMeta *m,
	      const void *icon, size_t iconlen);

/* ── the solve ─────────────────────────────────────────────────────────── */

/*
 * Order `want` and everything it requires so that nothing is mounted before
 * what it needs. `avail` is every pack the caller can see, AS POINTERS — a
 * KpkMeta is tens of kilobytes and an array of them is not something to put on
 * a stack; `order` receives indices into it, dependencies first.
 *
 * A requirement is satisfied by an id or by a `provides` name, and its version
 * comparison is kp_vercmp out of libkpkg — the one implementation of "is this
 * version newer" that kpkg and kdos-portup already share. A third would
 * eventually disagree with the other two.
 *
 * Returns the number of entries written, or -1 with `err` naming the first
 * requirement that could not be met.
 */
int kpk_solve(const KpkMeta *const *avail, int navail, const char *const *want,
	      int nwant, int *order, int max, char *err, size_t errcap);

/* Does `m` satisfy `r`? */
int kpk_req_met(const KpkReq *r, const KpkMeta *m);

/* ── the index ─────────────────────────────────────────────────────────── */

/*
 * kpkg's PACKAGES shape — single-character keys, one stanza per pack, a blank
 * line between — because it parses in sixty lines and reads fine in a pager,
 * and because a second index format in one distro is a second thing to sign.
 *
 *   P:app.gimp          the id
 *   V:3.0.4-1           version-release
 *   A:x86_64
 *   K:app               kind
 *   S:96468992          the pack file's size
 *   C:<sha256>          of the pack file, which is what makes the index's own
 *                       signature cover every pack in it transitively
 *   F:app.gimp.kpack    the file name, relative to the index
 *   O:<file>            present only on a delta: the pack it patches
 *   R:yes               the installer preselects it
 *   T:<one line>        the pack's own summary
 *
 * `R:` and `T:` are here rather than only in the pack's own metadata so that
 * a reader with no libkpack — kinstall links libkbase and libktui and nothing
 * else — can answer "what does KDOS suggest, and what is this" from a flat
 * file. A list of ids and byte counts is not a page anybody can choose from.
 */
typedef struct {
	char id[KPK_ID_MAX];
	char version[64];
	char release[16];
	char arch[16];
	char kind[16];
	char file[KPK_PATH];
	char sha256[65];
	char from[KPK_PATH];	/* a delta's base file, or ""               */
	char summary[128];	/* one line, for a reader with no libkpack  */
	int recommended;
	unsigned long long size;
} KpkIndexEnt;

typedef struct {
	KpkIndexEnt ent[KPK_INDEX_MAX];
	int n;
	int truncated;		/* the index held more than the cap         */
} KpkIndex;

/* Parse a PACKAGES file. Returns the entry count, or -1 when it cannot be
 * read. A stanza missing P: or C: is dropped, not half-recorded. */
int kpk_index_load(KpkIndex *ix, const char *path);

/* Verify PACKAGES against PACKAGES.sig beside it. Same three states. */
KpkSigState kpk_index_verify(const char *path, const KsigRing *ring,
			     char who[KSIG_ID_HEX]);

/* The NEWEST whole pack with that id — never a delta, which is a route to a
 * pack rather than a pack. */
const KpkIndexEnt *kpk_index_find(const KpkIndex *ix, const char *id);

/* The delta that reconstructs `id` (at `version`, when given) from the pack
 * file `havefile`, or NULL when there is no such shortcut. */
const KpkIndexEnt *kpk_index_delta(const KpkIndex *ix, const char *id,
				   const char *version, const char *havefile);

/* Write one, sorted by id so the same set of packs always produces the same
 * bytes and therefore the same signature. */
int kpk_index_write(const KpkIndex *ix, const char *path);

#endif /* KPACK_H */

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libksig — Ed25519 signatures, and nothing else
 * ---------------------------------
 */

#ifndef KSIG_H
#define KSIG_H

#include <stddef.h>
#include <stdint.h>

#define KSIG_PUB_LEN    32
#define KSIG_SEED_LEN   32
#define KSIG_SIG_LEN    64
/* 16 hex characters of SHA-256(public key). An IDENTIFIER for picking a key out
 * of a keyring and for telling a person which key signed something — never a
 * credential, and never enough to verify anything on its own. */
#define KSIG_ID_HEX     17	/* 16 + NUL */

/* A keyring is a directory of `*.pub` files. Sixteen keys is a distro's worth of
 * rotation; the cap exists so the loader cannot be made to allocate. */
#define KSIG_MAX_KEYS   16

typedef struct {
	uint8_t pub[KSIG_PUB_LEN];
	char id[KSIG_ID_HEX];
	char name[64];		/* free-form label from the file */
} KsigKey;

typedef struct {
	KsigKey key[KSIG_MAX_KEYS];
	int n;
} KsigRing;

/* ── keys ──────────────────────────────────────────────────────────────── */

/* A fresh key pair from the kernel's CSPRNG. Returns 0, or -1 when the system
 * would not give randomness — which is a refusal to generate a key, never a
 * fallback to something weaker. */
int ksig_keygen(uint8_t seed[KSIG_SEED_LEN], uint8_t pub[KSIG_PUB_LEN]);

void ksig_keyid(const uint8_t pub[KSIG_PUB_LEN], char out[KSIG_ID_HEX]);

/* Text files, one line each, because everything else in KDOS is text:
 *
 *   public:  kdos-pubkey-1 <64 hex>  <name>
 *   secret:  kdos-seckey-1 <64 hex seed> <64 hex public>
 *
 * The secret file is created 0600 and refuses to be written any other way. */
int ksig_write_public(const char *path, const uint8_t pub[KSIG_PUB_LEN],
		      const char *name);
int ksig_read_public(const char *path, uint8_t pub[KSIG_PUB_LEN], char *name,
		     size_t ncap);
int ksig_write_secret(const char *path, const uint8_t seed[KSIG_SEED_LEN],
		      const uint8_t pub[KSIG_PUB_LEN]);
int ksig_read_secret(const char *path, uint8_t seed[KSIG_SEED_LEN],
		     uint8_t pub[KSIG_PUB_LEN]);

/* ── signing ───────────────────────────────────────────────────────────── */

void ksig_sign(const uint8_t seed[KSIG_SEED_LEN], const uint8_t pub[KSIG_PUB_LEN],
	       const void *msg, size_t len, uint8_t sig[KSIG_SIG_LEN]);

/* 0 when the signature is good, -1 otherwise. Constant time in the comparison,
 * which is Monocypher's own guarantee. */
int ksig_verify(const uint8_t pub[KSIG_PUB_LEN], const void *msg, size_t len,
		const uint8_t sig[KSIG_SIG_LEN]);

/* ── the sidecar / signature file ──────────────────────────────────────── */

/*
 * One line per signature, so a file can carry SEVERAL:
 *
 *   kdos-sig-1 <keyid> <128 hex>
 *
 * Multi-signature from day one because key rotation is cheap to design in and
 * brutal to retrofit — during a rollover both keys sign, and a client that
 * trusts either one keeps working.
 */
int ksig_sig_append(const char *path, const uint8_t seed[KSIG_SEED_LEN],
		    const uint8_t pub[KSIG_PUB_LEN], const void *msg,
		    size_t len);

/* ── keyrings ──────────────────────────────────────────────────────────── */

/* Every `*.pub` in `dir`. Returns how many were loaded; a directory that does
 * not exist is zero keys, not an error. */
int ksig_ring_load(KsigRing *ring, const char *dir);

/*
 * Verify `msg` against a signature FILE holding one or more lines.
 *
 * Returns 0 when at least one line verifies against a key IN THE RING, and
 * writes that key's id to `who`. The key id inside the file selects which key
 * to try; it is never itself trusted, which is the whole difference between a
 * key id and a key.
 */
int ksig_verify_file(const KsigRing *ring, const char *sigpath, const void *msg,
		     size_t len, char who[KSIG_ID_HEX]);

/* Same, for a file on disk rather than a buffer. */
int ksig_verify_path(const KsigRing *ring, const char *path,
		     const char *sigpath, char who[KSIG_ID_HEX]);

#endif /* KSIG_H */

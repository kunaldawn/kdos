/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libksig — Ed25519 signatures for the package index
 *
 * THE ONE PIECE OF VENDORED THIRD-PARTY SOURCE IN `src/`, and the exception is
 * narrow on purpose. KDOS argued itself out of embedding a shell (mksh) partly
 * because `src/` vendors nothing; the difference here is that there is no
 * alternative that satisfies the rules at all:
 *
 *   libsodium   a shared library, and a large one
 *   BearSSL     no Ed25519 SIGNING
 *   TweetNaCl   unmaintained since 2014
 *   OpenSSL     the opposite of "links nothing but musl"
 *
 * Monocypher is 4 files, ~120 KB of C99, public domain (CC0, BSD-2 fallback),
 * with no dependencies, no libm and no malloc — which is precisely the `libk*`
 * rule. It is vendored verbatim under `monocypher/` and never edited; the
 * version and its hash are in `monocypher/UPSTREAM`.
 *
 * Everything ABOVE Monocypher is here: the file formats, the keyring, and the
 * rule that a key id selects a key but never authorises one.
 *
 * What this library will not do:
 *
 *   - trust a key that arrived with the thing it signs. The sidecar carries a
 *     key ID so a person can be told WHICH key signed; verification uses the
 *     key from the local keyring or fails. A signature file that could supply
 *     its own key verifies nothing at all.
 *   - invent randomness. `ksig_keygen` fails rather than falling back when the
 *     kernel will not give it entropy.
 *   - hold a secret key longer than the call that uses it. Callers wipe.
 * ---------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "ksig.h"
#include "monocypher/monocypher-ed25519.h"

/* ── hex ───────────────────────────────────────────────────────────────── */

static void hex_encode(const uint8_t *in, size_t n, char *out)
{
	static const char H[] = "0123456789abcdef";
	for (size_t i = 0; i < n; i++) {
		out[i * 2] = H[in[i] >> 4];
		out[i * 2 + 1] = H[in[i] & 15];
	}
	out[n * 2] = 0;
}

static int hex_val(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Strict: the whole run must be hex and exactly `n` bytes long. A short or
 * malformed key that decoded to "as much as parsed" would verify against
 * nothing and look like a key. */
static int hex_decode(const char *s, uint8_t *out, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		int hi = hex_val(s[i * 2]), lo = hex_val(s[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return -1;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return 0;
}

/* ── keys ──────────────────────────────────────────────────────────────── */

int ksig_keygen(uint8_t seed[KSIG_SEED_LEN], uint8_t pub[KSIG_PUB_LEN])
{
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	size_t got = 0;
	while (got < KSIG_SEED_LEN) {
		ssize_t k = read(fd, seed + got, KSIG_SEED_LEN - got);
		if (k <= 0) {
			close(fd);
			return -1;
		}
		got += (size_t)k;
	}
	close(fd);

	/* Monocypher derives the 64-byte secret key from a 32-byte seed and
	 * WIPES the seed as it goes — so it is handed a copy. The seed is what
	 * KDOS stores: 32 bytes, and the 64-byte form is reconstructible from
	 * it whenever something is signed. */
	uint8_t sk[64], seed_copy[KSIG_SEED_LEN];
	memcpy(seed_copy, seed, sizeof(seed_copy));
	crypto_ed25519_key_pair(sk, pub, seed_copy);
	crypto_wipe(sk, sizeof(sk));
	return 0;
}

void ksig_keyid(const uint8_t pub[KSIG_PUB_LEN], char out[KSIG_ID_HEX])
{
	KbSha256 s;
	uint8_t digest[32];
	char full[65];

	kb_sha256_init(&s);
	kb_sha256_update(&s, pub, KSIG_PUB_LEN);
	kb_sha256_final(&s, full);
	/* kb_sha256_final hands back hex; the id is its first 16 characters. */
	(void)digest;
	memcpy(out, full, 16);
	out[16] = 0;
}

int ksig_write_public(const char *path, const uint8_t pub[KSIG_PUB_LEN],
		      const char *name)
{
	char hex[KSIG_PUB_LEN * 2 + 1];
	hex_encode(pub, KSIG_PUB_LEN, hex);

	FILE *f = fopen(path, "w");
	if (!f)
		return -1;
	fprintf(f, "kdos-pubkey-1 %s %s\n", hex, name && *name ? name : "-");
	int rc = ferror(f) ? -1 : 0;
	if (fclose(f) != 0)
		rc = -1;
	return rc;
}

int ksig_read_public(const char *path, uint8_t pub[KSIG_PUB_LEN], char *name,
		     size_t ncap)
{
	char line[512];
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	int rc = -1;
	if (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "kdos-pubkey-1 ", 14) &&
		    strlen(line + 14) >= KSIG_PUB_LEN * 2 &&
		    hex_decode(line + 14, pub, KSIG_PUB_LEN) == 0) {
			rc = 0;
			if (name && ncap) {
				const char *sp = line + 14 + KSIG_PUB_LEN * 2;
				while (*sp == ' ')
					sp++;
				kb_strlcpy(name, sp, ncap);
			}
		}
	}
	fclose(f);
	return rc;
}

int ksig_write_secret(const char *path, const uint8_t seed[KSIG_SEED_LEN],
		      const uint8_t pub[KSIG_PUB_LEN])
{
	char shex[KSIG_SEED_LEN * 2 + 1], phex[KSIG_PUB_LEN * 2 + 1];

	/* O_EXCL: a signing key is never silently replaced, and the mode is set
	 * at CREATE time rather than chmod'ed afterwards — between the two there
	 * is a window in which the key is world-readable. */
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		return -1;

	hex_encode(seed, KSIG_SEED_LEN, shex);
	hex_encode(pub, KSIG_PUB_LEN, phex);
	char line[256];
	int n = snprintf(line, sizeof(line), "kdos-seckey-1 %s %s\n", shex, phex);
	int rc = (n > 0 && write(fd, line, (size_t)n) == n) ? 0 : -1;
	crypto_wipe(line, sizeof(line));
	crypto_wipe(shex, sizeof(shex));
	if (close(fd) != 0)
		rc = -1;
	return rc;
}

int ksig_read_secret(const char *path, uint8_t seed[KSIG_SEED_LEN],
		     uint8_t pub[KSIG_PUB_LEN])
{
	struct stat st;
	char line[512];

	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	/* A signing key readable by anyone else is not a signing key. Refusing
	 * is the only useful answer: continuing would sign with a key that has
	 * to be assumed compromised. */
	if (fstat(fileno(f), &st) == 0 && (st.st_mode & 077)) {
		fclose(f);
		return -2;
	}
	int rc = -1;
	if (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "kdos-seckey-1 ", 14) &&
		    strlen(line + 14) >= KSIG_SEED_LEN * 2 + 1 + KSIG_PUB_LEN * 2 &&
		    hex_decode(line + 14, seed, KSIG_SEED_LEN) == 0 &&
		    hex_decode(line + 14 + KSIG_SEED_LEN * 2 + 1, pub,
			       KSIG_PUB_LEN) == 0)
			rc = 0;
	}
	crypto_wipe(line, sizeof(line));
	fclose(f);
	return rc;
}

/* ── signing ───────────────────────────────────────────────────────────── */

void ksig_sign(const uint8_t seed[KSIG_SEED_LEN], const uint8_t pub[KSIG_PUB_LEN],
	       const void *msg, size_t len, uint8_t sig[KSIG_SIG_LEN])
{
	uint8_t sk[64], derived_pub[KSIG_PUB_LEN], seed_copy[KSIG_SEED_LEN];

	(void)pub;	/* re-derived rather than trusted from the caller */
	memcpy(seed_copy, seed, sizeof(seed_copy));
	crypto_ed25519_key_pair(sk, derived_pub, seed_copy);
	crypto_ed25519_sign(sig, sk, msg, len);
	/* The expanded key is the secret; it does not outlive this call. */
	crypto_wipe(sk, sizeof(sk));
}

int ksig_verify(const uint8_t pub[KSIG_PUB_LEN], const void *msg, size_t len,
		const uint8_t sig[KSIG_SIG_LEN])
{
	return crypto_ed25519_check(sig, pub, msg, len) == 0 ? 0 : -1;
}

int ksig_sig_append(const char *path, const uint8_t seed[KSIG_SEED_LEN],
		    const uint8_t pub[KSIG_PUB_LEN], const void *msg,
		    size_t len)
{
	uint8_t sig[KSIG_SIG_LEN];
	char hex[KSIG_SIG_LEN * 2 + 1], id[KSIG_ID_HEX];

	ksig_sign(seed, pub, msg, len, sig);
	hex_encode(sig, KSIG_SIG_LEN, hex);
	ksig_keyid(pub, id);

	/* Appended, not written: a second key signing the same artefact adds a
	 * line rather than replacing one. */
	FILE *f = fopen(path, "a");
	if (!f)
		return -1;
	fprintf(f, "kdos-sig-1 %s %s\n", id, hex);
	int rc = ferror(f) ? -1 : 0;
	if (fclose(f) != 0)
		rc = -1;
	return rc;
}

void ksig_sig_line(char out[KSIG_LINE_MAX], const uint8_t seed[KSIG_SEED_LEN],
		   const uint8_t pub[KSIG_PUB_LEN], const void *msg, size_t len)
{
	uint8_t sig[KSIG_SIG_LEN];
	char hex[KSIG_SIG_LEN * 2 + 1], id[KSIG_ID_HEX];

	ksig_sign(seed, pub, msg, len, sig);
	hex_encode(sig, KSIG_SIG_LEN, hex);
	ksig_keyid(pub, id);
	snprintf(out, KSIG_LINE_MAX, "kdos-sig-1 %s %s\n", id, hex);
}

/* ── keyrings ──────────────────────────────────────────────────────────── */

int ksig_ring_load(KsigRing *ring, const char *dir)
{
	ring->n = 0;
	char **names = kb_listdir(dir, NULL);
	for (char **p = names; p && *p && ring->n < KSIG_MAX_KEYS; p++) {
		size_t n = strlen(*p);
		if (n < 5 || strcmp(*p + n - 4, ".pub"))
			continue;
		char *path = kb_path_join(dir, *p);
		KsigKey *k = &ring->key[ring->n];
		if (ksig_read_public(path, k->pub, k->name, sizeof(k->name)) == 0) {
			ksig_keyid(k->pub, k->id);
			ring->n++;
		}
		free(path);
	}
	kb_strv_free(names);
	return ring->n;
}

int ksig_verify_lines(const KsigRing *ring, const char *text, size_t tlen,
		      const void *msg, size_t len, char who[KSIG_ID_HEX])
{
	if (who)
		who[0] = 0;
	if (!text)
		return -1;

	for (size_t off = 0; off < tlen;) {
		size_t end = off;
		while (end < tlen && text[end] != '\n')
			end++;

		char line[512];
		size_t n = end - off;
		if (n < sizeof(line)) {
			memcpy(line, text + off, n);
			line[n] = 0;
			line[strcspn(line, "\r")] = 0;
			if (!strncmp(line, "kdos-sig-1 ", 11)) {
				const char *id = line + 11;
				if (strlen(id) >= 16 + 1 + KSIG_SIG_LEN * 2) {
					const char *hex = id + 17;
					uint8_t sig[KSIG_SIG_LEN];
					if (hex_decode(hex, sig, KSIG_SIG_LEN) == 0) {
						/*
						 * The id in the block picks WHICH
						 * key to try. Every key in the ring
						 * is still an acceptable answer, so
						 * a block naming an unknown id is
						 * not fatal — it just selects
						 * nothing, and the signature is
						 * tried against every trusted key.
						 * What matters is that nothing
						 * outside the ring is ever used.
						 */
						for (int i = 0; i < ring->n; i++) {
							if (ksig_verify(ring->key[i].pub,
									msg, len, sig) != 0)
								continue;
							if (who)
								kb_strlcpy(who,
									   ring->key[i].id,
									   KSIG_ID_HEX);
							return 0;
						}
					}
				}
			}
		}
		off = end + 1;
	}
	return -1;
}

int ksig_verify_file(const KsigRing *ring, const char *sigpath, const void *msg,
		     size_t len, char who[KSIG_ID_HEX])
{
	size_t tlen = 0;
	char *text = kb_read_all(sigpath, &tlen);
	int rc;

	if (who)
		who[0] = 0;
	if (!text)
		return -1;
	rc = ksig_verify_lines(ring, text, tlen, msg, len, who);
	free(text);
	return rc;
}

int ksig_verify_path(const KsigRing *ring, const char *path,
		     const char *sigpath, char who[KSIG_ID_HEX])
{
	size_t len = 0;
	char *data = kb_read_all(path, &len);
	if (!data)
		return -1;
	int rc = ksig_verify_file(ring, sigpath, data, len, who);
	free(data);
	return rc;
}

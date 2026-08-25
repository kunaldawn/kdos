/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   hashing a pack, and what a signature is over
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kpack.h"

/*
 * Streamed, because a pack is the size of an application and a verifier that
 * had to hold one in memory could not run on the machine that most needs it.
 */
int kpk_payload_hash(const char *path, const KpkFooter *f, char out[65])
{
	KbSha256 s;
	char buf[65536];
	uint64_t left = f->sig_off;
	FILE *fp = fopen(path, "rb");

	if (!fp)
		return -1;
	kb_sha256_init(&s);
	while (left) {
		size_t want = left > sizeof(buf) ? sizeof(buf) : (size_t)left;
		size_t got = fread(buf, 1, want, fp);
		if (got != want) {
			fclose(fp);
			return -1;
		}
		kb_sha256_update(&s, buf, got);
		left -= got;
	}
	fclose(fp);
	kb_sha256_final(&s, out);
	return 0;
}

void kpk_sig_subject(const char *id, const char *hashhex, char out[192])
{
	snprintf(out, 192, "kdos-pack-1\n%s\n%s\n", id, hashhex);
}

const char *kpk_sig_state_name(KpkSigState s)
{
	switch (s) {
	case KPK_SIG_GOOD:	return "signed";
	case KPK_SIG_NONE:	return "unsigned";
	case KPK_SIG_BAD:	return "bad signature";
	case KPK_SIG_HASH:	return "bad payload hash";
	}
	return "unknown";
}

static void hex_of(const uint8_t *b, size_t n, char *out)
{
	static const char h[] = "0123456789abcdef";
	for (size_t i = 0; i < n; i++) {
		out[i * 2] = h[b[i] >> 4];
		out[i * 2 + 1] = h[b[i] & 15];
	}
	out[n * 2] = 0;
}

KpkSigState kpk_verify(const KpkPack *p, const KsigRing *ring,
		       char who[KSIG_ID_HEX])
{
	char have[65], want[65], subject[192];
	char *block;
	FILE *fp;
	int rc;

	if (who)
		who[0] = 0;

	/*
	 * The hash first, always. The signature is over a subject that NAMES a
	 * hash, so until the file has been shown to hash to it the signature is
	 * a statement about some other bytes.
	 */
	if (kpk_payload_hash(p->path, &p->foot, have) != 0)
		return KPK_SIG_HASH;
	hex_of(p->foot.payload_sha256, 32, want);
	if (strcmp(have, want) != 0)
		return KPK_SIG_HASH;

	if (!p->foot.sig_len)
		return KPK_SIG_NONE;

	block = kb_calloc(1, (size_t)p->foot.sig_len + 1);
	fp = fopen(p->path, "rb");
	if (!fp) {
		free(block);
		return KPK_SIG_BAD;
	}
	if (fseeko(fp, (off_t)p->foot.sig_off, SEEK_SET) != 0 ||
	    fread(block, 1, (size_t)p->foot.sig_len, fp) != p->foot.sig_len) {
		fclose(fp);
		free(block);
		return KPK_SIG_BAD;
	}
	fclose(fp);

	kpk_sig_subject(p->meta.id, have, subject);
	rc = ksig_verify_lines(ring, block, (size_t)p->foot.sig_len, subject,
			       strlen(subject), who);
	free(block);
	return rc == 0 ? KPK_SIG_GOOD : KPK_SIG_BAD;
}

int kpk_sign(const char *path, const uint8_t seed[KSIG_SEED_LEN],
	     const uint8_t pub[KSIG_PUB_LEN])
{
	KpkPack p;
	char hash[65], subject[192];
	char line[KSIG_LINE_MAX];
	uint8_t fbuf[KPK_FOOTER_LEN];
	KpkFooter f;
	FILE *fp;

	if (kpk_open(path, &p) != 0)
		return -1;
	if (kpk_payload_hash(path, &p.foot, hash) != 0)
		return -1;

	kpk_sig_subject(p.meta.id, hash, subject);
	ksig_sig_line(line, seed, pub, subject, strlen(subject));

	/*
	 * Appended INSIDE the file: the block grows, the footer moves, and a
	 * second key signing later adds a line rather than replacing one. The
	 * payload hash does not change, because the block is outside what it
	 * covers — which is the reason the block sits where it does.
	 */
	f = p.foot;
	f.sig_len += strlen(line);

	fp = fopen(path, "r+b");
	if (!fp)
		return -1;
	if (fseeko(fp, (off_t)(p.foot.sig_off + p.foot.sig_len), SEEK_SET) != 0 ||
	    fwrite(line, 1, strlen(line), fp) != strlen(line)) {
		fclose(fp);
		return -1;
	}
	kpk_footer_pack(&f, fbuf);
	if (fwrite(fbuf, 1, KPK_FOOTER_LEN, fp) != KPK_FOOTER_LEN) {
		fclose(fp);
		return -1;
	}
	if (fclose(fp) != 0)
		return -1;
	/* The footer moved forward by the line, so the file is now longer than
	 * it needs to be only if it was truncated — it never is; the write
	 * above extended it. */
	return 0;
}

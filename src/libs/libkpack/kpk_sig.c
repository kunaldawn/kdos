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
#include <unistd.h>

#include "kbase.h"
#include "kpack.h"

/*
 * Streamed, because a pack is the size of an application and a verifier that
 * had to hold one in memory could not run on the machine that most needs it.
 *
 * WHAT IT COVERS IS THE FOOTER'S OWN FORMAT NUMBER TO DECIDE, and a pack is
 * hashed the way the pack says, never the way this build would prefer —
 * otherwise every artefact baked before a widening answers HASH and nothing
 * will mount it. See KPK_FORMAT.
 *
 * AND FROM FORMAT 2 THE FOOTER IS INSIDE IT. The footer is what says where the
 * payload, the metadata and the icon are; over a hash that covered only
 * [0, sig_off) it could be rewritten freely under a signature that still
 * verified — point meta_off into the payload and the pack declares whatever
 * the attacker put there while the signature is over bytes nobody disputes.
 *
 * TWO FIELDS ARE ZEROED BEFORE IT IS HASHED, and they are the two that are
 * written after: `payload_sha256`, which is this answer, and `sig_len`, which
 * grows each time a second key signs a pack that is already signed. Both are
 * covered by other means — the digest is the subject the signature names, and
 * the block it measures ends at the footer by kpk_footer_read's own rule.
 *
 * It is packed from the struct rather than read off the disk, because the
 * writer calls this before the footer it is about exists.
 */
int kpk_payload_hash(const char *path, const KpkFooter *f, char out[65])
{
	KbSha256 s;
	char buf[65536];
	uint64_t left = f->sig_off;
	FILE *fp = fopen(path, "rb");
	KpkFooter bare = *f;
	uint8_t fb[KPK_FOOTER_LEN];

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

	if (f->format >= 2) {
		memset(bare.payload_sha256, 0, sizeof(bare.payload_sha256));
		bare.sig_len = 0;
		kpk_footer_pack(&bare, fb);
		kb_sha256_update(&s, fb, KPK_FOOTER_LEN);
	}

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
	case KPK_SIG_NOKEY:	return "signed, but this machine has no key to check it with";
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
	/*
	 * Asked before the block is even read: with nothing to check against,
	 * every signature fails and every failure looks the same. The ring is
	 * a property of the machine, so this answer names the machine.
	 */
	if (!ring || ring->n == 0)
		return KPK_SIG_NOKEY;

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
	/* The block still abuts the footer, which is what kpk_footer_read
	 * requires: the line went in where the old footer started and the new
	 * footer went straight after it. Leaving a gap here would put the
	 * footer the reader seeks to behind the line just written. */
	return 0;
}

int kpk_restamp(const char *path)
{
	KpkFooter f;
	char have[65], want[65];
	uint8_t fbuf[KPK_FOOTER_LEN];
	FILE *fp;

	if (kpk_footer_read(path, &f, NULL) != 0)
		return -1;
	if (kpk_payload_hash(path, &f, have) != 0)
		return -1;
	hex_of(f.payload_sha256, 32, want);
	if (f.format == KPK_FORMAT && !strcmp(have, want))
		return 1;

	/*
	 * The format is raised FIRST and the digest taken under it, because the
	 * number is what says which span was measured — a footer declaring one
	 * format over a digest taken under another is a pack every reader
	 * disagrees with, which is the state this verb exists to leave behind.
	 */
	f.format = KPK_FORMAT;

	/*
	 * The block is outside what the digest covers, so dropping it cannot
	 * change the answer — but a footer still declaring a length would send
	 * kpk_footer_read's seek past the end of the shortened file, and the
	 * lines in it name a digest this is about to replace.
	 */
	f.sig_len = 0;

	if (kpk_payload_hash(path, &f, have) != 0)
		return -1;
	for (int i = 0; i < 32; i++) {
		unsigned v;

		if (sscanf(have + i * 2, "%2x", &v) != 1)
			return -1;
		f.payload_sha256[i] = (uint8_t)v;
	}
	kpk_footer_pack(&f, fbuf);

	/* Truncated to the payload first: the footer goes back where the old
	 * signature block began, so the block and the old footer are gone in
	 * one step rather than left as a tail behind the new one. */
	if (truncate(path, (off_t)f.sig_off) != 0)
		return -1;
	fp = fopen(path, "r+b");
	if (!fp)
		return -1;
	if (fseeko(fp, (off_t)f.sig_off, SEEK_SET) != 0 ||
	    fwrite(fbuf, 1, KPK_FOOTER_LEN, fp) != KPK_FOOTER_LEN) {
		fclose(fp);
		return -1;
	}
	return fclose(fp) == 0 ? 0 : -1;
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the footer, and assembling a pack around one
 * ---------------------------------
 *
 * The footer is packed BYTE BY BYTE rather than written as a struct. A struct
 * on disk carries the compiler's padding and the machine's byte order, and a
 * pack is an artefact that travels between machines — the one place where
 * "it works here" is not an argument.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kbase.h"
#include "kpack.h"

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static void put64(uint8_t *p, uint64_t v)
{
	for (int i = 0; i < 8; i++)
		p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static uint64_t get64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return v;
}

void kpk_footer_pack(const KpkFooter *f, uint8_t out[KPK_FOOTER_LEN])
{
	memset(out, 0, KPK_FOOTER_LEN);
	memcpy(out, KPK_MAGIC, KPK_MAGIC_LEN);
	put32(out + 8, f->format);
	put32(out + 12, f->flags);
	put64(out + 16, f->erofs_len);
	put64(out + 24, f->meta_off);
	put64(out + 32, f->meta_len);
	put64(out + 40, f->icon_off);
	put64(out + 48, f->icon_len);
	put64(out + 56, f->sig_off);
	put64(out + 64, f->sig_len);
	memcpy(out + 72, f->payload_sha256, 32);
}

int kpk_footer_unpack(const uint8_t in[KPK_FOOTER_LEN], KpkFooter *f)
{
	if (memcmp(in, KPK_MAGIC, KPK_MAGIC_LEN) != 0)
		return -1;
	memset(f, 0, sizeof(*f));
	f->format = get32(in + 8);
	f->flags = get32(in + 12);
	f->erofs_len = get64(in + 16);
	f->meta_off = get64(in + 24);
	f->meta_len = get64(in + 32);
	f->icon_off = get64(in + 40);
	f->icon_len = get64(in + 48);
	f->sig_off = get64(in + 56);
	f->sig_len = get64(in + 64);
	memcpy(f->payload_sha256, in + 72, 32);
	/* A format this build does not know is not a pack it may guess at. */
	if (f->format != KPK_FORMAT)
		return -1;
	return 0;
}

/*
 * Every offset is checked against the file's own size, and the sections are
 * required to sit in the declared order. Without this a footer claiming
 * `meta_off = 0, meta_len = 2^63` is a read of the whole address space — the
 * kb_tar base-256 lesson, on a different field.
 */
static int footer_consistent(const KpkFooter *f, uint64_t fsize)
{
	uint64_t end = fsize - KPK_FOOTER_LEN;

	if (fsize < KPK_FOOTER_LEN)
		return 0;
	if (f->erofs_len > end)
		return 0;
	if (f->meta_off < f->erofs_len || f->meta_len > end ||
	    f->meta_off > end - f->meta_len)
		return 0;
	if (f->icon_len && (f->icon_off < f->meta_off + f->meta_len ||
			    f->icon_len > end || f->icon_off > end - f->icon_len))
		return 0;
	if (f->sig_off > end || f->sig_len > end || f->sig_off > end - f->sig_len)
		return 0;
	if (f->sig_off < f->meta_off + f->meta_len)
		return 0;
	return 1;
}

int kpk_footer_read(const char *path, KpkFooter *f, uint64_t *fsize)
{
	uint8_t buf[KPK_FOOTER_LEN];
	struct stat st;
	FILE *fp;

	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
		return -1;
	if ((uint64_t)st.st_size < KPK_FOOTER_LEN)
		return -1;	/* too short to hold one: absent, not partial */

	fp = fopen(path, "rb");
	if (!fp)
		return -1;
	if (fseeko(fp, (off_t)st.st_size - KPK_FOOTER_LEN, SEEK_SET) != 0 ||
	    fread(buf, 1, KPK_FOOTER_LEN, fp) != KPK_FOOTER_LEN) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	if (kpk_footer_unpack(buf, f) != 0)
		return -1;
	if (!footer_consistent(f, (uint64_t)st.st_size))
		return -1;
	if (fsize)
		*fsize = (uint64_t)st.st_size;
	return 0;
}

/* ── opening ───────────────────────────────────────────────────────────── */

static char *read_span(const char *path, uint64_t off, uint64_t len)
{
	FILE *fp;
	char *buf;

	if (!len)
		return NULL;
	fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	buf = kb_calloc(1, (size_t)len + 1);
	if (fseeko(fp, (off_t)off, SEEK_SET) != 0 ||
	    fread(buf, 1, (size_t)len, fp) != len) {
		free(buf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	return buf;
}

int kpk_open(const char *path, KpkPack *p)
{
	char *meta;

	memset(p, 0, sizeof(*p));
	kb_strlcpy(p->path, path, sizeof(p->path));
	if (kpk_footer_read(path, &p->foot, &p->fsize) != 0)
		return -1;

	meta = read_span(path, p->foot.meta_off, p->foot.meta_len);
	if (!meta)
		return -1;
	if (kpk_meta_parse(meta, (size_t)p->foot.meta_len, &p->meta) != 0) {
		free(meta);
		return -1;
	}
	free(meta);

	/* The pack's own recorded size is advisory; what the file measures is
	 * what it costs, and a store that reported the other would be reading
	 * a number the artefact chose for itself. */
	p->meta.size = p->fsize;
	return 0;
}

void *kpk_icon_read(const KpkPack *p, size_t *len)
{
	char *b;

	if (len)
		*len = 0;
	if (!p->foot.icon_len)
		return NULL;
	b = read_span(p->path, p->foot.icon_off, p->foot.icon_len);
	if (b && len)
		*len = (size_t)p->foot.icon_len;
	return b;
}

/* ── writing ───────────────────────────────────────────────────────────── */

static int copy_span(FILE *out, const char *src, uint64_t *written)
{
	char buf[65536];
	size_t n;
	FILE *in = fopen(src, "rb");

	if (!in)
		return -1;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			fclose(in);
			return -1;
		}
		*written += n;
	}
	int bad = ferror(in);
	fclose(in);
	return bad ? -1 : 0;
}

int kpk_write(const char *out, const char *erofs_image, const KpkMeta *m,
	      const void *icon, size_t iconlen)
{
	KpkFooter f = { .format = KPK_FORMAT };
	uint8_t fbuf[KPK_FOOTER_LEN];
	uint64_t at = 0;
	size_t mlen = 0;
	char *meta;
	FILE *fp;
	char hash[65];

	meta = kpk_meta_render(m, &mlen);
	if (!meta)
		return -1;

	fp = fopen(out, "wb");
	if (!fp) {
		free(meta);
		return -1;
	}
	if (copy_span(fp, erofs_image, &at) != 0)
		goto fail;
	f.erofs_len = at;

	f.meta_off = at;
	f.meta_len = mlen;
	if (fwrite(meta, 1, mlen, fp) != mlen)
		goto fail;
	at += mlen;

	if (icon && iconlen) {
		f.icon_off = at;
		f.icon_len = iconlen;
		if (fwrite(icon, 1, iconlen, fp) != iconlen)
			goto fail;
		at += iconlen;
	}

	/* The signature block is empty at build time and is appended by
	 * `kpk_sign` — signing is a separate act, often on a separate machine,
	 * and a pack that could only be signed while it was being built could
	 * not be signed by anybody but its builder. */
	f.sig_off = at;
	f.sig_len = 0;

	kpk_footer_pack(&f, fbuf);
	if (fwrite(fbuf, 1, KPK_FOOTER_LEN, fp) != KPK_FOOTER_LEN)
		goto fail;
	if (fclose(fp) != 0) {
		free(meta);
		return -1;
	}
	free(meta);

	/* The hash covers everything before the signature block, so it can only
	 * be computed once the file exists — and the footer is then rewritten
	 * in place, which is the one write that happens after the fact. */
	if (kpk_payload_hash(out, &f, hash) != 0)
		return -1;
	for (int i = 0; i < 32; i++) {
		unsigned v;
		sscanf(hash + i * 2, "%2x", &v);
		f.payload_sha256[i] = (uint8_t)v;
	}
	kpk_footer_pack(&f, fbuf);

	fp = fopen(out, "r+b");
	if (!fp)
		return -1;
	if (fseeko(fp, (off_t)(f.sig_off + f.sig_len), SEEK_SET) != 0 ||
	    fwrite(fbuf, 1, KPK_FOOTER_LEN, fp) != KPK_FOOTER_LEN) {
		fclose(fp);
		return -1;
	}
	return fclose(fp) == 0 ? 0 : -1;

fail:
	fclose(fp);
	free(meta);
	unlink(out);
	return -1;
}

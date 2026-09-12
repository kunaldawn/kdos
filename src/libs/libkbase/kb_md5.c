/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   MD5 — and it is a FILE NAME, never a security claim.
 *
 * THE THUMBNAIL STANDARD NAMES ITS CACHE FILES BY MD5 of the source URI, and
 * every other program on the machine that writes a thumbnail does the same. A
 * stronger hash here would produce a cache nothing else could read and would
 * read nothing else's — which is the whole value of a shared cache gone, in
 * exchange for a property nobody is relying on. Nothing in this tree
 * authenticates anything with this; `kb_sha256_*` is what does that.
 *
 * HAND-ROLLED FOR THE SAME REASON THE SHA-256 IS. libkbase links nothing but
 * the C library, and every consumer of it inherits that; pulling in libcrypto
 * for sixteen bytes of file name would put OpenSSL on the link line of the
 * package manager.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kbase.h"

static uint32_t rol(uint32_t x, int c)
{
	return (x << c) | (x >> (32 - c));
}

/* The per-round shift amounts and the sine table, as RFC 1321 defines them. */
static const uint8_t S[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static const uint32_t K[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static void block(KbMd5 *s, const uint8_t *p)
{
	uint32_t m[16], a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];

	/* LITTLE-ENDIAN, and read byte by byte rather than cast: the words are
	 * defined by the standard and not by this machine's order. */
	for (int i = 0; i < 16; i++)
		m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
		       ((uint32_t)p[i * 4 + 2] << 16) |
		       ((uint32_t)p[i * 4 + 3] << 24);

	for (int i = 0; i < 64; i++) {
		uint32_t f;
		int g;

		if (i < 16) {
			f = (b & c) | (~b & d);
			g = i;
		} else if (i < 32) {
			f = (d & b) | (~d & c);
			g = (5 * i + 1) % 16;
		} else if (i < 48) {
			f = b ^ c ^ d;
			g = (3 * i + 5) % 16;
		} else {
			f = c ^ (b | ~d);
			g = (7 * i) % 16;
		}
		f += a + K[i] + m[g];
		a = d;
		d = c;
		c = b;
		b += rol(f, S[i]);
	}
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
}

void kb_md5_init(KbMd5 *s)
{
	s->h[0] = 0x67452301;
	s->h[1] = 0xefcdab89;
	s->h[2] = 0x98badcfe;
	s->h[3] = 0x10325476;
	s->len = 0;
	s->n = 0;
}

void kb_md5_update(KbMd5 *s, const void *data, size_t n)
{
	const uint8_t *p = data;

	s->len += n;
	while (n) {
		size_t take = 64 - s->n;

		if (take > n)
			take = n;
		memcpy(s->buf + s->n, p, take);
		s->n += take;
		p += take;
		n -= take;
		if (s->n == 64) {
			block(s, s->buf);
			s->n = 0;
		}
	}
}

void kb_md5_final(KbMd5 *s, char out[33])
{
	uint64_t bits = s->len * 8;
	uint8_t pad = 0x80;

	kb_md5_update(s, &pad, 1);
	pad = 0;
	while (s->n != 56)
		kb_md5_update(s, &pad, 1);
	/* The length goes in little-endian too, and `kb_md5_update` would
	 * count it — so the eight bytes are placed rather than fed. */
	for (int i = 0; i < 8; i++)
		s->buf[56 + i] = (uint8_t)(bits >> (8 * i));
	block(s, s->buf);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			snprintf(out + i * 8 + j * 2, 3, "%02x",
				 (unsigned)((s->h[i] >> (8 * j)) & 0xff));
	out[32] = '\0';
}

void kb_md5_str(const char *s, char out[33])
{
	KbMd5 m;

	kb_md5_init(&m);
	kb_md5_update(&m, s, strlen(s));
	kb_md5_final(&m, out);
}

/* libkimg — see kimg.h.
 *
 * ONE TRANSLATION UNIT, and everything but the two entry points is static.
 * That is the shape the security argument needs: `nm` on the archive shows
 * exactly what can be reached from outside, and a helper that grew a caller
 * elsewhere would have to be exported deliberately rather than by being
 * non-static in a second file.
 *
 * EVERY DECODER IS OPTIONAL. A build without one refuses that format rather
 * than failing to link, because the console desktop must come up on an image
 * that ships no decoders at all.
 */

#include "kimg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef KIMG_HAVE_PNG
#include <png.h>
#endif
#ifdef KIMG_HAVE_JPEG
#include <jpeglib.h>
#include <setjmp.h>
#endif
#ifdef KIMG_HAVE_WEBP
#include <webp/decode.h>
#endif
#ifdef KIMG_HAVE_SIXEL
#include <sixel.h>
#endif

/* ── the budget ──────────────────────────────────────────────────────────
 *
 * Checked against the size the FORMAT declares, before a decoder is handed
 * the payload. Every decode path below calls this first and returns NULL on a
 * refusal; a decoder is never entered on an image that cannot be accepted.
 */
static int within(const KimgBudget *b, long w, long h)
{
	if (!b || w <= 0 || h <= 0)
		return 0;
	if (w > b->max_w || h > b->max_h)
		return 0;

	/* Overflow first, then the budget: w * h * 4 on two ints that passed
	 * the tests above still overflows a 32-bit size_t, and a wrapped
	 * product is a small allocation followed by a large write. */
	if ((unsigned long)w > (unsigned long)-1 / 4 / (unsigned long)h)
		return 0;
	return (size_t)w * (size_t)h * 4 <= b->max_bytes;
}

/* pixman hands the image back to its destroy function, and free() does not
 * take one — a cast between the two signatures is undefined behaviour that a
 * strict build refuses outright. Four lines with the right prototype instead. */
static void free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

static pixman_image_t *from_rgba(const uint8_t *rgba, int w, int h)
{
	uint32_t *px = malloc((size_t)w * (size_t)h * 4);

	if (!px)
		return NULL;

	/* pixman's a8r8g8b8 is native-endian words with alpha in the high
	 * byte; the decoders all hand back byte-order RGBA. Built a word at a
	 * time rather than memcpy'd, so this is correct on either endianness
	 * instead of correct on the one it was written on. */
	for (long i = 0; i < (long)w * h; i++) {
		const uint8_t *p = rgba + i * 4;

		px[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) |
			((uint32_t)p[1] << 8) | (uint32_t)p[2];
	}

	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						       px, w * 4);

	if (!img) {
		free(px);
		return NULL;
	}

	/* pixman does not own the buffer it was handed; this is what frees it
	 * when the last reference goes. Without it every picture on the screen
	 * is a leak the size of the picture. */
	pixman_image_set_destroy_function(img, free_bits, px);
	return img;
}

/* ── sniffing ───────────────────────────────────────────────────────────── */

static int sniff(const uint8_t *p, size_t n)
{
	if (n >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8))
		return KIMG_PNG;
	if (n >= 3 && p[0] == 0xff && p[1] == 0xd8 && p[2] == 0xff)
		return KIMG_JPEG;
	if (n >= 12 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WEBP", 4))
		return KIMG_WEBP;
	return 0;
}

/* ── PNG ─────────────────────────────────────────────────────────────────
 *
 * The dimensions are in IHDR, which the format requires to be the first
 * chunk: an 8-byte signature, a 4-byte length, "IHDR", then two big-endian
 * 32-bit dimensions. Read straight out of the bytes rather than by starting
 * libpng, so a 65535x65535 declaration costs a comparison instead of a
 * sixteen-gigabyte allocation inside a library.
 */
#ifdef KIMG_HAVE_PNG
/*
 * SILENCE. libpng writes "libpng error: truncated" to stderr by default, and
 * the bytes that provoked it came from a pty — so a picture somebody sent you
 * decides what appears on your terminal. Both hooks are installed rather than
 * only the error one: a warning is the same channel with a different word.
 */
static void png_quiet(png_structp png, png_const_charp msg)
{
	(void)png;
	(void)msg;
}

static void png_quiet_fatal(png_structp png, png_const_charp msg)
{
	(void)msg;
	png_longjmp(png, 1);
}

struct png_src {
	const uint8_t *p;
	size_t n, pos;
};

static void png_read(png_structp png, png_bytep out, png_size_t want)
{
	struct png_src *s = png_get_io_ptr(png);

	/* SHORT READS ARE FATAL, not zero-filled. A truncated payload that
	 * decoded to a partial image would put whatever was in the buffer on
	 * somebody's screen. */
	if (!s || s->pos + want > s->n)
		png_error(png, "truncated");
	memcpy(out, s->p + s->pos, want);
	s->pos += want;
}

static pixman_image_t *decode_png(const uint8_t *p, size_t n,
				  const KimgBudget *b)
{
	if (n < 24 || memcmp(p + 12, "IHDR", 4))
		return NULL;

	long w = ((long)p[16] << 24) | ((long)p[17] << 16) |
		 ((long)p[18] << 8) | (long)p[19];
	long h = ((long)p[20] << 24) | ((long)p[21] << 16) |
		 ((long)p[22] << 8) | (long)p[23];

	if (!within(b, w, h))
		return NULL;

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
						 png_quiet_fatal, png_quiet);

	if (!png)
		return NULL;

	png_infop info = png_create_info_struct(png);

	/*
	 * VOLATILE, AND IT HAS TO BE. Both are assigned after setjmp and read
	 * in the branch longjmp returns to; a local modified between the two
	 * has an indeterminate value there unless it is volatile, so the
	 * cleanup was free()ing whatever the register happened to hold and the
	 * real allocation leaked. Caught by the fuzz driver, not by reading.
	 */
	uint8_t *volatile rgba = NULL;
	png_bytep *volatile rows = NULL;

	if (!info) {
		png_destroy_read_struct(&png, NULL, NULL);
		return NULL;
	}
	if (setjmp(png_jmpbuf(png))) {
		free(rgba);
		free(rows);
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}


	struct png_src src = { p, n, 0 };

	png_set_read_fn(png, &src, png_read);
	png_read_info(png, info);

	/* One output format, whatever came in: 8-bit RGBA, palettes expanded,
	 * transparency turned into an alpha channel, 16-bit reduced. A decoder
	 * that could hand back six layouts is six paths to get wrong. */
	png_set_expand(png);
	png_set_strip_16(png);
	png_set_gray_to_rgb(png);
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);

	if ((long)png_get_image_width(png, info) != w ||
	    (long)png_get_image_height(png, info) != h)
		png_error(png, "header disagrees");

	rgba = malloc((size_t)w * (size_t)h * 4);
	rows = malloc((size_t)h * sizeof(*rows));
	if (!rgba || !rows)
		png_error(png, "out of memory");
	for (long y = 0; y < h; y++)
		rows[y] = rgba + y * w * 4;
	png_read_image(png, rows);

	/* `png_read_end` is skipped deliberately: the trailing chunks carry
	 * nothing this needs, and reading them is more attacker-controlled
	 * parsing for no picture. */
	png_destroy_read_struct(&png, &info, NULL);

	uint8_t *out = rgba;
	png_bytep *rowv = rows;

	free(rowv);

	pixman_image_t *img = from_rgba(out, (int)w, (int)h);

	free(out);
	return img;
}
#endif

/* ── JPEG ────────────────────────────────────────────────────────────────
 *
 * The dimensions live in a start-of-frame marker, which is not at a fixed
 * offset: the markers before it are variable-length. Walked here rather than
 * read from libjpeg, for the same reason as PNG — the budget has to be decided
 * before a decoder allocates anything.
 */
#ifdef KIMG_HAVE_JPEG
struct jpeg_fail {
	struct jpeg_error_mgr mgr;
	jmp_buf jmp;
};

static void jpeg_bail(j_common_ptr cinfo)
{
	struct jpeg_fail *f = (struct jpeg_fail *)cinfo->err;

	longjmp(f->jmp, 1);
}

/* SILENCE, for the reason png_quiet exists: libjpeg's default emits
 * "Corrupt JPEG data" to stderr, and the data came from a pty. Both hooks,
 * because a warning takes emit_message and never reaches output_message —
 * and NULL is not silence here, it is a call through a null pointer. */
static void jpeg_quiet(j_common_ptr cinfo)
{
	(void)cinfo;
}

static void jpeg_quiet_emit(j_common_ptr cinfo, int level)
{
	(void)cinfo;
	(void)level;
}

static int jpeg_size(const uint8_t *p, size_t n, long *w, long *h)
{
	size_t i = 2;		/* past SOI */

	while (i + 3 < n) {
		if (p[i] != 0xff)
			return 0;

		uint8_t m = p[i + 1];

		/* Standalone markers carry no length. */
		if (m == 0xd8 || m == 0x01 || (m >= 0xd0 && m <= 0xd7)) {
			i += 2;
			continue;
		}
		if (i + 4 > n)
			return 0;

		size_t seg = ((size_t)p[i + 2] << 8) | p[i + 3];

		if (seg < 2 || i + 2 + seg > n)
			return 0;

		/* Every SOFn except the four that are not frame headers. */
		if (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 &&
		    m != 0xcc) {
			if (seg < 7)
				return 0;
			*h = ((long)p[i + 5] << 8) | p[i + 6];
			*w = ((long)p[i + 7] << 8) | p[i + 8];
			return 1;
		}
		i += 2 + seg;
	}
	return 0;
}

static pixman_image_t *decode_jpeg(const uint8_t *p, size_t n,
				   const KimgBudget *b)
{
	long w = 0, h = 0;

	if (!jpeg_size(p, n, &w, &h) || !within(b, w, h))
		return NULL;

	struct jpeg_decompress_struct ci;
	struct jpeg_fail err;

	/* Volatile for the same reason the PNG path's are: assigned after
	 * setjmp, read in the branch longjmp returns to. */
	uint8_t *volatile rgba = NULL;
	uint8_t *volatile row = NULL;

	ci.err = jpeg_std_error(&err.mgr);
	err.mgr.error_exit = jpeg_bail;
	err.mgr.output_message = jpeg_quiet;
	err.mgr.emit_message = jpeg_quiet_emit;
	if (setjmp(err.jmp)) {
		jpeg_destroy_decompress(&ci);
		free(rgba);
		free(row);
		return NULL;
	}

	jpeg_create_decompress(&ci);
	jpeg_mem_src(&ci, p, (unsigned long)n);
	jpeg_read_header(&ci, TRUE);
	ci.out_color_space = JCS_RGB;
	jpeg_start_decompress(&ci);

	if ((long)ci.output_width != w || (long)ci.output_height != h ||
	    ci.output_components != 3) {
		jpeg_destroy_decompress(&ci);
		return NULL;
	}

	rgba = malloc((size_t)w * (size_t)h * 4);
	row = malloc((size_t)w * 3);
	if (!rgba || !row) {
		jpeg_destroy_decompress(&ci);
		free(rgba);
		free(row);
		return NULL;
	}

	while ((long)ci.output_scanline < h) {
		long y = ci.output_scanline;
		uint8_t *rowp = row;

		jpeg_read_scanlines(&ci, &rowp, 1);
		for (long x = 0; x < w; x++) {
			uint8_t *o = rgba + (y * w + x) * 4;

			o[0] = rowp[x * 3];
			o[1] = rowp[x * 3 + 1];
			o[2] = rowp[x * 3 + 2];
			o[3] = 0xff;
		}
	}
	jpeg_finish_decompress(&ci);
	jpeg_destroy_decompress(&ci);

	uint8_t *out = rgba;

	free(row);

	pixman_image_t *img = from_rgba(out, (int)w, (int)h);

	free(out);
	return img;
}
#endif

/* ── WebP ────────────────────────────────────────────────────────────────
 *
 * WebPGetInfo reads the header and allocates nothing, which is exactly the
 * pre-decode size the budget needs.
 */
#ifdef KIMG_HAVE_WEBP
static pixman_image_t *decode_webp(const uint8_t *p, size_t n,
				   const KimgBudget *b)
{
	int w = 0, h = 0;

	if (!WebPGetInfo(p, n, &w, &h) || !within(b, w, h))
		return NULL;

	uint8_t *rgba = WebPDecodeRGBA(p, n, &w, &h);

	if (!rgba)
		return NULL;

	/* Re-checked after the decode: WebPGetInfo reads the container's
	 * dimensions and an animation's frames need not match them. */
	pixman_image_t *img = within(b, w, h) ? from_rgba(rgba, w, h) : NULL;

	WebPFree(rgba);
	return img;
}
#endif

/* ── sixel ───────────────────────────────────────────────────────────────
 *
 * Sixel declares its size only if it feels like it — the raster attribute is
 * optional — so the budget is decided from a bound the ENCODING itself
 * guarantees instead: one data character is one column six pixels tall, and a
 * carriage-return/line-feed control starts a new band. That makes the widest
 * possible image the longest run of data characters, and the tallest six
 * times the number of bands, both computable from the input without decoding.
 *
 * A conservative bound is the point. It refuses a bomb before libsixel sees
 * it, and it lets an ordinary picture through.
 */
#ifdef KIMG_HAVE_SIXEL
static void sixel_bound(const uint8_t *p, size_t n, long *w, long *h)
{
	long run = 0, maxrun = 0, bands = 1;
	int in_raster = 0;
	size_t start = 0;

	/* Skip the introducer and its parameters. `P` and `q` are both inside
	 * the data-character range, so counting them would make every bound
	 * two columns wide before a single pixel. */
	for (size_t i = 0; i < n && i < 32; i++) {
		if (p[i] == 'q') {
			start = i + 1;
			break;
		}
	}

	for (size_t i = start; i < n; i++) {
		uint8_t c = p[i];

		/* A raster attribute or a colour introducer runs to the next
		 * non-numeric; its digits are not pixels. */
		if (c == '"' || c == '#') {
			in_raster = 1;
			continue;
		}
		if (in_raster) {
			if ((c >= '0' && c <= '9') || c == ';')
				continue;
			in_raster = 0;
		}
		if (c == '-') {
			bands++;
			if (run > maxrun)
				maxrun = run;
			run = 0;
			continue;
		}
		if (c == '$') {
			if (run > maxrun)
				maxrun = run;
			run = 0;
			continue;
		}
		if (c == '!') {
			/* A repeat introducer: the count that follows is a
			 * multiplier on ONE character, and it is the whole of
			 * how four lines of sixel become a bomb. */
			long rep = 0;

			while (i + 1 < n && p[i + 1] >= '0' && p[i + 1] <= '9') {
				rep = rep * 10 + (p[++i] - '0');
				if (rep > 1 << 20)
					break;
			}
			run += rep;
			continue;
		}
		if (c >= '?' && c <= '~')
			run++;
	}
	if (run > maxrun)
		maxrun = run;

	*w = maxrun;
	*h = bands * 6;
}

/*
 * THE BYTES ARE THE DCS BODY, not the whole sequence.
 *
 * libkvt delimits `ESC P … ST` and hands over what was between them, because
 * finding the end of an escape sequence is a parser's job and not a decoder's.
 * libsixel wants the frame back — it will not decode a bare body — so it is
 * put back here rather than at the twelve call sites that would otherwise each
 * have to know. Bytes that already carry the introducer are passed through, so
 * a caller reading a file off disk needs no special case.
 */
static pixman_image_t *decode_sixel(const uint8_t *p, size_t n,
				    const KimgBudget *b)
{
	long w = 0, h = 0;
	uint8_t *framed = NULL;
	const uint8_t *seq = p;
	size_t seqlen = n;

	sixel_bound(p, n, &w, &h);
	if (!within(b, w, h))
		return NULL;

	if (n < 2 || p[0] != 0x1b || p[1] != 'P') {
		/* ESC P + body + ESC \ */
		framed = malloc(n + 5);
		if (!framed)
			return NULL;
		framed[0] = 0x1b;
		framed[1] = 'P';
		memcpy(framed + 2, p, n);
		framed[n + 2] = 0x1b;
		framed[n + 3] = '\\';
		framed[n + 4] = '\0';
		seq = framed;
		seqlen = n + 4;
	}

	unsigned char *pixels = NULL, *palette = NULL;
	int pw = 0, ph = 0, ncolors = 0;

	if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)seq, (int)seqlen,
					  &pixels, &pw, &ph, &palette,
					  &ncolors, NULL))) {
		free(framed);
		return NULL;
	}
	free(framed);

	/* The bound above is an upper limit, not a promise: what came out is
	 * checked against the budget too. */
	if (!pixels || !palette || !within(b, pw, ph)) {
		free(pixels);
		free(palette);
		return NULL;
	}

	uint8_t *rgba = malloc((size_t)pw * (size_t)ph * 4);

	if (!rgba) {
		free(pixels);
		free(palette);
		return NULL;
	}
	for (long i = 0; i < (long)pw * ph; i++) {
		int ix = pixels[i];
		uint8_t *o = rgba + i * 4;

		/* AN INDEX FROM THE PAYLOAD IS AN INDEX FROM AN ATTACKER.
		 * libsixel writes indices its own palette covers, and the one
		 * time that stops being true is the read this clamp costs
		 * nothing to prevent. */
		if (ix < 0 || ix >= ncolors)
			ix = 0;
		o[0] = palette[ix * 3];
		o[1] = palette[ix * 3 + 1];
		o[2] = palette[ix * 3 + 2];
		o[3] = 0xff;
	}
	free(pixels);
	free(palette);

	pixman_image_t *img = from_rgba(rgba, pw, ph);

	free(rgba);
	return img;
}
#endif

/* ── the entry point ─────────────────────────────────────────────────── */

unsigned kimg_formats(void)
{
	unsigned f = 0;

#ifdef KIMG_HAVE_SIXEL
	f |= 1u << KIMG_SIXEL;
#endif
#ifdef KIMG_HAVE_PNG
	f |= 1u << KIMG_PNG;
#endif
#ifdef KIMG_HAVE_JPEG
	f |= 1u << KIMG_JPEG;
#endif
#ifdef KIMG_HAVE_WEBP
	f |= 1u << KIMG_WEBP;
#endif
	return f;
}

pixman_image_t *kimg_decode(const void *bytes, size_t len, int type,
			    const KimgBudget *budget)
{
	const uint8_t *p = bytes;

	if (!p || !budget || len < 8)
		return NULL;

	if (type == KIMG_AUTO)
		type = sniff(p, len);

	/*
	 * A DECLARED TYPE MUST AGREE WITH THE BYTES. A peer that says PNG and
	 * sends something else is not making a mistake worth accommodating,
	 * and re-sniffing would mean the type it declared decided nothing.
	 * Sixel is exempt: it has no magic, only a DCS the parser already
	 * stripped.
	 */
	if (type != KIMG_SIXEL) {
		int saw = sniff(p, len);

		if (saw == 0 || saw != type)
			return NULL;
	}

	switch (type) {
#ifdef KIMG_HAVE_SIXEL
	case KIMG_SIXEL:
		return decode_sixel(p, len, budget);
#endif
#ifdef KIMG_HAVE_PNG
	case KIMG_PNG:
		return decode_png(p, len, budget);
#endif
#ifdef KIMG_HAVE_JPEG
	case KIMG_JPEG:
		return decode_jpeg(p, len, budget);
#endif
#ifdef KIMG_HAVE_WEBP
	case KIMG_WEBP:
		return decode_webp(p, len, budget);
#endif
	default:
		break;
	}
	return NULL;
}

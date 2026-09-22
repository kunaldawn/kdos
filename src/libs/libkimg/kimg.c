/* libkimg — see kimg.h.
 *
 * ONE TRANSLATION UNIT, and everything but the two entry points is static.
 * That is the shape the security argument needs: `nm` on the archive shows
 * exactly what can be reached from outside, and a helper that grew a caller
 * elsewhere would have to be exported deliberately rather than by being
 * non-static in a second file.
 *
 * EVERY DECODER IS OPTIONAL. A build without one refuses that format rather
 * than failing to link, because a surface must come up on an image that ships
 * no decoders at all.
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
#ifdef KIMG_HAVE_GIF
#include <libnsgif.h>
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

/*
 * BYTE-ORDER RGBA BECOMES PREMULTIPLIED a8r8g8b8, IN PLACE.
 *
 * pixman's a8r8g8b8 is native-endian words with alpha in the high byte; the
 * decoders all hand back byte-order RGBA. Built a word at a time rather than
 * memcpy'd, so this is correct on either endianness instead of correct on the
 * one it was written on.
 *
 * AND PREMULTIPLIED, which is what that format MEANS: the colour channels are
 * already scaled by the alpha. A decoder's RGBA is straight alpha, and handing
 * the channels over unscaled does not fail — it composites too bright, so a
 * soft icon edge comes out with a pale halo and a half-transparent picture is
 * washed out over whatever is behind it. The rounding is the usual
 * `(c*a + 127) / 255` so that 255 stays 255 and 0 stays 0.
 *
 * IN PLACE BECAUSE THE PEAK IS THE BUDGET. A decoder buffer plus a second
 * full-size word buffer is twice the declared maximum live at once, on a path
 * whose whole point is to bound what a picture may cost; both are the same
 * four bytes a pixel, and the word is written after its own bytes are read.
 */
static void premul_words(uint32_t *px, long npix)
{
	for (long i = 0; i < npix; i++) {
		const uint8_t *p = (const uint8_t *)&px[i];
		uint32_t r = p[0], g = p[1], b = p[2], a = p[3];

		if (a == 255) {
			px[i] = 0xff000000u | (r << 16) | (g << 8) | b;
			continue;
		}
		px[i] = (a << 24) | (((r * a + 127) / 255) << 16) |
			(((g * a + 127) / 255) << 8) | ((b * a + 127) / 255);
	}
}

/* Takes ownership of `px`, which must hold w*h premultiplied words, and frees
 * it on failure. pixman does not own the buffer it is handed; the destroy
 * function is what frees it when the last reference goes, and without it
 * every picture on the screen is a leak the size of the picture. */
static pixman_image_t *wrap_words(uint32_t *px, int w, int h)
{
	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						       px, w * 4);

	if (!img) {
		free(px);
		return NULL;
	}
	pixman_image_set_destroy_function(img, free_bits, px);
	return img;
}

#ifdef KIMG_HAVE_GIF
/* For a decoder whose buffer is its own and cannot be written through — the
 * GIF canvas, which libnsgif keeps across frames, which is the only caller
 * there is. Guarded with it: a build without libnsgif leaves this with no
 * caller at all, and an unused static is an error under -Werror. */
static pixman_image_t *from_rgba(const uint8_t *rgba, int w, int h)
{
	uint32_t *px = malloc((size_t)w * (size_t)h * 4);

	if (!px)
		return NULL;
	memcpy(px, rgba, (size_t)w * (size_t)h * 4);
	premul_words(px, (long)w * h);
	return wrap_words(px, w, h);
}
#endif

/* ── sniffing ───────────────────────────────────────────────────────────── */

static int sniff(const uint8_t *p, size_t n)
{
	if (n >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8))
		return KIMG_PNG;
	if (n >= 3 && p[0] == 0xff && p[1] == 0xd8 && p[2] == 0xff)
		return KIMG_JPEG;
	if (n >= 12 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WEBP", 4))
		return KIMG_WEBP;
	if (n >= 6 && (!memcmp(p, "GIF87a", 6) || !memcmp(p, "GIF89a", 6)))
		return KIMG_GIF;
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

	/* STRAIGHT INTO THE BUFFER THE PICTURE KEEPS. libpng writes RGBA
	 * bytes, which premul_words then turns into words where they lie — a
	 * second full-size buffer would double the peak against a budget that
	 * is stated in decoded bytes. */
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

	uint32_t *out = (uint32_t *)rgba;
	png_bytep *rowv = rows;

	free(rowv);
	premul_words(out, w * h);
	return wrap_words(out, (int)w, (int)h);
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

	/* THE WORD IS BUILT AT THE SCANLINE, not in a second pass over a
	 * second buffer. A JPEG is opaque, so there is no premultiply to do
	 * and no reason for the picture to exist twice. */
	uint32_t *out = (uint32_t *)rgba;

	while ((long)ci.output_scanline < h) {
		long y = ci.output_scanline;
		uint8_t *rowp = row;

		jpeg_read_scanlines(&ci, &rowp, 1);
		for (long x = 0; x < w; x++)
			out[y * w + x] = 0xff000000u |
					 ((uint32_t)rowp[x * 3] << 16) |
					 ((uint32_t)rowp[x * 3 + 1] << 8) |
					 (uint32_t)rowp[x * 3 + 2];
	}
	jpeg_finish_decompress(&ci);
	jpeg_destroy_decompress(&ci);

	free(row);
	return wrap_words(out, (int)w, (int)h);
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

	/* INTO A BUFFER OF OUR OWN, which then becomes the picture in place:
	 * WebPDecodeRGBA would allocate a second one the same size and the
	 * peak would be twice the budget. The size is already bounded above,
	 * and a decode that disagrees with the container is refused. */
	uint32_t *px = malloc((size_t)w * (size_t)h * 4);

	if (!px)
		return NULL;
	if (!WebPDecodeRGBAInto(p, n, (uint8_t *)px, (size_t)w * h * 4,
				w * 4)) {
		free(px);
		return NULL;
	}
	premul_words(px, (long)w * h);
	return wrap_words(px, w, h);
}
#endif

/* ── GIF ─────────────────────────────────────────────────────────────────
 *
 * The logical screen descriptor is bytes 6..9 — two little-endian 16-bit
 * dimensions, immediately after the magic — so the budget is checked before
 * libnsgif is given anything, which is the rule every decoder here follows.
 *
 * A FRAME IS THE WHOLE CANVAS. libnsgif composes disposal and transparency
 * into one bitmap per frame and hands back that bitmap, so nothing here has to
 * know what a disposal method is; the budget is charged per frame because a
 * two-hundred-frame animation of a modest canvas is not a modest allocation.
 *
 * ANIMATED GIF IS THE ONLY FORMAT HERE WITH MORE THAN ONE PICTURE IN IT, which
 * is why `kimg_decode_all` exists at all: decoding frame N by starting over
 * would be quadratic in the frame count, and a hundred-frame animation would
 * cost five thousand frame decodes.
 */
#ifdef KIMG_HAVE_GIF
/*
 * libnsgif asks the caller for its bitmaps. They are plain RGBA buffers here —
 * the library writes into `bitmap_get_buffer` and this file turns the finished
 * one into a pixman image, so no allocator of libnsgif's is ever handed to
 * pixman and the ownership stays on one side.
 */
struct gifbm {
	int w, h;
	unsigned char *px;
};

/*
 * THE BUDGET REACHES THIS CALLBACK, because libnsgif allocates through it
 * before anything else can refuse. gif_initialise walks every frame header
 * and grows the canvas to cover a frame that extends past the logical screen,
 * calling this with the enlarged size: a 100x100 screen whose first image
 * descriptor says 65535x65535 asks for 17 GB here, and the size test after
 * gif_initialise returns is far too late. Set for the duration of one decode
 * and cleared after, so no state outlives the call.
 */
static const KimgBudget *gif_budget;

static void *gif_bm_create(int width, int height)
{
	struct gifbm *b;

	if (width <= 0 || height <= 0)
		return NULL;
	/* within() covers the overflow as well as the budget: this callback is
	 * reached with the frame's own size, which need not be the canvas's. */
	if (!within(gif_budget, (long)width, (long)height))
		return NULL;
	b = calloc(1, sizeof(*b));
	if (!b)
		return NULL;
	b->w = width;
	b->h = height;
	b->px = calloc((size_t)width * (size_t)height, 4);
	if (!b->px) {
		free(b);
		return NULL;
	}
	return b;
}

static void gif_bm_destroy(void *bitmap)
{
	struct gifbm *b = bitmap;

	if (!b)
		return;
	free(b->px);
	free(b);
}

static unsigned char *gif_bm_buffer(void *bitmap)
{
	struct gifbm *b = bitmap;

	return b ? b->px : NULL;
}

static void gif_bm_set_opaque(void *bitmap, bool opaque)
{
	(void)bitmap;
	(void)opaque;
}

static bool gif_bm_test_opaque(void *bitmap)
{
	(void)bitmap;
	return false;
}

static void gif_bm_modified(void *bitmap)
{
	(void)bitmap;
}

static int gif_all(const uint8_t *p, size_t n, const KimgBudget *b,
		   KimgFrame *out, int max)
{
	gif_bitmap_callback_vt vt = {
		gif_bm_create, gif_bm_destroy, gif_bm_buffer,
		gif_bm_set_opaque, gif_bm_test_opaque, gif_bm_modified
	};
	gif_animation gif;
	size_t charged = 0;
	int got = 0;
	long w, h;

	if (n < 10 || max < 1)
		return 0;
	w = (long)p[6] | ((long)p[7] << 8);
	h = (long)p[8] | ((long)p[9] << 8);
	if (!within(b, w, h))
		return 0;

	gif_budget = b;
	gif_create(&gif, &vt);
	/* libnsgif takes the buffer as non-const and does not write to it. */
	if (gif_initialise(&gif, n, (unsigned char *)p) != GIF_OK) {
		gif_finalise(&gif);
		gif_budget = NULL;
		return 0;
	}
	if (!within(b, (long)gif.width, (long)gif.height)) {
		gif_finalise(&gif);
		gif_budget = NULL;
		return 0;
	}

	for (unsigned f = 0; f < gif.frame_count && got < max; f++) {
		struct gifbm *bm;
		size_t cost = (size_t)gif.width * (size_t)gif.height * 4;

		/* THE BUDGET IS OVER THE WHOLE ANIMATION. A frame that would
		 * cross it ends the decode and the frames already accepted
		 * still play — half an animation is worth more than none, and
		 * far more than a decoder that allocated until it was killed. */
		if (charged > b->max_bytes - cost)
			break;
		if (gif_decode_frame(&gif, f) != GIF_OK)
			break;
		bm = gif.frame_image;
		if (!bm || !bm->px)
			break;
		out[got].img = from_rgba(bm->px, (int)gif.width,
					 (int)gif.height);
		if (!out[got].img)
			break;
		/* Centiseconds on the wire. Zero means "as fast as you like",
		 * which every browser reads as ten hundredths — a zero here
		 * would be a busy loop. */
		out[got].gap_ms = gif.frames[f].frame_delay > 0
					  ? (int)gif.frames[f].frame_delay * 10
					  : 100;
		charged += cost;
		got++;
	}
	gif_finalise(&gif);
	gif_budget = NULL;
	return got;
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
	long ras_w = 0, ras_h = 0;
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

		/*
		 * A raster attribute or a colour introducer runs to the next
		 * non-numeric, and its digits are not pixels — but a RASTER
		 * attribute's third and fourth parameters ARE a size, and
		 * libsixel acts on them before a single data character is
		 * read: it grows the image to the declared width and height
		 * the moment it parses them. A bound taken from the data
		 * characters alone therefore missed exactly the field an
		 * attacker would use, and `"1;1;40000;40000` with no pixels
		 * after it is a six-gigabyte allocation this was meant to
		 * refuse.
		 */
		if (c == '"') {
			long par[4] = { 0, 0, 0, 0 };
			int np = 0;

			i++;
			while (i < n && np < 4) {
				if (p[i] >= '0' && p[i] <= '9') {
					if (par[np] < (1L << 20))
						par[np] = par[np] * 10 +
							  (p[i] - '0');
				} else if (p[i] == ';') {
					np++;
				} else {
					break;
				}
				i++;
			}
			i--;	/* the loop's own step takes the terminator */
			if (par[2] > ras_w)
				ras_w = par[2];
			if (par[3] > ras_h)
				ras_h = par[3];
			continue;
		}
		if (c == '#') {
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

	/* The larger of what the data can draw and what the raster attribute
	 * declared: libsixel sizes the image to whichever is bigger, so a
	 * bound that took only one of them bounds nothing. */
	*w = maxrun > ras_w ? maxrun : ras_w;
	*h = bands * 6 > ras_h ? bands * 6 : ras_h;
}

/*
 * THE BYTES ARE THE DCS BODY, not the whole sequence.
 *
 * libkvt delimits `ESC P … ST` and hands over what was between them, because
 * finding the end of an escape sequence is a parser's job and not a decoder's.
 * libsixel wants the whole frame back — introducer, `q` and terminator — so
 * it is put back here rather than at the twelve call sites that would
 * otherwise each have to know. Bytes that already carry the introducer are
 * passed through, so a caller reading a file off disk needs no special case.
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
		/*
		 * THE `q` IS PART OF THE FRAME, and a body handed over without
		 * one decodes as a blank one-pixel image with no error:
		 * libsixel's parser swallows every byte that is not a digit,
		 * `;`, `q` or ESC while it is still looking for the final, so
		 * the whole picture goes past and the 1x1 buffer it
		 * initialised is what comes back.
		 *
		 * A body that already carries its parameters and the `q` —
		 * digits and semicolons, then `q` — keeps them; anything else
		 * is data and gets a bare `q`.
		 */
		size_t k = 0;

		while (k < n && ((p[k] >= '0' && p[k] <= '9') || p[k] == ';'))
			k++;
		int has_q = k < n && p[k] == 'q';
		size_t extra = has_q ? 0 : 1;

		framed = malloc(n + 5 + extra);
		if (!framed)
			return NULL;
		framed[0] = 0x1b;
		framed[1] = 'P';
		if (!has_q)
			framed[2] = 'q';
		memcpy(framed + 2 + extra, p, n);
		framed[n + 2 + extra] = 0x1b;
		framed[n + 3 + extra] = '\\';
		framed[n + 4 + extra] = '\0';
		seq = framed;
		seqlen = n + 4 + extra;
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

	/* The palette is looked up straight into the word the picture keeps:
	 * libsixel's index buffer and its palette are already live, and a
	 * fourth full-size buffer on top of them is the peak this budget is
	 * meant to bound. A sixel is opaque, so there is no premultiply. */
	uint32_t *px = malloc((size_t)pw * (size_t)ph * 4);

	if (!px) {
		free(pixels);
		free(palette);
		return NULL;
	}
	for (long i = 0; i < (long)pw * ph; i++) {
		int ix = pixels[i];

		/* AN INDEX FROM THE PAYLOAD IS AN INDEX FROM AN ATTACKER.
		 * libsixel writes indices its own palette covers, and the one
		 * time that stops being true is the read this clamp costs
		 * nothing to prevent. */
		if (ix < 0 || ix >= ncolors)
			ix = 0;
		px[i] = 0xff000000u | ((uint32_t)palette[ix * 3] << 16) |
			((uint32_t)palette[ix * 3 + 1] << 8) |
			(uint32_t)palette[ix * 3 + 2];
	}
	free(pixels);
	free(palette);

	return wrap_words(px, pw, ph);
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
#ifdef KIMG_HAVE_GIF
	f |= 1u << KIMG_GIF;
#endif
	return f;
}

/*
 * EVERY FRAME, for the one format that has more than one. A still answers 1
 * and fills `out[0]`, so a caller that wants an animation and a caller that
 * wants a picture take the same path; `kimg_decode` is this with `max` of one.
 */
int kimg_decode_all(const void *bytes, size_t len, int type,
		    const KimgBudget *budget, KimgFrame *out, int max)
{
	const uint8_t *p = bytes;

	if (!p || !budget || !out || max < 1 || len < 8)
		return 0;
	if (type == KIMG_AUTO)
		type = sniff(p, len);
	if (type != KIMG_SIXEL) {
		int saw = sniff(p, len);

		if (saw == 0 || saw != type)
			return 0;
	}
#ifdef KIMG_HAVE_GIF
	if (type == KIMG_GIF)
		return gif_all(p, len, budget, out, max);
#endif
	out[0].img = kimg_decode(bytes, len, type, budget);
	out[0].gap_ms = 0;
	return out[0].img ? 1 : 0;
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
#ifdef KIMG_HAVE_GIF
	case KIMG_GIF: {
		/* The first frame, which is what a still viewer wants: an
		 * animation's later frames are `kimg_decode_all`'s. */
		KimgFrame f = { NULL, 0 };

		return gif_all(p, len, budget, &f, 1) == 1 ? f.img : NULL;
	}
#endif
	default:
		break;
	}
	return NULL;
}

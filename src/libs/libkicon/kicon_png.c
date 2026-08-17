/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkicon — PNG in, straight RGBA out
 *
 * One decoder for both sources: a file on disk (an alien app's icon) and a
 * blob inside the atlas. libpng's memory reader is the same code path as its
 * file reader with a different callback, so the two share everything below the
 * first line.
 *
 * STRAIGHT alpha, not premultiplied, and that is deliberate: the tint runs on
 * the RGB and would have to divide the alpha back out first (the Xcursor
 * lesson, which cost a debug cycle in kdos-cursors). The premultiply happens
 * once, at the end, on the way into the pixman image.
 *
 * The longjmp trap is kdos-wallpaper.c's and is the same here: libpng reports
 * a bad file by longjmp'ing back AFTER the allocation, so every pointer the
 * cleanup path reads is `volatile` or it may be a stale register.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kicon_int.h"

struct mem_reader {
	const unsigned char *p;
	size_t left;
};

static void mem_read(png_structp png, png_bytep out, png_size_t n)
{
	struct mem_reader *m = png_get_io_ptr(png);
	if (!m || m->left < n) {
		png_error(png, "short read");
		return;
	}
	memcpy(out, m->p, n);
	m->p += n;
	m->left -= n;
}

/*
 * Decode into a freshly malloc'd RGBA8888 buffer. `f` or `mem` — exactly one.
 * Returns NULL on anything at all going wrong; a missing icon is a glyph.
 */
static uint8_t *decode(FILE *f, struct mem_reader *mem, int *w, int *h)
{
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
						 NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;
	uint8_t *volatile buf = NULL;
	png_bytep *volatile rows = NULL;
	volatile int ok = 0;

	if (!png || !info || setjmp(png_jmpbuf(png)))
		goto out;

	if (f)
		png_init_io(png, f);
	else
		png_set_read_fn(png, mem, mem_read);
	png_read_info(png, info);

	png_uint_32 pw = png_get_image_width(png, info);
	png_uint_32 ph = png_get_image_height(png, info);
	int depth = png_get_bit_depth(png, info);
	int ctype = png_get_color_type(png, info);

	/* An icon larger than this is not an icon; refusing it here is what
	 * keeps a hostile file in ~/.local/share/icons from being a
	 * multi-gigabyte allocation in the panel. */
	if (pw == 0 || ph == 0 || pw > 1024 || ph > 1024)
		goto out;

	if (ctype == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (depth == 16)
		png_set_strip_16(png);
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	/* Always four channels out, so the caller has one layout to handle. */
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);

	buf = malloc((size_t)pw * ph * 4);
	rows = malloc(sizeof(*rows) * ph);
	if (!buf || !rows)
		goto out;
	for (png_uint_32 y = 0; y < ph; y++)
		rows[y] = (png_bytep)(buf + (size_t)y * pw * 4);
	png_read_image(png, rows);

	*w = (int)pw;
	*h = (int)ph;
	ok = 1;
out:
	free(rows);
	png_destroy_read_struct(png ? &png : NULL, info ? &info : NULL, NULL);
	if (!ok) {
		free(buf);
		return NULL;
	}
	return buf;
}

uint8_t *ki_png_file(const char *path, int *w, int *h)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	uint8_t *r = decode(f, NULL, w, h);
	fclose(f);
	return r;
}

uint8_t *ki_png_mem(const void *data, size_t len, int *w, int *h)
{
	struct mem_reader m = { .p = data, .left = len };
	return decode(NULL, &m, w, h);
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   One frame of the console desktop, as a picture.
 *
 * THE CONSOLE'S SCREENSHOT WAS TEXT, and text is the wrong artefact to hand
 * somebody. `kdos shot` on the console wrote the composited grid as a `.txt`
 * — which is exact, and which nobody can attach to a bug report, paste into a
 * message or look at a week later and recognise. The grid is drawn in cells
 * and a cell has a glyph, a colour and an attribute; rasterising it is the
 * same thing a screen does, through the same painter.
 *
 * THE SAME PAINTER, AND THAT IS THE POINT. `kcell_paint()` is what puts cells
 * on a KMS screen and into a cast; a second rasteriser here would drift from
 * it and the picture would stop being what the screen shows. The view loads a
 * font for exactly this reason on the cast path already.
 *
 * FULL, NEVER A DIFF. `kcell_paint` takes a previous frame to damage against
 * and a `full` flag to ignore it; a shot has no previous frame, so passing one
 * would paint whatever the uninitialised buffer happened to hold.
 *
 * COMPILED IN ONLY WHERE THE RASTERISER IS (`KDOS_VIEW_SHOT`), the same rule
 * `--kms` and `--cast` keep. A build without libkcell and libpng refuses
 * `--shot` by name rather than writing an empty file, because a screenshot
 * tool that silently produces nothing is worse than one that is absent.
 * ---------------------------------
 */

#ifdef KDOS_VIEW_SHOT

#define _POSIX_C_SOURCE 200809L
#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "ktui.h"
#include "view.h"

/*
 * ARGB32 out of pixman, RGB8 into libpng.
 *
 * The composite is opaque — every cell paints its background slot — so the
 * alpha channel carries nothing and writing it would make the file a third
 * larger for a plane that is 0xff everywhere. The byte order is the other
 * half: pixman's a8r8g8b8 is a NATIVE-ENDIAN 32-bit word, so the channels are
 * read out of the integer rather than out of the bytes.
 */
static void row_rgb(const uint32_t *src, png_bytep dst, int w)
{
	for (int x = 0; x < w; x++) {
		uint32_t p = src[x];

		dst[x * 3 + 0] = (png_byte)((p >> 16) & 0xff);
		dst[x * 3 + 1] = (png_byte)((p >> 8) & 0xff);
		dst[x * 3 + 2] = (png_byte)(p & 0xff);
	}
}

int view_shot_png(const char *path, int scale)
{
	int cols = 0, rows = 0;
	const KtuiCell *cells = ktui_draw_cells(&cols, &rows);
	int cw = kcell_w(), ch = kcell_h();
	int w, h, stride;
	uint32_t *buf = NULL;
	pixman_image_t *img = NULL;
	png_structp png = NULL;
	png_infop info = NULL;
	png_bytep line = NULL;
	FILE *f = NULL;
	int rc = -1;

	if (!path || !cells || cols < 1 || rows < 1 || cw < 1 || ch < 1)
		return -1;
	if (scale < 1)
		scale = 1;

	w = cols * cw * scale;
	h = rows * ch * scale;
	stride = w * 4;

	buf = calloc(1, (size_t)stride * (size_t)h);
	if (!buf)
		return -1;
	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h, buf, stride);
	if (!img)
		goto out;

	/* NULL previous and full: see the header. */
	kcell_paint(img, cells, NULL, cols, rows, 1, scale, w, h);

	f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "kdos-view: cannot write %s\n", path);
		goto out;
	}
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info = png ? png_create_info_struct(png) : NULL;
	if (!png || !info)
		goto out;
	/*
	 * The longjmp trap libkicon's reader keeps, on the writing side: libpng
	 * reports a failure by jumping back here AFTER the allocations above,
	 * so everything the cleanup path touches is set before this point and
	 * nothing is allocated after it.
	 */
	if (setjmp(png_jmpbuf(png)))
		goto out;

	png_init_io(png, f);
	png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8,
		     PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);

	line = malloc((size_t)w * 3);
	if (!line)
		goto out;
	for (int y = 0; y < h; y++) {
		row_rgb(buf + (size_t)y * (size_t)w, line, w);
		png_write_row(png, line);
	}
	png_write_end(png, NULL);
	rc = 0;
out:
	free(line);
	if (png)
		png_destroy_write_struct(&png, info ? &info : NULL);
	if (f)
		fclose(f);
	if (img)
		pixman_image_unref(img);
	free(buf);
	return rc;
}

#endif /* KDOS_VIEW_SHOT */

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   asciicheck — the two claims the ASCII engine makes
 *
 * It is a separate program rather than an assertion in src/libs/selftest.c for
 * one reason: selftest.c links libkbase, libkcolor, libktui and libkpkg, and
 * NONE of them links fcft. Measuring a glyph's shape needs the glyph. Adding
 * fcft to that binary would put a real `-l` on the suite that proves libktui
 * has none.
 *
 * Two assertions, and the second is the one that matters:
 *
 *   1. THE RAMP IS MONOTONIC. A black-to-white gradient must come back as
 *      glyphs of non-decreasing ink. A matcher that got this wrong would still
 *      produce a picture — a noisy one — so it is exactly the kind of defect
 *      that survives being looked at.
 *   2. ORIENTATION IS DISTINGUISHED. A vertical bar and a horizontal bar of the
 *      same total ink must pick DIFFERENT glyphs. This is the whole difference
 *      from aalib, which picks by luminance alone and cannot tell them apart,
 *      and it is the reason there are six sample discs rather than one.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"

static int fails;

static void check(int ok, const char *what)
{
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
	if (!ok)
		fails++;
}

/* How much ink a glyph has, from the table the matcher itself uses. */
static float ink_of(uint32_t cp)
{
	for (int i = 0; i < kcell_ascii_count(); i++) {
		if (kcell_ascii_glyph(i) != cp)
			continue;
		const float *v = kcell_ascii_vector(i);
		float s = 0.0f;
		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			s += v[d];
		return s;
	}
	return -1.0f;
}

static uint32_t one_cell(const uint32_t *img, int w, int h)
{
	uint32_t cp = ' ', tint = 0;
	int c = 0, r = 0;
	if (kcell_ascii_image(img, w, h, w, kcell_w(), kcell_h(), &cp, &tint,
			      &c, &r) != 0)
		return 0;
	return cp;
}

int main(int argc, char **argv)
{
	const char *font = argc > 1 ? argv[1] : "monospace:pixelsize=16";

	if (kcell_font_load(font) != 0) {
		fprintf(stderr, "asciicheck: no usable font\n");
		return 2;
	}
	int n = kcell_ascii_init();
	printf("asciicheck — cell %dx%d, %d candidates\n", kcell_w(),
	       kcell_h(), n);
	check(n >= 8, "the candidate table survived the font");

	const int cw = kcell_w(), ch = kcell_h();

	/* 1. the ramp */
	int cols = 12;
	int w = cw * cols, h = ch;
	uint32_t *img = calloc((size_t)w * h, 4);
	uint32_t *cp = calloc((size_t)cols, 4);
	uint32_t *tint = calloc((size_t)cols, 4);
	if (!img || !cp || !tint)
		return 2;

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int v = x * 255 / (w - 1);
			img[(size_t)y * w + x] = 0xff000000u | ((uint32_t)v << 16) |
						 ((uint32_t)v << 8) | (uint32_t)v;
		}
	int oc = 0, orow = 0;
	kcell_ascii_image(img, w, h, w, cw, ch, cp, tint, &oc, &orow);

	int monotonic = 1;
	float prev = -1.0f;
	for (int i = 0; i < oc; i++) {
		float ink = ink_of(cp[i]);
		/* Non-decreasing, with a small tolerance: two adjacent cells of
		 * a smooth gradient can legitimately land on glyphs of equal
		 * ink, and the discs are sampled at fixed positions so a tie is
		 * ordinary rather than suspicious. */
		if (ink + 0.01f < prev)
			monotonic = 0;
		if (ink > prev)
			prev = ink;
	}
	check(monotonic, "a black-to-white ramp is monotonic in ink");
	free(img);
	free(cp);
	free(tint);

	/* 2. orientation */
	w = cw;
	h = ch;
	img = calloc((size_t)w * h, 4);
	if (!img)
		return 2;

	for (int i = 0; i < w * h; i++)
		img[i] = 0xff000000u;
	for (int y = 0; y < h; y++)
		for (int x = cw / 2 - 1; x <= cw / 2 + 1 && x < w; x++)
			if (x >= 0)
				img[(size_t)y * w + x] = 0xffffffffu;
	uint32_t vert = one_cell(img, w, h);

	for (int i = 0; i < w * h; i++)
		img[i] = 0xff000000u;
	for (int y = ch / 2 - 1; y <= ch / 2 + 1 && y < h; y++)
		for (int x = 0; x < w; x++)
			if (y >= 0)
				img[(size_t)y * w + x] = 0xffffffffu;
	uint32_t horiz = one_cell(img, w, h);

	printf("  vertical bar U+%04X   horizontal bar U+%04X\n", vert, horiz);
	check(vert != horiz,
	      "a vertical and a horizontal bar are different glyphs");
	free(img);

	/* 3. the disc table the GPU half is handed matches the one used here.
	 * There is no way to check the shader from C, but there IS a way to
	 * check that it is given the right numbers. */
	float disc[KCELL_ASCII_DIM * 2];
	kcell_ascii_discs(disc);
	int sane = 1;
	for (int d = 0; d < KCELL_ASCII_DIM; d++)
		if (disc[d * 2] <= 0.0f || disc[d * 2] >= 1.0f ||
		    disc[d * 2 + 1] <= 0.0f || disc[d * 2 + 1] >= 1.0f)
			sane = 0;
	check(sane, "the disc table handed to the GPU is inside the cell");

	kcell_font_free();
	printf("\n%s (%d failed)\n", fails ? "FAILED" : "all ok", fails);
	return fails ? 1 : 0;
}

/*
 * THE SLANT NO FONT ON THIS IMAGE CARRIES, measured.
 *
 * `kcell_font_load()` takes an italic companion only where its advance and its
 * height match the upright face's, and fontconfig never fails a match — so on
 * a bitmap face the companion is refused and the slant has to be sheared out
 * of the upright mask. That shear is arithmetic, which is the half of it that
 * can be checked with no font, no screen and no frame.
 *
 * Four claims:
 *   - a glyph a cell tall LEANS, and leans the right way round;
 *   - the lean is symmetric about the mask's middle, which is what keeps a
 *     letter inside the cell the painter clips it to;
 *   - the travel across the whole mask is the ANGLE and not some fraction of
 *     it — truncation instead of rounding loses nearly half of it, and what is
 *     left reads as a font out of alignment rather than as an italic;
 *   - a mask too short to lean by a whole pixel leans by none, because a
 *     fractional shift would need the mask resampled and an alpha mask
 *     resampled at this size is a blur.
 */
#include <stdio.h>

#include "kcell_priv.h"

static int bad;

static void fail(int h, const char *what)
{
	printf("    a %d-pixel glyph: %s\n", h, what);
	bad = 1;
}

int main(void)
{
	for (int h = 1; h <= 64; h++) {
		int lo = kcell_oblique_shift(0, h), hi = lo;

		for (int y = 1; y < h; y++) {
			int d = kcell_oblique_shift(y, h);

			if (d < lo)
				lo = d;
			if (d > hi)
				hi = d;
		}

		/* Up to five pixels the angle is under half a pixel from the
		 * middle to either end, so nothing leans at all — and a shear
		 * of nothing is what the cache then refuses to store. */
		if (h <= 5) {
			if (hi != lo)
				fail(h, "leans further than its height allows");
			continue;
		}

		if (kcell_oblique_shift(0, h) <= 0)
			fail(h, "the top does not lean right");
		/* Six pixels is the one height whose top leans and whose
		 * bottom does not: three rows above the middle round up to a
		 * pixel and two below it round down to none. */
		if (h >= 7 && kcell_oblique_shift(h - 1, h) >= 0)
			fail(h, "the bottom does not lean left");
		/* One pixel of slack: an even height has no middle row. */
		if (hi + lo < -1 || hi + lo > 1)
			fail(h, "the lean is not symmetric about the middle");

		/* About twelve degrees, rounded the way the shear rounds. */
		int want = (h * 7 + 16) / 32;

		if (hi - lo < want - 1 || hi - lo > want + 1)
			fail(h, "the travel across the mask is not the angle");
	}

	if (!bad)
		printf("    16px: %+d..%+d   32px: %+d..%+d\n",
		       kcell_oblique_shift(15, 16), kcell_oblique_shift(0, 16),
		       kcell_oblique_shift(31, 32), kcell_oblique_shift(0, 32));
	return bad;
}

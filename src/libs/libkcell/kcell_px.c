/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   pixel chrome — what a cell cannot say
 *
 * A cell is 10x20 and one colour. These paint UNDER the cell grid, straight
 * into the surface's own pixman image, for the three things a grid genuinely
 * cannot express: a plate whose corners are softened, a fill that varies
 * continuously down the bar, and a line one pixel thick.
 *
 * They are not a second renderer and must not become one. Layout, hit maps and
 * every `--dump` stay cells; this is paint. A surface that draws only with
 * these is a surface that has left the toolkit.
 *
 * Everything here is PREMULTIPLIED and composites with OVER, so a caller
 * layers body, then plates, then rules, in that order, over a cleared buffer.
 * ---------------------------------
 */

#include "kcell.h"

/*
 * pixman_color_t is premultiplied 16-bit. Scaling the channels by the alpha is
 * not optional: hand it un-premultiplied values and nothing fails, every fill
 * simply comes out too bright — which reads as a colour choice rather than a
 * bug and is why this conversion has exactly one implementation.
 */
static pixman_color_t px(uint32_t rgb, uint8_t a)
{
	uint32_t r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;

	return (pixman_color_t){
		.red   = (uint16_t)(r * 257u * a / 255u),
		.green = (uint16_t)(g * 257u * a / 255u),
		.blue  = (uint16_t)(b * 257u * a / 255u),
		.alpha = (uint16_t)((uint32_t)a * 257u),
	};
}

static void band(pixman_image_t *dst, pixman_op_t op, pixman_color_t *c,
		 int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	pixman_image_fill_rectangles(op, dst, c, 1,
				     &(pixman_rectangle16_t){
					     (int16_t)x, (int16_t)y,
					     (uint16_t)w, (uint16_t)h });
}

void kcell_px_clear(pixman_image_t *dst, int x, int y, int w, int h)
{
	pixman_color_t c = { 0, 0, 0, 0 };

	band(dst, PIXMAN_OP_SRC, &c, x, y, w, h);
}

void kcell_px_fill(pixman_image_t *dst, int x, int y, int w, int h,
		   uint32_t rgb, uint8_t a)
{
	pixman_color_t c = px(rgb, a);

	band(dst, PIXMAN_OP_OVER, &c, x, y, w, h);
}

/*
 * A rounded plate, hard-edged.
 *
 * No antialiasing on the arc, and that is the house style rather than a
 * shortcut: the compositor's own titlebar glyphs are 8x8 bitmaps scaled by a
 * whole number with a NEAREST filter for the same reason, because a smoothed
 * edge beside a cell grid is what makes chrome look like it came from a
 * different toolkit. Keep the radius small — 3 pixels softens a plate; 8 turns
 * it into somebody else's desktop, next to window frames this distro draws
 * deliberately square.
 *
 * The extent per row is found by walking rather than with sqrt(), because this
 * library does not link libm. The radius is single digits, so the walk is a
 * few dozen comparisons for a whole plate.
 */
static uint32_t lerp(uint32_t a, uint32_t b, int t)
{
	uint32_t o = 0;

	for (int sh = 16; sh >= 0; sh -= 8) {
		uint32_t x = (a >> sh) & 0xff, y = (b >> sh) & 0xff;

		o |= (x + (y - x) * t / 255) << sh;
	}
	return o;
}

/*
 * The one rounded fill, row by row so a gradient costs nothing extra.
 *
 * Row at a time rather than one band for the middle: a plate is a few dozen
 * pixels tall, so the fills are a few dozen either way, and a single path
 * means a graded plate and a flat one cannot round their corners differently.
 */
void kcell_px_round_grad(pixman_image_t *dst, int x, int y, int w, int h,
			 int r, uint32_t top, uint32_t bot, uint8_t a)
{
	if (w <= 0 || h <= 0)
		return;
	if (r > w / 2)
		r = w / 2;
	if (r > h / 2)
		r = h / 2;
	if (r < 0)
		r = 0;

	for (int i = 0; i < h; i++) {
		int inset = 0;
		pixman_color_t c;

		if (r > 0 && (i < r || i >= h - r)) {
			int dy = i < r ? r - i : r - (h - 1 - i);
			int e = 0;

			while (e + 1 <= r && (e + 1) * (e + 1) + dy * dy
					<= r * r)
				e++;
			inset = r - e;
		}
		c = px(top == bot ? top
				  : lerp(top, bot, h > 1 ? i * 255 / (h - 1)
							 : 0), a);
		band(dst, PIXMAN_OP_OVER, &c, x + inset, y + i,
		     w - 2 * inset, 1);
	}
}

void kcell_px_round(pixman_image_t *dst, int x, int y, int w, int h, int r,
		    uint32_t rgb, uint8_t a)
{
	kcell_px_round_grad(dst, x, y, w, h, r, rgb, rgb, a);
}

/*
 * A vertical gradient, one fill per row.
 *
 * A bar is a few dozen pixels tall, so a row at a time is a few dozen fills
 * and there is nothing to optimise. The interpolation is in 8-bit sRGB rather
 * than linear light on purpose — it is matching what kcol_mix() does
 * everywhere else in this palette, and a gradient that stepped differently
 * from the mixes beside it would be visible exactly where they meet.
 */
void kcell_px_vgrad(pixman_image_t *dst, int x, int y, int w, int h,
		    uint32_t top, uint32_t bot, uint8_t a)
{
	if (w <= 0 || h <= 0)
		return;
	for (int i = 0; i < h; i++) {
		int t = h > 1 ? i * 255 / (h - 1) : 0;
		uint32_t rgb = 0;

		for (int sh = 16; sh >= 0; sh -= 8) {
			uint32_t ca = (top >> sh) & 0xff;
			uint32_t cb = (bot >> sh) & 0xff;

			rgb |= (ca + (cb - ca) * t / 255) << sh;
		}
		kcell_px_fill(dst, x, y + i, w, 1, rgb, a);
	}
}

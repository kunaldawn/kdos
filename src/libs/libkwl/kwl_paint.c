/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   cells -> pixels
 *
 * One KtuiCell becomes one cell_w x cell_h rectangle: the background filled,
 * the glyph composited over it as an alpha mask in the foreground colour.
 *
 * The colours are libkcolor's, reached through libktui's eight slots — the
 * same eight the tty gets. That is the point of the whole exercise and not a
 * limitation to grow out of: the panel and the installer and the build screen
 * render the SAME picture whether they are on tty1 or in a compositor, because
 * there is one palette table and one set of slots. Do not add a ninth here
 * because a compositor could afford it.
 */

#include <string.h>

#include "kwl_priv.h"

static inline pixman_color_t to_pixman(KRgb c)
{
	/* pixman wants 16-bit premultiplied channels; alpha is always opaque
	 * here, so the premultiply is a widening and nothing more. */
	pixman_color_t p = {
		.red   = (uint16_t)(c.r * 257),
		.green = (uint16_t)(c.g * 257),
		.blue  = (uint16_t)(c.b * 257),
		.alpha = 0xffff,
	};
	return p;
}

/*
 * Paint one row of cells.
 *
 * Row at a time rather than cell at a time so that runs of identical
 * background can be filled in one pixman op — a panel is mostly one colour,
 * and a per-cell rectangle fill is the difference between a frame that fits in
 * the deadline and one that does not.
 */
static void paint_row(pixman_image_t *dst, const KtuiCell *row, int w,
		      int y_cell)
{
	const int cw = kwl_font_cell_w(), ch = kwl_font_cell_h();
	int y = y_cell * ch;

	for (int x = 0; x < w;) {
		uint8_t bg = row[x].bg;
		int run = 1;
		while (x + run < w && row[x + run].bg == bg)
			run++;

		pixman_color_t c = to_pixman(ktui_theme->slot[bg]);
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, dst, &c, 1,
			&(pixman_rectangle16_t){ (int16_t)(x * cw), (int16_t)y,
						 (uint16_t)(run * cw),
						 (uint16_t)ch });
		x += run;
	}

	for (int x = 0; x < w; x++) {
		uint32_t cp = row[x].ch ? row[x].ch : ' ';
		if (cp == ' ')
			continue;		/* the fill already drew it */

		uint8_t fg = row[x].fg, bg = row[x].bg;
		/* KT_A_REVERSE is how the mouse pointer and selected rows are
		 * drawn, and it is a swap, not a highlight — matching what the
		 * terminal does with the same attribute. */
		if (row[x].attr & KT_A_REVERSE) {
			uint8_t t = fg;
			fg = bg;
			bg = t;
			pixman_color_t c = to_pixman(ktui_theme->slot[bg]);
			pixman_image_fill_rectangles(
				PIXMAN_OP_SRC, dst, &c, 1,
				&(pixman_rectangle16_t){
					(int16_t)(x * cw), (int16_t)y,
					(uint16_t)cw, (uint16_t)ch });
		}

		const struct fcft_glyph *g = kwl_glyph(cp);
		if (!g || !g->pix)
			continue;

		pixman_color_t c = to_pixman(ktui_theme->slot[fg]);
		pixman_image_t *src = pixman_image_create_solid_fill(&c);
		if (!src)
			continue;

		/*
		 * Clipped to the cell. A glyph whose advance exceeds the cell
		 * would otherwise bleed into its neighbour, and libktui's
		 * box-drawing characters have to TILE — one pixel of overhang
		 * turns a continuous border into a dashed one.
		 */
		pixman_image_composite32(
			PIXMAN_OP_OVER, src, g->pix, dst, 0, 0, 0, 0,
			x * cw + g->x, y + kwl_font_ascent() - g->y,
			g->width, g->height);
		pixman_image_unref(src);

		if (row[x].attr & KT_A_UNDERLINE) {
			pixman_color_t u = to_pixman(ktui_theme->slot[fg]);
			pixman_image_fill_rectangles(
				PIXMAN_OP_OVER, dst, &u, 1,
				&(pixman_rectangle16_t){
					(int16_t)(x * cw),
					(int16_t)(y + ch - 2),
					(uint16_t)cw, 1 });
		}
	}
}

/*
 * `prev` is updated as we go, exactly as the tty backend does, so the next
 * frame only repaints rows that changed. Row granularity rather than cell:
 * the fill runs above already amortise a row, and per-cell damage tracking
 * would cost more bookkeeping than it saves on a grid this size.
 */
void kwl_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev, int w,
	       int h, int full)
{
	for (int y = 0; y < h; y++) {
		const KtuiCell *crow = cur + (size_t)y * w;
		KtuiCell *prow = prev + (size_t)y * w;
		if (!full && !memcmp(crow, prow, (size_t)w * sizeof(*crow)))
			continue;
		paint_row(dst, crow, w, y);
		memcpy(prow, crow, (size_t)w * sizeof(*crow));
	}
}

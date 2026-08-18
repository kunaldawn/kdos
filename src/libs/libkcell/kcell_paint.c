/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   cells -> pixels
 *
 * One KtuiCell becomes one (cell_w x cell_h) * scale rectangle: the background
 * filled, the glyph composited over it as an alpha mask in the foreground
 * colour.
 *
 * The colours are libkcolor's, reached through libktui's eight slots — the
 * same eight the tty gets. That is the point of the whole exercise and not a
 * limitation to grow out of: the panel and the installer and the build screen
 * and now the window frames render the SAME picture whether they are on tty1 or
 * in a compositor, because there is one palette table and one set of slots. Do
 * not add a ninth here because a compositor could afford it.
 * ---------------------------------
 */

#include <string.h>

#include "kcell.h"

/*
 * When set, cells whose background is KT_BG are left fully transparent instead
 * of filled. It exists for exactly one caller — the desktop, which sits on the
 * BACKGROUND layer above the compositor's wallpaper and must not paint over it.
 * Off everywhere else, because a panel with a see-through background is a panel
 * you cannot read.
 */
static bool transparent_bg;

void kcell_set_transparent_bg(bool on)
{
	transparent_bg = on;
}

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

pixman_color_t kcell_slot_color(int slot)
{
	return to_pixman(ktui_theme->slot[slot & 7]);
}

/*
 * Paint one row of cells.
 *
 * Row at a time rather than cell at a time so that runs of identical
 * background can be filled in one pixman op — a panel is mostly one colour, a
 * window frame almost entirely one colour, and a per-cell rectangle fill is the
 * difference between a frame that fits in the deadline and one that does not.
 */
static void paint_row(pixman_image_t *dst, const KtuiCell *row, int w,
		      int y_cell, int scale)
{
	const int cw = kcell_w() * scale, ch = kcell_h() * scale;
	int y = y_cell * ch;

	for (int x = 0; x < w;) {
		uint8_t bg = row[x].bg;
		int run = 1;
		while (x + run < w && row[x + run].bg == bg)
			run++;

		/* Still an OP_SRC fill, so the run is CLEARED to zero rather
		 * than skipped — the buffer is reused between frames and a skip
		 * would leave the last frame's pixels behind. */
		pixman_color_t c = (transparent_bg && bg == KT_BG)
			? (pixman_color_t){ 0, 0, 0, 0 }
			: to_pixman(ktui_theme->slot[bg]);
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, dst, &c, 1,
			&(pixman_rectangle16_t){ (int16_t)(x * cw), (int16_t)y,
						 (uint16_t)(run * cw),
						 (uint16_t)ch });
		x += run;
	}

	int covered = 0;	/* the cell before painted across this one */

	for (int x = 0; x < w; x++) {
		uint32_t cp = row[x].ch ? row[x].ch : ' ';
		int was_covered = covered;
		covered = 0;
		if (cp == ' ')
			continue;		/* the fill already drew it */
		/* Control cells carry colour only, never a glyph.
		 * KTUI_WIDE_CONT marks the continuation half of a double-width
		 * glyph; its lead cell paints across it below — but only when
		 * the lead's glyph really was two cells wide. A codepoint no
		 * font has, or one a fallback face renders single-width, leaves
		 * the continuation to swap its own colours, or a reversed run
		 * comes out with one un-lit cell per wide character. */
		if (cp < 0x20) {
			if (!was_covered && (row[x].attr & KT_A_REVERSE)) {
				pixman_color_t c =
					to_pixman(ktui_theme->slot[row[x].fg]);
				pixman_image_fill_rectangles(
					PIXMAN_OP_SRC, dst, &c, 1,
					&(pixman_rectangle16_t){
						(int16_t)(x * cw), (int16_t)y,
						(uint16_t)cw, (uint16_t)ch });
			}
			continue;
		}

		uint8_t fg = row[x].fg, bg = row[x].bg;

		/*
		 * A SPRITE cell: composite the matching sub-rectangle of the
		 * caller's picture over the background that was just filled.
		 *
		 * Over, not src, and that is the point — an icon with an alpha
		 * edge takes the panel's colour, or the selection's fill on a
		 * highlighted row, for free. The picture was scaled to the
		 * whole sprite's pixel size by whoever registered it (libkicon
		 * does it once per name+size and caches), so all this does is
		 * pick the cell out of it. Nothing here can fail: an
		 * unregistered slot leaves the background, which is what an
		 * icon that went away should look like.
		 */
		if (KTUI_IS_SPRITE(cp)) {
			const KtuiSprite *s =
				ktui_sprite_get((int)KTUI_SPRITE_SLOT(cp));
			/* Reverse under a sprite is a swap like everywhere
			 * else: the fill that the icon then sits on becomes
			 * the foreground slot. */
			if (row[x].attr & KT_A_REVERSE) {
				pixman_color_t rc =
					to_pixman(ktui_theme->slot[fg]);
				pixman_image_fill_rectangles(
					PIXMAN_OP_SRC, dst, &rc, 1,
					&(pixman_rectangle16_t){
						(int16_t)(x * cw), (int16_t)y,
						(uint16_t)cw, (uint16_t)ch });
			}
			if (s && s->pix)
				pixman_image_composite32(
					PIXMAN_OP_OVER,
					(pixman_image_t *)s->pix, NULL, dst,
					(int)KTUI_SPRITE_SX(cp) * cw,
					(int)KTUI_SPRITE_SY(cp) * ch,
					0, 0, x * cw, y, cw, ch);
			continue;
		}

		KCellGlyph g;
		bool have = kcell_glyph_scaled(cp, scale, &g);

		/*
		 * A glyph wider than its cell may spill into the next one ONLY
		 * when libktui reserved that cell for it — otherwise the clip
		 * below cuts it at the cell edge.
		 */
		int cells = 1;
		if (have && g.x + g.width > cw && x + 1 < w &&
		    row[x + 1].ch == KTUI_WIDE_CONT) {
			cells = 2;
			covered = 1;
		}

		/* KT_A_REVERSE is how the mouse pointer and selected rows are
		 * drawn, and it is a swap, not a highlight — matching what the
		 * terminal does with the same attribute. Covers the reserved
		 * continuation cell too: painting it on its own turn would
		 * overwrite the right half of the glyph already composited. */
		if (row[x].attr & KT_A_REVERSE) {
			uint8_t t = fg;
			fg = bg;
			bg = t;
			pixman_color_t c = to_pixman(ktui_theme->slot[bg]);
			pixman_image_fill_rectangles(
				PIXMAN_OP_SRC, dst, &c, 1,
				&(pixman_rectangle16_t){
					(int16_t)(x * cw), (int16_t)y,
					(uint16_t)(cells * cw),
					(uint16_t)ch });
		}

		if (!have)
			continue;

		pixman_color_t c = to_pixman(ktui_theme->slot[fg]);
		pixman_image_t *src = pixman_image_create_solid_fill(&c);
		if (!src)
			continue;

		/*
		 * Clipped to the cell (or the two cells of a wide pair) by
		 * arithmetic on the composite rectangle rather than a pixman
		 * clip region — swapping the destination's region per glyph
		 * would also clip the row's background fills. A fallback glyph
		 * whose bitmap exceeds the cell would otherwise bleed into its
		 * neighbour, and libktui's box-drawing characters have to TILE
		 * — one pixel of overhang turns a continuous border into a
		 * dashed one.
		 */
		{
			int dx = x * cw + g.x;
			int dy = y + kcell_ascent() * scale - g.y;
			int cx1 = x * cw + cells * cw, cy1 = y + ch;
			int mx = dx < x * cw ? x * cw - dx : 0;
			int my = dy < y ? y - dy : 0;
			int ex = dx + g.width < cx1 ? dx + g.width : cx1;
			int ey = dy + g.height < cy1 ? dy + g.height : cy1;
			int px = dx + mx, py = dy + my;

			if (ex > px && ey > py)
				pixman_image_composite32(
					PIXMAN_OP_OVER, src, g.pix, dst,
					0, 0, mx, my, px, py,
					ex - px, ey - py);
		}
		pixman_image_unref(src);

		if (row[x].attr & KT_A_UNDERLINE) {
			pixman_color_t u = to_pixman(ktui_theme->slot[fg]);
			pixman_image_fill_rectangles(
				PIXMAN_OP_OVER, dst, &u, 1,
				&(pixman_rectangle16_t){
					(int16_t)(x * cw),
					(int16_t)(y + ch - 2 * scale),
					(uint16_t)(cells * cw),
					(uint16_t)scale });
		}
	}
}

/*
 * Fill everything the grid does not reach with KT_BG.
 *
 * A cell grid on a surface whose height is not a multiple of the cell height
 * leaves a strip of whatever the buffer happened to contain — visible as a
 * black band along the bottom of the lock screen at 1280x800, which is a live
 * defect this fixes for every consumer at once. Two rectangles, not four: the
 * right-hand one is full height, so the bottom one only has to cover what is
 * left to its left.
 */
static void pad_remainder(pixman_image_t *dst, int used_w, int used_h,
			  int dst_w, int dst_h)
{
	pixman_color_t bg = transparent_bg ? (pixman_color_t){ 0, 0, 0, 0 }
					   : to_pixman(ktui_theme->slot[KT_BG]);

	if (used_w < dst_w)
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, dst, &bg, 1,
			&(pixman_rectangle16_t){ (int16_t)used_w, 0,
						 (uint16_t)(dst_w - used_w),
						 (uint16_t)dst_h });
	if (used_h < dst_h)
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, dst, &bg, 1,
			&(pixman_rectangle16_t){ 0, (int16_t)used_h,
						 (uint16_t)(used_w < dst_w ?
							    used_w : dst_w),
						 (uint16_t)(dst_h - used_h) });
}

void kcell_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev,
		 int cols, int rows, int full, int scale, int dst_w, int dst_h)
{
	if (scale < 1)
		scale = 1;
	if (scale > KCELL_MAX_SCALE)
		scale = KCELL_MAX_SCALE;

	/*
	 * CLIP THE DESTINATION, and this is not belt-and-braces — without it
	 * this function writes outside the caller's allocation.
	 *
	 * pixman clips a COMPOSITE to the destination automatically
	 * (_pixman_compute_composite_region32 clamps to bits.width/height), so
	 * the glyph blit below is safe and always has been. A FILL is not:
	 * pixman_image_fill_rectangles with PIXMAN_OP_SRC takes a fast path that
	 * intersects the rectangle only with `common.clip_region`, and
	 * pixman_image_create_bits leaves have_clip_region FALSE. It then calls
	 * pixman_fill32, which is `bits += y*stride + x` and two loops that have
	 * never heard of the image height.
	 *
	 * Every caller here paints ceil(w/cell) cells into a buffer that is only
	 * w pixels wide, on the assumption that the last cell is clipped. For
	 * the glyphs it is. For the background fill it was not: measured against
	 * pixman 0.46.4, a 16x673 strip filled at (0,672,16,32) writes 1984
	 * bytes past the end of the allocation. That is the heap corruption that
	 * crashed kdos-comp on the first real window, and it was invisible to
	 * ASan because the store happens inside uninstrumented libpixman.
	 *
	 * One region here fixes every caller, which is why it is here and not in
	 * deco.c.
	 */
	pixman_region32_t clip;
	pixman_region32_init_rect(&clip, 0, 0, (unsigned)dst_w, (unsigned)dst_h);
	pixman_image_set_clip_region32(dst, &clip);
	pixman_region32_fini(&clip);

	/*
	 * No `prev` means no history to diff against, which is the compositor's
	 * case: a frame strip is a handful of cells painted into a buffer that
	 * was just allocated. Forcing full here rather than making every such
	 * caller remember to pass it.
	 */
	if (!prev)
		full = 1;

	for (int y = 0; y < rows; y++) {
		const KtuiCell *crow = cur + (size_t)y * cols;
		if (!full && prev) {
			KtuiCell *prow = prev + (size_t)y * cols;
			if (!memcmp(crow, prow, (size_t)cols * sizeof(*crow)))
				continue;
			paint_row(dst, crow, cols, y, scale);
			memcpy(prow, crow, (size_t)cols * sizeof(*crow));
			continue;
		}
		paint_row(dst, crow, cols, y, scale);
		if (prev)
			memcpy(prev + (size_t)y * cols, crow,
			       (size_t)cols * sizeof(*crow));
	}

	/* Only on a full paint: the remainder cannot change without the
	 * destination changing size, and a resize is a full paint by
	 * construction. */
	if (full)
		pad_remainder(dst, cols * kcell_w() * scale,
			      rows * kcell_h() * scale, dst_w, dst_h);
}

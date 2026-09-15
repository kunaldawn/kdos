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
 * Per-slot background alpha. Opaque everywhere until a surface says otherwise:
 * the desktop clears KT_BG so the compositor's wallpaper shows, and the panel
 * clears KT_SURFACE so its own pixel backdrop does.
 *
 * `any_alpha` is a cached OR of the table. It is read once per buffer
 * allocation to pick the shm format, and asking it must not mean walking eight
 * bytes on a path that runs per surface.
 */
static uint8_t slot_alpha[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };
static bool any_alpha;

/*
 * WHAT A ZERO-ALPHA BACKGROUND DOES WITH ITS PIXELS, and the two answers are
 * not interchangeable.
 *
 * The row fill is OP_SRC on purpose — the buffer is reused between frames, so
 * a run that is skipped keeps the LAST frame's pixels. For the desktop, which
 * has nothing underneath it in its own buffer, clearing to transparent is
 * therefore right and skipping would smear.
 *
 * A surface with a BACKDROP is the opposite case: something has just painted
 * those pixels this frame, one layer down in the same image, and clearing them
 * erases it. The bar would come up as text floating on nothing.
 */
static bool bg_preserve;

void kcell_set_bg_preserve(bool on)
{
	bg_preserve = on;
}

/* True when this background belongs to a backdrop that has already painted
 * it. Only ever true for a slot the caller cleared to alpha 0. */
static inline bool bg_owned(uint8_t slot)
{
	return bg_preserve && slot_alpha[slot & 7] == 0;
}

void kcell_set_slot_alpha(int slot, uint8_t alpha)
{
	slot_alpha[slot & 7] = alpha;
	any_alpha = false;
	for (int i = 0; i < 8; i++)
		if (slot_alpha[i] != 255)
			any_alpha = true;
}

uint8_t kcell_slot_alpha(int slot)
{
	return slot_alpha[slot & 7];
}

void kcell_reset_slot_alpha(void)
{
	memset(slot_alpha, 255, sizeof(slot_alpha));
	any_alpha = false;
}

bool kcell_needs_alpha(void)
{
	return any_alpha;
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

/* One horizontal rule: what underline, strike and overline all are, differing
 * only in the row they land on. */
static void rule(pixman_image_t *dst, int x, int y, int w, int t,
		 pixman_color_t c)
{
	pixman_image_fill_rectangles(PIXMAN_OP_OVER, dst, &c, 1,
				     &(pixman_rectangle16_t){
					     (int16_t)x, (int16_t)y,
					     (uint16_t)w, (uint16_t)t });
}

/*
 * A BROKEN RULE IS ONE PIXMAN CALL, NOT ONE PER PIECE.
 *
 * pixman_image_fill_rectangles takes an array, and every fill_rectangles is a
 * region init and an intersect against the destination clip before a pixel
 * moves. A curl or a dotted line is dozens of disjoint pieces in a single
 * colour, so batching them is pixel-identical and turns a per-column cost into
 * a per-cell one — the difference between a row of undercurl costing a large
 * fraction of a frame and costing nothing worth measuring.
 *
 * The array is fixed and flushed when it fills: the cell width comes from the
 * font's widest advance times the scale, so there is no compile-time bound to
 * size it from, and a flush every RULE_RECTS pieces keeps the worst case at
 * one call per RULE_RECTS columns.
 */
#define RULE_RECTS 64

static void rules_flush(pixman_image_t *dst, pixman_color_t c,
			const pixman_rectangle16_t *r, int *n)
{
	if (*n) {
		pixman_image_fill_rectangles(PIXMAN_OP_OVER, dst, &c, *n, r);
		*n = 0;
	}
}

static inline void rules_add(pixman_image_t *dst, pixman_color_t c,
			     pixman_rectangle16_t *r, int *n,
			     int x, int y, int w, int t)
{
	if (*n == RULE_RECTS)
		rules_flush(dst, c, r, n);
	r[*n].x = (int16_t)x;
	r[*n].y = (int16_t)y;
	r[*n].width = (uint16_t)w;
	r[*n].height = (uint16_t)t;
	(*n)++;
}

/*
 * THE UNDERLINE'S SHAPE.
 *
 * A wave is drawn as a column at a time from a fixed eight-step table rather
 * than from `sin()`: this library links no maths library, the period is a
 * handful of pixels where a real sine is a straight line anyway, and a table
 * cannot drift between one scale and the next. It rides ABOVE the plain line's
 * row so the crest stays inside the cell — a curl that left the cell would be
 * clipped by the row below repainting, and would flicker.
 *
 * Adjacent columns at the same crest height are coalesced into one rectangle
 * before the batch is issued: the eight-step table holds each step for `scale`
 * columns and repeats offsets within a period, so the run is where most of the
 * saving is.
 *
 * Every unknown style falls to the plain line. A shape nobody drew is worse
 * than the wrong shape: the attribute means "this word is marked".
 */
static void underline(pixman_image_t *dst, int x, int y, int cw, int ch,
		      int scale, unsigned style, pixman_color_t c)
{
	static const int wave[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
	int base = y + ch - 2 * scale;
	pixman_rectangle16_t r[RULE_RECTS];
	int n = 0;

	switch (style) {
	case KT_UL_DOUBLE:
		rules_add(dst, c, r, &n, x, base - 2 * scale, cw, scale);
		rules_add(dst, c, r, &n, x, base, cw, scale);
		break;
	case KT_UL_CURLY:
		for (int i = 0; i < cw;) {
			int off = wave[(i / scale) % 8] * scale / 2;
			int run = 1;

			while (i + run < cw &&
			       wave[((i + run) / scale) % 8] * scale / 2 == off)
				run++;
			rules_add(dst, c, r, &n, x + i, base - scale - off,
				  run, scale);
			i += run;
		}
		break;
	case KT_UL_DOTTED:
		for (int i = 0; i < cw; i += 2 * scale)
			rules_add(dst, c, r, &n, x + i, base, scale, scale);
		break;
	case KT_UL_DASHED:
		for (int i = 0; i < cw; i += 6 * scale)
			rules_add(dst, c, r, &n, x + i, base, 3 * scale, scale);
		break;
	default:
		rules_add(dst, c, r, &n, x, base, cw, scale);
		break;
	}
	rules_flush(dst, c, r, &n);
}

pixman_color_t kcell_slot_color(int slot)
{
	return to_pixman(ktui_theme->slot[slot & 7]);
}

/*
 * A BACKGROUND slot as pixman wants it — the one place the per-slot alpha is
 * applied, so every fill in this file agrees about what a translucent
 * background is.
 *
 * Premultiplied, because that is what both PIXMAN_a8r8g8b8 and Wayland's
 * ARGB8888 mean: the channels are already scaled by the alpha. Handing pixman
 * un-premultiplied channels does not fail, it just paints everything too
 * bright, which is the kind of wrong that looks like a colour choice.
 *
 * The intermediate is 32-bit on purpose: c.r * 257 already reaches 65535, and
 * multiplying that by the alpha overflows a uint16 by two orders of magnitude.
 */
static pixman_color_t bg_color(uint8_t slot)
{
	uint8_t a = slot_alpha[slot & 7];
	KRgb c;

	if (a == 255)
		return to_pixman(ktui_theme->slot[slot & 7]);
	if (a == 0)
		return (pixman_color_t){ 0, 0, 0, 0 };
	c = ktui_theme->slot[slot & 7];
	return (pixman_color_t){
		.red   = (uint16_t)((uint32_t)c.r * 257 * a / 255),
		.green = (uint16_t)((uint32_t)c.g * 257 * a / 255),
		.blue  = (uint16_t)((uint32_t)c.b * 257 * a / 255),
		.alpha = (uint16_t)((uint32_t)a * 257),
	};
}

/* A literal a terminal asked for, as pixman wants it. Opaque, always: the
 * per-slot alpha is what makes a THEME background translucent, and a colour a
 * program named is not the theme's to fade. */
static pixman_color_t rgb_color(uint32_t rgb)
{
	KRgb c = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb };

	return to_pixman(c);
}

/* Two cells share a background run only if they share the SAME background —
 * two literals that differ, or a literal and a slot, are two runs however
 * close the colours look. */
static int same_bg(const KtuiCell *a, const KtuiCell *b)
{
	if ((a->attr & KT_A_BGRGB) != (b->attr & KT_A_BGRGB))
		return 0;
	return (a->attr & KT_A_BGRGB) ? a->bgc == b->bgc : a->bg == b->bg;
}

/*
 * THE FOREGROUND AS A PIXMAN SOURCE, CACHED.
 *
 * A glyph is composited through a solid-fill image, and creating one is a
 * malloc and an image init. Every text cell of every repainted row wanted its
 * own, which on a full-screen animation is tens of thousands of malloc/free
 * pairs a second before a single pixel is touched.
 *
 * A solid fill is immutable, so one per colour is all anybody needs: the eight
 * slots are kept by index and rebuilt when the palette moves, and the literals
 * a terminal names go in a small direct-mapped table, which is enough because
 * a frame draws a handful of distinct colours even when it draws thousands of
 * cells.
 *
 * THE LITERAL TABLE IS INDEXED BY THE HIGH BITS OF THE HASH. A multiplicative
 * hash puts its mixing at the top of the product — the low bits of `rgb * k`
 * depend only on the low bits of `rgb`, so a modulo would key the table on the
 * bottom of the BLUE channel alone and land the whole xterm colour cube in six
 * buckets. Taking the top bits keeps red and green in the key, which is the
 * difference between a hit rate near one and a malloc per cell per frame.
 */
#define SOLID_LIT_BITS 6
#define SOLID_LIT (1 << SOLID_LIT_BITS)

static pixman_image_t *solid_slot[8];
static const KtuiTheme *solid_theme;
static KRgb solid_rgb[KT_NCOLOR];
static pixman_image_t *solid_lit[SOLID_LIT];
static uint32_t solid_lit_key[SOLID_LIT];
static bool solid_lit_set[SOLID_LIT];

static void solid_drop(void)
{
	for (int i = 0; i < 8; i++)
		if (solid_slot[i]) {
			pixman_image_unref(solid_slot[i]);
			solid_slot[i] = NULL;
		}
	for (int i = 0; i < SOLID_LIT; i++)
		if (solid_lit[i]) {
			pixman_image_unref(solid_lit[i]);
			solid_lit[i] = NULL;
			solid_lit_set[i] = false;
		}
}

/*
 * The palette in force changed, so every cached slot names a colour that is no
 * longer the theme's. The literals are unaffected: a colour a program asked
 * for exactly is not the theme's to move.
 *
 * KEYED ON THE COLOURS, NOT ON THE POINTER. libktui projects night light by
 * rewriting one file-static theme IN PLACE and pointing `ktui_theme` at it
 * again, so a scheme change under night light moves every slot without moving
 * the address — and a cache keyed on identity would keep painting the old
 * scheme's ink for the life of the session while backgrounds and rules, which
 * read the table fresh, came up in the new one. This runs once per painted
 * ROW, so the compare is eight RGB triples against a frame of pixel work.
 */
static void solid_sync(void)
{
	if (solid_theme == ktui_theme &&
	    !memcmp(solid_rgb, ktui_theme->slot, sizeof(solid_rgb)))
		return;
	for (int i = 0; i < 8; i++)
		if (solid_slot[i]) {
			pixman_image_unref(solid_slot[i]);
			solid_slot[i] = NULL;
		}
	solid_theme = ktui_theme;
	memcpy(solid_rgb, ktui_theme->slot, sizeof(solid_rgb));
}

static pixman_image_t *solid_for_slot(uint8_t slot)
{
	slot &= 7;
	if (!solid_slot[slot]) {
		pixman_color_t c = to_pixman(ktui_theme->slot[slot]);

		solid_slot[slot] = pixman_image_create_solid_fill(&c);
	}
	return solid_slot[slot];
}

static pixman_image_t *solid_for_rgb(uint32_t rgb)
{
	unsigned h = (rgb * 2654435761u) >> (32 - SOLID_LIT_BITS);

	if (solid_lit_set[h] && solid_lit_key[h] == rgb && solid_lit[h])
		return solid_lit[h];
	if (solid_lit[h])
		pixman_image_unref(solid_lit[h]);

	pixman_color_t c = rgb_color(rgb);

	solid_lit[h] = pixman_image_create_solid_fill(&c);
	solid_lit_key[h] = rgb;
	solid_lit_set[h] = solid_lit[h] != NULL;
	return solid_lit[h];
}

void kcell_paint_forget(void)
{
	solid_drop();
	solid_theme = NULL;
}

/*
 * Paint one row of cells.
 *
 * Row at a time rather than cell at a time so that runs of identical
 * background can be filled in one pixman op — a panel is mostly one colour, a
 * window frame almost entirely one colour, and a per-cell rectangle fill is the
 * difference between a frame that fits in the deadline and one that does not.
 */
/*
 * `x0`/`x1` are the half-open span of CHANGED cells; the row outside it holds
 * what the last frame left and is not touched. The caller widens the span by a
 * cell on each side, which is what covers a wide glyph's continuation and the
 * overhang a box-drawing character puts into its neighbour.
 */
static void paint_row(pixman_image_t *dst, const KtuiCell *row, int w,
		      int y_cell, int scale, int x0, int x1)
{
	const int cw = kcell_w() * scale, ch = kcell_h() * scale;
	int y = y_cell * ch;

	solid_sync();

	for (int x = x0; x < x1;) {
		uint8_t bg = row[x].bg;
		int lit = (row[x].attr & KT_A_BGRGB) != 0;
		int run = 1;
		while (x + run < x1 && same_bg(&row[x], &row[x + run]))
			run++;

		/* Still an OP_SRC fill, so the run is CLEARED to zero rather
		 * than skipped — the buffer is reused between frames and a skip
		 * would leave the last frame's pixels behind. */
		if (!lit && bg_owned(bg)) {
			x += run;	/* the backdrop painted it */
			continue;
		}
		pixman_color_t c = lit ? rgb_color(row[x].bgc) : bg_color(bg);
		pixman_image_fill_rectangles(
			PIXMAN_OP_SRC, dst, &c, 1,
			&(pixman_rectangle16_t){ (int16_t)(x * cw), (int16_t)y,
						 (uint16_t)(run * cw),
						 (uint16_t)ch });
		x += run;
	}

	int covered = 0;	/* the cell before painted across this one */

	for (int x = x0; x < x1; x++) {
		uint32_t cp = row[x].ch ? row[x].ch : ' ';
		int was_covered = covered;
		covered = 0;
		/*
		 * A CELL WITH NO GLYPH STILL HAS TO HONOUR REVERSE. A space and
		 * a control cell both carry colour and nothing to draw, and the
		 * fill pass has already painted each in its background slot —
		 * so a swap that is not painted here is lost. That swap is the
		 * mouse pointer over empty desktop and the blank tail of a
		 * selected line: both are entirely blank cells, and skipping
		 * them makes the pointer visible only where it happens to sit
		 * over text.
		 *
		 * KTUI_WIDE_CONT marks the continuation half of a double-width
		 * glyph; its lead cell paints across it below — but only when
		 * the lead's glyph really was two cells wide. A codepoint no
		 * font has, or one a fallback face renders single-width, leaves
		 * the continuation to swap its own colours, or a reversed run
		 * comes out with one un-lit cell per wide character.
		 */
		if (cp == ' ' || cp < 0x20) {
			if (!was_covered && (row[x].attr & KT_A_REVERSE) &&
			    ((row[x].attr & KT_A_FGRGB) ||
			     !bg_owned(row[x].fg))) {
				pixman_color_t c =
					(row[x].attr & KT_A_FGRGB)
						? rgb_color(row[x].fgc)
						: bg_color(row[x].fg);
				pixman_image_fill_rectangles(
					PIXMAN_OP_SRC, dst, &c, 1,
					&(pixman_rectangle16_t){
						(int16_t)(x * cw), (int16_t)y,
						(uint16_t)cw, (uint16_t)ch });
			}
			/*
			 * AND THE RULES, WHICH A BLANK CELL CARRIES LIKE ANY
			 * OTHER. An underline runs under the spaces between
			 * words and to the end of a marked run; a strike goes
			 * through them. A cell with nothing to draw that skips
			 * them breaks every underline into one dash per word,
			 * which is the one thing the attribute exists to avoid.
			 *
			 * THE RULE TAKES THE SWAPPED COLOUR UNDER REVERSE, and
			 * the literal flag travels with the slot it belongs to.
			 * The fill just above painted this cell in what reverse
			 * made its background — the FOREGROUND colour — so a
			 * rule drawn in the foreground would be a rule drawn in
			 * the colour of the cell it sits on. That is a
			 * selection or the console pointer crossing an
			 * underlined space, where the line must stay visible.
			 * KT_A_ULCOLOR is not part of the exchange: a colour a
			 * program named for the underline is the underline's,
			 * reversed or not.
			 */
			if (row[x].attr & (KT_A_UNDERLINE | KT_A_STRIKE |
					   KT_A_OVERLINE)) {
				unsigned bat = row[x].attr;
				int rev = (bat & KT_A_REVERSE) != 0;
				int rlit = rev ? (bat & KT_A_BGRGB)
					       : (bat & KT_A_FGRGB);
				uint32_t rl = rev ? row[x].bgc : row[x].fgc;
				uint8_t rs = rev ? row[x].bg : row[x].fg;
				pixman_color_t rc =
					rlit ? rgb_color(rl)
					     : to_pixman(ktui_theme->slot[rs & 7]);

				if (bat & KT_A_UNDERLINE)
					underline(dst, x * cw, y, cw, ch, scale,
						  KT_UL_STYLE(bat),
						  (bat & KT_A_ULCOLOR)
							  ? rgb_color(row[x].ulc)
							  : rc);
				if (bat & KT_A_STRIKE)
					rule(dst, x * cw, y + ch / 2, cw,
					     scale, rc);
				if (bat & KT_A_OVERLINE)
					rule(dst, x * cw, y, cw, scale, rc);
			}
			continue;
		}

		uint8_t fg = row[x].fg, bg = row[x].bg;
		/*
		 * A LITERAL TRAVELS WITH ITS SLOT THROUGH THE SWAP. Reverse is
		 * an exchange of what is drawn and what is behind it, so a
		 * terminal's own colour has to change places with the same
		 * move — a swap that carried only the slots would draw a
		 * reversed cell in a colour neither half of it named.
		 */
		unsigned at = row[x].attr;
		uint32_t fgl = row[x].fgc, bgl = row[x].bgc;
		int fg_lit = (at & KT_A_FGRGB) != 0;
		int bg_lit = (at & KT_A_BGRGB) != 0;

		if (at & KT_A_REVERSE) {
			uint8_t ts = fg;
			uint32_t tl = fgl;
			int tb = fg_lit;

			fg = bg;
			bg = ts;
			fgl = bgl;
			bgl = tl;
			fg_lit = bg_lit;
			bg_lit = tb;
		}

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
			if ((at & KT_A_REVERSE) && (bg_lit || !bg_owned(bg))) {
				pixman_color_t rc = bg_lit ? rgb_color(bgl)
							   : bg_color(bg);
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
		int style = ((at & KT_A_ITALIC) ? KCELL_ST_ITALIC : 0) |
			    ((at & KT_A_BOLD) ? KCELL_ST_BOLD : 0);
		bool have = kcell_glyph_face(cp, scale, style, &g);

		/*
		 * A glyph wider than its cell may spill into the next one ONLY
		 * when libktui reserved that cell for it — otherwise the clip
		 * below cuts it at the cell edge.
		 */
		int cells = 1;
		if (have && g.x + g.width > cw && x + 1 < w && x + 1 < x1 &&
		    row[x + 1].ch == KTUI_WIDE_CONT) {
			cells = 2;
			covered = 1;
		}

		/* KT_A_REVERSE is how the mouse pointer and selected rows are
		 * drawn, and it is a swap, not a highlight — matching what the
		 * terminal does with the same attribute. Covers the reserved
		 * continuation cell too: painting it on its own turn would
		 * overwrite the right half of the glyph already composited. */
		if (at & KT_A_REVERSE) {
			if (!bg_lit && bg_owned(bg))
				goto glyph;
			pixman_color_t c = bg_lit ? rgb_color(bgl)
						  : bg_color(bg);
			pixman_image_fill_rectangles(
				PIXMAN_OP_SRC, dst, &c, 1,
				&(pixman_rectangle16_t){
					(int16_t)(x * cw), (int16_t)y,
					(uint16_t)(cells * cw),
					(uint16_t)ch });
		}

glyph:
		if (!have) {
			/* No bitmap — a codepoint no font carries, or one
			 * whose glyph is empty — and the rules are still the
			 * cell's. Same reason a blank cell keeps them. */
			pixman_color_t rc =
				fg_lit ? rgb_color(fgl)
				       : to_pixman(ktui_theme->slot[fg & 7]);

			if (at & KT_A_UNDERLINE)
				underline(dst, x * cw, y, cells * cw, ch, scale,
					  KT_UL_STYLE(at),
					  (at & KT_A_ULCOLOR)
						  ? rgb_color(row[x].ulc)
						  : rc);
			if (at & KT_A_STRIKE)
				rule(dst, x * cw, y + ch / 2, cells * cw, scale,
				     rc);
			if (at & KT_A_OVERLINE)
				rule(dst, x * cw, y, cells * cw, scale, rc);
			continue;
		}

		/*
		 * The slot is masked here as it is everywhere else in this
		 * file: `fg` is a byte off a socket by the time a console
		 * surface's cells reach the painter, and the theme has eight
		 * entries.
		 */
		pixman_color_t c = fg_lit ? rgb_color(fgl)
					  : to_pixman(ktui_theme->slot[fg & 7]);
		pixman_image_t *src = fg_lit ? solid_for_rgb(fgl)
					     : solid_for_slot(fg);
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
		 *
		 * BOLD WITH NO BOLD FACE IS THE SAME MASK STRUCK TWICE, one
		 * scaled pixel apart — a weight approximated rather than a
		 * weight dropped, which is what the attribute is for. Both
		 * strikes take the clip above, so a glyph already filling its
		 * cell gains no overhang from the second.
		 */
		for (int k = 0, strikes = g.synth_bold ? 2 : 1; k < strikes;
		     k++) {
			int dx = x * cw + g.x + k * scale;
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

		/*
		 * The rules go on AFTER the glyph, each at the height its
		 * name means: under the baseline, through the middle, and
		 * along the top of the cell. A strike a descender crossed
		 * would otherwise be the one a person could not see, which is
		 * the one case the attribute exists for.
		 */
		if (at & KT_A_UNDERLINE) {
			/* SGR 58's colour if the program named one, and the
			 * text's otherwise — an underline in a colour nobody
			 * asked for is a mark on the wrong word. */
			pixman_color_t uc = (at & KT_A_ULCOLOR)
						    ? rgb_color(row[x].ulc)
						    : c;

			underline(dst, x * cw, y, cells * cw, ch, scale,
				  KT_UL_STYLE(at), uc);
		}
		if (at & KT_A_STRIKE)
			rule(dst, x * cw, y + ch / 2, cells * cw, scale, c);
		if (at & KT_A_OVERLINE)
			rule(dst, x * cw, y, cells * cw, scale, c);
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
	pixman_color_t bg = bg_color(KT_BG);

	if (bg_owned(KT_BG))
		return;			/* the backdrop reaches it too */

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
	kcell_paint_damage(dst, cur, prev, cols, rows, full, scale, dst_w,
			   dst_h, NULL);
}

int kcell_paint_damage(pixman_image_t *dst, const KtuiCell *cur,
		       KtuiCell *prev, int cols, int rows, int full, int scale,
		       int dst_w, int dst_h, unsigned char *painted)
{
	int npainted = 0;

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
	 * bytes past the end of the allocation. That is heap corruption that
	 * crashes kdos-comp on the first real window, and it is invisible to
	 * ASan because the store happens inside uninstrumented libpixman.
	 *
	 * One region here fixes every caller, which is why it is here and not in
	 * deco.c.
	 */
	pixman_region32_t clip;
	pixman_region32_init_rect(&clip, 0, 0, (unsigned)dst_w, (unsigned)dst_h);
	pixman_image_set_clip_region32(dst, &clip);
	pixman_region32_fini(&clip);

	if (painted)
		memset(painted, 0, (size_t)rows);

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
		int x0 = 0, x1 = cols;

		if (!full) {
			KtuiCell *prow = prev + (size_t)y * cols;

			if (!memcmp(crow, prow, (size_t)cols * sizeof(*crow)))
				continue;

			/*
			 * THE CHANGED SPAN, NOT THE WHOLE ROW. A caret, a
			 * clock digit or one typed character costs the cells
			 * it touched rather than every glyph beside them —
			 * and a row of a full-screen animation usually
			 * changes end to end, where this costs one extra
			 * comparison from each side and finds it.
			 *
			 * WIDENED BY A CELL EACH WAY, because libktui's box
			 * characters are allowed to overhang and a changed
			 * cell's neighbour may hold pixels this paint has to
			 * put back.
			 *
			 * AND THEN ONTO THE LEAD OF A CONTINUATION. A
			 * double-width glyph is painted entirely by its lead
			 * cell; the KTUI_WIDE_CONT marker beside it draws
			 * nothing of its own. A span that starts on the marker
			 * fills the marker's pixels — erasing the right half of
			 * the glyph — and then finds nothing to redraw there,
			 * so the character stays half-gone until something
			 * touches the lead. One step is enough: libktui
			 * reserves at most one continuation per lead.
			 */
			while (x0 < cols &&
			       !memcmp(&crow[x0], &prow[x0], sizeof(*crow)))
				x0++;
			while (x1 > x0 &&
			       !memcmp(&crow[x1 - 1], &prow[x1 - 1],
				       sizeof(*crow)))
				x1--;
			if (x0 > 0)
				x0--;
			if (x0 > 0 && crow[x0].ch == KTUI_WIDE_CONT)
				x0--;
			if (x1 < cols)
				x1++;
			memcpy(prow + x0, crow + x0,
			       (size_t)(x1 - x0) * sizeof(*crow));
		} else if (prev) {
			memcpy(prev + (size_t)y * cols, crow,
			       (size_t)cols * sizeof(*crow));
		}

		paint_row(dst, crow, cols, y, scale, x0, x1);
		if (painted)
			painted[y] = 1;
		npainted++;
	}

	/* Only on a full paint: the remainder cannot change without the
	 * destination changing size, and a resize is a full paint by
	 * construction. */
	if (full)
		pad_remainder(dst, cols * kcell_w() * scale,
			      rows * kcell_h() * scale, dst_w, dst_h);
	return npainted;
}

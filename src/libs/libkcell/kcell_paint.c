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
 * colour — except the box-drawing and block sets, which are DRAWN as
 * rectangles from their codepoint rather than asked of any face, so that a
 * border is one unbroken line at every size.
 *
 * The colours are libkcolor's, reached through libktui's eight slots — the
 * same eight the tty gets. That is the point of the whole exercise and not a
 * limitation to grow out of: the panel and the installer and the build screen
 * and now the window frames render the SAME picture whether they are on tty1 or
 * in a compositor, because there is one palette table and one set of slots. Do
 * not add a ninth here because a compositor could afford it.
 * ---------------------------------
 */

#include <stdlib.h>
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
 * font's character advance times the scale, so there is no compile-time bound to
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

/*
 * ────────────────────────────────────────────────────────────────────────
 * FRAME CHARACTERS ARE DRAWN HERE, NOT RASTERISED.
 *
 * A border is a row of box-drawing cells and it has to read as one unbroken
 * line. Rasterised from a face, that holds only while the face's box glyphs
 * are drawn to exactly the advance the cell was measured from: a face whose
 * full block spans a hair more than its advance leaves a hairline between two
 * cells at some pixel sizes, and a face that draws its box glyphs to a
 * different metric dashes the border outright. Nothing in a font's contract
 * promises either way, so the whole box-drawing and block set is drawn as
 * pixman rectangles in the cell's own foreground instead.
 *
 * BY CODEPOINT ALONE. No attribute selects this and no caller opts in, so the
 * same character is the same picture in the panel, the terminal, the installer
 * and the build screen, under any face and at any size. A cell holding one of
 * these never reaches the glyph cache at all.
 *
 * NOT CACHED AS A MASK. A cached mask is composited OVER, per pixel, through a
 * solid source; these are a fill, and one fill of at most eight rectangles is
 * cheaper than the composite it replaces — so a cache here would spend memory
 * to make the draw slower. The three shades are the exception and say why
 * where their tile is built.
 * ────────────────────────────────────────────────────────────────────────
 */

/*
 * The six positions along either axis that every box character is built from.
 * `t` is the stroke: a single rule occupies D1..D2 and a double rule's pair
 * occupies D0..D1 and D2..D3, so the single one lands exactly between the
 * pair. That is what makes ├ meet ─ and ╪ meet ║ with no step — the junction
 * and the arm are the same two numbers.
 */
enum { S_LO, S_D0, S_D1, S_D2, S_D3, S_HI };

static inline int seg_pos(int code, int base, int span, int t)
{
	int d0 = base + (span - 3 * t) / 2;

	switch (code) {
	case S_LO:
		return base;
	case S_D0:
		return d0;
	case S_D1:
		return d0 + t;
	case S_D2:
		return d0 + 2 * t;
	case S_D3:
		return d0 + 3 * t;
	default:
		return base + span;
	}
}

/*
 * The stroke, from the cell and nothing else, so a line thickens with the font
 * exactly as the face's own would have. Capped at a third of the SHORTER side:
 * a double rule is three strokes across, and a cell too small to hold three
 * draws a thinner line rather than a line that leaves its cell.
 */
static inline int stroke_width(int cw, int ch)
{
	int t = ch / 16;
	int cap = (cw < ch ? cw : ch) / 3;

	if (t > cap)
		t = cap;
	return t < 1 ? 1 : t;
}

/*
 * One rectangle of a synthesised character, CLAMPED TO ITS OWN CELL.
 *
 * Nothing drawn here may put ink in a neighbour. The repaint walks dirty ROWS
 * and hands the caller the list, and the wide-glyph clip assumes a cell owns
 * its own pixels — a character reaching past its cell leaves pixels in a row
 * nobody flushed and erases half of the glyph beside it. The clamp is the
 * guarantee, not the arithmetic that feeds it.
 */
static inline void synth_add(pixman_rectangle16_t *r, int *n,
			     int x0, int x1, int y0, int y1,
			     int X, int Y, int cw, int ch)
{
	if (x0 < X)
		x0 = X;
	if (y0 < Y)
		y0 = Y;
	if (x1 > X + cw)
		x1 = X + cw;
	if (y1 > Y + ch)
		y1 = Y + ch;
	if (x1 <= x0 || y1 <= y0)
		return;
	r[*n].x = (int16_t)x0;
	r[*n].y = (int16_t)y0;
	r[*n].width = (uint16_t)(x1 - x0);
	r[*n].height = (uint16_t)(y1 - y0);
	(*n)++;
}

/*
 * Every box character as segments, each `x0 x1 y0 y1` in S_* positions.
 *
 * A double corner is four segments and not two: the outer line turns at the
 * outer corner and the inner line at the inner one, which is what makes ╔ a
 * corner rather than two crossed pairs. ╬ is eight, the four arms stopping at
 * the square hole in the middle. ASCENDING BY CODEPOINT — the lookup is a
 * binary search and a row out of order makes a character disappear.
 */
struct box_shape {
	uint16_t cp;
	uint8_t n;
	uint8_t seg[8][4];	/* x0, x1, y0, y1, as S_* positions */
};

static const struct box_shape box_shape[] = {
	{ 0x2500, 1, { { S_LO, S_HI, S_D1, S_D2 } } },
	{ 0x2502, 1, { { S_D1, S_D2, S_LO, S_HI } } },
	{ 0x250C, 2, { { S_D1, S_HI, S_D1, S_D2 },
		       { S_D1, S_D2, S_D1, S_HI } } },
	{ 0x2510, 2, { { S_LO, S_D2, S_D1, S_D2 },
		       { S_D1, S_D2, S_D1, S_HI } } },
	{ 0x2514, 2, { { S_D1, S_HI, S_D1, S_D2 },
		       { S_D1, S_D2, S_LO, S_D2 } } },
	{ 0x2518, 2, { { S_LO, S_D2, S_D1, S_D2 },
		       { S_D1, S_D2, S_LO, S_D2 } } },
	{ 0x251C, 2, { { S_D1, S_D2, S_LO, S_HI },
		       { S_D1, S_HI, S_D1, S_D2 } } },
	{ 0x2524, 2, { { S_D1, S_D2, S_LO, S_HI },
		       { S_LO, S_D2, S_D1, S_D2 } } },
	{ 0x252C, 2, { { S_LO, S_HI, S_D1, S_D2 },
		       { S_D1, S_D2, S_D1, S_HI } } },
	{ 0x2534, 2, { { S_LO, S_HI, S_D1, S_D2 },
		       { S_D1, S_D2, S_LO, S_D2 } } },
	{ 0x253C, 2, { { S_LO, S_HI, S_D1, S_D2 },
		       { S_D1, S_D2, S_LO, S_HI } } },
	{ 0x2550, 2, { { S_LO, S_HI, S_D0, S_D1 },
		       { S_LO, S_HI, S_D2, S_D3 } } },
	{ 0x2551, 2, { { S_D0, S_D1, S_LO, S_HI },
		       { S_D2, S_D3, S_LO, S_HI } } },
	{ 0x2554, 4, { { S_D0, S_HI, S_D0, S_D1 },
		       { S_D2, S_HI, S_D2, S_D3 },
		       { S_D0, S_D1, S_D0, S_HI },
		       { S_D2, S_D3, S_D2, S_HI } } },
	{ 0x2557, 4, { { S_LO, S_D3, S_D0, S_D1 },
		       { S_LO, S_D1, S_D2, S_D3 },
		       { S_D2, S_D3, S_D0, S_HI },
		       { S_D0, S_D1, S_D2, S_HI } } },
	{ 0x255A, 4, { { S_D2, S_HI, S_D0, S_D1 },
		       { S_D0, S_HI, S_D2, S_D3 },
		       { S_D0, S_D1, S_LO, S_D3 },
		       { S_D2, S_D3, S_LO, S_D1 } } },
	{ 0x255D, 4, { { S_LO, S_D1, S_D0, S_D1 },
		       { S_LO, S_D3, S_D2, S_D3 },
		       { S_D0, S_D1, S_LO, S_D1 },
		       { S_D2, S_D3, S_LO, S_D3 } } },
	{ 0x256A, 3, { { S_LO, S_HI, S_D0, S_D1 },
		       { S_LO, S_HI, S_D2, S_D3 },
		       { S_D1, S_D2, S_LO, S_HI } } },
	{ 0x256C, 8, { { S_LO, S_D1, S_D0, S_D1 },
		       { S_D2, S_HI, S_D0, S_D1 },
		       { S_LO, S_D1, S_D2, S_D3 },
		       { S_D2, S_HI, S_D2, S_D3 },
		       { S_D0, S_D1, S_LO, S_D1 },
		       { S_D2, S_D3, S_LO, S_D1 },
		       { S_D0, S_D1, S_D2, S_HI },
		       { S_D2, S_D3, S_D2, S_HI } } },
};

static const struct box_shape *box_lookup(uint32_t cp)
{
	int lo = 0, hi = (int)(sizeof box_shape / sizeof box_shape[0]) - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2;

		if (box_shape[mid].cp == cp)
			return &box_shape[mid];
		if (box_shape[mid].cp < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/*
 * An eighth of the cell, measured from its top or its left edge — and ONE
 * PIXEL when that rounds away to nothing. A block that vanishes is worse than
 * a block a fraction too wide: the rich tier's ramp is these characters, and
 * its lowest step going blank reads as no data rather than as a small number.
 * The clamp can only bite on a cell under eight pixels across, so nothing at a
 * real cell size moves.
 */
static inline int eighth(int span, int k)
{
	int v = span * k / 8;

	return v < 1 ? 1 : v;
}

/*
 * THE BLOCK SET, AND EVERY EDGE OF IT ON THE SAME EIGHTH.
 *
 * Every edge is `span * k / 8` from the cell's top or left, so an eighth
 * block, a half block and a quadrant in neighbouring cells meet on the same
 * pixel row and the same pixel column, and a block and its complement cover
 * the cell exactly. Rounding each shape from its own fraction — a half from
 * ch/2 and a three-eighth from 3*ch/8 — is what puts a one-pixel seam across a
 * bar chart.
 *
 * `eighth()` FLOORS THAT AT ONE PIXEL and the edges measured from the far side
 * do not, because the two guarantees are different. A shape measured from the
 * near edge has a thickness the floor keeps visible at a small cell; one
 * measured from the far edge has a thickness that grows as the fraction
 * shrinks, so it can never round away, and flooring it would move the edge off
 * the eighth and break the seam guarantee above.
 */
static void block_rects(uint32_t cp, int X, int Y, int cw, int ch,
			pixman_rectangle16_t *r, int *n)
{
	/* U+2596..U+259F as a quadrant mask: bit 0 upper left, 1 upper right,
	 * 2 lower left, 3 lower right. */
	static const uint8_t quad[10] = { 0x4, 0x8, 0x1, 0xD, 0x9,
					  0x7, 0xB, 0x2, 0x6, 0xE };

	if (cp >= 0x2588 && cp <= 0x258F) {
		/* The full block and the left eighths: one right edge, on the
		 * eighth its codepoint counts down to. */
		synth_add(r, n, X, X + eighth(cw, (int)(0x2590 - cp)),
			  Y, Y + ch, X, Y, cw, ch);
		return;
	}
	if (cp >= 0x2581 && cp <= 0x2587) {
		/* The lower eighths, placed by their TOP edge, so an n-eighth
		 * block and the (8-n)-eighth one above it share a row. */
		synth_add(r, n, X, X + cw,
			  Y + ch * (int)(0x2588 - cp) / 8, Y + ch,
			  X, Y, cw, ch);
		return;
	}
	switch (cp) {
	case 0x2580:		/* ▀ */
		synth_add(r, n, X, X + cw, Y, Y + eighth(ch, 4), X, Y, cw, ch);
		return;
	case 0x2594:		/* ▔ */
		synth_add(r, n, X, X + cw, Y, Y + eighth(ch, 1), X, Y, cw, ch);
		return;
	case 0x2590:		/* ▐ */
		synth_add(r, n, X + cw * 4 / 8, X + cw, Y, Y + ch,
			  X, Y, cw, ch);
		return;
	case 0x2595:		/* ▕ */
		synth_add(r, n, X + cw * 7 / 8, X + cw, Y, Y + ch,
			  X, Y, cw, ch);
		return;
	default:
		break;
	}
	if (cp >= 0x2596 && cp <= 0x259F) {
		unsigned m = quad[cp - 0x2596];
		int mx = X + cw * 4 / 8, my = Y + ch * 4 / 8;

		/* Two quadrants side by side are one rectangle, which is what
		 * makes the half blocks and the full block fall out of the
		 * same table as a single fill. */
		if ((m & 0x3) == 0x3)
			synth_add(r, n, X, X + cw, Y, my, X, Y, cw, ch);
		else if (m & 0x1)
			synth_add(r, n, X, mx, Y, my, X, Y, cw, ch);
		else if (m & 0x2)
			synth_add(r, n, mx, X + cw, Y, my, X, Y, cw, ch);
		if ((m & 0xC) == 0xC)
			synth_add(r, n, X, X + cw, my, Y + ch, X, Y, cw, ch);
		else if (m & 0x4)
			synth_add(r, n, X, mx, my, Y + ch, X, Y, cw, ch);
		else if (m & 0x8)
			synth_add(r, n, mx, X + cw, my, Y + ch, X, Y, cw, ch);
	}
}

/*
 * THE THREE SHADES ARE THE ONE THING HERE THAT IS NOT A RECTANGLE.
 *
 * A quarter-tone dither at a 16x32 cell is 128 disjoint pixels, and issuing
 * those as rectangles would cost far more per cell than the glyph it replaced.
 * So the pattern is an a8 mask ONE PERIOD ACROSS — four by two cell pixels per
 * scale step — set to repeat, and the foreground is composited through it, one
 * call per cell exactly like a glyph.
 *
 * THE MASK IS READ FROM THE DESTINATION'S OWN COORDINATES, not from zero. A
 * repeating mask offset by the cell's absolute position is one continuous
 * pattern across a whole shaded area; a mask anchored at each cell's origin
 * changes phase at every boundary whose cell width is not a multiple of the
 * period, and that shows as a grid drawn over the fill.
 *
 * One tile per tone per scale — at most twelve images of a few dozen bytes —
 * and they survive a font change, because the period is the scale and nothing
 * the face decides — they live for the process.
 */
#define SHADE_W 4
#define SHADE_H 2

static pixman_image_t *shade_tile[3][KCELL_MAX_SCALE + 1];

/* pixman hands the image back to its destroy function and free() does not take
 * one; a cast between the two signatures is undefined behaviour. */
static void shade_free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

static pixman_image_t *shade_for(int tone, int scale)
{
	if (shade_tile[tone][scale])
		return shade_tile[tone][scale];

	int w = SHADE_W * scale, h = SHADE_H * scale;
	/* pixman wants a 32-bit-aligned stride whatever the format says. */
	int stride = (w + 3) & ~3;
	uint8_t *bits = calloc(1, (size_t)stride * (size_t)h);

	if (!bits)
		return NULL;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int cx = x / scale, cy = y / scale;
			/*
			 * A quarter is one pixel of each four-by-two period,
			 * staggered so no two set pixels share a column — an
			 * unstaggered quarter reads as vertical stripes. A
			 * half is the checkerboard, and three quarters is the
			 * quarter inverted, so the three tones step evenly.
			 */
			int on = tone == 1 ? ((cx + cy) & 1) == 0
					   : (cx % 4 == (cy % 2) * 2);

			if (tone == 2)
				on = !on;
			bits[y * stride + x] = on ? 0xff : 0;
		}

	pixman_image_t *t = pixman_image_create_bits(PIXMAN_a8, w, h,
						     (uint32_t *)bits, stride);
	if (!t) {
		free(bits);
		return NULL;
	}
	pixman_image_set_destroy_function(t, shade_free_bits, bits);
	pixman_image_set_repeat(t, PIXMAN_REPEAT_NORMAL);
	shade_tile[tone][scale] = t;
	return t;
}

/*
 * Whether this codepoint is drawn rather than rasterised. Asked for every
 * non-blank cell, so the whole block range answers on a range test and only
 * the box range pays the search — and the box range is sparse, because the
 * heavy, dashed and rounded characters are not in this set and must still
 * reach the face that carries them.
 */
static bool synth_has(uint32_t cp)
{
	if (cp >= 0x2580 && cp <= 0x259F)
		return true;
	return cp >= 0x2500 && cp <= 0x256C && box_lookup(cp) != NULL;
}

/* `c` and `src` are the same foreground twice: a fill takes the colour, the
 * shade tile takes an image to composite through. */
static void synth_draw(pixman_image_t *dst, uint32_t cp, int X, int Y,
		       int cw, int ch, int scale, pixman_color_t c,
		       pixman_image_t *src)
{
	pixman_rectangle16_t r[8];
	int n = 0;

	if (cp >= 0x2591 && cp <= 0x2593) {
		pixman_image_t *tile = shade_for((int)(cp - 0x2591), scale);

		if (tile && src)
			pixman_image_composite32(PIXMAN_OP_OVER, src, tile,
						 dst, 0, 0, X, Y, X, Y,
						 cw, ch);
		return;
	}

	if (cp >= 0x2580 && cp <= 0x259F) {
		block_rects(cp, X, Y, cw, ch, r, &n);
	} else {
		const struct box_shape *b = box_lookup(cp);
		int t = stroke_width(cw, ch);

		if (!b)
			return;
		for (int i = 0; i < b->n; i++)
			synth_add(r, &n,
				  seg_pos(b->seg[i][0], X, cw, t),
				  seg_pos(b->seg[i][1], X, cw, t),
				  seg_pos(b->seg[i][2], Y, ch, t),
				  seg_pos(b->seg[i][3], Y, ch, t),
				  X, Y, cw, ch);
	}

	/* One call for the whole character: a fill is a region intersect
	 * against the destination clip before a pixel moves, and ╬ is eight
	 * pieces of one colour. */
	if (n)
		pixman_image_fill_rectangles(PIXMAN_OP_OVER, dst, &c, n, r);
}

/*
 * WHETHER THE CELL `k` PLACES AFTER A SPRITE CELL IS THE SAME PICTURE'S NEXT
 * ONE ALONG, so that a row of a block can be composited in a single call.
 *
 * pixman charges most of a small composite to its SETUP — choosing a combiner,
 * building the iterators, walking the clip — and an 8x16 cell is small enough
 * that the setup is the whole cost. A full 16x16-cell block copied cell by cell
 * is 256 of those; copied a row at a time it is 16, and the identical pixels
 * land more than three times cheaper. An embedded guest publishes a screenful
 * of blocks per frame, so that difference is most of the frame budget.
 *
 * The test is deliberately narrow and everything outside it FLUSHES THE RUN
 * AND FALLS BACK TO ONE CALL PER CELL, because a run is only the same pixels
 * when the cells are the same picture, the same sprite row, and adjacent
 * columns of it — and when nothing else is drawn into them. KT_A_REVERSE is
 * the one attribute a sprite cell honours: it fills under the picture first,
 * which is how the pointer marks the cell it is over, so a reversed cell can
 * never join a run.
 */
static inline int sprite_run_next(uint32_t cp0, int k, const KtuiCell *c)
{
	uint32_t cp = c->ch;

	return KTUI_IS_SPRITE(cp) && !(c->attr & KT_A_REVERSE) &&
	       KTUI_SPRITE_SLOT(cp) == KTUI_SPRITE_SLOT(cp0) &&
	       KTUI_SPRITE_SY(cp) == KTUI_SPRITE_SY(cp0) &&
	       KTUI_SPRITE_SX(cp) == KTUI_SPRITE_SX(cp0) + (uint32_t)k;
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
 * cell on each side, which is what covers a wide glyph's continuation.
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
			 * selection or the mouse pointer crossing an
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
			if (s && s->pix) {
				/*
				 * THE RUN STOPS AT `x1` LIKE EVERY OTHER PASS
				 * IN THIS FUNCTION. Past it the row holds what
				 * the last frame left and the diff says it is
				 * still right, so a run that reached beyond
				 * would repaint cells the caller excluded.
				 *
				 * A reversed cell has already had its fill put
				 * down for this one cell only, so it takes its
				 * own call and starts no run.
				 */
				int run = 1;

				if (!(at & KT_A_REVERSE))
					while (x + run < x1 &&
					       sprite_run_next(cp, run,
							       &row[x + run]))
						run++;

				pixman_image_composite32(
					PIXMAN_OP_OVER,
					(pixman_image_t *)s->pix, NULL, dst,
					(int)KTUI_SPRITE_SX(cp) * cw,
					(int)KTUI_SPRITE_SY(cp) * ch,
					0, 0, x * cw, y, run * cw, ch);
				/*
				 * The loop's own step takes the last cell of
				 * the run; `covered` is already clear and the
				 * cells skipped would each have cleared it
				 * again, so a run leaves the same state behind
				 * as the calls it replaced.
				 */
				x += run - 1;
			}
			continue;
		}

		/*
		 * A FRAME CHARACTER NEVER REACHES THE FACE. The box-drawing
		 * and block sets are drawn from their codepoint — see the
		 * synthesis above — so the face is not asked for one, and a
		 * border is the same unbroken line whatever font is loaded.
		 * Such a character is always ONE cell wide, so the wide-glyph
		 * pairing below cannot select it.
		 */
		bool synth = synth_has(cp);

		KCellGlyph g;
		int style = ((at & KT_A_ITALIC) ? KCELL_ST_ITALIC : 0) |
			    ((at & KT_A_BOLD) ? KCELL_ST_BOLD : 0);
		bool have = !synth && kcell_glyph_face(cp, scale, style, &g);

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
		if (synth)
			synth_draw(dst, cp, x * cw, y, cw, ch, scale,
				   fg_lit ? rgb_color(fgl)
					  : to_pixman(ktui_theme->slot[fg & 7]),
				   fg_lit ? solid_for_rgb(fgl)
					  : solid_for_slot(fg));

		if (!have) {
			/* No mask to composite — a frame character just drawn,
			 * a codepoint no font carries, or one whose glyph is
			 * empty — and the rules are still the cell's. Same
			 * reason a blank cell keeps them. */
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
		 * file: `fg` is a byte a child's SGR sequence chose and the
		 * theme has eight entries, so an unmasked index reads past the
		 * table.
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
		 * would also clip the row's background fills. A fallback face
		 * answers with whatever metric it has, and a bitmap exceeding
		 * the cell would otherwise overwrite the character beside it —
		 * which the neighbour has no reason to repaint, so the damage
		 * outlives the frame that caused it.
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

/*
 * The paint itself, saying WHICH ROWS it touched: `painted` is one byte per
 * row, cleared first and set for every row this call drew, and the return is
 * how many. Internal to the library — a caller outside it sees `kcell_paint`,
 * which asks for no row list.
 */
static int kcell_paint_damage(pixman_image_t *dst, const KtuiCell *cur,
			      KtuiCell *prev, int cols, int rows, int full,
			      int scale, int dst_w, int dst_h,
			      unsigned char *painted)
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
			 * WIDENED BY A CELL EACH WAY, because a changed cell's
			 * neighbour may hold pixels this paint has to put
			 * back: a face's glyph is clipped to its own cell, but
			 * the PAIR a double-width character occupies is
			 * painted as one rectangle by its lead.
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

void kcell_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev,
		 int cols, int rows, int full, int scale, int dst_w, int dst_h)
{
	kcell_paint_damage(dst, cur, prev, cols, rows, full, scale, dst_w,
			   dst_h, NULL);
}

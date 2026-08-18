/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcell — a pixel canvas that lands in the cell grid
 *
 * THE PROBLEM THIS SOLVES. Everything this desktop draws is a character cell,
 * 16x32, one glyph. That is the identity and it is why the panel, the splash
 * and tty1 look like one machine — but it means a control is only ever as tall
 * as one row of text. The Start button is the case that shows it: on the
 * two-row bar it is a 32x64 slab of accent with its icon and its word sitting
 * in the top half, because there is no way to say "this word is forty pixels
 * tall and centred in the whole button". It reads as a mistake, and it is not
 * one — it is the grid being honest about what a cell is.
 *
 * THE ANSWER IS NOT A SECOND RENDERER. libktui already has a mechanism for a
 * picture that occupies WHOLE CELLS: a sprite. Its slot and sub-cell
 * coordinate ride inside the cell's codepoint, so the ordinary row diff is
 * already its damage mechanism, `ktui_sprite_put` already accepts up to 16x16
 * CELLS, and a text backend already renders the fallback codepoint instead.
 * Nothing about that needed changing. What was missing was a way to RASTERISE
 * something other than an icon file into one.
 *
 * So: a canvas is a pixman image exactly N x M cells at the current output
 * scale, with fills, lines and TEXT AT AN ARBITRARY PIXEL SIZE drawn into it,
 * handed to `ktui_sprite_put` when it is finished. The grid still owns the
 * layout, the damage, the fallback and the tty; the canvas owns the pixels
 * inside one rectangle of it. A caller gets pixel freedom without the toolkit
 * gaining a second drawing model, and a consumer that cannot draw pixels is
 * unaffected because it never asked for a canvas.
 *
 * WHAT A CALLER MUST STILL DO. A sprite's cells encode the SLOT, not the
 * picture, so redrawing a canvas in place changes nothing the row diff can
 * see. An animated tile must therefore publish under a key that changes when
 * its content does — see the shell's tile.c, which double-buffers two slots
 * and alternates, so exactly the rows the tile covers repaint and nothing
 * else does.
 *
 * FONTS AT A SIZE. kcell_font.c loads ONE font at the cell's pixel size; a
 * canvas wants several, so this keeps its own small cache keyed by pixel size.
 * A bitmap font (Terminus is one) answers a request for a size it does not
 * carry with the nearest it has, which is fcft's business and not ours — the
 * text simply comes out at the size that exists, which is the honest result
 * and is why nothing here tries to scale a glyph by hand.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcft/fcft.h>

#include "kcell.h"

/* ── fonts, by pixel size ──────────────────────────────────────────────── */

/*
 * Six is not a budget, it is an observation: the chrome asks for a title
 * size, a body size and a caption size, and a seventh distinct size on one
 * screen would be a design that had stopped choosing. The table never evicts
 * — a font that has been asked for once will be asked for again every frame.
 */
#define CV_MAX_FONTS 6

static struct {
	int px;
	struct fcft_font *font;
} cv_fonts[CV_MAX_FONTS];
static int cv_nfonts;
static char cv_name[192];

void kcell_canvas_font(const char *name)
{
	/* A change of family drops every size: they were all resolved from
	 * the old one. */
	if (name && !strcmp(name, cv_name))
		return;
	for (int i = 0; i < cv_nfonts; i++)
		if (cv_fonts[i].font)
			fcft_destroy(cv_fonts[i].font);
	memset(cv_fonts, 0, sizeof(cv_fonts));
	cv_nfonts = 0;
	snprintf(cv_name, sizeof(cv_name), "%s", name ? name : "");
}

static struct fcft_font *cv_font(int px)
{
	char spec[256];
	const char *names[1];

	if (px < 4)
		px = 4;
	if (px > 400)
		px = 400;
	for (int i = 0; i < cv_nfonts; i++)
		if (cv_fonts[i].px == px)
			return cv_fonts[i].font;	/* NULL is cached too */
	if (cv_nfonts >= CV_MAX_FONTS)
		return NULL;

	/*
	 * The family the chrome was told to use, at THIS pixel size — and the
	 * size it already carried has to be REMOVED, not overridden.
	 *
	 * `chrome_font` is `Terminus:pixelsize=32`, so the obvious
	 * `"%s:pixelsize=%d"` produces `Terminus:pixelsize=32:pixelsize=39`,
	 * and fontconfig APPENDS a repeated property rather than replacing it:
	 * `fc-match -v` on that pattern reports `pixelsize: 32 64` and derives
	 * `size` from the FIRST. Every canvas would have come out at the cell's
	 * own size — silently, since the text still renders — which is the
	 * whole feature quietly not working. Measured before it was written.
	 */
	char base[192];
	size_t bl = 0;
	const char *src = cv_name[0] ? cv_name : "monospace";
	for (const char *seg = src; *seg;) {
		const char *end = strchr(seg, ':');
		size_t n = end ? (size_t)(end - seg) : strlen(seg);
		int is_size = (n > 10 && !strncmp(seg, "pixelsize=", 10)) ||
			      (n > 5 && !strncmp(seg, "size=", 5));
		if (!is_size && n && bl + n + 2 < sizeof(base)) {
			if (bl)
				base[bl++] = ':';
			memcpy(base + bl, seg, n);
			bl += n;
		}
		seg = end ? end + 1 : seg + n;
	}
	base[bl] = '\0';
	snprintf(spec, sizeof(spec), "%s:pixelsize=%d",
		 bl ? base : "monospace", px);
	names[0] = spec;

	struct fcft_font *f = fcft_from_name(1, names, NULL);

	/*
	 * A BITMAP FONT CANNOT BE ASKED FOR AN ARBITRARY SIZE, and the chrome
	 * ships one: `chrome_font` is `Terminus:pixelsize=32`, Terminus is a
	 * PCF, and a request for 39 comes back as the nearest strike it
	 * carries — silently, because the text still renders. The whole point
	 * of a canvas is a control taller than a row of text, so a size that
	 * quietly clamps to the cell's own is the feature not working.
	 *
	 * So it is MEASURED, not assumed: if what came back is much shorter
	 * than what was asked for, retry the same pattern restricted to
	 * scalable faces and keep whichever is closer. A machine with no
	 * scalable font at all keeps the bitmap, which is the honest result
	 * and is what a minimal install has.
	 */
	if (f && f->height < px * 3 / 4) {
		char sspec[288];
		const char *snames[1];
		snprintf(sspec, sizeof(sspec), "%s:scalable=true", spec);
		snames[0] = sspec;
		struct fcft_font *sf = fcft_from_name(1, snames, NULL);
		if (sf) {
			int db = px - f->height, ds = px - sf->height;
			if (db < 0)
				db = -db;
			if (ds < 0)
				ds = -ds;
			if (ds < db) {
				fcft_destroy(f);
				f = sf;
			} else {
				fcft_destroy(sf);
			}
		}
	}

	cv_fonts[cv_nfonts].px = px;
	cv_fonts[cv_nfonts].font = f;
	return cv_fonts[cv_nfonts++].font;
}

/* ── the canvas ────────────────────────────────────────────────────────── */

struct KCellCanvas {
	pixman_image_t *img;
	uint32_t *px;
	int w, h;		/* pixels */
	int cw, ch;		/* cells */
};

KCellCanvas *kcell_canvas_new(int cells_w, int cells_h, int cell_w, int cell_h,
			      int scale)
{
	KCellCanvas *c;

	if (cells_w < 1 || cells_h < 1 || cells_w > 16 || cells_h > 16)
		return NULL;
	if (cell_w < 1 || cell_h < 1 || scale < 1)
		return NULL;

	c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;
	c->cw = cells_w;
	c->ch = cells_h;
	c->w = cells_w * cell_w * scale;
	c->h = cells_h * cell_h * scale;
	c->px = calloc((size_t)c->w * c->h, 4);
	if (!c->px) {
		free(c);
		return NULL;
	}
	c->img = pixman_image_create_bits(PIXMAN_a8r8g8b8, c->w, c->h, c->px,
					  c->w * 4);
	if (!c->img) {
		free(c->px);
		free(c);
		return NULL;
	}
	return c;
}

void kcell_canvas_free(KCellCanvas *c)
{
	if (!c)
		return;
	if (c->img)
		pixman_image_unref(c->img);
	free(c->px);
	free(c);
}

pixman_image_t *kcell_canvas_image(KCellCanvas *c) { return c ? c->img : NULL; }
int kcell_canvas_w(const KCellCanvas *c) { return c ? c->w : 0; }
int kcell_canvas_h(const KCellCanvas *c) { return c ? c->h : 0; }

void kcell_canvas_clear(KCellCanvas *c)
{
	if (c)
		memset(c->px, 0, (size_t)c->w * c->h * 4);
}

/*
 * SRC, not OVER, and deliberately: a canvas is cleared to transparent and the
 * caller paints its own background, so a fill has to be able to REPLACE what
 * is under it — including putting transparency back. An OVER fill could only
 * ever add.
 */
void kcell_canvas_fill(KCellCanvas *c, int x, int y, int w, int h, int slot,
		       int alpha)
{
	pixman_color_t col;

	if (!c || w <= 0 || h <= 0)
		return;
	col = kcell_slot_color(slot);
	if (alpha < 255) {
		if (alpha < 0)
			alpha = 0;
		col.red = (uint16_t)(col.red * alpha / 255);
		col.green = (uint16_t)(col.green * alpha / 255);
		col.blue = (uint16_t)(col.blue * alpha / 255);
		col.alpha = (uint16_t)(0xffff * alpha / 255);
	}
	pixman_image_fill_rectangles(PIXMAN_OP_SRC, c->img, &col, 1,
				     &(pixman_rectangle16_t){
					     (int16_t)x, (int16_t)y,
					     (uint16_t)w, (uint16_t)h });
}

int kcell_canvas_text_ascent(int px)
{
	struct fcft_font *f = cv_font(px);

	return f ? f->ascent : px;
}

int kcell_canvas_text_height(int px)
{
	struct fcft_font *f = cv_font(px);

	return f ? f->height : px;
}

/* One codepoint at a time, advancing by the glyph's own advance — the string
 * is chrome, not a paragraph, and shaping it would mean harfbuzz. */
static int cv_walk(struct fcft_font *f, const char *s, KCellCanvas *c, int x,
		   int baseline, pixman_image_t *fill)
{
	int adv = 0;

	for (const unsigned char *p = (const unsigned char *)s; *p;) {
		uint32_t cp = *p;
		int len = 1;

		if (cp >= 0xf0) { cp &= 0x07; len = 4; }
		else if (cp >= 0xe0) { cp &= 0x0f; len = 3; }
		else if (cp >= 0xc0) { cp &= 0x1f; len = 2; }
		for (int i = 1; i < len; i++) {
			if ((p[i] & 0xc0) != 0x80) { len = 1; cp = *p; break; }
			cp = (cp << 6) | (p[i] & 0x3f);
		}
		p += len;

		const struct fcft_glyph *g =
			fcft_rasterize_char_utf32(f, cp, FCFT_SUBPIXEL_NONE);
		if (!g)
			continue;
		if (c && fill && g->pix)
			/* The glyph's coverage is the MASK and the colour is
			 * a solid source — the same shape kcell_paint uses,
			 * so canvas text and cell text are the same pixels at
			 * the same size. */
			pixman_image_composite32(PIXMAN_OP_OVER, fill, g->pix,
						 c->img, 0, 0, 0, 0,
						 x + adv + g->x,
						 baseline - g->y,
						 g->width, g->height);
		adv += g->advance.x;
	}
	return adv;
}

int kcell_canvas_text_width(int px, const char *utf8)
{
	struct fcft_font *f = cv_font(px);

	if (!f || !utf8)
		return 0;
	return cv_walk(f, utf8, NULL, 0, 0, NULL);
}

int kcell_canvas_text(KCellCanvas *c, int x, int baseline, int px,
		      const char *utf8, int slot)
{
	struct fcft_font *f = cv_font(px);
	pixman_color_t col;
	pixman_image_t *fill;
	int adv;

	if (!c || !f || !utf8)
		return 0;
	col = kcell_slot_color(slot);
	fill = pixman_image_create_solid_fill(&col);
	if (!fill)
		return 0;
	adv = cv_walk(f, utf8, c, x, baseline, fill);
	pixman_image_unref(fill);
	return adv;
}

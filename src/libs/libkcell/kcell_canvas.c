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
 * already its damage mechanism, and a text backend already renders the
 * fallback codepoint instead. Nothing about that needed changing. What was
 * missing was a way to RASTERISE something other than an icon file into one.
 *
 * So: a canvas is a pixman image exactly N x M cells at the current output
 * scale, with fills, lines and TEXT AT AN ARBITRARY PIXEL SIZE drawn into it,
 * handed to the sprite table when it is finished — as one sprite when it is
 * at most 16x16 cells, the most a sprite cell's four-bit sub-cell coordinate
 * can address, and as a grid of VIEWS (kcell_canvas_view) when it is larger,
 * each one a sprite over its own piece of the same pixels. The grid still owns the
 * layout, the damage, the fallback and the tty; the canvas owns the pixels
 * inside one rectangle of it. A caller gets pixel freedom without the toolkit
 * gaining a second drawing model, and a consumer that cannot draw pixels is
 * unaffected because it never asked for a canvas.
 *
 * WHAT A CALLER MUST STILL DO. A sprite's cells encode the SLOT, not the
 * picture, so redrawing a canvas in place changes nothing the row diff can
 * see. An animated tile must therefore publish under a key that changes when
 * its content does — see libkchrome's kch_tile.c, which double-buffers two slots
 * and alternates, so exactly the rows the tile covers repaint and nothing
 * else does.
 *
 * FONTS AT A SIZE. kcell_font.c loads ONE font at the cell's pixel size; a
 * canvas wants several, so this keeps its own small cache keyed by pixel size,
 * resolved by the size policy in cv_font(). The family is the one
 * kcell_canvas_font() named, and the cell font's own when nothing did — the
 * chrome's face at every size, never fontconfig's `monospace`.
 *
 * AND THE SAME TEXT DRAWS INTO ANY IMAGE. kcell_text_draw() is the walk a
 * canvas's text takes, pointed at a destination the caller owns and clipped
 * to a rectangle by coordinates alone — libkchrome's display text is drawn by
 * it straight into a backdrop, under the cell grid, with no canvas and no
 * sprite in between.
 *
 * AND FCFT IS BROUGHT UP HERE WHEN NOTHING ELSE HAS. A consumer that draws
 * canvases and no cells never loads a cell font, and an uninitialised fcft
 * answers every request with NULL — so the text silently measures zero and
 * draws nothing. The canvas therefore takes a REFERENCE on libkcell's one
 * fcft owner, which lives in kcell_font.c, and holds it for as long as it has
 * a face cached. Either entry point may be used first and in either order,
 * and neither tears the library down under the other; see cv_fcft_ready().
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcft/fcft.h>

#include "kcell.h"
#include "kcell_priv.h"

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
/* The family every face in cv_fonts[] was resolved from. */
static char cv_family[224];

/*
 * FCFT HAS TO BE UP BEFORE A FACE CAN BE ASKED FOR, AND THE CANVAS CANNOT
 * ASSUME SOMEONE ELSE DID IT.
 *
 * `fcft_from_name()` answers NULL with `fcft_init() not called` on the log
 * when FreeType's handle is NULL, and a consumer that draws only canvases
 * never loads a CELL font. The failure is silent twice over: every measurement then
 * answers zero and every string draws nothing, which reads as a layout that
 * chose to leave the text out.
 *
 * SO THE CANVAS TAKES ONE REFERENCE OF ITS OWN and holds it for exactly as
 * long as cv_fonts[] names a face. kcell_font.c owns fcft's lifetime for the
 * whole library and counts its holders — read the rule there. The count is
 * what frees the two entry points from each other: a cell font loaded after a
 * canvas has drawn finds the library already standing and leaves it alone,
 * and a kcell_font_free() with a canvas still drawing counts down to this
 * reference and not to zero. Take the reference away and that free destroys
 * FreeType under every face in cv_fonts[], and the next frame composites out
 * of freed glyph images.
 *
 * -1 is a machine with no usable FreeType, latched so a bar drawing sixty
 * times a second does not retry the whole library init on every frame.
 */
static int cv_fcft;		/* 0 none held, 1 held, -1 refused */

static int cv_fcft_ready(void)
{
	if (cv_fcft > 0)
		return 1;
	if (cv_fcft < 0)
		return 0;
	if (!kcell_fcft_ref()) {
		cv_fcft = -1;
		return 0;
	}
	cv_fcft = 1;
	return 1;
}

/*
 * Every size, and the fcft reference with them — it is held for the faces
 * and nothing else, so a process that never draws another canvas leaves the
 * library free to shut down — and the next cv_font() takes a fresh one.
 */
static void cv_drop(void)
{
	for (int i = 0; i < cv_nfonts; i++)
		if (cv_fonts[i].font)
			fcft_destroy(cv_fonts[i].font);
	memset(cv_fonts, 0, sizeof(cv_fonts));
	cv_nfonts = 0;
	cv_family[0] = '\0';
	if (cv_fcft > 0) {
		kcell_fcft_unref();
		cv_fcft = 0;
	}
}

void kcell_canvas_font(const char *name)
{
	/* A change of family drops every size: they were all resolved from
	 * the old one.
	 *
	 * WHICH IS WHY THE SAME NAME AGAIN MUST DO NOTHING, NULL INCLUDED: a
	 * caller that re-states the family per frame would otherwise tear fcft
	 * down and stand it back up on every one of them whenever no cell font
	 * is loaded beside it. */
	if (!strcmp(name ? name : "", cv_name))
		return;
	cv_drop();
	snprintf(cv_name, sizeof(cv_name), "%s", name ? name : "");
}

/*
 * The family text is drawn in: the one named, else the cell font's — so a
 * surface that never names one draws its display text in the face its cells
 * are in — else `monospace`. A cell font loaded or replaced after a size was
 * cached changes the answer, and every size resolved from the old one goes.
 */
static const char *cv_family_now(void)
{
	const char *fam = cv_name[0] ? cv_name : kcell_font_name();

	if (!fam || !*fam)
		fam = "monospace";
	if (strcmp(fam, cv_family)) {
		cv_drop();
		snprintf(cv_family, sizeof(cv_family), "%s", fam);
	}
	return cv_family;
}

static struct fcft_font *cv_font(int px)
{
	char spec[256];
	const char *names[1];
	const char *src = cv_family_now();

	if (!cv_fcft_ready())
		return NULL;
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
	 * size it already carried has to be REMOVED, not overridden, or every
	 * canvas comes out at the cell's own size: kcell_name_at_px().
	 */
	if (!kcell_name_at_px(src, px, spec, sizeof(spec)))
		return NULL;
	names[0] = spec;

	struct fcft_font *f = fcft_from_name(1, names, NULL);

	/*
	 * A BITMAP FONT CANNOT BE ASKED FOR AN ARBITRARY SIZE, and the chrome
	 * ships one: `chrome_font` is `Terminus:pixelsize=32`, Terminus is a
	 * PCF with strikes up to 32, and fontconfig answers any other size
	 * from the nearest strike — scaled by a fraction between strikes (39
	 * is the 32 strike drawn 1.22 times, its strokes uneven), and not
	 * scaled at all within a fifth of one (36 comes back 32 tall). Either
	 * way the text is not the size asked for, and silently, because it
	 * still renders. The whole point of a canvas is a control taller than
	 * a row of text, so a size that quietly comes back smaller or smeared
	 * is the feature not working.
	 *
	 * So it is MEASURED, not assumed, by the rule the cell faces use (see
	 * take_twin() in kcell_font.c): a bitmap that fontconfig cannot draw
	 * exactly at this size — short of it, or scaled by a fraction — is
	 * replaced by the family's scalable twin, `Terminus (TTF)`, kept only
	 * when fcft's name for it really is that family, since fontconfig
	 * substitutes a sans for a twin that is not installed. A face still
	 * more than a tenth short is then retried restricted to scalable faces,
	 * whichever of the two is closer; that one lands on Noto Sans for
	 * Terminus, a different typeface, which is why the twin is asked
	 * first. A machine with no scalable font at all keeps the bitmap, which
	 * is the honest result and is what a minimal install has.
	 */
	if (f && kcell_bitmap_off_strike(spec, px)) {
		char tspec[288];
		const char *tnames[1] = { tspec };

		if (kcell_twin_name(spec, tspec, sizeof(tspec))) {
			struct fcft_font *tf = fcft_from_name(1, tnames, NULL);

			if (tf && kcell_face_named(tf->name, tspec)) {
				fcft_destroy(f);
				f = tf;
			} else if (tf) {
				fcft_destroy(tf);
			}
		}
	}
	if (f && kcell_face_short(f->height, px)) {
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

/*
 * Eight clips deep: a chart clips its plot and then each band inside it, and
 * a nesting past a handful is a layout that should have been two canvases.
 */
#define CV_CLIP_MAX 8

struct KCellCanvas {
	pixman_image_t *img;
	uint32_t *px;
	int w, h;		/* pixels */
	int cw, ch;		/* cells */
	int nclip;		/* clip[nclip - 1] is in force; none is 0 */
	struct { int x0, y0, x1, y1; } clip[CV_CLIP_MAX];
};

/*
 * THE CANVAS IS BOUNDED IN PIXELS, NOT IN CELLS. A fill is a pixman 16-bit
 * rectangle, so a canvas wider or taller than 32767 pixels would have fills
 * that wrap to negative coordinates and land somewhere else. The cell count is
 * the caller's business: a canvas larger than one sprite reaches the table as
 * several views of itself.
 */
#define CV_MAX_PX 32767

KCellCanvas *kcell_canvas_new(int cells_w, int cells_h, int cell_w, int cell_h,
			      int scale)
{
	KCellCanvas *c;

	if (cells_w < 1 || cells_h < 1 || cell_w < 1 || cell_h < 1 || scale < 1)
		return NULL;
	if ((long long)cells_w * cell_w * scale > CV_MAX_PX ||
	    (long long)cells_h * cell_h * scale > CV_MAX_PX)
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

/*
 * A WINDOW ONTO THE CANVAS, NOT A COPY. The view's first pixel is the canvas's
 * pixel at the cell origin and its stride is the canvas's, so a raster into
 * the canvas is already in every view and publishing a large canvas costs no
 * pixel work at all.
 *
 * THE VIEW OWNS NOTHING. It has no destroy function and the bits are the
 * canvas's, so every reference to it — the caller's and any the sprite table
 * holds — must be gone before kcell_canvas_free(): a view that outlives its
 * canvas composites out of freed memory.
 */
pixman_image_t *kcell_canvas_view(KCellCanvas *c, int cell_x, int cell_y,
				  int cells_w, int cells_h)
{
	int pcw, pch;

	if (!c || cell_x < 0 || cell_y < 0 || cells_w < 1 || cells_h < 1 ||
	    cell_x + cells_w > c->cw || cell_y + cells_h > c->ch)
		return NULL;
	pcw = c->w / c->cw;
	pch = c->h / c->ch;
	return pixman_image_create_bits(PIXMAN_a8r8g8b8, cells_w * pcw,
					cells_h * pch,
					c->px + (size_t)cell_y * pch * c->w +
						(size_t)cell_x * pcw,
					c->w * 4);
}
int kcell_canvas_w(const KCellCanvas *c) { return c ? c->w : 0; }
int kcell_canvas_h(const KCellCanvas *c) { return c ? c->h : 0; }

/*
 * The whole canvas, and the clip stack with it: a canvas is cleared at the
 * start of a frame, and a clip left pushed by the last one would narrow this
 * one's fills to a rectangle nobody asked for.
 */
void kcell_canvas_clear(KCellCanvas *c)
{
	if (!c)
		return;
	memset(c->px, 0, (size_t)c->w * c->h * 4);
	if (c->nclip) {
		c->nclip = 0;
		pixman_image_set_clip_region32(c->img, NULL);
	}
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

/*
 * One codepoint at a time, advancing by the glyph's own advance — the string
 * is chrome, not a paragraph, and shaping it would mean harfbuzz.
 *
 * `clip`, when given, is x0, y0, x1, y1 in `dst`'s pixels, and each glyph is
 * cut to it BY ITS COORDINATES: the mask's origin moves in by what is cut off.
 * A clip region on `dst` would do the same and must not be used — a backdrop
 * is drawn into an image whose clip belongs to its caller (a partial repaint
 * under a clip to what changed), and setting one here would replace it.
 */
static int cv_walk(struct fcft_font *f, const char *s, pixman_image_t *dst,
		   int x, int baseline, pixman_image_t *fill, const int *clip)
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
		if (dst && fill && g->pix) {
			int dx = x + adv + g->x, dy = baseline - g->y;
			int gw = g->width, gh = g->height, mx = 0, my = 0;

			if (clip) {
				if (dx < clip[0]) {
					mx = clip[0] - dx;
					gw -= mx;
					dx = clip[0];
				}
				if (dy < clip[1]) {
					my = clip[1] - dy;
					gh -= my;
					dy = clip[1];
				}
				if (dx + gw > clip[2])
					gw = clip[2] - dx;
				if (dy + gh > clip[3])
					gh = clip[3] - dy;
			}
			/* The glyph's coverage is the MASK and the colour is
			 * a solid source — the same shape kcell_paint uses,
			 * so canvas text and cell text are the same pixels at
			 * the same size. */
			if (gw > 0 && gh > 0)
				pixman_image_composite32(PIXMAN_OP_OVER, fill,
							 g->pix, dst, 0, 0, mx,
							 my, dx, dy, gw, gh);
		}
		adv += g->advance.x;
	}
	return adv;
}

int kcell_canvas_text_width(int px, const char *utf8)
{
	struct fcft_font *f = cv_font(px);

	if (!f || !utf8)
		return 0;
	return cv_walk(f, utf8, NULL, 0, 0, NULL, NULL);
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
	/* Unclipped here: the canvas's own clip stack is pixman's region on
	 * its image, which is what narrows this. */
	adv = cv_walk(f, utf8, c->img, x, baseline, fill, NULL);
	pixman_image_unref(fill);
	return adv;
}

int kcell_text_draw(pixman_image_t *dst, int x, int baseline, int px,
		    const char *utf8, uint32_t rgb, int cx, int cy, int cw,
		    int ch)
{
	struct fcft_font *f = cv_font(px);
	pixman_color_t col = {
		.red = (uint16_t)(((rgb >> 16) & 0xff) * 257),
		.green = (uint16_t)(((rgb >> 8) & 0xff) * 257),
		.blue = (uint16_t)((rgb & 0xff) * 257),
		.alpha = 0xffff,
	};
	int clip[4] = { cx, cy, cx + cw, cy + ch };
	pixman_image_t *fill;
	int adv;

	if (!dst || !f || !utf8 || cw <= 0 || ch <= 0)
		return 0;
	fill = pixman_image_create_solid_fill(&col);
	if (!fill)
		return 0;
	adv = cv_walk(f, utf8, dst, x, baseline, fill, clip);
	pixman_image_unref(fill);
	return adv;
}

/* ── the clip ──────────────────────────────────────────────────────────── */

/* The rectangle in force, as pixel edges: the whole canvas when none is. */
static void cv_bounds(const KCellCanvas *c, int *x0, int *y0, int *x1,
		      int *y1)
{
	if (c->nclip) {
		*x0 = c->clip[c->nclip - 1].x0;
		*y0 = c->clip[c->nclip - 1].y0;
		*x1 = c->clip[c->nclip - 1].x1;
		*y1 = c->clip[c->nclip - 1].y1;
	} else {
		*x0 = 0;
		*y0 = 0;
		*x1 = c->w;
		*y1 = c->h;
	}
}

/*
 * The clip is held twice, and both halves are load-bearing: as pixman's clip
 * region on the canvas image, which is what narrows kcell_canvas_fill() and
 * kcell_canvas_text() — they go through pixman — and as edges here, which is
 * what narrows the data marks, because those write the canvas's pixels
 * directly and pixman never sees them. A clip only one of the two knew about
 * would let half the calls on a canvas paint outside it.
 */
static void cv_clip_apply(KCellCanvas *c)
{
	pixman_region32_t rg;
	int x0, y0, x1, y1;

	if (!c->nclip) {
		pixman_image_set_clip_region32(c->img, NULL);
		return;
	}
	cv_bounds(c, &x0, &y0, &x1, &y1);
	pixman_region32_init_rect(&rg, x0, y0,
				  (unsigned)(x1 > x0 ? x1 - x0 : 0),
				  (unsigned)(y1 > y0 ? y1 - y0 : 0));
	pixman_image_set_clip_region32(c->img, &rg);
	pixman_region32_fini(&rg);
}

int kcell_canvas_clip_push(KCellCanvas *c, int x, int y, int w, int h)
{
	int x0, y0, x1, y1;

	if (!c || c->nclip >= CV_CLIP_MAX)
		return -1;
	cv_bounds(c, &x0, &y0, &x1, &y1);
	if (w < 0)
		w = 0;
	if (h < 0)
		h = 0;
	/* Intersected, never replaced: a band clipped inside a plot must not
	 * be able to widen itself back out past the plot. */
	if (x > x0)
		x0 = x;
	if (y > y0)
		y0 = y;
	if ((long long)x + w < x1)
		x1 = x + w;
	if ((long long)y + h < y1)
		y1 = y + h;
	if (x1 < x0)
		x1 = x0;
	if (y1 < y0)
		y1 = y0;
	c->clip[c->nclip].x0 = x0;
	c->clip[c->nclip].y0 = y0;
	c->clip[c->nclip].x1 = x1;
	c->clip[c->nclip].y1 = y1;
	c->nclip++;
	cv_clip_apply(c);
	return 0;
}

void kcell_canvas_clip_pop(KCellCanvas *c)
{
	if (!c || !c->nclip)
		return;
	c->nclip--;
	cv_clip_apply(c);
}

/* ── the data marks ────────────────────────────────────────────────────── */

/*
 * FIXED POINT, 1/256 OF A PIXEL, and that is what makes a chart a checkable
 * picture. The values arrive as doubles and each is converted ONCE, by one
 * multiply and one divide — operations IEEE rounds exactly the same way on
 * every machine. Everything after that is integer arithmetic, so the pixels a
 * given series produces are the same bytes on every build and every
 * architecture, and a digest of them can be written into a test. Floating
 * point through the whole raster would let a compiler's contraction of a
 * multiply-add move a pixel's coverage by one step on one target and not on
 * another.
 *
 * And no libm: this library does not link it (see kcell_px.c), so the one
 * square root the line needs is an integer one.
 */
#define FX 256
#define FX2 ((int64_t)FX * FX)

static int64_t fx_of(double v)
{
	double f = v * FX;

	if (!(f == f))
		return 0;			/* NaN */
	if (f > 1e15)
		f = 1e15;
	if (f < -1e15)
		f = -1e15;
	return (int64_t)(f < 0 ? f - 0.5 : f + 0.5);
}

/* Floor division by a positive divisor — C's `/` truncates toward zero, and
 * a coordinate left of the canvas would round into its first column. */
static int64_t fx_floor(int64_t a, int64_t b)
{
	int64_t q = a / b;

	return (a % b != 0 && a < 0) ? q - 1 : q;
}

static int64_t fx_ceil(int64_t a, int64_t b)
{
	return -fx_floor(-a, b);
}

/* How much of [a0, a1) lies inside [b0, b1). */
static int64_t fx_overlap(int64_t a0, int64_t a1, int64_t b0, int64_t b1)
{
	int64_t lo = a0 > b0 ? a0 : b0, hi = a1 < b1 ? a1 : b1;

	return hi > lo ? hi - lo : 0;
}

static int64_t isqrt64(int64_t v)
{
	int64_t x, y;

	if (v <= 0)
		return 0;
	x = v;
	y = (x + 1) / 2;
	while (y < x) {
		x = y;
		y = (x + v / x) / 2;
	}
	return x;
}

/*
 * x / 255, rounded, for x up to 255 * 255 — exactly, and with no divide:
 * this runs for every pixel under a chart's area, where a real division per
 * channel was most of the raster's cost.
 */
static uint32_t div255(uint32_t x)
{
	x += 128;
	return (x + (x >> 8)) >> 8;
}

/*
 * A run of `n` pixels `stride` apart, the slot's colour at `a` OVER what is
 * there, premultiplied — the canvas is PIXMAN_a8r8g8b8, whose channels are
 * already scaled by their alpha. The palette slot is opaque by definition, so
 * the source's channels are its colour times `a`, worked out once for the
 * run: the rows under a chart's area are long runs at one alpha.
 */
static void cv_over(uint32_t *p, size_t stride, int n, KRgb col, int64_t a)
{
	uint32_t sr, sg, sb, sa, ia;

	if (a <= 0 || n <= 0)
		return;
	if (a > 255)
		a = 255;
	ia = 255 - (uint32_t)a;
	sa = 255u * (uint32_t)a;
	sr = col.r * (uint32_t)a;
	sg = col.g * (uint32_t)a;
	sb = col.b * (uint32_t)a;
	for (int i = 0; i < n; i++, p += stride) {
		uint32_t d = *p;

		*p = (div255(sa + (d >> 24) * ia) << 24) |
		     (div255(sr + ((d >> 16) & 255) * ia) << 16) |
		     (div255(sg + ((d >> 8) & 255) * ia) << 8) |
		     div255(sb + (d & 255) * ia);
	}
}

/* `alpha` of a pixel covered `cov` of the way down and `hc` of the way
 * across, both in 1/256ths. */
static int64_t fx_alpha(int alpha, int64_t cov, int64_t hc)
{
	return ((int64_t)alpha * cov * hc + FX2 / 2) / FX2;
}

/*
 * The trace's height at local x `X`, linear between sample centres and held
 * flat past the newest and the oldest. `hv` holds the `m` newest heights and
 * `cn` is the newest one's centre.
 */
static int64_t series_at(const int64_t *hv, int m, int64_t cn, int64_t step,
			 int64_t X)
{
	int64_t d = cn - X, j, r, a, b;
	int i;

	if (d <= 0)
		return hv[m - 1];
	j = d / step;
	r = d % step;
	if (j >= m - 1)
		return hv[0];
	i = m - 1 - (int)j;
	a = hv[i];
	b = hv[i - 1];
	return a + (b - a) * r / step;
}

/*
 * ONE SERIES, ONE PASS, ONE COLUMN AT A TIME — the analytic column method.
 *
 * An x-monotone series needs no polygon rasteriser: each pixel column holds
 * one stretch of the trace, so its coverage is an overlap of intervals and
 * nothing else. The area is the column's run from the base to the trace at
 * the column's centre, with the row the trace ends in covered by the
 * fraction of it that is under the trace. The line is the column's run from
 * the trace at its left edge to the trace at its right edge, widened by half
 * the line width each way, with the rows at either end covered by the
 * fraction of them it reaches. Every pixel is touched at most twice — once
 * for the area, once for the line — and no pixman call is made in the loop.
 *
 * WHAT IT COSTS, measured in the development container on a canvas cleared
 * first: about 27 us at 160x64 — a panel band, less than two hard-edged
 * pixman fills per column take — and 0.8-1.0 ms at 1120x320, a page-wide
 * chart, where those fills take 0.4-0.6 ms. The difference is the blend:
 * every pixel under the area is read and mixed, where a fill only stores.
 * A chart is rasterised once a sample, not once a frame, so a millisecond
 * a second is the price of a page-wide antialiased chart.
 *
 * A column cut by the start of the data (a series shorter than the band) is
 * covered by the fraction of it the data reaches, so the left end of a
 * filling chart is as smooth as its top.
 */
void kcell_canvas_series(KCellCanvas *c, int x, int y, int w, int h,
			 const KCellSeries *s, int slot)
{
	int bx0, by0, bx1, by1;
	int64_t W, H, step, half_step, lw, xs, cn;
	int n, k0, mirror;
	int64_t *hv;
	double vmax;
	KRgb col;

	if (!c || !s || !s->v || s->n < 1 || w < 1 || h < 1 ||
	    !(s->mode & KCELL_SERIES_BOTH))
		return;
	cv_bounds(c, &bx0, &by0, &bx1, &by1);
	if (x > bx0)
		bx0 = x;
	if (y > by0)
		by0 = y;
	if ((long long)x + w < bx1)
		bx1 = x + w;
	if ((long long)y + h < by1)
		by1 = y + h;
	if (bx1 <= bx0 || by1 <= by0)
		return;

	col = ktui_theme->slot[slot & 7];
	n = s->n;
	vmax = s->vmax > 0.0 ? s->vmax : 1.0;
	W = (int64_t)w * FX;
	H = (int64_t)h * FX;
	step = s->step > 0.0 ? fx_of(s->step) : FX;
	if (step < 1)
		step = 1;
	half_step = step / 2;
	lw = s->line_w > 0.0 ? fx_of(s->line_w) : FX;
	if (lw < 1)
		lw = 1;
	if (lw > H)
		lw = H;
	mirror = !!(s->mode & KCELL_SERIES_MIRROR);

	/* The newest sample's centre, and the oldest sample any column can
	 * reach — the one left of the left edge, which the first column
	 * interpolates towards. Samples further left are never looked at, so
	 * a caller may hand in a whole ring for a narrow band. */
	cn = W - half_step;
	k0 = n - 2 - (int)(cn / step);
	if (k0 < 0)
		k0 = 0;
	xs = (s->mode & KCELL_SERIES_HOLD) ? 0 : W - (int64_t)n * step;
	if (xs < 0)
		xs = 0;

	hv = malloc((size_t)(n - k0) * sizeof(*hv));
	if (!hv)
		return;
	for (int k = k0; k < n; k++) {
		double v = s->v[k];
		double f = (v == v && v > 0.0) ? v / vmax : 0.0;
		int64_t hk;

		if (f > 1.0)
			f = 1.0;
		hk = fx_of(f * (double)h);
		/* A sample above zero is at least a pixel: a dribble drawn at
		 * its true height is indistinguishable from silence. */
		if (v == v && v > 0.0 && hk < FX)
			hk = FX < H ? FX : H;
		hv[k - k0] = hk;
	}

	for (int px = bx0; px < bx1; px++) {
		int64_t xa = (int64_t)(px - x) * FX, xb = xa + FX;
		int64_t ha, hb, hm, hc;

		if (xa < xs)
			xa = xs;
		if (xb <= xa)
			continue;
		hc = xb - xa;
		ha = series_at(hv, n - k0, cn, step, xa);
		hb = series_at(hv, n - k0, cn, step, xb);
		hm = series_at(hv, n - k0, cn, step, (xa + xb) / 2);

		if ((s->mode & KCELL_SERIES_AREA) && s->area_alpha > 0 &&
		    hm > 0) {
			int64_t lo = mirror ? 0 : H - hm, hi = mirror ? hm : H;
			int r0 = (int)fx_floor(lo, FX) + y, r1 = (int)fx_ceil(hi, FX) + y;
			/* The rows wholly inside, as one run at one alpha;
			 * the one or two rows the edge cuts, one at a time. */
			int f0 = (int)fx_ceil(lo, FX) + y, f1 = (int)fx_floor(hi, FX) + y;

			if (r0 < by0)
				r0 = by0;
			if (r1 > by1)
				r1 = by1;
			if (f0 < r0)
				f0 = r0;
			if (f1 > r1)
				f1 = r1;
			if (f1 < f0)
				f1 = f0;
			for (int r = r0; r < r1; r++) {
				int64_t ry = (int64_t)(r - y) * FX;

				if (r == f0 && f1 > f0) {
					cv_over(&c->px[(size_t)r * c->w + px],
						(size_t)c->w, f1 - f0, col,
						fx_alpha(s->area_alpha, FX,
							 hc));
					r = f1 - 1;
					continue;
				}
				cv_over(&c->px[(size_t)r * c->w + px], 0, 1, col,
					fx_alpha(s->area_alpha,
						 fx_overlap(ry, ry + FX, lo, hi),
						 hc));
			}
		}

		if (s->mode & KCELL_SERIES_LINE) {
			int64_t ya = mirror ? ha : H - ha;
			int64_t yb = mirror ? hb : H - hb;
			int64_t in_lo = lw / 2, in_hi = H - lw / 2;
			int la = s->line_alpha;

			/* Kept inside the band, so a trace on its base is a
			 * whole line and not half of one hanging off the
			 * edge. */
			if (in_hi < in_lo)
				in_lo = in_hi = H / 2;
			if (ya < in_lo)
				ya = in_lo;
			if (ya > in_hi)
				ya = in_hi;
			if (yb < in_lo)
				yb = in_lo;
			if (yb > in_hi)
				yb = in_hi;
			if (!ha && !hb && s->rest_alpha > 0)
				la = s->rest_alpha;

			int64_t lo = (ya < yb ? ya : yb) - lw / 2;
			int64_t hi = (ya < yb ? yb : ya) + (lw - lw / 2);
			int r0 = (int)fx_floor(lo, FX) + y, r1 = (int)fx_ceil(hi, FX) + y;

			if (r0 < by0)
				r0 = by0;
			if (r1 > by1)
				r1 = by1;
			for (int r = r0; r < r1; r++) {
				int64_t ry = (int64_t)(r - y) * FX;
				int64_t cov = fx_overlap(ry, ry + FX, lo, hi);

				cv_over(&c->px[(size_t)r * c->w + px], 0, 1, col,
					fx_alpha(la, cov, hc));
			}
		}
	}
	free(hv);
}

/*
 * A segment of any slope, by the same column method turned on its side when
 * it is steeper than 45 degrees: walked along its MAJOR axis one pixel at a
 * time, each step is covered across the minor axis by the segment's
 * thickness measured along that axis — the width times the length over the
 * major run, which is the one square root.
 */
void kcell_canvas_line(KCellCanvas *c, double x0, double y0, double x1,
		       double y1, double w_px, int slot, int alpha)
{
	int bx0, by0, bx1, by1;
	int64_t a0, b0, a1, b1, da, db, len, lw, half;
	int steep, m0, m1, n0, n1;
	KRgb col;

	if (!c || alpha <= 0)
		return;
	cv_bounds(c, &bx0, &by0, &bx1, &by1);
	if (bx1 <= bx0 || by1 <= by0)
		return;
	col = ktui_theme->slot[slot & 7];

	int64_t X0 = fx_of(x0), Y0 = fx_of(y0), X1 = fx_of(x1), Y1 = fx_of(y1);
	int64_t adx = X1 > X0 ? X1 - X0 : X0 - X1;
	int64_t ady = Y1 > Y0 ? Y1 - Y0 : Y0 - Y1;

	if (!adx && !ady)
		return;
	steep = ady > adx;
	/* a is the major axis, b the minor one. */
	a0 = steep ? Y0 : X0;
	b0 = steep ? X0 : Y0;
	a1 = steep ? Y1 : X1;
	b1 = steep ? X1 : Y1;
	if (a1 < a0) {
		int64_t t;

		t = a0; a0 = a1; a1 = t;
		t = b0; b0 = b1; b1 = t;
	}
	da = a1 - a0;
	db = b1 - b0;
	/* Coordinates are bounded by fx_of() and the canvas by CV_MAX_PX, so
	 * the squares stay far inside 63 bits for any segment that reaches
	 * the canvas at all. */
	if (da > (int64_t)1 << 30 || (db > 0 ? db : -db) > (int64_t)1 << 30)
		return;
	len = isqrt64(da * da + db * db);
	lw = w_px > 0.0 ? fx_of(w_px) : FX;
	if (lw < 1)
		lw = 1;
	if (lw > (int64_t)CV_MAX_PX * FX)
		lw = (int64_t)CV_MAX_PX * FX;
	half = lw * len / (2 * da);

	m0 = steep ? by0 : bx0;
	m1 = steep ? by1 : bx1;
	n0 = steep ? bx0 : by0;
	n1 = steep ? bx1 : by1;
	/* Narrowed in 64 bits and only then made an int: an end far off the
	 * canvas is a pixel index no int holds. */
	if (fx_floor(a0, FX) > m0)
		m0 = fx_floor(a0, FX) < m1 ? (int)fx_floor(a0, FX) : m1;
	if (fx_ceil(a1, FX) < m1)
		m1 = fx_ceil(a1, FX) > m0 ? (int)fx_ceil(a1, FX) : m0;

	for (int m = m0; m < m1; m++) {
		int64_t ma = (int64_t)m * FX, mb = ma + FX, hc, mc, bc, lo, hi;
		int r0, r1;

		if (ma < a0)
			ma = a0;
		if (mb > a1)
			mb = a1;
		hc = mb - ma;
		if (hc <= 0)
			continue;
		mc = (ma + mb) / 2;
		bc = b0 + (mc - a0) * db / da;
		lo = bc - half;
		hi = bc + half;
		if (hi - lo < 1)
			hi = lo + 1;
		r0 = fx_floor(lo, FX) > n0 ? (fx_floor(lo, FX) < n1
						 ? (int)fx_floor(lo, FX) : n1)
					   : n0;
		r1 = fx_ceil(hi, FX) < n1 ? (fx_ceil(hi, FX) > r0
						 ? (int)fx_ceil(hi, FX) : r0)
					  : n1;
		for (int r = r0; r < r1; r++) {
			int64_t rr = (int64_t)r * FX;
			int64_t cov = fx_overlap(rr, rr + FX, lo, hi);
			int px = steep ? r : m, py = steep ? m : r;

			cv_over(&c->px[(size_t)py * c->w + px], 0, 1, col,
				fx_alpha(alpha, cov, hc));
		}
	}
}

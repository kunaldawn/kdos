/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcell — a cell grid, rasterised
 *
 * The fcft glyph cache and the cell -> ARGB painter, with NO WAYLAND IN EITHER,
 * because both sides of the desktop link this. libkwl is a CLIENT: it can only
 * paint into a surface it owns. kdos-comp paints window frames into buffers of
 * its own, in the middle of the scene graph, where no client can be. Turning a
 * KtuiCell into pixels is the half they share, so it may know about a font
 * renderer and a pixel library and about nothing else.
 *
 * WHAT THIS COSTS, stated rather than discovered. This archive carries real
 * `-l` dependencies — a font renderer and a pixel library — and kdos-comp
 * gains fcft by linking it. The load-bearing rule must stay untouched: NOTHING
 * IN 10_bootstrap LINKS THIS OR libkwl, so kinstall links libkbase, libktui
 * and libkcolor and nothing else on the first bootable image. If you are about to
 * add a `-l` to libktui to save a file here, that is the trade you are making.
 *
 * Dependency direction gains one edge and reverses none:
 *
 *      libkcell -> libktui -> libkcolor -> libkbase
 *      libkwl   -> libkcell
 *
 * kcell_paint() takes a KtuiCell because that is the cell the whole toolkit
 * already produces. A second cell type so this archive could stand alone would
 * mean a conversion on every frame and two answers to what a cell is.
 * ---------------------------------
 */

#ifndef KCELL_H
#define KCELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fcft/fcft.h>
#include <pixman.h>

#include "ktui.h"

/*
 * The `scale` every painter here takes is a WHOLE NUMBER, and the ceiling is
 * deliberate.
 *
 * At scale N a glyph is its own scale-1 coverage blitted N x N, nearest
 * neighbour, which is exactly what kdos-splash does with the console PSF: the
 * same letters, every pixel doubled, never a resampled blur. A scale above 4
 * is a 128-pixel cell and nothing that could want one exists.
 *
 * A FRACTIONAL scale is not a multiplier here at all. It is a different font:
 * the caller loads the cell font at the device pixel size
 * (kcell_name_at_px()) and paints at scale 1, so a 1.5 output gets a face
 * rasterised at 48 pixels rather than a 32-pixel one stretched by 1.5.
 */
enum { KCELL_MAX_SCALE = 4 };

/*
 * fontconfig name; NULL takes the default. Returns -1 if fcft cannot be
 * initialised or the name resolves to nothing usable.
 *
 * CALLABLE REPEATEDLY, AND A LOAD REPLACES EVERYTHING. It tears the previous
 * faces down first — the glyph cache, the ascii candidate table and the tiling
 * scratch with them — so every KCellGlyph handed out before it, and every
 * kcell_w()/kcell_h()/kcell_ascent() a caller cached, is invalid once it
 * returns. A failed load leaves no font at all and the metrics at zero.
 *
 * kcell_font_free() is the same teardown plus the cell font's REFERENCE on
 * fcft, which this library counts: fcft goes down on the last holder's
 * release and not on this call, so a canvas still drawing keeps its faces and
 * a process that only ever drew canvases never needed a cell font at all. It
 * is still not a prerequisite of a reload — a load replaces the faces by
 * itself — and every KCellGlyph and every cached metric is invalid after it,
 * exactly as after a load.
 */
int kcell_font_load(const char *name);
void kcell_font_free(void);

/*
 * THE SAME NAME AT ANOTHER PIXEL SIZE, for a caller that must draw the font it
 * was given at a size it was not: `name` with every `pixelsize=` and `size=`
 * property removed and `:pixelsize=px` appended — `monospace` when nothing is
 * left of it. Removed and not overridden, because fontconfig appends a
 * repeated property rather than replacing it and derives the size from the
 * FIRST. 0 when `out` is too small or `px` is not a size.
 *
 * kcell_name_pixelsize() is the pixel size fontconfig resolves `name` to —
 * its `pixelsize=` where it carries one, else what its point size (or
 * fontconfig's default) comes to — and 0 when fontconfig cannot say. The
 * base a scaled size is counted from.
 */
int kcell_name_at_px(const char *name, int px, char *out, size_t n);
double kcell_name_pixelsize(const char *name);

/* Metrics at scale 1. A caller at scale N multiplies. */
int kcell_w(void);
int kcell_h(void);
int kcell_ascent(void);

/* The raw fcft glyph, cached, misses cached too. NULL is a rasterisation
 * failure and NOT an absence — a codepoint no font carries comes back as the
 * face's .notdef. Ask kcell_has() about absence. */
const struct fcft_glyph *kcell_glyph(uint32_t cp);

/*
 * Whether the loaded font actually carries a codepoint.
 *
 * This exists because a missing glyph is a BLANK CELL or a tofu box, not a
 * visible error, and a titlebar that silently loses its close box is the kind
 * of defect that ships. The chrome's glyph budget is checked through this at
 * startup rather than discovered on somebody's screen.
 *
 * The test is against a SENTINEL: fcft answers an absent codepoint with the
 * primary face's glyph index 0, so the load rasterises a permanent Unicode
 * noncharacter — which nothing can carry — and anything that comes back as the
 * same picture at the same metrics is that same .notdef and is reported
 * missing.
 */
bool kcell_has(uint32_t cp);

/* One of libktui's eight slots as pixman wants it. Shared rather than
 * reimplemented per caller: kwl.c fills a blank lock surface with the same
 * KT_BG the grid would have painted, and two copies of this would be two
 * answers to what the background is. */
pixman_color_t kcell_slot_color(int slot);

/*
 * Per-slot BACKGROUND alpha, 0..255, default 255. A cell whose background is
 * `slot` is filled at that alpha, PREMULTIPLIED; 0 leaves the cell clear.
 *
 * For a surface that sits over something and has to let it through: the
 * desktop above the compositor's wallpaper (KT_BG at 0), and the panel above
 * its own pixel backdrop (KT_SURFACE at 0). The caller must ALSO be on a
 * buffer format that carries alpha — kcell_needs_alpha() is what a backend
 * asks to decide that, and on an opaque format the alpha is simply lost and
 * the fill comes out at full strength.
 *
 * A FOREGROUND is never affected: a glyph is ink and is always opaque. What a
 * REVERSE swaps into the background position IS a background and does take the
 * alpha, or every reversed cell is an opaque hole in a translucent surface.
 */
void kcell_set_slot_alpha(int slot, uint8_t alpha);
/* What the slot's background is filled at now — 0 for a slot a backdrop owns. */
uint8_t kcell_slot_alpha(int slot);
void kcell_reset_slot_alpha(void);
bool kcell_needs_alpha(void);

/*
 * Whether a zero-alpha background is CLEARED or LEFT ALONE.
 *
 * Off (the default): cleared, which is what a surface with nothing under it in
 * its own buffer needs — the buffer is reused between frames and a skip keeps
 * the last frame's pixels. On: left alone, for a surface whose backdrop has
 * already painted those pixels one layer down this frame. Set by the backend
 * around a backdrop paint, not by an application.
 */
void kcell_set_bg_preserve(bool on);

/* ────────────────────────────────────────────────────────────────────────
 * Pixel chrome (kcell_px.c) — painted UNDER the cell grid
 *
 * For the three things a 10x20 cell of one colour cannot say: a softened
 * plate, a fill that varies continuously down a bar, and a one-pixel line.
 * The caller clears, then layers body, plates and rules with OVER. Every
 * call is clipped to `dst`, so a rect partly or wholly past its edge is safe.
 *
 * PAINT ONLY. Layout, hit maps and every --dump stay cells; a surface drawn
 * with nothing but these has left the toolkit.
 * ──────────────────────────────────────────────────────────────────────── */
void kcell_px_clear(pixman_image_t *dst, int x, int y, int w, int h);
void kcell_px_fill(pixman_image_t *dst, int x, int y, int w, int h,
		   uint32_t rgb, uint8_t a);
/* A rounded plate, graded top to bottom. A gradient reads as a button; a flat
 * slab of full-strength accent reads as an error state — pass the same colour
 * twice for a flat one. */
void kcell_px_round_grad(pixman_image_t *dst, int x, int y, int w, int h,
			 int r, uint32_t top, uint32_t bot, uint8_t a);
void kcell_px_vgrad(pixman_image_t *dst, int x, int y, int w, int h,
		    uint32_t top, uint32_t bot, uint8_t a);

/* A glyph's mask at a given scale, with the offsets already multiplied. The
 * image is owned by the cache and must not be unref'd — and the cache is
 * CAPPED, so it is valid only until the next glyph lookup: use it and let go,
 * never keep it across cells or frames. */
typedef struct {
	pixman_image_t *pix;
	int x, y;		/* bearing, scaled */
	int width, height;	/* pixels, scaled */
	/* Bold was asked for and no bold face answered. The caller draws the
	 * mask a second time one scaled pixel to the right; the cache never
	 * holds a pre-emboldened copy, because the weight belongs to the blit
	 * and not to the rasterisation. */
	bool synth_bold;
} KCellGlyph;

/*
 * The style bits kcell_glyph_face() takes. A companion face is optional — see
 * kcell_font_load() — so a request always resolves to a face that exists: the
 * slant survives in preference to the weight, and `synth_bold` says when the
 * weight has to be drawn rather than rasterised. A caller therefore never has
 * to ask which faces the font has.
 */
enum {
	KCELL_ST_ITALIC = 1 << 0,
	KCELL_ST_BOLD   = 1 << 1,
	KCELL_NSTYLE    = 4
};

/* The upright face; equivalent to kcell_glyph_face() with no style bits. */
bool kcell_glyph_scaled(uint32_t cp, int scale, KCellGlyph *out);
bool kcell_glyph_face(uint32_t cp, int scale, int style, KCellGlyph *out);

/*
 * Paint the grid.
 *
 * `prev` is the last-presented buffer and is updated as we go, so the next
 * frame repaints only the CHANGED SPAN of each changed row. The span is
 * widened by one cell each way, because a cell's pixels are not always its
 * own, and then back onto the LEAD of a double-width glyph if it starts on
 * that glyph's continuation cell — the lead paints both halves, so a span that
 * began on the continuation would clear the right half and redraw nothing. A
 * caret or a clock digit therefore costs the cells it touched and not every
 * glyph beside them. Pass NULL for `prev` to paint unconditionally, which is
 * what a compositor-side frame strip wants: it is a handful of cells and it is
 * rendered into a buffer that was just allocated.
 *
 * `dst_w`/`dst_h` are the destination's real pixel size. Anything past
 * `cols * cell_w * scale` or `rows * cell_h * scale` is filled with KT_BG —
 * a cell grid whose surface height is not a multiple of the cell height would
 * otherwise leave an UNPAINTED STRIP, which is a live defect at the bottom of
 * the lock screen at 1280x800 and is fixed here for every consumer at once.
 *
 * THE FRAME CHARACTERS ARE NOT RASTERISED. U+2500's single and double box
 * sets and the whole of U+2580..U+259F — blocks, eighths, quadrants and the
 * three shades — are drawn as rectangles derived from the cell, in the cell's
 * foreground, by codepoint alone. Nothing selects it and no face can change
 * it, so a border joins at every size and every face; a face is still asked
 * for every other character, the heavy, dashed and rounded box variants
 * included. A synthesised character is one cell wide and puts no ink outside
 * its own cell, which is what the row diff and the wide-glyph clip both
 * assume.
 */
void kcell_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev,
		 int cols, int rows, int full, int scale, int dst_w, int dst_h);

/*
 * The same partial paint in two halves, for a caller that has to act on the
 * cells BEFORE they are painted — libkwl restores a backdrop under exactly
 * these cells first, because the painter leaves a backdrop-owned background
 * alone and a glyph drawn over the last frame's glyph is both of them.
 *
 * kcell_diff_spans() writes one half-open cell span per row (x0 == x1: the row
 * is unchanged), widened exactly as kcell_paint() widens its own, and returns
 * how many rows changed. It is also the damage: the pixels a changed cell can
 * reach are the cells of its span and no others. kcell_paint_spans() paints
 * those spans and updates `prev` (which may be NULL) over them; it never pads
 * the remainder, which only a full paint can have changed.
 */
typedef struct {
	int x0, x1;
} KCellSpan;

int kcell_diff_spans(const KtuiCell *cur, const KtuiCell *prev, int cols,
		     int rows, KCellSpan *spans);
void kcell_paint_spans(pixman_image_t *dst, const KtuiCell *cur,
		       KtuiCell *prev, int cols, int rows,
		       const KCellSpan *spans, int scale, int dst_w, int dst_h);

/*
 * SCROLLING BY MOVING PIXELS, for a caller that keeps `prev` as the exact
 * record of what `dst` holds (libkwl's per-buffer shadow).
 *
 * A grid that shifted vertically — a terminal taking a line of output, a list
 * scrolled by a row — changes every row it moved, and the row diff repaints
 * every one of them. Each row's pixels are a function of that row's cells
 * alone, so the same pixels already stand in `dst`, one or more rows away.
 *
 * kcell_scroll_find() looks for the one band of whole rows of `cur` that
 * equals a band of `prev` shifted vertically, and answers 1 with it in `s`
 * when moving it saves enough repainting to pay for the move: at least two
 * rows that differ from `prev` where they stand, and at least half the band.
 * `diff` is kcell_diff_spans(cur, prev) — the caller has it already, for the
 * paint — and is what says which rows differ where they stand, so a frame
 * with fewer than two changed rows costs one pass over `diff` and nothing
 * else.
 * A row holding a shade character (U+2591..U+2593) does not join a band whose
 * shift is not a whole period of the shade pattern, which is anchored to the
 * destination and not to the cell.
 *
 * kcell_scroll_apply() moves those pixel rows inside `dst` — which is its own
 * source, so the move is a memmove and never an overlapping composite — and
 * moves the band's rows of `prev` with them, so `prev` still describes `dst`
 * row for row. Only the first `dst_w` pixels of each row move: a caller whose
 * image is wider than its grid passes the grid's width, since a strip past
 * the last cell is no cell's pixels and must stay where it is. The rows the band left keep both their pixels and their
 * `prev`, and the row diff that follows repaints them only where `cur`
 * differs. A destination row whose source row was clipped by `dst_h` is
 * marked stale, as is any row the caller marks with kcell_row_stale(). It
 * returns -1, moving nothing, when `dst` is not 32 bits per pixel.
 *
 * KCELL_STALE is a `ch` no cell ever carries: a `prev` row holding it differs
 * from any `cur` row, so the diff repaints it whole.
 */
typedef struct {
	int y;		/* first destination row                       */
	int n;		/* rows in the band                            */
	int from;	/* the band's first row in `prev` before the move */
} KCellScroll;

#define KCELL_STALE 0xffffffffu

int kcell_scroll_find(const KtuiCell *cur, const KtuiCell *prev, int cols,
		      int rows, int scale, const KCellSpan *diff,
		      KCellScroll *s);
int kcell_scroll_apply(pixman_image_t *dst, KtuiCell *prev, int cols,
		       int rows, const KCellScroll *s, int scale, int dst_w,
		       int dst_h);
void kcell_row_stale(KtuiCell *prev, int cols, int row);

/* ── a pixel canvas that lands in the cell grid (kcell_canvas.c) ─────────
 *
 * A canvas is a pixman image exactly N x M CELLS at the output scale, drawn
 * into with fills, text at any pixel size and the antialiased data marks a
 * chart is made of, under a clip, and handed to the sprite table
 * when it is finished — whole when it is at most 16x16 cells, and as a grid of
 * views (`kcell_canvas_view`) when it is larger — so the grid keeps the layout, the
 * row diff keeps being the damage mechanism, and a text backend keeps drawing
 * the fallback codepoint. It is what lets a control be taller than one row of
 * text without the toolkit growing a second drawing model. See the file for
 * the whole argument, and for the one thing a caller must still do: a
 * sprite's cells encode the SLOT, so an ANIMATED tile has to publish under a
 * key that changes when its content does or the diff sees nothing.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct KCellCanvas KCellCanvas;

/* The family canvas text is drawn in — the chrome's own `--font`, whose size
 * is replaced per request. Dropping every cached size is the point of setting
 * it: they were all resolved from the previous family. */
void kcell_canvas_font(const char *name);

KCellCanvas *kcell_canvas_new(int cells_w, int cells_h, int cell_w, int cell_h,
			      int scale);
void kcell_canvas_free(KCellCanvas *c);
/* Borrowed, and valid until the canvas is freed — which the caller must not do
 * while a sprite slot still points at it. */
pixman_image_t *kcell_canvas_image(KCellCanvas *c);
/* A new image over the cells [cell_x, cell_x + cells_w) x [cell_y, cell_y +
 * cells_h) of the canvas, sharing its pixels: nothing is copied, so what is
 * drawn into the canvas is already in the view. The caller owns the one
 * reference it is handed, and every reference must be gone before the canvas
 * is freed. NULL for a rectangle outside the canvas. */
pixman_image_t *kcell_canvas_view(KCellCanvas *c, int cell_x, int cell_y,
				  int cells_w, int cells_h);
int kcell_canvas_w(const KCellCanvas *c);
int kcell_canvas_h(const KCellCanvas *c);
void kcell_canvas_clear(KCellCanvas *c);

/* SRC, so a fill can put transparency back; `alpha` 0..255 scales the slot. */
void kcell_canvas_fill(KCellCanvas *c, int x, int y, int w, int h, int slot,
		       int alpha);

/* Text at an arbitrary pixel size, drawn from its BASELINE. Returns the
 * advance. `kcell_canvas_text_width` measures without drawing. The family is
 * the one kcell_canvas_font() named, else the cell font's (kcell_font_load),
 * else `monospace`; a size no bitmap strike draws exactly comes from the
 * family's `(TTF)` twin, as for the cell. */
int kcell_canvas_text(KCellCanvas *c, int x, int baseline, int px,
		      const char *utf8, int slot);
int kcell_canvas_text_width(int px, const char *utf8);
int kcell_canvas_text_ascent(int px);
int kcell_canvas_text_height(int px);
/*
 * The same text into ANY 32-bit image — a backdrop, not a canvas — in `rgb`
 * (0xRRGGBB, opaque) OVER what is there, and cut to the pixel rectangle
 * (cx, cy, cw, ch) by each glyph's coordinates. `dst`'s own clip region is
 * never touched, so a caller drawing under a clip of its own keeps it, and a
 * pixel drawn under that clip is the pixel an unclipped draw puts there.
 * Returns the advance; 0 with nothing drawn when no face answers `px`.
 */
int kcell_text_draw(pixman_image_t *dst, int x, int baseline, int px,
		    const char *utf8, uint32_t rgb, int cx, int cy, int cw,
		    int ch);

/*
 * THE DATA MARKS: a series and a line, antialiased, and a clip under both.
 *
 * Every call takes a SLOT and an alpha, never a colour, so a chart retints
 * with the palette like everything else. These two are the only antialiased
 * pixels libkcell draws: a data trace is a measurement and its slope is
 * information, while chrome stays hard-edged (see kcell_px.c). Both are
 * composited OVER what is already there, so a plate or a gridline drawn first
 * stays under the trace. Both are computed in 1/256-pixel fixed point from
 * the values handed in, so one input is one picture on every build.
 */
enum {
	KCELL_SERIES_AREA   = 1,	/* the fill between the trace and its base */
	KCELL_SERIES_LINE   = 2,	/* the trace itself                        */
	KCELL_SERIES_BOTH   = 3,
	KCELL_SERIES_MIRROR = 4,	/* the base is the TOP edge; grows down    */
	KCELL_SERIES_HOLD   = 8,	/* the oldest value extends to the left    */
};

typedef struct {
	const double *v;	/* samples, OLDEST FIRST; the newest is drawn
				 * against the right edge                   */
	int n;
	double vmax;		/* the value that reaches the far edge       */
	double step;		/* pixels per sample; <= 0 is one per pixel  */
	int mode;		/* KCELL_SERIES_*                            */
	int area_alpha;		/* 0..255                                    */
	int line_alpha;
	int rest_alpha;		/* the trace where it lies on its base; 0 is
				 * line_alpha                                */
	double line_w;		/* pixels; <= 0 is one                       */
} KCellSeries;

/*
 * One series into the pixel rectangle (x, y, w, h), newest sample at the
 * right edge, linear between sample centres. A sample above zero is never
 * drawn lower than one pixel — "is anything happening at all" is the first
 * question a chart is asked — and the trace is kept inside the rectangle
 * when it lies on its base, so a series at rest is a line and not nothing.
 */
void kcell_canvas_series(KCellCanvas *c, int x, int y, int w, int h,
			 const KCellSeries *s, int slot);
/* A straight segment `w_px` wide at any slope, with square ends at the two
 * points. Coordinates are pixel edges: (0,0) is the top-left corner. */
void kcell_canvas_line(KCellCanvas *c, double x0, double y0, double x1,
		       double y1, double w_px, int slot, int alpha);
/* Narrow every later fill, text, series and line to (x, y, w, h), intersected
 * with the clip already in force. Answers 0, or -1 when the stack is full —
 * and a refused push must not be popped. kcell_canvas_clear() empties the
 * stack, so a canvas reused for the next frame starts unclipped. */
int kcell_canvas_clip_push(KCellCanvas *c, int x, int y, int w, int h);
void kcell_canvas_clip_pop(KCellCanvas *c);

/* ── a decoded picture becomes sprite tiles (kcell_tile.c) ───────────────
 *
 * One scale and one cut, between a pixman image of whatever size its format
 * declared and libktui's sprite table, which does no pixel work of its own.
 *
 * `key` is the caller's content identity, `cw`/`ch` the size in CELLS, and
 * `cell_w`/`cell_h` how many pixels one cell is at the current scale. Returns
 * what the table said: > 0 when every tile took a slot, -1 when the caller
 * must draw the fallback instead — which a tty, a full table and a build with
 * no pixel path all look like.
 *
 * `kcell_tile_free` is the evictor to hand `ktui_sprite_evictor`: the table
 * holds a borrowed pointer, so nothing else can know when a tile stops being
 * named. A caller that registers tiles WITHOUT an evictor leaks one picture
 * per put.
 * ──────────────────────────────────────────────────────────────────────── */

int kcell_tile_picture(pixman_image_t *img, uint64_t key, int cw, int ch,
		       int cell_w, int cell_h, uint32_t fallback);
void kcell_tile_free(uint64_t key, const void *pix, void *user);

/* ── pixels to characters, by shape (kcell_ascii.c) ──────────────────────
 *
 * The engine behind `Super+A`, `kdos-shot --text` and the pixman fallback for
 * the compositor's GPU pass. NOT aalib: aalib picks a glyph by luminance alone
 * and never asks which part of a cell a character covers, which is why chafa
 * and notcurses look better than the 1990s tools. This is the shape-vector
 * method, clean-room from its published description.
 *
 * Six samples per cell, so a vertical stroke and a horizontal one are
 * distinguishable — which is the whole difference between `|` and `-`.
 *
 * No libm anywhere in it: the contrast exponent is exactly 2 and distances are
 * compared squared. libkcell has to stay linkable by everything libktui is.
 */
#define KCELL_ASCII_DIM 6

/* Measure the candidate set against the loaded font. Returns how many glyphs
 * survived — a candidate the font does not carry is dropped, because a glyph
 * that rasterises to nothing would win every dark cell. Idempotent. */
int kcell_ascii_init(void);
/* Throw the measurement away, so the next kcell_ascii_init() takes it again.
 * The table is a measurement of one face at one cell size; kcell_font_load()
 * calls this, and nothing else has to. */
void kcell_ascii_forget(void);
int kcell_ascii_count(void);
uint32_t kcell_ascii_glyph(int i);
const float *kcell_ascii_vector(int i);
/* The six disc positions as x,y pairs — for the GPU half, which has to sample
 * at exactly the same places or the two implementations disagree. */
void kcell_ascii_discs(float *out_xy);

/* Normalise by the cell's own maximum, square, scale back. Without it every
 * cell collapses onto the mid-grey glyph. In place. */
void kcell_ascii_contrast(float *s);
/* Nearest neighbour over the table. Squared Euclidean distance. */
int kcell_ascii_match(const float *s);

/* The six samples for one cell of an ARGB image, plus that cell's mean colour.
 * `tint` may be NULL. */
void kcell_ascii_sample(const uint32_t *argb, int w, int h, int stride_px,
			int cx, int cy, int cell_w, int cell_h, float *out,
			uint32_t *tint);

/* The whole image. `out_cp` and `out_tint` must hold (w/cell_w)*(h/cell_h)
 * entries; `out_tint` may be NULL. Returns -1 if there is no font. */
int kcell_ascii_image(const uint32_t *argb, int w, int h, int stride_px,
		      int cell_w, int cell_h, uint32_t *out_cp,
		      uint32_t *out_tint, int *out_cols, int *out_rows);

#endif /* KCELL_H */

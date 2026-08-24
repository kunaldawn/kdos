/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcell — a cell grid, rasterised
 *
 * The fcft glyph cache and the cell -> ARGB painter, with no Wayland in either.
 * They were libkwl's, and libkwl is a CLIENT: it can only paint into a surface
 * it owns. kdos-comp has to paint window frames into buffers of its own, in the
 * middle of the scene graph, where no client can be — so the half of libkwl that
 * knows how to turn a KtuiCell into pixels had to become something both sides
 * can link.
 *
 * WHAT THIS COSTS, stated rather than discovered. CLAUDE.md called libkwl "the
 * ONE library with real -l dependencies"; it is two now, and kdos-comp gains
 * fcft. The part of that rule which is actually load-bearing is untouched and
 * must stay untouched: NOTHING IN PHASE 1 LINKS EITHER OF THEM, so kinstall
 * still links zero libraries on the first bootable image. If you are about to
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
#include <stdint.h>

#include <fcft/fcft.h>
#include <pixman.h>

#include "ktui.h"

/*
 * Integer scale only, and the ceiling is deliberate.
 *
 * Terminus has no 64-pixel bitmap and fontconfig would answer a request for one
 * by synthesising a blurry scale of the 32. So HiDPI here is the glyph's own
 * coverage blitted N x N, nearest neighbour, which is exactly what kdos-splash
 * already does with the console PSF. A scale above 4 is a 128-pixel cell and
 * nothing that could want one exists.
 */
enum { KCELL_MAX_SCALE = 4 };

/* fontconfig name; NULL takes the default. Returns -1 if fcft cannot be
 * initialised or the name resolves to nothing usable. */
int kcell_font_load(const char *name);
void kcell_font_free(void);

/* Metrics at scale 1. A caller at scale N multiplies. */
int kcell_w(void);
int kcell_h(void);
int kcell_ascent(void);

/* The raw fcft glyph, cached, misses cached too. NULL means the font has no
 * such codepoint. */
const struct fcft_glyph *kcell_glyph(uint32_t cp);

/*
 * Whether the loaded font actually carries a codepoint.
 *
 * This exists because a missing glyph is a BLANK CELL, not a visible error, and
 * a titlebar that silently loses its close box is the kind of defect that ships.
 * The chrome's glyph budget is checked through this at startup rather than
 * discovered on somebody's screen.
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
 * The caller clears, then layers body, plates and rules with OVER.
 *
 * PAINT ONLY. Layout, hit maps and every --dump stay cells; a surface drawn
 * with nothing but these has left the toolkit.
 * ──────────────────────────────────────────────────────────────────────── */
void kcell_px_clear(pixman_image_t *dst, int x, int y, int w, int h);
void kcell_px_fill(pixman_image_t *dst, int x, int y, int w, int h,
		   uint32_t rgb, uint8_t a);
void kcell_px_round(pixman_image_t *dst, int x, int y, int w, int h, int r,
		    uint32_t rgb, uint8_t a);
/* The same plate, graded top to bottom. A gradient reads as a button; a flat
 * slab of full-strength accent reads as an error state. */
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
} KCellGlyph;

bool kcell_glyph_scaled(uint32_t cp, int scale, KCellGlyph *out);

/*
 * Paint the grid.
 *
 * `prev` is the last-presented buffer and is updated as we go, so the next
 * frame only repaints rows that changed — row granularity rather than cell,
 * because the background fill runs already amortise a row and per-cell damage
 * would cost more bookkeeping than it saves at this size. Pass NULL for `prev`
 * to paint unconditionally, which is what a compositor-side frame strip wants:
 * it is a handful of cells and it is rendered into a buffer that was just
 * allocated.
 *
 * `dst_w`/`dst_h` are the destination's real pixel size. Anything past
 * `cols * cell_w * scale` or `rows * cell_h * scale` is filled with KT_BG —
 * a cell grid whose surface height is not a multiple of the cell height would
 * otherwise leave an UNPAINTED STRIP, which is a live defect at the bottom of
 * the lock screen at 1280x800 and is fixed here for every consumer at once.
 */
void kcell_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev,
		 int cols, int rows, int full, int scale, int dst_w, int dst_h);

/* ── a pixel canvas that lands in the cell grid (kcell_canvas.c) ─────────
 *
 * A canvas is a pixman image exactly N x M CELLS at the output scale, drawn
 * into with fills and text at any pixel size, and handed to
 * `ktui_sprite_put()` when it is finished — so the grid keeps the layout, the
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
int kcell_canvas_w(const KCellCanvas *c);
int kcell_canvas_h(const KCellCanvas *c);
void kcell_canvas_clear(KCellCanvas *c);

/* SRC, so a fill can put transparency back; `alpha` 0..255 scales the slot. */
void kcell_canvas_fill(KCellCanvas *c, int x, int y, int w, int h, int slot,
		       int alpha);

/* Text at an arbitrary pixel size, drawn from its BASELINE. Returns the
 * advance. `kcell_canvas_text_width` measures without drawing. */
int kcell_canvas_text(KCellCanvas *c, int x, int baseline, int px,
		      const char *utf8, int slot);
int kcell_canvas_text_width(int px, const char *utf8);
int kcell_canvas_text_ascent(int px);
int kcell_canvas_text_height(int px);

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

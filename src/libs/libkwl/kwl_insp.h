/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwl internals — the KDOS_INSPECT overlay, not installed
 *
 * A DEVELOPER OVERLAY PAINTED INTO THE SHM BUFFER AND NOWHERE ELSE. With
 * KDOS_INSPECT set, every commit of the surface wears:
 *
 *   - a tint over each row whose cells changed, fading over a second, so a
 *     row that repaints on every frame stays lit and one that changed once
 *     flashes and goes out;
 *   - a corner panel of numbers: commits a second (and the overlay's own
 *     refreshes, which are not counted in them), frames stashed a second by
 *     the frame-callback throttle or two held buffers, the paint time of the
 *     last commit and the most in the last second, the compositor's
 *     frame-callback latency, the rows and cells the last commit changed, the
 *     sprite table's bytes, and the hit rects of the last frame;
 *   - an outline round each hit rect of a frame surface (ktui_hit_at()):
 *     KT_ACCENT for the focused control, KT_WARN for the rest, KT_MID for
 *     chrome.
 *
 * NOTHING HERE REACHES THE CELL BUFFER. The panel is cells, but cells laid on
 * a COPY of the frame the painter is handed; the frame the damage is diffed
 * against, libktui's buffers, a --dump and a golden never carry it, and a tty
 * has no buffer to paint into. Every colour is a theme slot.
 *
 * AN INSPECTED SURFACE PAINTS AND DAMAGES EVERY COMMIT IN FULL, because the
 * overlay moves pixels no cell diff knows about: a fading tint and a panel of
 * changing numbers would otherwise leave stale pixels in whichever buffer and
 * whichever part of the compositor's texture the diff did not name. So the
 * paint time is a full paint's, and the rows lit are the rows whose CELLS
 * changed — every row on a commit that was full anyway — not the
 * damage an uninspected run would have sent.
 *
 * Off, each call site tests a flag read once from the environment.
 * ---------------------------------
 */

#ifndef KWL_INSP_H
#define KWL_INSP_H

#include <stdint.h>

#include "kcell.h"

/* KDOS_INSPECT is set, non-empty and not "0". Read once. */
int kwl_insp_on(void);

/* A monotonic microsecond clock for the paint time. */
int64_t kwl_insp_us(void);

/* A frame went into the stash instead of a commit. */
void kwl_insp_stash(int64_t now_ms);

/*
 * The cells this commit changed: `spans` one per row (x0 == x1 for a row
 * that did not), or every row when `full`. Only a surface's own commits are
 * recorded — never the overlay's refresh.
 */
void kwl_insp_damage(const KCellSpan *spans, int w, int h, int full,
		     int64_t now_ms);

/*
 * `cur` with the panel laid over its top right corner, in a buffer this file
 * owns; `cur` itself when the grid is too small for the panel or the copy
 * cannot be had. A wide glyph whose right half the panel covers loses its
 * left half too, so no glyph reaches under the panel.
 */
const KtuiCell *kwl_insp_cells(const KtuiCell *cur, int w, int h,
			       int64_t now_ms);

/*
 * The tint and the outlines, over the painted grid. `grid` is the image the
 * cells were painted into, `cw` x `ch` a cell in its pixels.
 */
void kwl_insp_paint(pixman_image_t *grid, int w, int h, int cw, int ch,
		    int64_t now_ms);

/* A commit went out: the surface's own (own == 0), with its paint time, or
 * the overlay's refresh. */
void kwl_insp_committed(int own, int64_t paint_us, int64_t now_ms);

/* The frame callback for the last commit answered, `ms` after it. */
void kwl_insp_frame_done(int64_t ms);

/*
 * Milliseconds until the overlay wants a refresh commit of its own: a
 * tenth of a second while a tint is fading, as soon as the panel would read
 * differently, and -1 when neither — a still surface under the overlay goes
 * still too.
 */
int kwl_insp_wait(int64_t now_ms);

/* A buffer of `n` cells for the refresh to copy the screen into. */
KtuiCell *kwl_insp_scratch(size_t n);

#endif /* KWL_INSP_H */

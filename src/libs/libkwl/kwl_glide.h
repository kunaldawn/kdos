/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwl — a scrolled list presented gliding (private to libkwl)
 *
 * A list declares where it is and which item is its first row
 * (kwl_list_view); when that row changes between two commits, the pixels of
 * the list are presented sliding from the picture that was on the screen to
 * the new one over KWL_GLIDE_MS, and then stand exactly where the cells put
 * them. THE CELLS NEVER MOVE BY LESS THAN A ROW: the view, the hit map, the
 * dump and every golden see whole rows, and the fraction of a row exists only
 * in the pixels of the commits in between.
 *
 * No Wayland here: kwl.c hands it the buffer's grid image, the cells and the
 * clock, so a fixture drives it through the real flush (paintcheck.c).
 * ---------------------------------
 */

#ifndef KWL_GLIDE_H
#define KWL_GLIDE_H

#include <stdint.h>

#include <pixman.h>

#include "ktui.h"

/* How long one step of a list takes to arrive. A later step while one is
 * under way starts from wherever the picture is and takes this long again. */
#define KWL_GLIDE_MS 100
/* Lists one surface may declare per frame; one past it is not glided. */
#define KWL_GLIDES 4

/* The views declared since the last client flush become the frame's. Called
 * once per flush the surface asks for, before the commit that carries it. */
void kwl_glide_frame(void);

/*
 * Before the paint of a commit. `shown` is the grid of the buffer on the
 * screen — the same geometry as the one about to be painted, or NULL — with
 * whether it went out under an opaque region, and `screen` the cells it
 * shows; `cw`, `ch` are the cell in buffer pixels. A
 * view whose first row moved since the last commit starts gliding when
 * `allow` says motion is on, the move is shorter than the view, and the
 * cells agree that it was a move: at least half the rows the two frames share
 * are the same rows shifted.
 */
void kwl_glide_begin(pixman_image_t *shown, int shown_opaque,
		     const KtuiCell *cur, const KtuiCell *screen, int w, int h,
		     int cw, int ch, int64_t now, int allow);

/*
 * After the paint: every gliding view's pixels are moved to where the glide
 * is at `now`, the rows the move left open are filled from the picture that
 * was on the screen, and those cells are marked stale in `shadow` — these
 * pixels are not the cells', and the next paint of this buffer must repaint
 * them. A glide whose old picture went out translucent lands at once when
 * this commit claims `opaque`. Fills `rects` (x, y, w, h in `grid` pixels)
 * with what must be damaged beyond the cells that changed, and returns how
 * many.
 */
int kwl_glide_apply(pixman_image_t *grid, KtuiCell *shadow, int w, int h,
		    int opaque, int64_t now, int (*rects)[4], int max);

/* A commit is owed: a view is moving, or the screen shows one in between. */
int kwl_glide_owed(void);

#endif /* KWL_GLIDE_H */

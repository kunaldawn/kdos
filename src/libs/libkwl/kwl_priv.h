/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwl internals — not installed
 * ---------------------------------
 */

#ifndef KWL_PRIV_H
#define KWL_PRIV_H

#include <stdbool.h>
#include <stdint.h>

#include <fcft/fcft.h>
#include <pixman.h>

#include "kcell.h"
#include "kwl.h"

/*
 * The glyph cache and the cell painter are libkcell's now — kdos-comp needs
 * them to draw window frames into buffers of its own, in the middle of the
 * scene graph, and a client library cannot be linked from there. What is left
 * in this archive is exactly the Wayland half.
 */

/* Keys */
#include <xkbcommon/xkbcommon.h>
int kwl_keysym_to_ktui(xkb_keysym_t sym, struct xkb_state *state,
		       xkb_keycode_t code);

/* The shm buffer the cells are painted into. */
typedef struct {
	pixman_image_t *img;
	void *data;
	int w, h;		/* pixels */
	size_t size;
	struct wl_buffer *wl;
	bool busy;		/* held by the compositor until release */
	/*
	 * What THIS buffer's pixels currently show, cell for cell. Commits
	 * alternate buffers, so a partial paint must diff against the buffer
	 * being painted — diffing against the frame on SCREEN (the other
	 * buffer) is how a grid grows stale rows that never repair.
	 */
	KtuiCell *shadow;
	int scols, srows;
	/*
	 * The shadow's cells are current but its PIXELS are not: something
	 * changed the cell→pixel mapping (the palette, the scale) while the
	 * other buffer was the one being painted. Cleared by the next full
	 * paint of this buffer.
	 */
	bool stale;
} KwlBuffer;

#endif /* KWL_PRIV_H */

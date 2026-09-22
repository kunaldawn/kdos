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

/*
 * THE FACE LIST AND THE LIVE SWITCH — kwl_font.c, and the four libkdisp
 * vtable slots it fills.
 *
 * The list is FONTCONFIG'S MONOSPACE FAMILIES, deduplicated and sorted. A cell
 * grid has one advance for every glyph, so a proportional face is not a face
 * this backend can wear and offering one is offering a broken screen.
 *
 * `kwl_font_set` takes an INDEX into that list. A negative one puts back the
 * name the surface opened with. `keep` writes the family into comp.conf's
 * `chrome_font` and `panel_font`, which is the only way a choice reaches the
 * OTHER surfaces: under the compositor each one is its own process with its
 * own font, and both keys are read once at startup.
 */
int kwl_font_count(void);
int kwl_font_at(int i, char *out, int cap);
int kwl_font_current(void);
void kwl_font_set(int index, int keep);

/*
 * The name in force, in fontconfig's own syntax — family, then the size and
 * whatever options ride behind it.
 */
const char *kwl_font_name(void);
/*
 * Load `name` and re-cut the grid around the cell it gives, the sequence
 * kwl_font_step() takes. 0 when it is in force, including when it already was;
 * -1 with NOTHING MOVED when it will not load, the old face back on the
 * screen.
 */
int kwl_font_use(const char *name);

/* The shm buffer the cells are painted into. */
typedef struct {
	pixman_image_t *img;
	/*
	 * The same memory, offset past the top rule — what the cell grid is
	 * painted into. Identical to `img` when there is no rule. See
	 * buffer_alloc() for why a second image rather than an origin.
	 */
	pixman_image_t *grid;
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

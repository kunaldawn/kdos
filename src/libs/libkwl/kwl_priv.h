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

#include "kwl.h"

/* Glyphs */
int kwl_font_load(const char *name);
void kwl_font_free(void);
int kwl_font_cell_w(void);
int kwl_font_cell_h(void);
int kwl_font_ascent(void);
const struct fcft_glyph *kwl_glyph(uint32_t cp);

/* Keys */
#include <xkbcommon/xkbcommon.h>
int kwl_keysym_to_ktui(xkb_keysym_t sym, struct xkb_state *state,
		       xkb_keycode_t code);

/* Paint */
void kwl_paint(pixman_image_t *dst, const KtuiCell *cur, KtuiCell *prev, int w,
	       int h, int full);

/* The shm buffer the cells are painted into. */
typedef struct {
	pixman_image_t *img;
	void *data;
	int w, h;		/* pixels */
	size_t size;
	struct wl_buffer *wl;
	bool busy;		/* held by the compositor until release */
} KwlBuffer;

#endif /* KWL_PRIV_H */

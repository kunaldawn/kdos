/* libkkms — shared between the seat/mode half and the input half. */

#ifndef KKMS_PRIV_H
#define KKMS_PRIV_H

#include <stdint.h>

/*
 * The real headers, not hand-written forward declarations. pixman_image_t is a
 * UNION and drmModeRes is a typedef of an anonymous struct; declaring either by
 * hand compiles until it meets the real one and then contradicts it.
 */
#include <libinput.h>
#include <libseat.h>
#include <pixman.h>
#include <xf86drmMode.h>
#include <xkbcommon/xkbcommon.h>

#include "ktui.h"

struct kkms {
	struct libseat *seat;
	int active;

	int drm_fd, drm_dev;
	drmModeRes *res;
	uint32_t connector, crtc, fb, handle;
	uint32_t stride;
	uint64_t size;
	drmModeModeInfo mode;
	int width, height;
	void *pixels;
	pixman_image_t *image;
	int force_full;

	struct libinput *li;
	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *state;

	/* The pointer, in cells, and the queue the backend drains. */
	int ptr_x, ptr_y;
	double ptr_px, ptr_py;
	/* Set by the first motion. Nothing is drawn before it: a machine with no
	 * pointing device would otherwise wear an arrow in its top-left corner
	 * for the life of the session. */
	int ptr_seen;
	KtuiEvent q[64];
	int qhead, qtail;
};

extern struct kkms K;

int kkms_input_init(void);
void kkms_input_shutdown(void);
void kkms_input_pump(void);
int kkms_input_fd(void);
int kkms_poll_event(KtuiEvent *ev, int timeout_ms);

#endif /* KKMS_PRIV_H */

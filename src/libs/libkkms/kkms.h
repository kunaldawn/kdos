/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkkms — the cell grid, on a screen, with no compositor under it
 *
 * libktui's fourth backend. The other three are escape sequences, an offscreen
 * buffer, and a Wayland surface; this one is a DRM device, a mode, and a
 * framebuffer it paints into itself.
 *
 * IT IS THE ONE PLACE ON THE CONSOLE PATH THAT NEEDS A GPU DEVICE, and it is a
 * separate archive for exactly that reason: kdos-con links none of it, so a
 * session still comes up on a machine whose driver does not. Only the view
 * links this.
 *
 * THE SEAT OWNS THE DEVICES. Every descriptor comes from libseat, so a VT
 * switch takes them away and gives them back rather than this fighting the
 * compositor for DRM master — which is the arrangement wlroots already proved
 * on this machine.
 * ---------------------------------
 */

#ifndef KKMS_H
#define KKMS_H

#include "ktui.h"

/*
 * Take a screen. `font` is a fontconfig name or NULL for the default; `seat`
 * is a seat name or NULL for $XDG_SEAT and then seat0.
 *
 * Returns 0 and installs itself as libktui's backend, or -1 having installed
 * nothing — a caller that cannot draw should say so and fall back, not run
 * blind. There is no partial success: a device that opened but has no
 * connected output is a failure here, because there is nothing to look at.
 */
int kkms_init(const char *seat, const char *font);
void kkms_shutdown(void);

/* Descriptors a caller polls beside its own: the seat and libinput. */
int kkms_seat_fd(void);
int kkms_input_fd(void);

/* Service both. Call whenever either is readable, and on a timeout — a seat
 * event can arrive with no input and a VT switch must not wait for a keypress
 * that cannot happen while the session is inactive. */
void kkms_pump(void);

/*
 * False while the session is switched away. Nothing is drawn then: the
 * devices are gone and the framebuffer belongs to whoever has the VT.
 */
int kkms_active(void);

/* Power the screen down (1) or back up (0). The mode is re-set on the way
 * back: a CRTC that was turned off has no mode to return to. */
void kkms_blank(int on);

#endif /* KKMS_H */

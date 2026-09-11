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
/*
 * `card` NAMES A DRM DEVICE, or NULL to sweep /dev/dri/card0..7 and take the
 * first with a connected output. The sweep is right on a machine with one
 * screen and wrong on one with a card that has none — and it is what makes a
 * virtual device untestable, because the rig's emulated card is always card0.
 */
int kkms_init(const char *seat, const char *card, const char *font);
void kkms_shutdown(void);

/*
 * LOAD A DIFFERENT FONT ON THE SCREEN THIS ALREADY HAS. 0 and the cell is the
 * new font's; -1 and the old font is still drawing.
 *
 * The grid is derived from the mode and the cell, so the caller must call
 * ktui_draw_resize() afterwards and tell whoever is composing for it — this
 * library knows the pixels and nothing about the session on top of them.
 *
 * kkms_font() is the name currently loaded, empty for the built-in default.
 */
int kkms_set_font(const char *font);
const char *kkms_font(void);

/*
 * ── more than one screen ────────────────────────────────────────────────
 *
 * EVERY CONNECTED CONNECTOR IS LIT, and the grid libktui is told about is all
 * of them laid edge to edge from the left in connector order. A window dragged
 * past the right edge of one screen is on the next because there was never a
 * boundary in the grid to stop at — the cut into screens happens at the paint,
 * below everything that knows what a window is.
 *
 * `kkms_outputs()` is how many; `kkms_output()` fills `out` for one and
 * returns 1, or returns 0 past the end. The columns are that output's slice of
 * the shared grid, which is what a surface listing screens shows a person.
 */
typedef struct {
	int width, height;	/* this output's mode, in pixels           */
	int col, cols, rows;	/* its slice of the shared grid, in cells  */
	unsigned connector;	/* the DRM connector id, for a name        */
} KkmsOutput;

int kkms_outputs(void);
int kkms_output(int i, KkmsOutput *out);

/*
 * A SCREEN PLUGGED IN OR PULLED OUT, on a monitor of this library's own —
 * libinput's udev context watches `input` and nothing else, so nothing here
 * would otherwise ever hear about `drm`.
 *
 * `kkms_hotplug_fd()` is a descriptor a caller adds to its poll, or -1 where
 * there is no monitor. `kkms_hotplug_pump()` drains it and returns 1 when the
 * GRID MOVED, which the caller announces exactly as it announces a resize —
 * because that is the same event to everything above this line.
 */
int kkms_hotplug_fd(void);
int kkms_hotplug_pump(void);

/*
 * WHICH STEP FAILED, valid after kkms_init() returns -1 and until the next
 * call to it. Eight steps share one return value — a missing driver, a seat
 * that never went active, a monitor that is not plugged in, a font that would
 * not load, a modeset the driver rejected — and a supervisor that cannot tell
 * them apart cannot tell a person which one to fix. Never NULL.
 */
const char *kkms_reason(void);

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

/*
 * SWITCH TO ANOTHER VIRTUAL TERMINAL, 1-based, as Ctrl+Alt+F<n> does.
 *
 * It belongs here because the seat is here. Once libseat puts this VT into
 * graphics mode the kernel's own chord stops working, so a desktop that does
 * not offer the switch itself takes away the recovery console `/etc/inittab`
 * exists to guarantee. The session must not gain this: it links no device code
 * and comes up on a machine whose GPU driver does not.
 *
 * Returns 0 when the seat accepted the request. The switch itself is
 * asynchronous — the disable arrives on the seat's own descriptor.
 */
int kkms_switch_vt(int n);

#endif /* KKMS_H */

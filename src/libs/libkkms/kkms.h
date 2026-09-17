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
 * ── how the screen is driven ─────────────────────────────────────────────
 *
 * The three answers that are a POLICY rather than a fact about the device, so
 * the machine standing in front of a person decides them and this library does
 * not. Passed to kkms_init() rather than set afterwards: every one of them is
 * read again at a hotplug and a mode change, and a setter would have an
 * ordering rule that a caller can get wrong exactly once.
 *
 * `buffers` is a CEILING of 1 to 3 on the scanout buffers per screen, 0 for
 * the default of 3. Three is one on the screen, one a flip is waiting on and
 * one the painter may compose into meanwhile; two stalls the painter at the
 * vblank, which behind a vsync-locked flip is the 60-to-30 cliff. A driver
 * with no memory for the third gets two and one with none for the second gets
 * one, so this never costs a display.
 *
 * `mode` is KKMS_MODE_PREFERRED or KKMS_MODE_FASTEST — the monitor's own
 * choice, or the highest refresh among the modes at the size the monitor
 * chose. PREFERRED is the default because the EDID's preferred mode is the one
 * the panel certifies; a 144 Hz mode at the same size is a different link rate
 * and this library is the one component whose mistake leaves no screen to fix
 * it from. FASTEST never changes the RESOLUTION, only the refresh at it.
 *
 * `tearing` presents each frame the moment it is composed instead of at the
 * vblank, which removes up to a refresh period of latency and TEARS — the
 * raster is inside the buffer when the CRTC is pointed at the next one, so a
 * moving edge is cut across the screen. Off by default, and silently off on a
 * device that does not publish DRM_CAP_ASYNC_PAGE_FLIP.
 */
#define KKMS_MODE_PREFERRED 0
#define KKMS_MODE_FASTEST   1

typedef struct {
	int buffers;		/* 1..3, 0 for the default                 */
	int mode;		/* KKMS_MODE_*                             */
	int tearing;		/* present unlocked; costs a torn frame    */
} KkmsTune;

/*
 * ── how a pointing device behaves ────────────────────────────────────────
 *
 * libinput ships a default per device class and every one of these overrides
 * it for every device that accepts it. A device that does not — a mouse asked
 * about tap-to-click — is left alone rather than treated as a failure: the
 * policy is one answer for the seat, and the seat is a mixture.
 *
 * KKMS_IN_KEEP in any field is "whatever the device came with", which is NOT
 * the same as 0: tap-to-click off and tap-to-click unset differ on a touchpad
 * whose driver enables it.
 *
 * `speed` is libinput's acceleration, -10 to 10, where 0 is the middle of the
 * device's own range and not "no acceleration". The scale is a tenth of
 * libinput's -1.0..1.0 so that a configuration file and a slider can both be
 * whole numbers.
 *
 * `natural` scrolls the content with the fingers instead of the view.
 * `tap` is tap-to-click and `tap_drag` is the drag that a second tap begins.
 * `dwt` suppresses the touchpad while the keyboard is being typed on.
 * `left_handed` swaps the two main buttons.
 * `middle_emulate` makes both buttons pressed together the middle one, which
 * is the only middle button a two-button trackpad has.
 */
#define KKMS_IN_KEEP (-128)

typedef struct {
	int speed;		/* -10..10, or KKMS_IN_KEEP                */
	int natural;		/* 0, 1, or KKMS_IN_KEEP                   */
	int tap;		/* 0, 1, or KKMS_IN_KEEP                   */
	int tap_drag;		/* 0, 1, or KKMS_IN_KEEP                   */
	int dwt;		/* 0, 1, or KKMS_IN_KEEP                   */
	int left_handed;	/* 0, 1, or KKMS_IN_KEEP                   */
	int middle_emulate;	/* 0, 1, or KKMS_IN_KEEP                   */
} KkmsInput;

/*
 * Set the policy above and apply it to every device already open. Devices
 * that arrive later are configured as they arrive, so this is called once
 * with the session's answer and never polled.
 *
 * `in` NULL restores KKMS_IN_KEEP everywhere, which stops this library
 * touching device configuration at all; it does not put back a value a
 * previous call overrode, because libinput has no record of what the device
 * came with once it has been set.
 */
void kkms_set_input(const KkmsInput *in);

/*
 * Take a screen. `font` is a fontconfig name or NULL for the default; `seat`
 * is a seat name or NULL for $XDG_SEAT and then seat0; `tune` is the policy
 * above or NULL for all of its defaults.
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
int kkms_init(const char *seat, const char *card, const char *font,
	      const KkmsTune *tune);
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
	/*
	 * WHAT THIS SCREEN IS ACTUALLY REFRESHING AT, in millihertz, computed
	 * from the timings in force and not read from the rounded `vrefresh`.
	 * It is what a session paces itself to: a frame budget compiled in as
	 * a constant is right on one panel and wrong on every other, and the
	 * number is here because this is the only place the mode is.
	 */
	int refresh;		/* mHz, 0 where the timings do not give one */
	int col, cols, rows;	/* its slice of the shared grid, in cells  */
	unsigned connector;	/* the DRM connector id                    */
	/* WHAT A PERSON CALLS THIS SCREEN — `HDMI-A-1`, `eDP-1`. A connector
	 * id is a kernel object number and means nothing to somebody choosing
	 * between two monitors. */
	char name[32];
	int nmodes, cur_mode;
} KkmsOutput;

/*
 * THE MODES A SCREEN PUBLISHED, and which of them it is wearing.
 *
 * `kkms_modes()` is how many the nth output has; `kkms_mode()` fills `m` and
 * returns 1, or 0 past either end; `kkms_mode_current()` is the index in
 * force, which is the monitor's PREFERRED one until somebody chooses.
 *
 * The refresh is in millihertz, so 59.94 Hz is not reported as 59.
 */
typedef struct {
	int width, height;
	int refresh;		/* mHz                                     */
	int preferred;		/* the monitor's own choice                */
} KkmsMode;

int kkms_outputs(void);
int kkms_output(int i, KkmsOutput *out);
int kkms_modes(int out);
int kkms_mode(int out, int i, KkmsMode *m);
int kkms_mode_current(int out);

/*
 * THE RATE A SESSION SHOULD PACE ITSELF AT, in millihertz: the HIGHEST refresh
 * among the screens that are lit, and 0 when none is.
 *
 * The highest and not an average, because one grid is cut across every screen:
 * a frame slow enough for the 60 Hz panel is a frame the 144 Hz one shows
 * twice, and the cost of the other direction is a frame the slower screen
 * never scans out. A caller divides it into its own budget — 1000000000 /
 * refresh is the period in microseconds — rather than compiling a constant in,
 * which is right on one panel and wrong on every other.
 *
 * It moves when a monitor is plugged in or a mode is chosen, so it is read
 * again wherever kkms_hotplug_pump() or kkms_set_mode() reports a change, not
 * once at startup.
 */
int kkms_refresh_mhz(void);

/*
 * WEAR THE NTH MODE ON THE NTH SCREEN. 0 and it is lit; -1 and the old mode is
 * still on the screen.
 *
 * ONE OUTPUT AND THE MODE ALONE. Everything else a display tool offers has a
 * home already here: the scale is the FONT, the position is connector ORDER,
 * and off is kkms_blank(). What is left is the mode.
 *
 * EVERY OUTPUT IS RECUT, not only this one: screens are laid edge to edge, so
 * a wider mode moves every column after it and the slices behind it are the
 * wrong width.
 *
 * The grid is derived from the mode and the cell, so the caller calls
 * ktui_draw_resize() and announces the new grid — the rule kkms_set_font()
 * already keeps. THE OLD MODE COMES BACK IF THE NEW ONE WILL NOT SET: a screen
 * is the one thing a person cannot work around from somewhere else.
 */
int kkms_set_mode(int out, int i);

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

/* Descriptors a caller polls beside its own: the seat, libinput, and the DRM
 * device — a page flip completes on the last of those, and a caller that does
 * not poll it waits a whole frame for the next timeout instead. */
int kkms_seat_fd(void);
int kkms_input_fd(void);
int kkms_drm_fd(void);

/*
 * Whether a frame may be painted now: false while any screen has no buffer the
 * kernel is not reading. A caller that paints anyway would be writing into the
 * buffer the screen is showing or is about to show.
 *
 * WITH A THIRD BUFFER THIS IS TRUE WHILE A FLIP IS STILL IN FLIGHT, which is
 * the whole of what the third buffer buys: the next frame is composed during
 * the wait for the vblank instead of after it, and it is presented by the
 * completion itself rather than by the caller's next visit. With two buffers
 * it is false until the flip retires, and a compose longer than a refresh
 * period then costs a whole further period.
 *
 * Reading it is how the console's view paces itself to the refresh rate: the
 * DRM descriptor becomes readable at the vblank, kkms_pump() reaps the flip,
 * and the next frame goes out then rather than on a timer.
 */
int kkms_ready(void);

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
 * back: a CRTC that was turned off has no mode to return to. Nothing is
 * painted while it is down, and waking repaints every screen whole.
 *
 * The state is remembered even when the call lands while the session is
 * switched away, where the device cannot be programmed: coming back puts the
 * screen in whichever state the last call named. So one call per transition
 * is enough and the caller never has to re-send. */
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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkdisp — which display server, decided in one place
 *
 * A KDOS surface reaches a screen three ways: as a Wayland client under
 * kdos-comp, as a cell client under kdos-con, or through escape sequences on a
 * terminal. kdos-shell alone opens a surface from more than twenty places, and
 * each of them then asks whether it should close, resizes itself, or hides its
 * panel. Branching on the server at every one of those is the same decision
 * written twenty times in one program and again in the next — which is exactly
 * what the shared window model was introduced to stop.
 *
 * So the LIFECYCLE is an interface and the servers are implementations of it.
 *
 * THE CONSUMER DECIDES WHAT IT LINKS. This library names no implementation and
 * pulls in none; a caller hands over the ones it compiled, in preference order,
 * and a console-only program never sees Wayland:
 *
 *     extern const KDispImpl kwl_impl;   // libkwl
 *     extern const KDispImpl kcon_impl;  // libkcon
 *     static const KDispImpl *const have[] = { &kcon_impl, &kwl_impl };
 *     kdisp_init(&cfg, have, 2);
 *
 * LINKS libktui AND NOTHING ELSE, so gaining it costs a surface a vtable and a
 * struct rather than a font renderer.
 *
 * NOTE THE TWO EDGE VOCABULARIES IN THIS TREE AND DO NOT CONFLATE THEM.
 * KDISP_EDGE_* below is a SEQUENCE naming which edge a panel is anchored to.
 * libkwm's KWM_EDGE_* is a BITMASK whose values match the compositor's own
 * enum, so that corners are combinations. They are different questions.
 * ---------------------------------
 */

#ifndef KDISP_H
#define KDISP_H

#include <stdbool.h>
#include <stddef.h>

#include "ktui.h"

typedef union pixman_image pixman_image_t;

enum kdisp_role {
	KDISP_ROLE_TOPLEVEL = 0,	/* an ordinary window (xdg-shell)          */
	/*
	 * Connect, bind the globals, install NO backend and create no surface.
	 * This is what `--dump` runs under: the caller wants the protocol state
	 * a panel would draw from, rendered offscreen as text rather than into
	 * a buffer nobody will look at.
	 */
	KDISP_ROLE_NONE,
	KDISP_ROLE_PANEL,		/* layer-shell, anchored, exclusive zone   */
	KDISP_ROLE_OVERLAY,	/* layer-shell, no exclusive zone          */
	/*
	 * The desktop itself: the BACKGROUND layer, anchored on all four edges,
	 * no exclusive zone. Above the wallpaper the compositor draws and below
	 * every window. Reserving space for it would shrink the usable box and
	 * every maximised window with it, which is why it is its own role
	 * rather than a panel with the zone turned off.
	 */
	KDISP_ROLE_BACKGROUND,
	/*
	 * ext-session-lock-v1: a surface the compositor keeps on screen even if
	 * this process dies, which is the entire reason a lock screen is not
	 * just a fullscreen window.
	 *
	 * The protocol wants ONE surface PER OUTPUT and will not report the
	 * session locked until every output has one. libktui has a single cell
	 * buffer, so the prompt is drawn on the first output and every other
	 * one is filled with the theme background — covered, and honest about
	 * it. Multi-output cell drawing is a libktui limitation, not a
	 * protocol one.
	 */
	KDISP_ROLE_LOCK,
	/*
	 * A screensaver: the whole screen, above everything, taking nothing.
	 *
	 * It is not an OVERLAY with a size. An overlay is centred and sized in
	 * cells by the client, and a client cannot see the output — one that
	 * measured the screen itself would leave a strip of desktop showing
	 * wherever the arithmetic rounded down. THE DISPLAY DECIDES THE SIZE
	 * AND SAYS SO, exactly as it does for BACKGROUND.
	 *
	 * It is not a LOCK either, and the difference is the whole safety
	 * story: this surface takes NO keyboard and claims NO pointer region,
	 * so every keystroke and every click goes to whatever is underneath and
	 * the display's own idle policy sees the activity. A saver that
	 * swallowed input would be a lock screen with no password.
	 */
	KDISP_ROLE_SAVER,
};

enum kdisp_edge {
	KDISP_EDGE_TOP = 0,
	KDISP_EDGE_BOTTOM,
	KDISP_EDGE_LEFT,
	KDISP_EDGE_RIGHT,
};

enum kdisp_corner {
	KDISP_CORNER_CENTER = 0,	/* the launcher: what you are looking at   */
	KDISP_CORNER_TOP_RIGHT,	/* a toast: what you are not looking at    */
	/*
	 * A dropdown, under the word on the menu bar that opened it. Together
	 * with margin_x/margin_y this is as close to a coordinate as
	 * layer-shell gets: the protocol has no positions, only anchors and
	 * margins, so "at x" is "anchored left, with a left margin of x".
	 * Without it every menu opened in the CENTRE of the screen, which
	 * reads as a dialog rather than as a menu belonging to the word that
	 * was clicked.
	 */
	KDISP_CORNER_TOP_LEFT,
	/*
	 * The same, measured from the bottom — what a menu belonging to a bar
	 * on the BOTTOM edge needs. A client cannot express this by anchoring
	 * TOP with a computed margin: it does not know the output's pixel
	 * height, so it cannot say where "just above the taskbar" is. margin_y
	 * is the gap from the bottom edge (the panel's own height), margin_x
	 * the offset from the left.
	 */
	KDISP_CORNER_BOTTOM_LEFT,
	/*
	 * A bezel: the OSD's place, where every desktop has put the volume
	 * overlay since the laptop grew media keys. Anchored to the bottom
	 * edge ONLY — anchoring left and right as well would stretch the
	 * surface across the output, so the horizontal centring is the
	 * compositor's own for an unanchored axis. margin_y is the gap from
	 * the bottom; margin_x is meaningless here and ignored.
	 */
	KDISP_CORNER_BOTTOM_CENTER,
};

typedef struct {
	enum kdisp_role role;
	enum kdisp_edge edge;	/* panels only                             */
	int cells;		/* panel thickness in CELLS, not pixels    */
	const char *title;	/* toplevel only                           */
	const char *app_id;	/* must equal the .desktop id — `kdos appid` */
	const char *font;	/* fontconfig name; NULL for the default   */
	/*
	 * Which screen, by the compositor's own name for it (`eDP-1`,
	 * `HDMI-A-1`). NULL leaves the choice to the compositor, and what a
	 * compositor chooses is exactly one output — so a panel with no
	 * `output` on a two-monitor machine is a panel on one of them and
	 * nothing on the other. Ignored for roles that are not layer-shell.
	 *
	 * An unknown name is not an error: it falls back to the compositor's
	 * choice, because a screen that was unplugged between the supervisor
	 * deciding and this process starting is a race, not a mistake.
	 */
	const char *output;
	int exclusive;		/* reserve the zone so windows do not overlap */
	/*
	 * Overlay only: the size in CELLS, because a launcher is a grid of text
	 * and its natural unit is rows of results, not pixels.
	 */
	int cols, rows;
	/*
	 * Overlay only: where it sits. Centre is right for a launcher, which is
	 * what the user is looking at; it is wrong for a toast, which must not
	 * cover the middle of the screen for as long as it is up. Anything
	 * non-zero also gets a margin, because a notification flush against the
	 * screen edge reads as a rendering fault.
	 */
	int corner;
	/*
	 * Overlay only, PIXELS, and only meaningful with a corner: the gap
	 * from the two edges the corner anchors to. Zero means the library's
	 * own margin, which is what a toast wants.
	 */
	int margin_x, margin_y;
	/*
	 * Take the keyboard. A panel must NOT — it would steal focus from
	 * whatever you were typing into every time the clock redrew — but a
	 * launcher is useless without it, and layer-shell surfaces get no
	 * keyboard at all unless they ask.
	 */
	int keyboard;
	/*
	 * Close when the keyboard focus goes elsewhere. Right for a MENU and
	 * for the launcher and the run box — clicking on a window while one is
	 * open used to leave it floating over that window until somebody found
	 * Escape, and there is no useful "unfocused menu" state.
	 *
	 * WRONG for a dialog, which is why it is opt-in rather than implied by
	 * `keyboard`. The file chooser is the case: it is what every boxed
	 * application's Open reaches through the portal, people click back to
	 * the application mid-choice as a matter of course, and a picker that
	 * vanished when they did would answer the portal "cancelled" for a
	 * dialog the user had not finished with. Same for the yes/no prompt.
	 */
	int dismiss_on_unfocus;
	/*
	 * A RULE ALONG THE SURFACE'S TOP EDGE, in logical pixels, drawn in
	 * `rule_slot` and outside the cell grid entirely.
	 *
	 * The panel is the reason. Every other surface on this desktop puts a
	 * double-line box round itself and reads as a framed thing; the bar at
	 * the bottom of the screen had no edge at all, so on a dark wallpaper
	 * it read as a region of the desktop rather than as a piece of chrome.
	 * A box needs four sides and two spare rows, which on a two-row bar is
	 * the whole bar — but the top edge is the only one a bottom-anchored
	 * panel has, and it is worth three pixels rather than a row of cells.
	 *
	 * Outside the grid because a cell is 32 pixels tall and a rule is
	 * three: the grid starts BELOW it, the pointer's row is measured from
	 * below it, and the layer surface asks for those pixels on top of its
	 * cells. Zero — the default — is exactly what every other surface
	 * wants.
	 */
	int rule;
	int rule_slot;

	/*
	 * The panel's body opacity, in PERCENT. 0 means unset and is treated
	 * as 100 — a surface that said nothing gets the opaque behaviour it
	 * has always had, and every consumer but the panel says nothing.
	 *
	 * Below 100 this clears KT_SURFACE's alpha in libkcell and forces an
	 * alpha-capable buffer format, so the wallpaper and the windows show
	 * through the bar's own background while its text and its fills stay
	 * ink. It is the BACKGROUND slot only: a translucent glyph is a glyph
	 * nobody can read, which is the whole reason this is per-slot rather
	 * than a multiplier on the surface.
	 */
	int opacity;
} KDispConfig;

typedef void (*KDispBackdropFn)(pixman_image_t *dst, int w, int h, int scale);

enum kdisp_cursor {
	KDISP_CUR_DEFAULT = 0,	/* the arrow                               */
	KDISP_CUR_TEXT,		/* an I-beam: over a text field            */
	KDISP_CUR_POINTER,	/* a hand: over something clickable        */
	KDISP_CUR_PROGRESS,	/* working, but still interactive          */
};

/* ────────────────────────────────────────────────────────────────────────
 * The interface
 *
 * Every entry is something a surface already asks libkwl today. A server that
 * cannot answer one leaves it NULL, and the forwarder below returns the
 * neutral answer rather than crashing — a console has no server-side
 * decoration to report and no Wayland handle to hand out.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	const char *name;

	/* Is the server this implements actually reachable? Cheap, and with no
	 * side effects: it is called on implementations that will not be used. */
	int (*probe)(void);

	int (*init)(const KDispConfig *cfg);
	void (*shutdown)(void);

	int (*should_close)(void);
	int (*fd)(void);
	void (*pump)(void);

	int (*overlay_resize)(int cols, int rows);
	int (*overlay_show)(int cols, int rows);
	void (*overlay_hide)(void);
	void (*layer_autohide)(bool hidden);

	int (*copy)(const char *text, size_t len, int primary);

	/* Begin a drag carrying `data`. The payload is COPIED: the drag
	 * outlives the frame that started it, and a caller's buffer does not. */
	int (*drag_start)(const char *mime, const char *data, size_t len);

	int (*cell_w)(void);
	int (*cell_h)(void);
	int (*px_h)(void);
	int (*scale)(void);
	int (*decorated)(void);
	int (*popup_offset)(void);
	int (*edge_bottom)(void);

	void (*cursor_set)(enum kdisp_cursor c);
	void (*set_backdrop)(KDispBackdropFn fn);
	void (*input_cells)(const KRect *rects, int n);
	void (*report_error)(void);

	int (*lock_engaged)(void);
	int (*lock_finished)(void);
	void (*unlock)(void);
} KDispImpl;

/*
 * Pick the first implementation whose probe succeeds, in the order given, and
 * initialise it. Returns 0 on success, -1 when none of them can draw — a
 * caller that cannot draw should say so and exit rather than run blind.
 *
 * Passing n == 0 selects nothing and leaves libktui on its built-in terminal
 * backend, which is what a --tty flag means.
 */
int kdisp_init(const KDispConfig *cfg, const KDispImpl *const *impls, int n);

/* Which one was chosen, or NULL. For a program that wants to say so. */
const KDispImpl *kdisp_current(void);

void kdisp_shutdown(void);
int kdisp_should_close(void);
int kdisp_fd(void);
void kdisp_pump(void);
int kdisp_overlay_resize(int cols, int rows);
int kdisp_overlay_show(int cols, int rows);
void kdisp_overlay_hide(void);
void kdisp_layer_autohide(bool hidden);
int kdisp_copy(const char *text, size_t len, int primary);
int kdisp_drag_start(const char *mime, const char *data, size_t len);
int kdisp_cell_w(void);
int kdisp_cell_h(void);
int kdisp_px_h(void);
int kdisp_scale(void);
int kdisp_decorated(void);
int kdisp_popup_offset(void);
int kdisp_edge_bottom(void);
void kdisp_cursor_set(enum kdisp_cursor c);
void kdisp_set_backdrop(KDispBackdropFn fn);
void kdisp_input_cells(const KRect *rects, int n);
void kdisp_report_error(void);
int kdisp_lock_engaged(void);
int kdisp_lock_finished(void);
void kdisp_unlock(void);

#endif /* KDISP_H */

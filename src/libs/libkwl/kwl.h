/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwl — libktui's Wayland backend
 *
 * The same cell grid libktui already draws, painted into a wl_shm buffer with
 * fcft instead of written to a terminal as escapes. Every widget, every glyph
 * tier, every layout in libktui works here unchanged, because none of them ever
 * knew where the cells went.
 *
 * WHY THIS IS A SEPARATE ARCHIVE. libktui links nothing but musl and must keep
 * doing so: kinstall links it in PHASE 1, before any library exists to link
 * against. This one links wayland-client, fcft, pixman and xkbcommon. Keeping
 * them apart is what stops kinstall being dragged to phase 4 — so if you are
 * about to add a `-l` to libktui to save a file here, that is the trade you are
 * actually making.
 *
 * Surface roles, because a shell needs both and neither is xdg-shell: a panel
 * is a layer-shell surface with an exclusive zone, and a lock screen is an
 * ext-session-lock-v1 surface. A lock that used xdg-shell would be a window the
 * compositor could be persuaded to close.
 * ---------------------------------
 */

#ifndef KWL_H
#define KWL_H

#include <stdbool.h>

#include "ktui.h"

enum kwl_role {
	KWL_ROLE_TOPLEVEL = 0,	/* an ordinary window (xdg-shell)          */
	/*
	 * Connect, bind the globals, install NO backend and create no surface.
	 * This is what `--dump` runs under: the caller wants the protocol state
	 * a panel would draw from, rendered offscreen as text rather than into
	 * a buffer nobody will look at.
	 */
	KWL_ROLE_NONE,
	KWL_ROLE_PANEL,		/* layer-shell, anchored, exclusive zone   */
	KWL_ROLE_OVERLAY,	/* layer-shell, no exclusive zone          */
	/*
	 * The desktop itself: the BACKGROUND layer, anchored on all four edges,
	 * no exclusive zone. Above the wallpaper the compositor draws and below
	 * every window. Reserving space for it would shrink the usable box and
	 * every maximised window with it, which is why it is its own role
	 * rather than a panel with the zone turned off.
	 */
	KWL_ROLE_BACKGROUND,
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
	KWL_ROLE_LOCK,
};

enum kwl_edge {
	KWL_EDGE_TOP = 0,
	KWL_EDGE_BOTTOM,
	KWL_EDGE_LEFT,
	KWL_EDGE_RIGHT,
};

enum kwl_corner {
	KWL_CORNER_CENTER = 0,	/* the launcher: what you are looking at   */
	KWL_CORNER_TOP_RIGHT,	/* a toast: what you are not looking at    */
	/*
	 * A dropdown, under the word on the menu bar that opened it. Together
	 * with margin_x/margin_y this is as close to a coordinate as
	 * layer-shell gets: the protocol has no positions, only anchors and
	 * margins, so "at x" is "anchored left, with a left margin of x".
	 * Without it every menu opened in the CENTRE of the screen, which
	 * reads as a dialog rather than as a menu belonging to the word that
	 * was clicked.
	 */
	KWL_CORNER_TOP_LEFT,
	/*
	 * The same, measured from the bottom — what a menu belonging to a bar
	 * on the BOTTOM edge needs. A client cannot express this by anchoring
	 * TOP with a computed margin: it does not know the output's pixel
	 * height, so it cannot say where "just above the taskbar" is. margin_y
	 * is the gap from the bottom edge (the panel's own height), margin_x
	 * the offset from the left.
	 */
	KWL_CORNER_BOTTOM_LEFT,
	/*
	 * A bezel: the OSD's place, where every desktop has put the volume
	 * overlay since the laptop grew media keys. Anchored to the bottom
	 * edge ONLY — anchoring left and right as well would stretch the
	 * surface across the output, so the horizontal centring is the
	 * compositor's own for an unanchored axis. margin_y is the gap from
	 * the bottom; margin_x is meaningless here and ignored.
	 */
	KWL_CORNER_BOTTOM_CENTER,
};

typedef struct {
	enum kwl_role role;
	enum kwl_edge edge;	/* panels only                             */
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
} KwlConfig;

/*
 * Connect, create the surface, and install this as libktui's backend. After
 * this returns 0, every ktui_* drawing call paints here.
 *
 * Returns -1 with nothing installed if there is no compositor, no font, or no
 * layer-shell when one was asked for — a caller that cannot draw should say so
 * and exit, not run blind.
 */
int kwl_init(const KwlConfig *cfg);
void kwl_shutdown(void);
/*
 * Overlay only: ask for a new size in cells. A toast stack is the case this
 * exists for — its surface is opaque for its whole height, so a daemon sized
 * once for the MAXIMUM number of toasts paints an empty dark rectangle on the
 * desktop for the whole session. The compositor answers with a configure and
 * the cell buffer follows; the caller redraws on the next frame.
 */
void kwl_overlay_resize(int cols, int rows);

/*
 * Panel only: collapse to a one-cell strip with no exclusive zone, and back.
 *
 * Autohide is a PANEL property rather than a drawing one because the half that
 * matters is the exclusive zone — a panel that merely painted itself away
 * would still hold its strip of the screen against every maximised window.
 * Hidden the surface stays where it is, one cell thick, above the windows and
 * still answering the pointer, which is what makes hovering the edge able to
 * bring it back.
 *
 * The call blocks for one protocol roundtrip: the configure answering the
 * resize must be in hand before anything is painted, or the buffer committed
 * against it is the wrong size. It sets `ktui_resized`, so the caller's normal
 * resize path redraws.
 */
void kwl_layer_autohide(bool hidden);

/*
 * Overlay only: destroy the layer surface while idle and recreate it on
 * demand. The alternative — a NULL-buffer commit — resets the surface to
 * uninitialised in wlroots and the next toast is drawn nowhere; a one-cell
 * surface is the workaround this pair replaces. show() with a live surface
 * is just a resize; after a recreate it has completed the initial-commit
 * handshake and the surface is ready to draw on when it returns 0.
 */
void kwl_overlay_hide(void);
int kwl_overlay_show(int cols, int rows);

/*
 * COPY. Puts `text` on the clipboard (primary = 0) or the primary selection
 * (primary = 1), by creating a data source this client answers `send` on.
 *
 * Returns -1 when there is no manager for that selection, when the allocation
 * fails, and — the one worth knowing about — when this surface has never seen
 * an input event: set_selection presents the SERIAL of the event that
 * justified it, and a compositor refuses one it has never issued. That is the
 * protocol saying a background client may not take the clipboard, not a bug.
 *
 * The text is copied; the caller keeps its own. Sends are drained from
 * kwl_pump and never block the frame.
 */
int kwl_copy(const char *text, size_t len, int primary);

/* Cell metrics, once the font is loaded. A panel's pixel height is
 * cells * kwl_cell_h(). */
int kwl_cell_w(void);
int kwl_cell_h(void);
/* The surface's own height in LOGICAL pixels — the cell grid plus the rule.
 * What a panel passes as a popup's margin; `rows * cell_h` is short by the
 * rule. */
int kwl_px_h(void);
/*
 * 1 when the COMPOSITOR is drawing this window's frame, so the program must
 * not draw a second one round the outside of its own content. False on a
 * terminal, a panel and every popup — see kwl.c.
 */
int kwl_decorated(void);
/* The integer output scale in force. Anything a consumer rasterises for itself
 * has to be produced at cell * scale — libkicon is the caller. */
int kwl_scale(void);

/*
 * The pointer's shape over this surface, via cursor-shape-v1. Sticky: it is
 * re-sent on every pointer enter, so a consumer sets it when its hover target
 * changes, not per frame. A compositor without the protocol ignores it, which
 * leaves the arrow — the state every surface had before this existed.
 */
enum kwl_cursor {
	KWL_CUR_DEFAULT = 0,	/* the arrow                               */
	KWL_CUR_TEXT,		/* an I-beam: over a text field            */
	KWL_CUR_POINTER,	/* a hand: over something clickable        */
	KWL_CUR_PROGRESS,	/* working, but still interactive          */
};
void kwl_cursor_set(enum kwl_cursor c);

/*
 * Which cells of this surface answer the pointer at all.
 *
 * `n` rectangles in CELLS; n == 0 means the surface takes no pointer input and
 * every click falls through to whatever is behind it. Pass n < 0 to go back to
 * the default, which is the whole surface.
 *
 * THE DESKTOP IS WHY THIS EXISTS. kdos-desk covers the entire output, so with
 * the default region it ate every click on the root window — and the
 * compositor's own root-menu mousebind (right-press -> ShowMenu, menu.xml)
 * therefore never fired for as long as desktop icons were on, which is the
 * shipped default. The desk now claims only the cells its icons occupy, so a
 * click on bare wallpaper reaches the compositor exactly as it does with the
 * icons switched off.
 */
void kwl_input_cells(const KRect *rects, int n);

/*
 * The connection and the seat, for a consumer that needs to bind protocols of
 * its own — kdos-shell binds foreign-toplevel and ext-workspace on exactly this
 * display. Deliberately shared rather than opened a second time: two
 * wl_displays would be two clients, with two seats and two sets of globals,
 * which would then have to be kept agreeing with each other about what one
 * panel is showing. Both are NULL before kwl_init() succeeds.
 *
 * They are `void *` because kwl.h is included by files that do not otherwise
 * pull in wayland-client.h; cast at the use site.
 */
void *kwl_display(void);
void *kwl_seat(void);

/*
 * The connection's fd, and one turn of the protocol, for a consumer that has
 * OTHER fds to wait on — kdos-notifyd waits on the session bus at the same
 * time. A program with only the surface to service should use the backend's
 * poll_event instead; this pair exists so a second event source does not have
 * to be serviced by polling on a timer.
 */
int kwl_fd(void);
void kwl_pump(void);	/* dispatch what has arrived, flush what is queued */

/* Non-zero once the compositor or the user has asked the surface to go away. */
int kwl_should_close(void);

/*
 * Session lock only.
 *
 * `kwl_lock_engaged()` is non-zero once the COMPOSITOR has confirmed the
 * session is locked. A lock screen must not accept a password before that:
 * until `locked` arrives the screen may still be showing the session, and the
 * user would be typing into a machine that is not yet secured.
 *
 * `kwl_lock_finished()` is the compositor refusing the lock — it is already
 * locked by someone else, or the request came too late. A client that sees it
 * must exit WITHOUT unlocking anything.
 *
 * `kwl_unlock()` sends unlock_and_destroy and is the only way out. It must be
 * called before the process exits, or the compositor keeps the screen locked
 * exactly as if the client had crashed.
 */
int kwl_lock_engaged(void);
int kwl_lock_finished(void);
void kwl_unlock(void);

#endif /* KWL_H */

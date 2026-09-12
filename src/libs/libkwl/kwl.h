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
#include "kdisp.h"

/*
 * pixman_image_t without dragging <pixman.h> onto every consumer's include
 * path. It is a UNION, not a struct — declaring the wrong tag is a hard error
 * — and C11 allows a typedef to be repeated with the same type, so this and
 * pixman's own definition coexist in either order.
 */
union pixman_image;

/*
 * Connect, create the surface, and install this as libktui's backend. After
 * this returns 0, every ktui_* drawing call paints here.
 *
 * Returns -1 with nothing installed if there is no compositor, no font, or no
 * layer-shell when one was asked for — a caller that cannot draw should say so
 * and exit, not run blind.
 */
int kwl_init(const KDispConfig *cfg);

/* libkwl as a libkdisp implementation. A consumer hands the ADDRESS of this
 * to kdisp_init(); naming it is what links Wayland into that program, which
 * is why libkdisp itself never names it. */
extern const KDispImpl kwl_impl;
void kwl_shutdown(void);
/*
 * Overlay only: ask for a new size in cells. A toast stack is the case this
 * exists for — its surface is opaque for its whole height, so a daemon sized
 * once for the MAXIMUM number of toasts paints an empty dark rectangle on the
 * desktop for the whole session. The compositor answers with a configure and
 * the cell buffer follows; the caller redraws on the next frame.
 */
/*
 * Ask for a new size and WAIT for it. 0 when the surface really is that size,
 * -1 when the compositor said otherwise or said nothing — a caller that gets
 * -1 must draw at the size it already had, because a buffer that disagrees
 * with the last configure is a protocol error and those disconnect the client.
 */
int kwl_overlay_resize(int cols, int rows);

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

/* Begin a drag carrying `data` as `mime`. The payload is copied. Needs a
 * recent input event: the compositor checks the serial a drag starts from. */
int kwl_drag_start(const char *mime, const char *data, size_t len);

/* Cell metrics, once the font is loaded. A panel's pixel height is
 * cells * kwl_cell_h(). */
int kwl_cell_w(void);
int kwl_cell_h(void);
/* The surface's own height in LOGICAL pixels — the cell grid plus the rule.
 * What a panel passes as a popup's margin; `rows * cell_h` is short by the
 * rule. */
int kwl_px_h(void);

/*
 * The offset a popup of THIS panel passes as its own margin from the same
 * screen edge: the surface's height plus the panel's gap.
 *
 * Use this and never kwl_px_h() for that purpose. They are the same number
 * only while the bar is flush with the edge; with `margin_y` set they differ
 * by exactly the gap, and a popup placed with the height opens behind the bar
 * it belongs to.
 */
int kwl_popup_offset(void);
/* Which side of this surface faces away from the bar — see kwl.c. */
int kwl_edge_bottom(void);
/* Print whatever the display has gone wrong with, if anything — see kwl.c. */
void kwl_report_error(void);

/*
 * THE BACKDROP — pixel chrome painted UNDER the cell grid, every frame.
 *
 * Called with the surface's own pixman image just before the cells go down,
 * so a caller can lay a body, a plate or a one-pixel rule where a 10x20 cell
 * of one colour cannot. `w`/`h` are the buffer in real pixels and `scale` is
 * the output's integer scale: everything the callback draws is in pixels and
 * must be multiplied by it.
 *
 * Two consequences the caller does not get a choice about:
 *
 *  - Setting one forces a FULL cell repaint every frame. The row diff cannot
 *    know which cells the backdrop disturbed, and half a repaint over a fresh
 *    backdrop is a bar with last frame's text on it.
 *  - Any slot the caller cleared to alpha 0 is then LEFT ALONE by the cell
 *    painter rather than cleared, because the clear would erase what this
 *    just drew. Clear the slot the backdrop owns and no other.
 *
 * NULL removes it.
 */
void kwl_set_backdrop(KDispBackdropFn fn);
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
void kwl_cursor_set(enum kdisp_cursor c);

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

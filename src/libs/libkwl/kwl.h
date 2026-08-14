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
};

typedef struct {
	enum kwl_role role;
	enum kwl_edge edge;	/* panels only                             */
	int cells;		/* panel thickness in CELLS, not pixels    */
	const char *title;	/* toplevel only                           */
	const char *app_id;	/* must equal the .desktop id — `kdos appid` */
	const char *font;	/* fontconfig name; NULL for the default   */
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
	 * Take the keyboard. A panel must NOT — it would steal focus from
	 * whatever you were typing into every time the clock redrew — but a
	 * launcher is useless without it, and layer-shell surfaces get no
	 * keyboard at all unless they ask.
	 */
	int keyboard;
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
 * Overlay only: destroy the layer surface while idle and recreate it on
 * demand. The alternative — a NULL-buffer commit — resets the surface to
 * uninitialised in wlroots and the next toast is drawn nowhere; a one-cell
 * surface is the workaround this pair replaces. show() with a live surface
 * is just a resize; after a recreate it has completed the initial-commit
 * handshake and the surface is ready to draw on when it returns 0.
 */
void kwl_overlay_hide(void);
int kwl_overlay_show(int cols, int rows);

/* Cell metrics, once the font is loaded. A panel's pixel height is
 * cells * kwl_cell_h(). */
int kwl_cell_w(void);
int kwl_cell_h(void);

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

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
	KWL_ROLE_PANEL,		/* layer-shell, anchored, exclusive zone   */
	KWL_ROLE_OVERLAY,	/* layer-shell, no exclusive zone          */
};

enum kwl_edge {
	KWL_EDGE_TOP = 0,
	KWL_EDGE_BOTTOM,
	KWL_EDGE_LEFT,
	KWL_EDGE_RIGHT,
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

#endif /* KWL_H */

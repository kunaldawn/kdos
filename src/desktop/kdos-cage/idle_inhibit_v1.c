/*
 * Cage: A Wayland kiosk.
 *
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>

#include "embed.h"
#include "idle_inhibit_v1.h"
#include "server.h"
#include "view.h"

struct cg_idle_inhibitor_v1 {
	struct cg_server *server;

	/*
	 * THE WINDOW THE INHIBITOR IS FOR, or NULL where the surface is not a
	 * toplevel's. The parent's saver is per screen and its sleep is per
	 * window, so a player told to stop rendering has to be the one that
	 * asked the screen to stay awake.
	 */
	struct cg_view *view;

	struct wl_list link; // server::inhibitors
	struct wl_listener destroy;
};

/*
 * IS ANYTHING IN THIS WINDOW STILL PLAYING. A toolkit may hold several
 * inhibitors on one toplevel at once — a video and a presentation mode — and
 * the window stops holding the screen awake only when the last of them goes.
 */
static bool
inhibited_for(struct cg_server *server, struct cg_view *view)
{
	struct cg_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each (inhibitor, &server->inhibitors, link) {
		if (inhibitor->view == view) {
			return true;
		}
	}
	return false;
}

static void
idle_inhibit_v1_check_active(struct cg_server *server, struct cg_view *view)
{
	/* Due to Cage's unique window management, we don't need to
	   check for visibility. In the worst cage, the inhibitor is
	   spawned by a dialog that _may_ be obscured by another
	   dialog, but this is really an edge case that, until
	   reported, does not warrant the additional complexity.
	   Hence, we simply check for any inhibitors and inhibit
	   accordingly. */
	bool inhibited = !wl_list_empty(&server->inhibitors);
	wlr_idle_notifier_v1_set_inhibited(server->idle, inhibited);

	/*
	 * AND THE PARENT, WHICH IS WHERE THE IDLE POLICY LIVES. The notifier
	 * above is this process's own and this process has no saver, no lock
	 * and no DPMS clock — they belong to the desktop this window sits on.
	 * Without the message a video in a boxed player is covered by the saver
	 * five minutes in, with nothing the application can do about it.
	 */
	if (embed_active(server) && view) {
		embed_set_inhibit(view, inhibited_for(server, view));
	}
}

void
idle_inhibit_forget_view(struct cg_server *server, struct cg_view *view)
{
	struct cg_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each (inhibitor, &server->inhibitors, link) {
		if (inhibitor->view == view) {
			inhibitor->view = NULL;
		}
	}
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct cg_idle_inhibitor_v1 *inhibitor = wl_container_of(listener, inhibitor, destroy);
	struct cg_server *server = inhibitor->server;
	struct cg_view *view = inhibitor->view;

	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	free(inhibitor);

	idle_inhibit_v1_check_active(server, view);
}

void
handle_idle_inhibitor_v1_new(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_idle_inhibitor_v1);
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;

	struct cg_idle_inhibitor_v1 *inhibitor = calloc(1, sizeof(struct cg_idle_inhibitor_v1));
	if (!inhibitor) {
		return;
	}

	inhibitor->server = server;
	/*
	 * A SUBSURFACE IS NOT A WINDOW, and a toolkit that hangs its inhibitor
	 * on the video widget rather than on the toplevel is the ordinary case.
	 * The root surface is the toplevel's; where even that is not a view the
	 * front window answers for it, because a player holding a screen awake
	 * against no window at all holds it awake nowhere.
	 */
	if (wlr_inhibitor->surface) {
		inhibitor->view = wlr_surface_get_root_surface(wlr_inhibitor->surface)->data;
	}
	if (!inhibitor->view) {
		inhibitor->view = embed_view_from_win(server, 0);
	}
	wl_list_insert(&server->inhibitors, &inhibitor->link);

	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	idle_inhibit_v1_check_active(server, inhibitor->view);
}

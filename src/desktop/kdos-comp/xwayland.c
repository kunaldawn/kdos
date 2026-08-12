/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   X11 windows, rootless
 *
 * This closes a TODO the project carried for months: cosmic-comp never
 * started Xwayland, `/tmp/.X11-unix` was absent, and nothing X11-only could
 * run at all. wlroots implements the whole X window manager; what is left is
 * putting each X surface in the scene and answering its configure requests.
 *
 * Two things are deliberate:
 *
 *   - LAZY. Xwayland is not started at login, only when the first X client
 *     actually connects. A session with no X11 app never pays for the
 *     process, and the vast majority of KDOS sessions are that session.
 *   - DISPLAY is exported into the compositor's own environment on `ready`,
 *     which is what makes it inheritable by anything the compositor spawns.
 *     Alien apps get it a second way, because the appbox is a different
 *     process tree: kdos-appbox probes /tmp/.X11-unix and pushes DISPLAY into
 *     the container's environment itself. Both paths are needed — the box
 *     shares /tmp with the host but not the compositor's env.
 *
 * X surfaces are NOT xdg toplevels and the difference matters: an X client
 * sets its own position and can be override-redirect (menus, tooltips, drag
 * icons), so the compositor honours the geometry it asks for instead of
 * imposing one. The associate/dissociate pair exists because an X window can
 * exist long before it has a wl_surface to draw with, and map/unmap can only
 * be listened to once that surface arrives.
 * ---------------------------------
 */

#include <stdlib.h>
#include <unistd.h>

#include "kdos-comp.h"

static void xs_map(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, map);
	(void)data;
	/* Into the current workspace's tree, not the scene root: an X11 window
	 * parented at the root stays visible on every workspace. */
	x->ws = x->server->cur_ws;
	x->scene_tree = wlr_scene_subsurface_tree_create(
		x->server->ws_tree[x->ws], x->xsurface->surface);
	if (!x->scene_tree)
		return;
	x->scene_tree->node.data = x;

	/*
	 * An X11 window's identity is WM_CLASS, and a Wayland app_id is NOT the
	 * same string — GIMP's entry says `StartupWMClass=gimp-3.0` while its
	 * Wayland toplevel calls set_app_id("gimp"). Recording both, from the
	 * path that actually produced each, is the whole point of measuring
	 * rather than guessing.
	 */
	kc_appid_observe(x->server, x->xsurface->class);
	wlr_scene_node_set_position(&x->scene_tree->node, x->xsurface->x,
				    x->xsurface->y);

	/* Override-redirect surfaces are menus and tooltips: they must never
	 * take the keyboard, or the menu steals focus from the window that
	 * opened it and the next keystroke goes nowhere. */
	if (!x->xsurface->override_redirect) {
		wlr_xwayland_surface_activate(x->xsurface, true);
		struct wlr_keyboard *kb = wlr_seat_get_keyboard(x->server->seat);
		if (kb)
			wlr_seat_keyboard_notify_enter(x->server->seat,
				x->xsurface->surface, kb->keycodes,
				kb->num_keycodes, &kb->modifiers);
	}
}

static void xs_unmap(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, unmap);
	(void)data;
	if (x->scene_tree) {
		wlr_scene_node_destroy(&x->scene_tree->node);
		x->scene_tree = NULL;
	}
}

/* The wl_surface has arrived; only now can map/unmap be listened to. */
static void xs_associate(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, associate);
	(void)data;
	x->map.notify = xs_map;
	wl_signal_add(&x->xsurface->surface->events.map, &x->map);
	x->unmap.notify = xs_unmap;
	wl_signal_add(&x->xsurface->surface->events.unmap, &x->unmap);
}

static void xs_dissociate(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, dissociate);
	(void)data;
	wl_list_remove(&x->map.link);
	wl_list_remove(&x->unmap.link);
}

/*
 * An X client asks for its own geometry and expects to get it. Refusing is
 * how X apps end up drawing into a rectangle they do not believe they have —
 * the classic symptom is a menu painted in the wrong corner. M2's window
 * management will start overriding this for real toplevels; until then the
 * request is honoured verbatim.
 */
static void xs_request_configure(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(x->xsurface, ev->x, ev->y, ev->width,
				       ev->height);
	if (x->scene_tree)
		wlr_scene_node_set_position(&x->scene_tree->node, ev->x, ev->y);
}

static void xs_destroy(struct wl_listener *l, void *data)
{
	struct kc_xsurface *x = wl_container_of(l, x, destroy);
	(void)data;
	wl_list_remove(&x->associate.link);
	wl_list_remove(&x->dissociate.link);
	wl_list_remove(&x->request_configure.link);
	wl_list_remove(&x->destroy.link);
	free(x);
}

static void new_xwayland_surface(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;

	struct kc_xsurface *x = calloc(1, sizeof(*x));
	if (!x)
		return;
	x->node_type = KC_NODE_XSURFACE;
	x->server = s;
	x->xsurface = xsurface;

	x->associate.notify = xs_associate;
	wl_signal_add(&xsurface->events.associate, &x->associate);
	x->dissociate.notify = xs_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &x->dissociate);
	x->request_configure.notify = xs_request_configure;
	wl_signal_add(&xsurface->events.request_configure, &x->request_configure);
	x->destroy.notify = xs_destroy;
	wl_signal_add(&xsurface->events.destroy, &x->destroy);
}

static void xwayland_ready(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, xwayland_ready);
	(void)data;
	/* The seat has to be handed over or the X server has no idea which
	 * input device owns the pointer, and X clients get no events. */
	wlr_xwayland_set_seat(s->xwayland, s->seat);
	setenv("DISPLAY", s->xwayland->display_name, true);
	wlr_log(WLR_INFO, "Xwayland ready on DISPLAY=%s",
		s->xwayland->display_name);
}

void kc_xwayland_init(struct kc_server *s, struct wlr_compositor *compositor)
{
	/* lazy = true: the server starts on the first X connection, not at
	 * login. Most KDOS sessions never open an X11 app. */
	s->xwayland = wlr_xwayland_create(s->display, compositor, true);
	if (!s->xwayland) {
		/* Not fatal. A session without X11 is a working session; every
		 * Wayland-native app and the whole shell are unaffected. */
		wlr_log(WLR_ERROR, "Xwayland unavailable — X11-only apps "
				   "will not run");
		return;
	}

	s->new_xwayland_surface.notify = new_xwayland_surface;
	wl_signal_add(&s->xwayland->events.new_surface, &s->new_xwayland_surface);
	s->xwayland_ready.notify = xwayland_ready;
	wl_signal_add(&s->xwayland->events.ready, &s->xwayland_ready);
}

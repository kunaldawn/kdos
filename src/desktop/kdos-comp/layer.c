/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   wlr-layer-shell — panels, docks, lock screens and OSDs
 *
 * A panel is not a window. It anchors to an edge, it does not take the
 * keyboard unless it asks, and it reserves a strip of the screen that no
 * ordinary window may cover. xdg-shell cannot express any of that, which is
 * why every Wayland shell — ours included — needs this protocol.
 *
 * kdos-shell is the consumer, through libkwl. Nothing else on KDOS should be
 * allowed to bind it: a boxed app with layer-shell can draw over the panel and
 * paint anything it likes at the top of the screen, which is why
 * KC_CAP_LAYER_SHELL is on the security-context deny list and denied by
 * default.
 *
 * FOUR LAYERS, and the ordering is the protocol's, not ours: background,
 * bottom, TOP-LEVEL WINDOWS, top, overlay. The two below and the two above are
 * separate scene trees with the toplevels' workspace trees sandwiched between
 * them, which is the only arrangement in which a panel is above a window and a
 * wallpaper is below one.
 */

#include <stdlib.h>

#include "kdos-comp.h"

static void arrange_output(struct kc_server *s, struct kc_output *o);

static void layer_map(struct wl_listener *l, void *data)
{
	struct kc_layer *ly = wl_container_of(l, ly, map);
	(void)data;
	/*
	 * Keyboard focus only if the surface asked for it. A panel that stole
	 * the keyboard on map would take it from whatever the user was typing
	 * into every time the clock redrew — and `on_demand` exists precisely
	 * so a launcher can ask and a clock cannot.
	 */
	if (ly->layer_surface->current.keyboard_interactive !=
	    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
		struct wlr_keyboard *kb = wlr_seat_get_keyboard(ly->server->seat);
		if (kb)
			wlr_seat_keyboard_notify_enter(
				ly->server->seat, ly->layer_surface->surface,
				kb->keycodes, kb->num_keycodes, &kb->modifiers);
	}
	arrange_output(ly->server, ly->output);
}

static void layer_unmap(struct wl_listener *l, void *data)
{
	struct kc_layer *ly = wl_container_of(l, ly, unmap);
	(void)data;
	/* The strip it reserved goes back to the windows. */
	arrange_output(ly->server, ly->output);
}

static void layer_commit(struct wl_listener *l, void *data)
{
	struct kc_layer *ly = wl_container_of(l, ly, commit);
	(void)data;
	/*
	 * The initial commit is the client asking what size it may be. It has
	 * to be answered with a configure before the surface can map at all —
	 * a layer surface that is never configured never appears, and the
	 * client sits waiting for an event that is not coming.
	 */
	if (ly->layer_surface->initial_commit ||
	    ly->layer_surface->current.committed)
		arrange_output(ly->server, ly->output);
}

static void layer_destroy(struct wl_listener *l, void *data)
{
	struct kc_layer *ly = wl_container_of(l, ly, destroy);
	struct kc_server *s = ly->server;
	struct kc_output *o = ly->output;
	(void)data;
	wl_list_remove(&ly->map.link);
	wl_list_remove(&ly->unmap.link);
	wl_list_remove(&ly->commit.link);
	wl_list_remove(&ly->destroy.link);
	wl_list_remove(&ly->link);
	free(ly);
	arrange_output(s, o);
}

/*
 * Lay out every layer surface on one output, and work out what is left for
 * ordinary windows.
 *
 * wlroots does the geometry; the ordering is ours. Exclusive zones are applied
 * in protocol layer order so that two panels on the same edge stack rather
 * than overlap — and `usable_area` is threaded through every call because each
 * surface's zone shrinks what the next one may claim.
 */
static void arrange_output(struct kc_server *s, struct kc_output *o)
{
	if (!o || !o->wlr_output)
		return;

	struct wlr_box full = {0};
	wlr_output_layout_get_box(s->output_layout, o->wlr_output, &full);
	if (wlr_box_empty(&full))
		return;
	struct wlr_box usable = full;

	static const enum zwlr_layer_shell_v1_layer ORDER[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
		ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
	};

	for (size_t i = 0; i < sizeof(ORDER) / sizeof(ORDER[0]); i++) {
		struct kc_layer *ly;
		wl_list_for_each(ly, &s->layers, link) {
			if (ly->output != o)
				continue;
			if (ly->layer_surface->current.layer != ORDER[i])
				continue;
			if (!ly->layer_surface->initialized)
				continue;
			wlr_scene_layer_surface_v1_configure(ly->scene, &full,
							     &usable);
		}
	}

	o->usable = usable;
}

void kc_layer_arrange(struct kc_server *s)
{
	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link)
		arrange_output(s, o);
}

/*
 * The one accessor for `o->usable`, because every window-positioning path has
 * to agree with every other one. It answers with the LAYOUT box for an output
 * whose panels have not arranged yet — a zeroed usable box would place the
 * first window of a session at 0x0 with no size at all.
 */
bool kc_usable_at(struct kc_server *s, double x, double y, struct wlr_box *out)
{
	struct wlr_output *wo = wlr_output_layout_output_at(s->output_layout,
							    x, y);
	if (!wo)
		return false;

	struct wlr_box full = {0};
	wlr_output_layout_get_box(s->output_layout, wo, &full);
	if (wlr_box_empty(&full))
		return false;

	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		if (o->wlr_output != wo)
			continue;
		*out = wlr_box_empty(&o->usable) ? full : o->usable;
		return true;
	}
	*out = full;
	return true;
}

static void new_layer_surface(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_layer_surface);
	struct wlr_layer_surface_v1 *surface = data;

	/*
	 * A client may leave the output unset, meaning "wherever you like".
	 * Picking the one under the cursor is the only answer that is right
	 * more often than not; refusing the surface, which the protocol also
	 * allows, would mean a panel simply never appears on a fresh session.
	 */
	if (!surface->output) {
		struct wlr_output *out = wlr_output_layout_output_at(
			s->output_layout, s->cursor->x, s->cursor->y);
		if (!out && !wl_list_empty(&s->outputs)) {
			struct kc_output *first =
				wl_container_of(s->outputs.next, first, link);
			out = first->wlr_output;
		}
		if (!out) {
			wlr_layer_surface_v1_destroy(surface);
			return;
		}
		surface->output = out;
	}

	struct kc_output *o = NULL, *cand;
	wl_list_for_each(cand, &s->outputs, link)
		if (cand->wlr_output == surface->output) {
			o = cand;
			break;
		}
	if (!o) {
		wlr_layer_surface_v1_destroy(surface);
		return;
	}

	struct kc_layer *ly = calloc(1, sizeof(*ly));
	if (!ly) {
		wlr_layer_surface_v1_destroy(surface);
		return;
	}
	ly->server = s;
	ly->layer_surface = surface;
	ly->output = o;

	/*
	 * Parented by protocol layer, not by workspace. A panel belongs to the
	 * screen rather than to workspace 2 — it must stay visible across a
	 * workspace switch, which is exactly what the workspace trees do NOT
	 * do.
	 */
	struct wlr_scene_tree *parent =
		surface->pending.layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND ||
		surface->pending.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM
			? s->layer_below
			: s->layer_above;

	ly->scene = wlr_scene_layer_surface_v1_create(parent, surface);
	if (!ly->scene) {
		free(ly);
		wlr_layer_surface_v1_destroy(surface);
		return;
	}

	ly->map.notify = layer_map;
	wl_signal_add(&surface->surface->events.map, &ly->map);
	ly->unmap.notify = layer_unmap;
	wl_signal_add(&surface->surface->events.unmap, &ly->unmap);
	ly->commit.notify = layer_commit;
	wl_signal_add(&surface->surface->events.commit, &ly->commit);
	ly->destroy.notify = layer_destroy;
	wl_signal_add(&surface->events.destroy, &ly->destroy);

	wl_list_insert(&s->layers, &ly->link);
}

void kc_layer_init(struct kc_server *s)
{
	wl_list_init(&s->layers);
	/* Version 4: `set_exclusive_edge`, which is what lets a panel that
	 * spans two edges say which one its zone belongs to. */
	s->layer_shell = wlr_layer_shell_v1_create(s->display, 4);
	if (!s->layer_shell) {
		wlr_log(WLR_ERROR, "no layer-shell — the panel cannot run");
		return;
	}
	s->new_layer_surface.notify = new_layer_surface;
	wl_signal_add(&s->layer_shell->events.new_surface,
		      &s->new_layer_surface);
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   outputs, and letting something else arrange them
 *
 * kdos-comp does not carry a display-settings UI and is not going to: that is
 * kdos-shell's, and the protocol between them is the standard one. Implementing
 * wlr-output-management here means `wlr-randr` works today, the shell's monitor
 * page works when it exists, and neither needs a private channel into the
 * compositor.
 *
 * A configuration is applied through wlr_backend_commit() with EVERY output's
 * state in one array, not by committing each output in turn. Two reasons, and
 * the second is the one that bites: a per-output loop leaves the session in a
 * half-applied state when output three of four refuses its mode, and on DRM
 * several outputs frequently cannot be committed separately at all — they share
 * bandwidth, and the only arrangement the driver will accept is the whole set at
 * once.
 * ---------------------------------
 */

#include <stdlib.h>

#include "kdos-comp.h"

/* ── the frame loop ────────────────────────────────────────────────────── */

static void output_frame(struct wl_listener *l, void *data)
{
	struct kc_output *o = wl_container_of(l, o, frame);
	(void)data;
	struct wlr_scene_output *so =
		wlr_scene_get_scene_output(o->server->scene, o->wlr_output);
	if (!so)
		return;
	/* The frame clock, before the work rather than after it: the gap between
	 * two frame events is what the compositor was GIVEN, and moving this line
	 * below the render would fold our own cost into it. */
	kc_frames_frame(o);

	int64_t t0 = kc_frames_now();
	/* The CRT pass composites the scene into a buffer of its own and commits
	 * the result itself. False means it did not — off, or fallen back — and
	 * the frame is committed the ordinary way. */
	if (!kc_crt_frame(o, so))
		wlr_scene_output_commit(so, NULL);
	/* What the desktop itself cost. A miss with a small number here was
	 * somebody else's fault, and that distinction is the whole point. */
	kc_frames_rendered(o, kc_frames_now() - t0);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(so, &now);
}

/* Presentation: the authoritative clock where the backend has one. DRM does;
 * headless and nested do not, which is what kc_frames_frame falls back for. */
static void output_present(struct wl_listener *l, void *data)
{
	struct kc_output *o = wl_container_of(l, o, present);
	kc_frames_present(o, data);
}

static void output_request_state(struct wl_listener *l, void *data)
{
	struct kc_output *o = wl_container_of(l, o, request_state);
	const struct wlr_output_event_request_state *ev = data;
	wlr_output_commit_state(o->wlr_output, ev->state);
}

/* ── attaching and detaching ───────────────────────────────────────────── */

/*
 * Put an output into the layout and give it a scene output, or move one that is
 * already there.
 *
 * The scene output is created only when it is missing, because
 * wlr_scene_output_layout ties its lifetime to the layout entry: removing an
 * output from the layout destroys it, and re-enabling one later has to build a
 * new one. Creating a second while the first still exists gives the output two
 * scene outputs and it renders twice per frame.
 */
static void layout_attach(struct kc_server *s, struct wlr_output *out,
			  bool auto_pos, int x, int y)
{
	struct wlr_output_layout_output *lo =
		auto_pos ? wlr_output_layout_add_auto(s->output_layout, out)
			 : wlr_output_layout_add(s->output_layout, out, x, y);
	if (!lo)
		return;
	if (wlr_scene_get_scene_output(s->scene, out))
		return;			/* already has one; the add repositioned it */
	struct wlr_scene_output *so = wlr_scene_output_create(s->scene, out);
	if (so)
		wlr_scene_output_layout_add_output(s->scene_layout, lo, so);
}

/*
 * Pull windows back onto a visible output.
 *
 * Unplugging a monitor takes its area out of the layout, and every window that
 * was on it keeps its coordinates — outside every output, drawn nowhere, with no
 * way to reach it. "My windows disappeared when I undocked" is a bug report, not
 * a configuration.
 */
static void reclaim_offscreen(struct kc_server *s)
{
	struct wlr_box layout_box;
	wlr_output_layout_get_box(s->output_layout, NULL, &layout_box);
	if (wlr_box_empty(&layout_box))
		return;		/* no outputs at all — nothing to reclaim onto */

	struct kc_toplevel *t;
	wl_list_for_each(t, &s->toplevels, link) {
		/* The test is the window's ORIGIN, not an intersection: a window
		 * that is merely clipped by an edge is where the user put it and
		 * must not be shoved. Only one with its top-left outside every
		 * output is unreachable. */
		if (wlr_box_contains_point(&layout_box, t->x, t->y))
			continue;
		t->x = layout_box.x;
		t->y = layout_box.y;
		wlr_scene_node_set_position(&t->scene_tree->node, t->x, t->y);
	}
}

static void output_destroy(struct wl_listener *l, void *data)
{
	struct kc_output *o = wl_container_of(l, o, destroy);
	struct kc_server *s = o->server;
	(void)data;
	wl_list_remove(&o->frame.link);
	wl_list_remove(&o->request_state.link);
	wl_list_remove(&o->present.link);
	wl_list_remove(&o->destroy.link);
	wl_list_remove(&o->link);
	/* The swapchains and the imported textures hold buffers of this output's
	 * renderer; they go with the output, not with the session. */
	kc_crt_output_free(o);
	free(o);
	reclaim_offscreen(s);
}

static void new_output(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, s->allocator, s->renderer);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	/* DRM needs a mode before the output is usable; the headless and
	 * Wayland backends have none, and NULL here is normal, not a failure. */
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode)
		wlr_output_state_set_mode(&state, mode);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	struct kc_output *o = calloc(1, sizeof(*o));
	if (!o)
		return;
	o->wlr_output = wlr_output;
	o->server = s;
	o->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &o->frame);
	o->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &o->request_state);
	o->present.notify = output_present;
	wl_signal_add(&wlr_output->events.present, &o->present);
	o->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &o->destroy);
	wl_list_insert(&s->outputs, &o->link);

	/* auto: a new monitor goes to the right of what is already there, which
	 * is the only guess available before a client says otherwise. */
	layout_attach(s, wlr_output, true, 0, 0);
	/* A new output has no usable area until the panels have been placed on
	 * it, and o->usable is read by anything that positions a window. */
	kc_layer_arrange(s);
	kc_wallpaper_arrange(s);
	/* And if the session is locked, a monitor plugged in NOW must come up
	 * blank rather than showing the desktop it was never part of. */
	kc_lock_arrange(s);
}

/* ── wlr-output-management ─────────────────────────────────────────────── */

static bool config_apply(struct kc_server *s,
			 struct wlr_output_configuration_v1 *cfg, bool test_only)
{
	size_t n = 0;
	struct wlr_backend_output_state *states =
		wlr_output_configuration_v1_build_state(cfg, &n);
	if (!states)
		return false;

	bool ok = test_only ? wlr_backend_test(s->backend, states, n)
			    : wlr_backend_commit(s->backend, states, n);

	if (ok && !test_only) {
		/* Positions are NOT part of the backend commit — the layout is
		 * the compositor's, so wlroots leaves it to us. */
		struct wlr_output_configuration_head_v1 *head;
		wl_list_for_each(head, &cfg->heads, link) {
			struct wlr_output *out = head->state.output;
			if (head->state.enabled)
				layout_attach(s, out, false, head->state.x,
					      head->state.y);
			else
				wlr_output_layout_remove(s->output_layout, out);
		}
		reclaim_offscreen(s);
		kc_layer_arrange(s);
		kc_lock_arrange(s);
	}

	for (size_t i = 0; i < n; i++)
		wlr_output_state_finish(&states[i].base);
	free(states);
	return ok;
}

static void output_mgr_apply(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, output_mgr_apply);
	struct wlr_output_configuration_v1 *cfg = data;
	if (config_apply(s, cfg, false))
		wlr_output_configuration_v1_send_succeeded(cfg);
	else
		wlr_output_configuration_v1_send_failed(cfg);
	wlr_output_configuration_v1_destroy(cfg);
	/* The client is told the truth about what actually took effect, which
	 * is not necessarily what it asked for. */
}

static void output_mgr_test(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, output_mgr_test);
	struct wlr_output_configuration_v1 *cfg = data;
	if (config_apply(s, cfg, true))
		wlr_output_configuration_v1_send_succeeded(cfg);
	else
		wlr_output_configuration_v1_send_failed(cfg);
	wlr_output_configuration_v1_destroy(cfg);
}

/*
 * The layout changed — by a hotplug, by our own apply, or by an output being
 * destroyed. Clients are told the new truth by being handed a freshly built
 * configuration.
 *
 * This does not loop back into apply: set_configuration only publishes state,
 * it never commits anything.
 */
static void layout_change(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, layout_change);
	(void)data;
	/* Every output's wallpaper is sized to that output's box, so the box
	 * moving is exactly when it has to be redone — plugging a second monitor
	 * in otherwise leaves it on a black rectangle. */
	kc_wallpaper_arrange(s);
	struct wlr_output_configuration_v1 *cfg =
		wlr_output_configuration_v1_create();
	if (!cfg)
		return;

	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		struct wlr_output_configuration_head_v1 *head =
			wlr_output_configuration_head_v1_create(cfg, o->wlr_output);
		if (!head)
			continue;
		struct wlr_box box;
		wlr_output_layout_get_box(s->output_layout, o->wlr_output, &box);
		head->state.x = box.x;
		head->state.y = box.y;
		/* An output that is not in the layout is not on screen, whatever
		 * its own enabled flag says. */
		head->state.enabled =
			wlr_output_layout_get(s->output_layout, o->wlr_output) != NULL;
	}
	wlr_output_manager_v1_set_configuration(s->output_mgr, cfg);
}

void kc_output_init(struct kc_server *s)
{
	wl_list_init(&s->outputs);
	s->new_output.notify = new_output;
	wl_signal_add(&s->backend->events.new_output, &s->new_output);

	s->output_mgr = wlr_output_manager_v1_create(s->display);
	s->output_mgr_apply.notify = output_mgr_apply;
	wl_signal_add(&s->output_mgr->events.apply, &s->output_mgr_apply);
	s->output_mgr_test.notify = output_mgr_test;
	wl_signal_add(&s->output_mgr->events.test, &s->output_mgr_test);

	s->layout_change.notify = layout_change;
	wl_signal_add(&s->output_layout->events.change, &s->layout_change);

	/* xdg-output carries the logical geometry that wlr-randr, screenshot
	 * tools and every panel read. Without it they see outputs with no
	 * position and stack them all at the origin. */
	wlr_xdg_output_manager_v1_create(s->display, s->output_layout);
}

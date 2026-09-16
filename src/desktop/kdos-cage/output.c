/*
 * Cage: A Wayland kiosk.
 *
 * Copyright (C) 2018-2021 Jente Hidskes
 * Copyright (C) 2019 The Sway authors
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/wayland.h>
#include <wlr/config.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_swapchain_manager.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>

#include "embed.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#if CAGE_HAS_XWAYLAND
#include "xwayland.h"
#endif

#define OUTPUT_CONFIG_UPDATED                                                                                          \
	(WLR_OUTPUT_STATE_ENABLED | WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM |      \
	 WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED)


static int embed_free_slot(struct cg_server *server);

static void
update_output_manager_config(struct cg_server *server)
{
	struct wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();

	struct cg_output *output;
	wl_list_for_each (output, &server->outputs, link) {
		struct wlr_output *wlr_output = output->wlr_output;
		struct wlr_output_configuration_head_v1 *config_head =
			wlr_output_configuration_head_v1_create(config, wlr_output);
		struct wlr_box output_box;

		wlr_output_layout_get_box(server->output_layout, wlr_output, &output_box);
		if (!wlr_box_empty(&output_box)) {
			config_head->state.x = output_box.x;
			config_head->state.y = output_box.y;
		}
	}

	wlr_output_manager_v1_set_configuration(server->output_manager_v1, config);
}

static inline void
output_layout_add_auto(struct cg_output *output)
{
	assert(output->scene_output != NULL);
	struct wlr_output_layout_output *layout_output =
		wlr_output_layout_add_auto(output->server->output_layout, output->wlr_output);
	wlr_scene_output_layout_add_output(output->server->scene_output_layout, layout_output, output->scene_output);
}

static inline void
output_layout_add(struct cg_output *output, int32_t x, int32_t y)
{
	assert(output->scene_output != NULL);
	bool exists = wlr_output_layout_get(output->server->output_layout, output->wlr_output);
	struct wlr_output_layout_output *layout_output =
		wlr_output_layout_add(output->server->output_layout, output->wlr_output, x, y);
	if (exists) {
		return;
	}
	wlr_scene_output_layout_add_output(output->server->scene_output_layout, layout_output, output->scene_output);
}

static inline void
output_layout_remove(struct cg_output *output)
{
	wlr_output_layout_remove(output->server->output_layout, output->wlr_output);
}

static void
output_enable(struct cg_output *output)
{
	struct wlr_output *wlr_output = output->wlr_output;

	/* Outputs get enabled by the backend before firing the new_output event,
	 * so we can't do a check for already enabled outputs here unless we
	 * duplicate the enabled property in cg_output. */
	wlr_log(WLR_DEBUG, "Enabling output %s", wlr_output->name);

	struct wlr_output_state state = {0};
	wlr_output_state_set_enabled(&state, true);

	if (wlr_output_commit_state(wlr_output, &state)) {
		output_layout_add_auto(output);
	}

	update_output_manager_config(output->server);
}

static void
output_disable(struct cg_output *output)
{
	struct wlr_output *wlr_output = output->wlr_output;
	if (!wlr_output->enabled) {
		wlr_log(WLR_DEBUG, "Not disabling already disabled output %s", wlr_output->name);
		return;
	}

	wlr_log(WLR_DEBUG, "Disabling output %s", wlr_output->name);
	struct wlr_output_state state = {0};
	wlr_output_state_set_enabled(&state, false);
	wlr_output_commit_state(wlr_output, &state);
	output_layout_remove(output);
}

static void
handle_output_frame(struct wl_listener *listener, void *data)
{
	struct cg_output *output = wl_container_of(listener, output, frame);

	if (!output->wlr_output->enabled || !output->scene_output) {
		return;
	}

	/*
	 * A WINDOW NOBODY CAN SEE IS NOT RENDERED. Frame-done is still sent,
	 * because a client that never gets one stops drawing and then never
	 * redraws when the window comes back; what is skipped is the render and
	 * the copy. An anchor no toplevel has claimed has nothing to publish to
	 * and takes the same road.
	 */
	if (embed_active(output->server) &&
	    (!output->view || embed_asleep(output->view))) {
		struct timespec now = {0};
		clock_gettime(CLOCK_MONOTONIC, &now);
		wlr_scene_output_send_frame_done(output->scene_output, &now);
		return;
	}

	if (embed_active(output->server)) {
		/*
		 * NOTHING TO DRAW IS NOTHING TO DO, and the two branches must
		 * agree about that. wlr_scene_output_commit() — the branch
		 * below — returns early on !wlr_scene_output_needs_frame();
		 * wlr_scene_output_build_state() has no such guard, so without
		 * this one the embed branch renders, copies a whole
		 * framebuffer and publishes it on every tick of the headless
		 * output whether or not a client has committed anything.
		 *
		 * AND THE PARENT READS A PUBLISHED FRAME AS PROOF THE GUEST IS
		 * ALIVE. Its close deadline is cleared by a frame, because a
		 * toolkit that draws "save your work?" inside the window it was
		 * asked to close maps nothing the parent can see. A frame
		 * published for a client that committed nothing would clear
		 * that deadline for a wedged guest and leave a window on the
		 * desktop that nothing can ever remove.
		 *
		 * Frame-done still goes out, or a client that is waiting for
		 * one stops drawing.
		 */
		if (!wlr_scene_output_needs_frame(output->scene_output)) {
			struct timespec now = {0};
			clock_gettime(CLOCK_MONOTONIC, &now);
			wlr_scene_output_send_frame_done(output->scene_output,
							 &now);
			return;
		}

		/*
		 * The state is BUILT and then committed, rather than committed
		 * in one call, because the buffer that was rendered into is
		 * only reachable in between — and that buffer is the whole
		 * point of the embedded mode.
		 */
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		if (wlr_scene_output_build_state(output->scene_output, &state, NULL)) {
			if (state.committed & WLR_OUTPUT_STATE_BUFFER)
				embed_publish(output->view, state.buffer,
					      (state.committed & WLR_OUTPUT_STATE_DAMAGE)
						      ? &state.damage : NULL);
			wlr_output_commit_state(output->wlr_output, &state);
		}
		wlr_output_state_finish(&state);
	} else {
		wlr_scene_output_commit(output->scene_output, NULL);
	}

	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
}

static void
handle_output_commit(struct wl_listener *listener, void *data)
{
	struct cg_output *output = wl_container_of(listener, output, commit);
	struct wlr_output_event_commit *event = data;

	/* Notes:
	 * - output layout change will also be called if needed to position the views
	 * - always update output manager configuration even if the output is now disabled */

	if (event->state->committed & OUTPUT_CONFIG_UPDATED) {
		update_output_manager_config(output->server);
	}
}

static void
handle_output_request_state(struct wl_listener *listener, void *data)
{
	struct cg_output *output = wl_container_of(listener, output, request_state);
	struct wlr_output_event_request_state *event = data;

	if (wlr_output_commit_state(output->wlr_output, event->state)) {
		update_output_manager_config(output->server);
	}
}

void
handle_output_layout_change(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, output_layout_change);

	/* The background covers whatever the layout now is. Sized here rather
	 * than at creation, because at creation there are no outputs yet. */
	if (server->background) {
		struct wlr_box box;

		wlr_output_layout_get_box(server->output_layout, NULL, &box);
		/*
		 * AND WHEN THIS CAGE IS A WINDOW IT IS SIZED ONCE, to the whole
		 * grid rather than to the layout in use. Every window opening
		 * and closing changes that union, and a resized rectangle
		 * damages whatever of it a client is not covering opaquely — a
		 * full repaint of every open window each time a dialog appears,
		 * paid for in blocks the parent has to re-send. Fixed, the
		 * resize is a no-op and nothing is damaged.
		 */
		if (server->embed.embedded) {
			box.x = 0;
			box.y = 0;
			box.width = CG_EMBED_COLS * CG_EMBED_SPAN;
			box.height = CG_EMBED_COLS * CG_EMBED_SPAN;
		}
		wlr_scene_rect_set_size(server->background, box.width, box.height);
		wlr_scene_node_set_position(&server->background->node, box.x, box.y);
	}

	view_position_all(server);
	update_output_manager_config(server);
}

static bool
is_nested_output(struct cg_output *output)
{
	if (wlr_output_is_wl(output->wlr_output)) {
		return true;
	}
#if WLR_HAS_X11_BACKEND
	if (wlr_output_is_x11(output->wlr_output)) {
		return true;
	}
#endif
	return false;
}

static void
output_destroy(struct cg_output *output)
{
	struct cg_server *server = output->server;
	bool was_nested_output = is_nested_output(output);

	/*
	 * THE BACK-POINTERS GO FIRST, in both directions. A view still naming a
	 * freed output is a frame copied through a dangling pointer, and this
	 * runs from wlroots' destroy signal as well as from output_release() —
	 * a card unplugged under a window arrives here and nowhere else.
	 */
	if (output->view) {
		output->view->win.out = NULL;
		output->view = NULL;
	}
	if (server->embed.anchor == output) {
		server->embed.anchor = NULL;
	}

	output->wlr_output->data = NULL;

	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->commit.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->link);

	output_layout_remove(output);

	free(output);

	if (wl_list_empty(&server->outputs) && was_nested_output) {
		server_terminate(server);
	} else if (!server->embed.embedded && server->output_mode == CAGE_MULTI_OUTPUT_MODE_LAST &&
		   !wl_list_empty(&server->outputs)) {
		struct cg_output *prev = wl_container_of(server->outputs.next, prev, link);
		output_enable(prev);
		view_position_all(server);
	}
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct cg_output *output = wl_container_of(listener, output, destroy);
	output_destroy(output);
}

void
handle_new_output(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if (wlr_output->non_desktop) {
		wlr_log(WLR_DEBUG, "Not configuring non-desktop output: %s", wlr_output->name);
#if WLR_HAS_DRM_BACKEND
		if (server->drm_lease_v1) {
			wlr_drm_lease_v1_manager_offer_output(server->drm_lease_v1, wlr_output);
		}
#endif
		return;
	}

	if (!wlr_output_init_render(wlr_output, server->allocator, server->renderer)) {
		wlr_log(WLR_ERROR, "Failed to initialize output rendering");
		return;
	}

	struct cg_output *output = calloc(1, sizeof(struct cg_output));
	if (!output) {
		wlr_log(WLR_ERROR, "Failed to allocate output");
		return;
	}

	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;

	wl_list_insert(&server->outputs, &output->link);

	output->commit.notify = handle_output_commit;
	wl_signal_add(&wlr_output->events.commit, &output->commit);
	output->request_state.notify = handle_output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
	output->destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = handle_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
	if (!output->scene_output) {
		wlr_log(WLR_ERROR, "Failed to allocate scene output");
		return;
	}

	struct wlr_output_state state = {0};
	wlr_output_state_set_enabled(&state, true);
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *preferred_mode = wlr_output_preferred_mode(wlr_output);
		if (preferred_mode) {
			wlr_output_state_set_mode(&state, preferred_mode);
		}
		if (!wlr_output_test_state(wlr_output, &state)) {
			struct wlr_output_mode *mode;
			wl_list_for_each (mode, &wlr_output->modes, link) {
				if (mode == preferred_mode) {
					continue;
				}

				wlr_output_state_set_mode(&state, mode);
				if (wlr_output_test_state(wlr_output, &state)) {
					break;
				}
			}
		}
	}

	/*
	 * `-m last` DISABLES EVERY OUTPUT BUT THE NEWEST, which with one output
	 * per window would blank every window but the one that opened last. The
	 * mode is only reachable by hand and kdos-con never passes it.
	 */
	if (!server->embed.embedded && server->output_mode == CAGE_MULTI_OUTPUT_MODE_LAST &&
	    wl_list_length(&server->outputs) > 1) {
		struct cg_output *next = wl_container_of(output->link.next, next, link);
		output_disable(next);
	}

	if (!wlr_xcursor_manager_load(server->seat->xcursor_manager, wlr_output->scale)) {
		wlr_log(WLR_ERROR, "Cannot load XCursor theme for output '%s' with scale %f", wlr_output->name,
			wlr_output->scale);
	}

	wlr_log(WLR_DEBUG, "Enabling new output %s", wlr_output->name);
	if (wlr_output_commit_state(wlr_output, &state)) {
		/*
		 * PLACED BY HAND AND NEVER `auto` WHEN THIS CAGE IS A WINDOW.
		 * wlroots re-packs auto-configured outputs left to right on
		 * every add, remove and resize, so opening or closing one
		 * window would move every other window's origin — every scene
		 * position, every pointer coordinate and every popup's
		 * unconstrain box — underneath the guest, mid-gesture.
		 */
		if (server->embed.embedded) {
			/*
			 * AND AN OUTPUT THE GRID HAS NO SPAN FOR IS LEFT OUT OF
			 * THE LAYOUT. output_claim() refuses before it gets
			 * here, so this is the backstop for an output added any
			 * other way: unplaced it renders nothing and hovers
			 * nothing, where a span past the last one would render
			 * into a box no window owns.
			 */
			output->slot = embed_free_slot(server);
			if (output->slot >= 0) {
				output_layout_add(output, (output->slot % CG_EMBED_COLS) * CG_EMBED_SPAN,
						  (output->slot / CG_EMBED_COLS) * CG_EMBED_SPAN);
				if (!server->embed.anchor) {
					server->embed.anchor = output;
				}
			}
		} else {
			output_layout_add_auto(output);
		}
	}

	view_position_all(output->server);
	update_output_manager_config(output->server);
}

/*
 * THE LOWEST SPAN NOBODY IS IN, or -1 when the grid is full. Linear because the
 * list is the windows this guest has open, which is a handful; reused because a
 * counter that only goes up runs out, and reuse is safe — an origin is handed
 * out at the moment an output is created and no existing output's origin moves.
 *
 * BOUNDED BY THE GRID AND NOT BY THE COUNTER. The scene background is sized to
 * CG_EMBED_WINS spans once and never resized, and an X11 root cannot name an
 * origin past 32767, so a span past the last one renders against nothing and
 * cannot be told to an Xwayland guest at all.
 */
static int
embed_free_slot(struct cg_server *server)
{
	for (int slot = 0; slot < CG_EMBED_WINS; slot++) {
		struct cg_output *o;
		bool used = false;

		/* IN THE LAYOUT IS WHAT MAKES A SPAN TAKEN. An output that has
		 * just been made is in the list and not yet placed — this runs
		 * inside its own new_output — and one whose commit failed
		 * occupies nothing. */
		wl_list_for_each (o, &server->outputs, link) {
			if (o->slot == slot &&
			    wlr_output_layout_get(server->output_layout, o->wlr_output)) {
				used = true;
				break;
			}
		}
		if (!used) {
			return slot;
		}
	}
	return -1;
}

bool
output_claim_anchor(struct cg_view *view)
{
	struct cg_server *server = view->server;

	if (!server->embed.embedded || view->win.out) {
		return false;
	}
	if (!server->embed.anchor || server->embed.anchor->view) {
		return false;
	}

	/*
	 * AND ONLY AN ORDINARY TOPLEVEL TAKES IT. The anchor carries the
	 * rectangle the parent forked this cage with, and the parent gives that
	 * rectangle to the first toplevel with no owner and no role. A splash
	 * or a dock that gets there first would be sized to the application's
	 * remembered window while the parent frames the image window behind it
	 * at that same rectangle, and neither end tells the other.
	 */
	if (!embed_win_is_ordinary(view)) {
		return false;
	}

	server->embed.anchor->view = view;
	view->win.out = server->embed.anchor;
	return true;
}

bool
output_claim(struct cg_view *view, int w, int h)
{
	struct cg_server *server = view->server;
	struct wlr_output *wlr_output;
	struct cg_output *output;

	if (!server->embed.embedded || view->win.out) {
		return false;
	}

	/*
	 * THE ANCHOR FIRST. It exists from start-up at the size --embed named,
	 * so the first toplevel is configured at the size the parent forked
	 * this cage with and nothing is allocated for a one-window guest.
	 */
	if (output_claim_anchor(view)) {
		return true;
	}

	if (!server->headless) {
		return false;
	}

	/*
	 * AND A WINDOW THE GRID HAS NO SPAN LEFT FOR IS REFUSED RATHER THAN
	 * ALLOCATED. view_map() then leaves win.out NULL and no KEMBED_OPEN is
	 * sent, so the toplevel is simply not a window on the parent's desktop:
	 * an output made anyway would render, copy a whole framebuffer and
	 * publish for the life of the process for a window the parent has
	 * already refused and nothing can close.
	 */
	if (embed_free_slot(server) < 0) {
		wlr_log(WLR_ERROR, "embed: no span left for a new window");
		return false;
	}

	/* A TOPLEVEL THAT REPORTS NO SIZE OF ITS OWN GETS THE ANCHOR'S, because
	 * an output has to have one and the parent's window is what that size
	 * came from. */
	if (w < 1 || w > CG_EMBED_SPAN) {
		w = server->embed.first_w;
	}
	if (h < 1 || h > CG_EMBED_SPAN) {
		h = server->embed.first_h;
	}

	/*
	 * handle_new_output() RUNS INSIDE THIS CALL, because the backend has
	 * started: it makes the cg_output, places it and hangs it off
	 * wlr_output->data, so the binding below needs no pending-view state.
	 */
	wlr_output = wlr_headless_add_output(server->headless, (unsigned int)w, (unsigned int)h);
	if (!wlr_output) {
		wlr_log(WLR_ERROR, "embed: no output for a new window");
		return false;
	}

	output = wlr_output->data;
	if (!output) {
		wlr_output_destroy(wlr_output);
		return false;
	}

	output->view = view;
	view->win.out = output;
	return true;
}

void
output_release(struct cg_view *view)
{
	struct cg_output *output = view->win.out;

	if (!output) {
		return;
	}

	view->win.out = NULL;
	output->view = NULL;

	/*
	 * THE ANCHOR IS RELEASED AND NOT DESTROYED. A cage with no output at
	 * all stalls every client that waits for a wl_output before mapping,
	 * and leaves Xwayland's root screen at 0x0 where no X client can map;
	 * output_destroy() also terminates a server whose output list empties.
	 */
	if (output == view->server->embed.anchor) {
		return;
	}

	wlr_output_destroy(output->wlr_output);
}

void
output_set_window_title(struct cg_output *output, const char *title)
{
	struct wlr_output *wlr_output = output->wlr_output;

	if (!wlr_output->enabled) {
		wlr_log(WLR_DEBUG, "Not setting window title for disabled output %s", wlr_output->name);
		return;
	}

	if (wlr_output_is_wl(wlr_output)) {
		wlr_wl_output_set_title(wlr_output, title);
#if WLR_HAS_X11_BACKEND
	} else if (wlr_output_is_x11(wlr_output)) {
		wlr_x11_output_set_title(wlr_output, title);
#endif
	}
}

static bool
output_config_apply(struct cg_server *server, struct wlr_output_configuration_v1 *config, bool test_only)
{
	bool ok = false;

	/*
	 * A GUEST DOES NOT REARRANGE ITS OWN WINDOWS. Each output is one window
	 * on the parent's desktop and the parent decides where that window
	 * sits; a configuration honoured here could place two windows' boxes on
	 * top of each other, which is one window's pixels in another window's
	 * framebuffer.
	 */
	if (server->embed.embedded) {
		return false;
	}

	size_t states_len;
	struct wlr_backend_output_state *states = wlr_output_configuration_v1_build_state(config, &states_len);
	if (states == NULL) {
		return false;
	}

	struct wlr_output_swapchain_manager swapchain_manager;
	wlr_output_swapchain_manager_init(&swapchain_manager, server->backend);

	ok = wlr_output_swapchain_manager_prepare(&swapchain_manager, states, states_len);
	if (!ok || test_only) {
		goto out;
	}

	for (size_t i = 0; i < states_len; i++) {
		struct wlr_backend_output_state *backend_state = &states[i];
		struct cg_output *output = backend_state->output->data;

		struct wlr_swapchain *swapchain =
			wlr_output_swapchain_manager_get_swapchain(&swapchain_manager, backend_state->output);
		struct wlr_scene_output_state_options options = {
			.swapchain = swapchain,
		};
		struct wlr_output_state *state = &backend_state->base;
		if (!wlr_scene_output_build_state(output->scene_output, state, &options)) {
			ok = false;
			goto out;
		}
	}

	ok = wlr_backend_commit(server->backend, states, states_len);
	if (!ok) {
		goto out;
	}

	wlr_output_swapchain_manager_apply(&swapchain_manager);

	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each (head, &config->heads, link) {
		struct cg_output *output = head->state.output->data;

		if (head->state.enabled) {
			output_layout_add(output, head->state.x, head->state.y);
		} else {
			output_layout_remove(output);
		}
	}

out:
	wlr_output_swapchain_manager_finish(&swapchain_manager);
	for (size_t i = 0; i < states_len; i++) {
		wlr_output_state_finish(&states[i].base);
	}
	free(states);
	return ok;
}

void
handle_output_manager_apply(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	if (output_config_apply(server, config, false)) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}

	wlr_output_configuration_v1_destroy(config);
}

void
handle_output_manager_test(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, output_manager_test);
	struct wlr_output_configuration_v1 *config = data;

	if (output_config_apply(server, config, true)) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}

	wlr_output_configuration_v1_destroy(config);
}

/*
 * Cage: A Wayland kiosk.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"

#include <assert.h>
#include <time.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#if CAGE_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "embed.h"
#include "kembed.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#if CAGE_HAS_XWAYLAND
#include "xwayland.h"
#endif

static void drag_icon_update_position(struct cg_drag_icon *drag_icon);

/* XDG toplevels may have nested surfaces, such as popup windows for context
 * menus or tooltips. This function tests if any of those are underneath the
 * coordinates lx and ly (in output Layout Coordinates). If so, it sets the
 * surface pointer to that wlr_surface and the sx and sy coordinates to the
 * coordinates relative to that surface's top-left corner.
 *
 * This function iterates over all of our surfaces and attempts to find one
 * under the cursor. If desktop_view_at returns a view, there is also a
 * surface. There cannot be a surface without a view, either. It's both or
 * nothing.
 */
static struct cg_view *
desktop_view_at(struct cg_server *server, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;

	/* Walk up the tree until we find a node with a data pointer. When done,
	 * we've found the node representing the view. */
	while (!node->data) {
		if (!node->parent) {
			node = NULL;
			break;
		}

		node = &node->parent->node;
	}

	assert(node != NULL);
	return node->data;
}

static void
press_cursor_button(struct cg_seat *seat, struct wlr_input_device *device, uint32_t time, uint32_t button,
		    uint32_t state, double lx, double ly)
{
	struct cg_server *server = seat->server;

	/*
	 * THE PARENT IS THE ONLY SOURCE OF KEYBOARD FOCUS WHEN THIS CAGE IS A
	 * WINDOW. A click that moved focus here would move it away from the
	 * window the parent believes has it: every key it sends afterwards
	 * reaches a window the person is not looking at, and the releases it
	 * owes go to a window that never saw the presses.
	 */
	if (server->embed.embedded) {
		return;
	}

	if (state == WLR_BUTTON_PRESSED) {
		double sx, sy;
		struct wlr_surface *surface;
		struct cg_view *view = desktop_view_at(server, lx, ly, &surface, &sx, &sy);
		struct cg_view *current = seat_get_focus(seat);
		if (view == current) {
			return;
		}

		/*
		 * Focus that client if the button was pressed and it has no
		 * open dialogs.
		 *
		 * A CLEARED FOCUS IS NOT A FOCUSED VIEW. Nothing is focused
		 * between a toplevel's unmap and the next map, and
		 * view_is_transient_for() dereferences the view it is asked
		 * about. Nothing is transient for nothing, so the click
		 * focuses.
		 */
		if (view && (!current || !view_is_transient_for(current, view))) {
			seat_set_focus(seat, view);
		}
	}
}

static void
update_capabilities(struct cg_seat *seat)
{
	uint32_t caps = 0;

	if (!wl_list_empty(&seat->keyboard_groups)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	if (!wl_list_empty(&seat->pointers)) {
		caps |= WL_SEAT_CAPABILITY_POINTER;
	}
	if (!wl_list_empty(&seat->touch)) {
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	}

	/*
	 * AN EMBEDDED SEAT HAS NO DEVICES AND STILL HAS BOTH CAPABILITIES.
	 * A client binds a pointer only if the seat announces one, so a seat
	 * whose input is injected by the parent would advertise nothing and
	 * every click would be delivered to a client that never asked to
	 * receive any. The keyboard group above already covers the keyboard;
	 * there is no pointer device to make, and none is needed — the cursor
	 * is warped and the ordinary motion path runs.
	 */
	if (seat->embed_keys) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	}

	wlr_seat_set_capabilities(seat->seat, caps);

	/* Hide cursor if the seat doesn't have pointer capability. */
	if ((caps & WL_SEAT_CAPABILITY_POINTER) == 0) {
		wlr_cursor_unset_image(seat->cursor);
	} else {
		wlr_cursor_set_xcursor(seat->cursor, seat->xcursor_manager, DEFAULT_XCURSOR);
	}
}

static void
map_input_device_to_output(struct cg_seat *seat, struct wlr_input_device *device, const char *output_name)
{
	if (!output_name) {
		wlr_log(WLR_INFO, "Input device %s cannot be mapped to an output device\n", device->name);
		return;
	}

	struct cg_output *output;
	wl_list_for_each (output, &seat->server->outputs, link) {
		if (strcmp(output_name, output->wlr_output->name) == 0) {
			wlr_log(WLR_INFO, "Mapping input device %s to output device %s\n", device->name,
				output->wlr_output->name);
			wlr_cursor_map_input_to_output(seat->cursor, device, output->wlr_output);
			return;
		}
	}

	wlr_log(WLR_INFO, "Couldn't map input device %s to an output\n", device->name);
}

static void
handle_touch_destroy(struct wl_listener *listener, void *data)
{
	struct cg_touch *touch = wl_container_of(listener, touch, destroy);
	struct cg_seat *seat = touch->seat;

	wl_list_remove(&touch->link);
	wlr_cursor_detach_input_device(seat->cursor, &touch->touch->base);
	wl_list_remove(&touch->destroy.link);
	free(touch);

	update_capabilities(seat);
}

static void
handle_new_touch(struct cg_seat *seat, struct wlr_touch *wlr_touch)
{
	struct cg_touch *touch = calloc(1, sizeof(struct cg_touch));
	if (!touch) {
		wlr_log(WLR_ERROR, "Cannot allocate touch");
		return;
	}

	touch->seat = seat;
	touch->touch = wlr_touch;
	wlr_cursor_attach_input_device(seat->cursor, &wlr_touch->base);

	wl_list_insert(&seat->touch, &touch->link);
	touch->destroy.notify = handle_touch_destroy;
	wl_signal_add(&wlr_touch->base.events.destroy, &touch->destroy);

	map_input_device_to_output(seat, &wlr_touch->base, wlr_touch->output_name);
}

static void
handle_pointer_destroy(struct wl_listener *listener, void *data)
{
	struct cg_pointer *pointer = wl_container_of(listener, pointer, destroy);
	struct cg_seat *seat = pointer->seat;

	wl_list_remove(&pointer->link);
	wlr_cursor_detach_input_device(seat->cursor, &pointer->pointer->base);
	wl_list_remove(&pointer->destroy.link);
	free(pointer);

	update_capabilities(seat);
}

static void
handle_new_pointer(struct cg_seat *seat, struct wlr_pointer *wlr_pointer)
{
	struct cg_pointer *pointer = calloc(1, sizeof(struct cg_pointer));
	if (!pointer) {
		wlr_log(WLR_ERROR, "Cannot allocate pointer");
		return;
	}

	pointer->seat = seat;
	pointer->pointer = wlr_pointer;
	wlr_cursor_attach_input_device(seat->cursor, &wlr_pointer->base);

	wl_list_insert(&seat->pointers, &pointer->link);
	pointer->destroy.notify = handle_pointer_destroy;
	wl_signal_add(&wlr_pointer->base.events.destroy, &pointer->destroy);

	map_input_device_to_output(seat, &wlr_pointer->base, wlr_pointer->output_name);
}

static void
handle_virtual_pointer(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_virtual_pointer);
	struct cg_seat *seat = server->seat;
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
	struct wlr_pointer *wlr_pointer = &pointer->pointer;

	/* We'll want to map the device back to an output later, this is a bit
	 * sub-optimal (we could just keep the suggested_output), but just copy
	 * its name so we do like other devices
	 */
	if (event->suggested_output != NULL) {
		wlr_pointer->output_name = strdup(event->suggested_output->name);
	}
	/* TODO: event->suggested_seat should be checked if we handle multiple seats */
	handle_new_pointer(seat, wlr_pointer);
	update_capabilities(seat);
}

static void
handle_modifier_event(struct wlr_keyboard *keyboard, struct cg_seat *seat)
{
	wlr_seat_set_keyboard(seat->seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat->seat, &keyboard->modifiers);

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static bool
handle_keybinding(struct cg_server *server, xkb_keysym_t sym)
{
#ifdef DEBUG
	if (sym == XKB_KEY_Escape) {
		server_terminate(server);
		return true;
	}
#endif
	if (server->allow_vt_switch && sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
		if (wlr_backend_is_multi(server->backend)) {
			if (server->session) {
				unsigned vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
				wlr_session_change_vt(server->session, vt);
			}
		}
	} else {
		return false;
	}
	wlr_idle_notifier_v1_notify_activity(server->idle, server->seat->seat);
	return true;
}

static void
handle_key_event(struct wlr_keyboard *keyboard, struct cg_seat *seat, void *data)
{
	struct wlr_keyboard_key_event *event = data;

	/* Translate from libinput keycode to an xkbcommon keycode. */
	xkb_keycode_t keycode = event->keycode + 8;

	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* If Alt is held down and this button was pressed, we
		 * attempt to process it as a compositor
		 * keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(seat->server, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat->seat, keyboard);
		wlr_seat_keyboard_notify_key(seat->seat, event->time_msec, event->keycode, event->state);
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_keyboard_group_key(struct wl_listener *listener, void *data)
{
	struct cg_keyboard_group *cg_group = wl_container_of(listener, cg_group, key);
	handle_key_event(&cg_group->wlr_group->keyboard, cg_group->seat, data);
}

static void
handle_keyboard_group_modifiers(struct wl_listener *listener, void *data)
{
	struct cg_keyboard_group *group = wl_container_of(listener, group, modifiers);
	handle_modifier_event(&group->wlr_group->keyboard, group->seat);
}

static void
cg_keyboard_group_add(struct wlr_keyboard *keyboard, struct cg_seat *seat, bool virtual)
{
	/* We apparently should not group virtual keyboards,
	 * so create a new group with it
	 */
	if (!virtual) {
		struct cg_keyboard_group *group;
		wl_list_for_each (group, &seat->keyboard_groups, link) {
			if (group->is_virtual)
				continue;
			struct wlr_keyboard_group *wlr_group = group->wlr_group;
			if (wlr_keyboard_group_add_keyboard(wlr_group, keyboard)) {
				wlr_log(WLR_DEBUG, "Added new keyboard to existing group");
				return;
			}
		}
	}

	/* This is reached if and only if the keyboard could not be inserted into
	 * any group */
	struct cg_keyboard_group *cg_group = calloc(1, sizeof(struct cg_keyboard_group));
	if (cg_group == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate keyboard group.");
		return;
	}
	cg_group->seat = seat;
	cg_group->is_virtual = virtual;
	cg_group->wlr_group = wlr_keyboard_group_create();
	if (cg_group->wlr_group == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr keyboard group.");
		goto cleanup;
	}

	cg_group->wlr_group->data = cg_group;
	wlr_keyboard_set_keymap(&cg_group->wlr_group->keyboard, keyboard->keymap);

	wlr_keyboard_set_repeat_info(&cg_group->wlr_group->keyboard, keyboard->repeat_info.rate,
				     keyboard->repeat_info.delay);

	wlr_log(WLR_DEBUG, "Created keyboard group");

	wlr_keyboard_group_add_keyboard(cg_group->wlr_group, keyboard);
	wl_list_insert(&seat->keyboard_groups, &cg_group->link);

	wl_signal_add(&cg_group->wlr_group->keyboard.events.key, &cg_group->key);
	cg_group->key.notify = handle_keyboard_group_key;
	wl_signal_add(&cg_group->wlr_group->keyboard.events.modifiers, &cg_group->modifiers);
	cg_group->modifiers.notify = handle_keyboard_group_modifiers;

	return;

cleanup:
	if (cg_group && cg_group->wlr_group) {
		wlr_keyboard_group_destroy(cg_group->wlr_group);
	}
	free(cg_group);
}

static void
keyboard_group_destroy(struct cg_keyboard_group *keyboard_group)
{
	wl_list_remove(&keyboard_group->key.link);
	wl_list_remove(&keyboard_group->modifiers.link);
	wlr_keyboard_group_destroy(keyboard_group->wlr_group);
	wl_list_remove(&keyboard_group->link);
	free(keyboard_group);
}

static void
handle_new_keyboard(struct cg_seat *seat, struct wlr_keyboard *keyboard, bool virtual)
{
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Unable to create XKB context");
		return;
	}

	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		wlr_log(WLR_ERROR, "Unable to configure keyboard: keymap does not exist");
		xkb_context_unref(context);
		return;
	}

	wlr_keyboard_set_keymap(keyboard, keymap);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(keyboard, 25, 600);

	cg_keyboard_group_add(keyboard, seat, virtual);

	wlr_seat_set_keyboard(seat->seat, keyboard);
}

static void
handle_virtual_keyboard(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, new_virtual_keyboard);
	struct cg_seat *seat = server->seat;
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	struct wlr_keyboard *wlr_keyboard = &keyboard->keyboard;

	/* TODO: If multiple seats are supported, check keyboard->seat
	 * to select the appropriate one */

	handle_new_keyboard(seat, wlr_keyboard, true);
	update_capabilities(seat);
}

static void
handle_new_input(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		handle_new_keyboard(seat, wlr_keyboard_from_input_device(device), false);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		handle_new_pointer(seat, wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		handle_new_touch(seat, wlr_touch_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		wlr_log(WLR_DEBUG, "Switch input is not implemented");
		return;
	case WLR_INPUT_DEVICE_TABLET:
	case WLR_INPUT_DEVICE_TABLET_PAD:
		wlr_log(WLR_DEBUG, "Tablet input is not implemented");
		return;
	}

	update_capabilities(seat);
}

static void
handle_request_set_primary_selection(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;

	wlr_seat_set_primary_selection(seat->seat, event->source, event->serial);
}

static void
handle_request_set_selection(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void
handle_request_set_cursor(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_surface *focused_surface = event->seat_client->seat->pointer_state.focused_surface;
	bool has_focused = focused_surface != NULL && focused_surface->resource != NULL;
	struct wl_client *focused_client = NULL;
	if (has_focused) {
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	/* This can be sent by any client, so we check to make sure
	 * this one actually has pointer focus first. */
	if (focused_client == event->seat_client->client) {
		wlr_cursor_set_surface(seat->cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

static void
handle_touch_down(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, touch_down);
	struct wlr_touch_down_event *event = data;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base, event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_surface *surface;
	struct cg_view *view = desktop_view_at(seat->server, lx, ly, &surface, &sx, &sy);

	uint32_t serial = 0;
	if (view) {
		serial = wlr_seat_touch_notify_down(seat->seat, surface, event->time_msec, event->touch_id, sx, sy);
	}

	if (serial && wlr_seat_touch_num_points(seat->seat) == 1) {
		seat->touch_id = event->touch_id;
		seat->touch_lx = lx;
		seat->touch_ly = ly;
		press_cursor_button(seat, &event->touch->base, event->time_msec, BTN_LEFT, WLR_BUTTON_PRESSED, lx, ly);
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_touch_up(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, touch_up);
	struct wlr_touch_up_event *event = data;

	if (!wlr_seat_touch_get_point(seat->seat, event->touch_id)) {
		return;
	}

	if (wlr_seat_touch_num_points(seat->seat) == 1) {
		press_cursor_button(seat, &event->touch->base, event->time_msec, BTN_LEFT, WLR_BUTTON_RELEASED,
				    seat->touch_lx, seat->touch_ly);
	}

	wlr_seat_touch_notify_up(seat->seat, event->time_msec, event->touch_id);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_touch_motion(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, touch_motion);
	struct wlr_touch_motion_event *event = data;

	if (!wlr_seat_touch_get_point(seat->seat, event->touch_id)) {
		return;
	}

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base, event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_surface *surface;
	struct cg_view *view = desktop_view_at(seat->server, lx, ly, &surface, &sx, &sy);

	if (view) {
		wlr_seat_touch_point_focus(seat->seat, surface, event->time_msec, event->touch_id, sx, sy);
		wlr_seat_touch_notify_motion(seat->seat, event->time_msec, event->touch_id, sx, sy);
	} else {
		wlr_seat_touch_point_clear_focus(seat->seat, event->time_msec, event->touch_id);
	}

	if (event->touch_id == seat->touch_id) {
		seat->touch_lx = lx;
		seat->touch_ly = ly;
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_touch_frame(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, touch_frame);

	wlr_seat_touch_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_frame(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_frame);

	wlr_seat_pointer_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_axis(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	wlr_seat_pointer_notify_axis(seat->seat, event->time_msec, event->orientation, event->delta,
				     event->delta_discrete, event->source, event->relative_direction);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_button(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_button);
	struct wlr_pointer_button_event *event = data;

	wlr_seat_pointer_notify_button(seat->seat, event->time_msec, event->button, event->state);
	press_cursor_button(seat, &event->pointer->base, event->time_msec, event->button, event->state, seat->cursor->x,
			    seat->cursor->y);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

/*
 * ── THE GUEST TAKES THE POINTER ──────────────────────────────────────────
 *
 * A game and a three-dimensional editor take the pointer away from the desktop
 * and read motion as a delta. A lock stops the pointer dead; a confine lets it
 * move, inside the region the guest named. Either way this end owns where the
 * pointer is for as long as the grab lasts, and every device event must still
 * arrive. The protocol activates a constraint only for the surface that has
 * pointer focus, and this end adds one more condition — the parent must have
 * given this window the keyboard — so moving the focus is the one escape from
 * a pointer the guest has taken, and there is no chord to add and no machine
 * that can be left holding a pointer nothing can take back.
 *
 * The PARENT is told, because it draws the arrow and decides which window is
 * hovered. A console that went on moving its own pointer over a grabbed guest
 * would hover and raise windows behind the person's back.
 * ──────────────────────────────────────────────────────────────────────── */

struct cg_pointer_constraint {
	struct cg_seat *seat;
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
};

static bool
pointer_is_locked(struct cg_seat *seat)
{
	return seat->constraint &&
	       seat->constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED;
}

/*
 * THE WINDOW A CONSTRAINT IS ON, or NULL. The protocol matches a constraint
 * against the surface the pointer is over, which for any toolkit that draws its
 * content on a subsurface is a subsurface — and only a toplevel's root surface
 * carries the view. Resolved through the root or the origin answers zero, and a
 * region evaluated about the layout origin belongs to whichever window holds
 * the first span of the grid.
 */
static struct cg_view *
constraint_view(struct wlr_pointer_constraint_v1 *c)
{
	if (!c || !c->surface) {
		return NULL;
	}
	return wlr_surface_get_root_surface(c->surface)->data;
}

/*
 * WHERE A CONFINED POINTER MAY GO. The region is the guest's own coordinates
 * and the cursor is the layout's, and the window starts at the view's own
 * position: a region used without it confines the pointer to a rectangle
 * somewhere else on the layout. A delta that would carry the pointer out of
 * the region is cut at the boundary, so a guest holding the pointer inside its
 * viewport keeps it there without motion along the edge stopping dead.
 * Answers false when the pointer is not inside the region at all — there is no
 * boundary to cut against, and it stays where it is.
 */
static bool
confine_delta(struct cg_seat *seat, double dx, double dy, double *lx_out, double *ly_out)
{
	struct wlr_pointer_constraint_v1 *c = seat->constraint;
	struct cg_view *view = constraint_view(c);
	double ox = view ? view->lx : 0, oy = view ? view->ly : 0;
	double sx = seat->cursor->x - ox, sy = seat->cursor->y - oy;
	double cx, cy;

	if (!wlr_region_confine(&c->region, sx, sy, sx + dx, sy + dy, &cx, &cy)) {
		return false;
	}

	*lx_out = cx + ox;
	*ly_out = cy + oy;
	return true;
}

/*
 * WHAT THE PARENT IS TOLD, AND THE HINT IS IN THE WINDOW'S OWN PIXELS. The
 * guest names a point on its surface, the window's top-left is (0,0) on this
 * channel, and the two are the same point: an origin added here is one the
 * parent would take off again, and while the window's output sits anywhere but
 * the layout origin it is a hint a whole screen away.
 */
static void
constraint_tell(struct cg_seat *seat, struct cg_view *view, struct wlr_pointer_constraint_v1 *c)
{
	int kind = KEMBED_GRAB_NONE;
	bool hint = false;
	int hx = 0, hy = 0;

	if (c) {
		kind = c->type == WLR_POINTER_CONSTRAINT_V1_LOCKED ? KEMBED_GRAB_LOCKED
								   : KEMBED_GRAB_CONFINED;
		if (c->current.cursor_hint.enabled) {
			hint = true;
			hx = (int)c->current.cursor_hint.x;
			hy = (int)c->current.cursor_hint.y;
		}
	}

	embed_set_grab(view, kind, hint, hx, hy);
}

/*
 * A CONFINE WHOSE REGION DOES NOT HOLD THE CURSOR HAS NO BOUNDARY TO CUT
 * AGAINST, and nothing else can put the cursor back: the parent sends no
 * position while a grab is held, so wlr_region_confine() would answer false for
 * every delta and the guest would see no motion for the life of the grab. The
 * cursor is put inside before the client is told, at the hint where the guest
 * named one and at the region's centre otherwise.
 */
static void
constraint_enter(struct cg_seat *seat, struct wlr_pointer_constraint_v1 *c)
{
	struct cg_view *view;
	double ox, oy;

	if (c->type == WLR_POINTER_CONSTRAINT_V1_LOCKED || !c->surface) {
		return;
	}
	view = constraint_view(c);
	ox = view ? view->lx : 0;
	oy = view ? view->ly : 0;

	if (pixman_region32_contains_point(&c->region, (int) (seat->cursor->x - ox),
					   (int) (seat->cursor->y - oy), NULL)) {
		return;
	}

	if (c->current.cursor_hint.enabled) {
		wlr_cursor_warp_closest(seat->cursor, NULL, c->current.cursor_hint.x + ox,
					c->current.cursor_hint.y + oy);
		return;
	}

	const pixman_box32_t *box = pixman_region32_extents(&c->region);

	wlr_cursor_warp_closest(seat->cursor, NULL, ox + (box->x1 + box->x2) / 2.0,
				oy + (box->y1 + box->y2) / 2.0);
}

static void
constraint_update(struct cg_seat *seat)
{
	struct wlr_surface *surface = seat->seat->pointer_state.focused_surface;
	struct wlr_pointer_constraint_v1 *want = NULL;

	if (seat->pointer_constraints && surface) {
		want = wlr_pointer_constraints_v1_constraint_for_surface(seat->pointer_constraints, surface,
									 seat->seat);
	}

	/*
	 * THE KEYBOARD IS THE ESCAPE HATCH, so a window that does not have it
	 * does not hold the pointer either — and with a window per toplevel
	 * that is a test against THIS window rather than against any of them. A
	 * grab left active on a window the parent is not typing into is one no
	 * focus change can release, which is the machine nothing takes the
	 * pointer back from.
	 */
	if (want && seat->server->embed.active &&
	    constraint_view(want) != seat->server->embed.kbd) {
		want = NULL;
	}

	if (want == seat->constraint) {
		return;
	}

	struct wlr_pointer_constraint_v1 *had = seat->constraint;

	/*
	 * A DELTA STASHED BEFORE THE GRAB IS NOT SPENT AFTER IT. Outside a grab
	 * a delta waits for the motion it belongs to; the grab begins before
	 * that motion arrives, and a delta kept across it is spent as a jump on
	 * the first motion once the grab ends.
	 */
	seat->rel_pending = false;

	/*
	 * THE FIELD IS CLEARED BEFORE THE CLIENT IS TOLD. Deactivating a
	 * one-shot constraint destroys it, and the destroy handler reads this
	 * field: one still naming the constraint being destroyed is one the
	 * handler would deactivate a second time.
	 */
	seat->constraint = want;
	if (had) {
		constraint_tell(seat, seat->grab_view, NULL);
		seat->grab_view = NULL;
		wlr_pointer_constraint_v1_send_deactivated(had);
	}
	if (want) {
		/*
		 * THE WINDOW THE CONSTRAINT IS ON IS THE ONE TOLD ABOUT IT. The
		 * parent routes every pointer event to the window the grab was
		 * reported against and the region is evaluated about that
		 * window's origin, so a grab told against another window is
		 * deltas and a region in two different places on the grid. The
		 * guard above is what makes this the window with the keyboard.
		 */
		seat->grab_view = constraint_view(want);
		constraint_enter(seat, want);
		wlr_pointer_constraint_v1_send_activated(want);
		constraint_tell(seat, seat->grab_view, want);
	}
}

static void
handle_constraint_destroy(struct wl_listener *listener, void *data)
{
	struct cg_pointer_constraint *pc = wl_container_of(listener, pc, destroy);
	struct cg_seat *seat = pc->seat;

	if (seat->constraint == pc->constraint) {
		seat->constraint = NULL;
		constraint_tell(seat, seat->grab_view, NULL);
		seat->grab_view = NULL;
	}

	wl_list_remove(&pc->destroy.link);
	free(pc);
}

static void
handle_new_constraint(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, new_constraint);
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct cg_pointer_constraint *pc = calloc(1, sizeof(struct cg_pointer_constraint));

	if (!pc) {
		wlr_log(WLR_ERROR, "Cannot allocate pointer constraint");
		return;
	}

	pc->seat = seat;
	pc->constraint = constraint;
	pc->destroy.notify = handle_constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &pc->destroy);

	constraint_update(seat);
}

static void
process_cursor_motion(struct cg_seat *seat, uint32_t time_msec, double dx, double dy, double dx_unaccel,
		      double dy_unaccel)
{
	double sx, sy;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_surface *surface = NULL;

	struct cg_view *view = desktop_view_at(seat->server, seat->cursor->x, seat->cursor->y, &surface, &sx, &sy);
	if (!view) {
		wlr_seat_pointer_clear_focus(wlr_seat);
	} else {
		wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(wlr_seat, time_msec, sx, sy);
	}

	if (dx != 0 || dy != 0) {
		wlr_relative_pointer_manager_v1_send_relative_motion(seat->server->relative_pointer_manager, wlr_seat,
								     (uint64_t) time_msec * 1000, dx, dy, dx_unaccel,
								     dy_unaccel);
	}

	struct cg_drag_icon *drag_icon;
	wl_list_for_each (drag_icon, &seat->drag_icons, link) {
		drag_icon_update_position(drag_icon);
	}

	/* The pointer may have crossed into, or out of, a surface that holds a
	 * constraint: a constraint is the focused surface's alone. */
	constraint_update(seat);

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

/*
 * ── INPUT WITH NO INPUT DEVICE ───────────────────────────────────────────
 *
 * A headless backend has no keyboard, no pointer and no touchscreen, and an
 * embedded guest's input arrives from the cell desktop over a private channel
 * instead. It is injected HERE rather than in the transport, because these are
 * the paths a real device already takes: the keyboard group runs the xkb state
 * machine, so a key the parent sent resolves against the layout and a modifier
 * held down is held down, and the pointer path finds the surface under the
 * cursor the same way a real motion does.
 *
 * A KEY IS A SWITCH AND NOT A CHARACTER. The parent sends a press and a
 * release as two messages carrying an evdev code, so a guest holds W until the
 * release arrives, repeats from its own keymap and reads a modifier whose key
 * produces no character at all. A press with no release is a key held for
 * ever, which is why losing the keyboard releases every key still down.
 *
 * NO VIRTUAL-KEYBOARD OR VIRTUAL-POINTER PROTOCOL. Those exist so a client can
 * inject into a compositor it does not own; this compositor is the one being
 * driven, by its own parent, over a channel nobody else can reach.
 *
 * A keyboard OBJECT is still needed, and that is not a contradiction: without
 * one the seat has no keymap to send, and a client with no keymap ignores every
 * key. It is a keyboard group with no keyboards in it — a real wlr_keyboard
 * with an implementation that drives nothing.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * KEY REPEAT, AND IT IS THE GUEST'S OWN. The parent sends one press and one
 * release and nothing in between, so a key held down repeats only because the
 * guest's toolkit repeats it from these two numbers: twenty-five a second
 * after 600 ms, which is what a keyboard on this desktop is given.
 */
#define EMBED_REPEAT_RATE 25
#define EMBED_REPEAT_DELAY 600

/* Monotonic milliseconds, which is the clock every input event on a Wayland
 * seat is stamped with. */
static uint32_t embed_now_msec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/*
 * THE PARENT'S CLOCK IS THIS MACHINE'S. Parent and child are one build on one
 * machine and both read CLOCK_MONOTONIC, so a stamp that crossed the channel
 * is already in the guest's timebase — and a client compares the times of a
 * key and a click, so they must come from one clock. A message with no stamp
 * is stamped here rather than delivered at time zero, which a toolkit reads as
 * every event having happened at once.
 */
static uint32_t
embed_when(uint32_t time_msec)
{
	return time_msec ? time_msec : embed_now_msec();
}

void
seat_embed_keymap(struct cg_seat *seat, const char *text, size_t len)
{
	if (!seat || !seat->embed_keys || !text || len == 0) {
		return;
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	if (!context) {
		wlr_log(WLR_ERROR, "Unable to create XKB context");
		return;
	}

	struct xkb_keymap *keymap =
		xkb_keymap_new_from_string(context, text, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (!keymap) {
		/*
		 * A LAYOUT THAT DOES NOT COMPILE LEAVES THE ONE ALREADY SET.
		 * The guest then types the letters it was typing, which is a
		 * wrong layout; clearing the keymap instead would be a guest
		 * that ignores every key, and that is worse.
		 */
		wlr_log(WLR_ERROR, "kembed: the keymap the parent sent does not compile");
		xkb_context_unref(context);
		return;
	}

	wlr_keyboard_set_keymap(&seat->embed_keys->keyboard, keymap);
	wlr_keyboard_set_repeat_info(&seat->embed_keys->keyboard, EMBED_REPEAT_RATE, EMBED_REPEAT_DELAY);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
}

void
seat_embed_mods(struct cg_seat *seat, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	if (!seat || !seat->embed_keys) {
		return;
	}

	/*
	 * THE GROUP'S OWN KEYBOARD, NOT THE SEAT. Telling the seat directly
	 * leaves this keyboard's xkb state disagreeing with what the client was
	 * told, and the next key then resolves under the state the keyboard
	 * still believes in.
	 */
	wlr_keyboard_notify_modifiers(&seat->embed_keys->keyboard, depressed, latched, locked, group);
}

void
seat_embed_key(struct cg_seat *seat, uint32_t keycode, bool pressed, uint32_t time_msec)
{
	if (!seat || !seat->embed_keys)
		return;

	struct wlr_keyboard_key_event event = {
		.time_msec = embed_when(time_msec),
		.keycode = keycode,
		.update_state = true,
		.state = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
				 : WL_KEYBOARD_KEY_STATE_RELEASED,
	};

	/*
	 * THE CODE IS EVDEV'S AND IS NOT SHIFTED BY EIGHT. That is what a
	 * wl_keyboard carries and what this event takes; the eight is xkb's
	 * own offset and wlroots adds it where it runs the state machine. A
	 * code shifted here would be shifted twice, and every letter a guest
	 * typed — Xwayland's included, because the X server reads the same
	 * keymap this seat sends — would be the wrong one.
	 */
	wlr_keyboard_notify_key(&seat->embed_keys->keyboard, &event);
}

/*
 * EVERY KEY THE GUEST STILL HOLDS, RELEASED.
 *
 * A press and its release are two messages, so a window that loses the
 * keyboard between them holds that key for ever: the guest repeats it, or
 * reads Ctrl as down for the rest of its life. The parent stops sending keys
 * to a window it has taken the keyboard from, so this end is the only one that
 * can end them.
 *
 * The codes are copied first, because releasing one removes it from the array
 * being walked.
 */
static void
embed_release_keys(struct cg_seat *seat)
{
	struct wlr_keyboard *keyboard = &seat->embed_keys->keyboard;
	uint32_t held[WLR_KEYBOARD_KEYS_CAP];
	size_t n = keyboard->num_keycodes;

	if (n > WLR_KEYBOARD_KEYS_CAP) {
		n = WLR_KEYBOARD_KEYS_CAP;
	}
	memcpy(held, keyboard->keycodes, n * sizeof(held[0]));

	for (size_t i = 0; i < n; i++) {
		struct wlr_keyboard_key_event event = {
			.time_msec = embed_now_msec(),
			.keycode = held[i],
			.update_state = true,
			.state = WL_KEYBOARD_KEY_STATE_RELEASED,
		};

		wlr_keyboard_notify_key(keyboard, &event);
	}
}

void
seat_embed_focus(struct cg_seat *seat, struct cg_view *view)
{
	if (!seat) {
		return;
	}

	if (!view) {
		struct cg_view *had = seat_get_focus(seat);

		if (had) {
			view_activate(had, false);
		}
		if (seat->embed_keys) {
			embed_release_keys(seat);
		}
		wlr_seat_keyboard_notify_clear_focus(seat->seat);
		constraint_update(seat);
		return;
	}

	/*
	 * THE PARENT NAMED THE WINDOW AND NOTHING HERE GUESSES AT ANOTHER. A
	 * window this end chose instead is one the parent is not sending keys
	 * for and not releasing them against; the drain has already dropped a
	 * focus naming a window that does not exist.
	 */
	seat_set_focus(seat, view);
	constraint_update(seat);
}

void
seat_embed_leave(struct cg_seat *seat)
{
	if (!seat) {
		return;
	}

	/*
	 * A CLEARED POINTER FOCUS AND A FRAME, NOT A MOTION TO SOMEWHERE ELSE.
	 * The cursor warp clamps to the layout and the view covers it, so a
	 * position outside the window lands on a view edge and is delivered as
	 * an ENTER — and a guest never told the pointer left keeps the link it
	 * last crossed lit and a button looking pressed for as long as it is
	 * open.
	 */
	wlr_seat_pointer_clear_focus(seat->seat);
	wlr_seat_pointer_notify_frame(seat->seat);
	/*
	 * AND THE ARROW ITSELF COMES OFF. This compositor's cursor is drawn
	 * into the buffer the parent composites, so one left behind is a second
	 * pointer on the desktop at the place the real one left the window. The
	 * parent draws its own from here on, and seat_embed_show_pointer() puts
	 * this one back at the position that brings the pointer in.
	 */
	if (!seat->ptr_hidden) {
		seat->ptr_hidden = true;
		wlr_cursor_unset_image(seat->cursor);
	}
	constraint_update(seat);
}

/*
 * THE ARROW IS BACK, at the default shape. A client sets its own cursor from
 * the enter event that the motion below raises, so the shape it wants replaces
 * this one within the same frame group; a guest that sets none keeps the
 * arrow every compositor draws.
 */
static void
seat_embed_show_pointer(struct cg_seat *seat)
{
	if (!seat->ptr_hidden) {
		return;
	}
	seat->ptr_hidden = false;
	wlr_cursor_set_xcursor(seat->cursor, seat->xcursor_manager, DEFAULT_XCURSOR);
}

void
seat_embed_motion(struct cg_seat *seat, struct cg_view *view, double x, double y, uint32_t time_msec)
{
	if (!seat || !view)
		return;

	/*
	 * THE WINDOW'S ORIGIN IS ADDED HERE, AND HERE ALONE. (0,0) on the
	 * channel is this window's top-left; its output sits somewhere else on
	 * the layout, and the cursor, the scene hit test and the confine region
	 * are all in layout coordinates.
	 */
	double lx = x + view->lx;
	double ly = y + view->ly;

	/*
	 * A GRABBED POINTER IS NOT PLACED FROM OUTSIDE. A lock asked for the
	 * pointer to stay where it is and to be read as deltas, so a warp here
	 * would fight the lock and the guest would read the difference as
	 * motion of its own making; under a confine this end walks the cursor
	 * itself and the parent's positions are the ones it stopped at when the
	 * grab began. The parent sends no position while it holds a grab, and
	 * the one message that still carries one is the button — at the place
	 * the guest last saw, which is not where either kind of grab has since
	 * put the pointer.
	 */
	if (seat->constraint)
		return;

	double dx = lx - seat->cursor->x;
	double dy = ly - seat->cursor->y;
	double dx_unaccel = dx, dy_unaccel = dy;

	/*
	 * THE DEVICE'S OWN DELTA, WHERE THE PARENT SENT ONE. Two positions
	 * differ by the pointer's speed after acceleration, clamping and the
	 * desktop's cell grid; a guest reading relative motion asked what the
	 * mouse did, and the parent sends that first for exactly this.
	 */
	if (seat->rel_pending) {
		dx = seat->rel_dx;
		dy = seat->rel_dy;
		dx_unaccel = seat->rel_dx_un;
		dy_unaccel = seat->rel_dy_un;
		seat->rel_pending = false;
	}

	seat_embed_show_pointer(seat);
	wlr_cursor_warp_closest(seat->cursor, NULL, lx, ly);
	process_cursor_motion(seat, embed_when(time_msec), dx, dy, dx_unaccel, dy_unaccel);
	/*
	 * AND THE FRAME THAT ENDS THE GROUP. Pointer events are a group a
	 * client acts on when the frame arrives, and a real device's group is
	 * closed by the cursor's own frame signal — which nothing raises here,
	 * because there is no device to raise it. Without this a toolkit that
	 * honours the frame, which is every current one, accumulates the enter
	 * and the motion and applies neither: the guest's hover lights nothing
	 * and its buttons answer nothing, while the arrow drawn over it moves
	 * normally.
	 */
	wlr_seat_pointer_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

void
seat_embed_rel(struct cg_seat *seat, double dx, double dy, double dx_unaccel, double dy_unaccel,
	       uint32_t time_msec)
{
	if (!seat)
		return;

	/*
	 * UNDER A GRAB THERE IS NOTHING ELSE COMING, WHICHEVER KIND IT IS. The
	 * parent stops sending positions the moment a guest takes the pointer —
	 * for a confine as much as for a lock — so a delta stashed here would
	 * wait for a motion that never arrives and be overwritten by the next
	 * one. The delta is spent where it is received instead.
	 */
	if (seat->constraint) {
		uint32_t when = embed_when(time_msec);

		wlr_relative_pointer_manager_v1_send_relative_motion(seat->server->relative_pointer_manager,
								     seat->seat, (uint64_t) when * 1000, dx, dy,
								     dx_unaccel, dy_unaccel);

		/*
		 * A LOCK DOES NOT MOVE THE POINTER AND A CONFINE DOES. A lock
		 * gets the delta and nothing else: an enter or a motion would
		 * tell the guest the pointer moved when the whole point is that
		 * it did not. A confine asked only that the pointer not leave
		 * the region it named, so nothing but this walks the cursor —
		 * a guest that confines rather than locks, which is a walk
		 * navigation or a toolkit stopping a drag from leaving, would
		 * otherwise see no motion at all for as long as it holds it.
		 */
		double lx, ly;

		if (!pointer_is_locked(seat) && confine_delta(seat, dx, dy, &lx, &ly)) {
			wlr_cursor_warp_closest(seat->cursor, NULL, lx, ly);
			/* The deltas are spent above; a second pair here is one
			 * device movement reported twice. */
			process_cursor_motion(seat, when, 0, 0, 0, 0);
		}

		wlr_seat_pointer_notify_frame(seat->seat);
		wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
		return;
	}

	/* Otherwise the motion for the same physical event follows, and spends
	 * it. A delta with no motion after it is one the pointer never made. */
	seat->rel_dx = dx;
	seat->rel_dy = dy;
	seat->rel_dx_un = dx_unaccel;
	seat->rel_dy_un = dy_unaccel;
	seat->rel_pending = true;
}

void
seat_embed_button(struct cg_seat *seat, uint32_t button, bool pressed,
		  uint32_t time_msec)
{
	if (!seat)
		return;

	enum wl_pointer_button_state state = pressed
		? WL_POINTER_BUTTON_STATE_PRESSED
		: WL_POINTER_BUTTON_STATE_RELEASED;
	uint32_t when = embed_when(time_msec);

	wlr_seat_pointer_notify_button(seat->seat, when, button, state);
	press_cursor_button(seat, NULL, when, button, state,
			    seat->cursor->x, seat->cursor->y);
	/* The frame that ends the group; see seat_embed_motion(). A press with
	 * no frame after it is one the client has been told about and has not
	 * been told to act on. */
	wlr_seat_pointer_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

void
seat_embed_axis(struct cg_seat *seat, double value, int32_t value120, int axis, int source, bool inverted,
		uint32_t time_msec)
{
	if (!seat)
		return;

	/* One is the horizontal axis; see kembed.h. A cell desktop has no
	 * horizontal scroll to give, and a guest with a wide document does. */
	enum wl_pointer_axis orientation = axis != 0 ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
						     : WL_POINTER_AXIS_VERTICAL_SCROLL;
	enum wl_pointer_axis_source wl_source;

	/*
	 * THE PROTOCOL'S NUMBERS ARE ITS OWN AND ARE MAPPED HERE. kdos-con
	 * links no Wayland, so a value that happened to equal an upstream enum
	 * would be a coupling neither end could see.
	 */
	switch (source) {
	case KEMBED_AXIS_FINGER:
		wl_source = WL_POINTER_AXIS_SOURCE_FINGER;
		break;
	case KEMBED_AXIS_CONTINUOUS:
		wl_source = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
		break;
	case KEMBED_AXIS_WHEEL_TILT:
		wl_source = WL_POINTER_AXIS_SOURCE_WHEEL_TILT;
		break;
	default:
		wl_source = WL_POINTER_AXIS_SOURCE_WHEEL;
		break;
	}

	/*
	 * BOTH NUMBERS, because a client reads one or the other and never both:
	 * value120 is what a modern toolkit steps by and the continuous value
	 * is what one that predates it scrolls by. A value of zero on both is
	 * the end of the gesture — the finger left the pad — and is delivered
	 * rather than dropped, because it is what stops a guest's kinetic
	 * scrolling.
	 */
	wlr_seat_pointer_notify_axis(seat->seat, embed_when(time_msec), orientation, value, value120, wl_source,
				     inverted ? WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED
					      : WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
	wlr_seat_pointer_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

/*
 * THE KEYBOARD THE EMBEDDED GUEST SEES. Created once, with the keymap xkb
 * builds from the environment — which is what a guest reads until the parent
 * sends the layout its own view is running, and what one whose view has no
 * keyboard of its own reads for the life of the window.
 */
void
seat_embed_enable(struct cg_seat *seat)
{
	if (!seat || seat->embed_keys)
		return;

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	if (!context) {
		wlr_log(WLR_ERROR, "Unable to create XKB context");
		return;
	}

	struct xkb_keymap *keymap =
		xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (!keymap) {
		wlr_log(WLR_ERROR, "Unable to configure keyboard: keymap failed");
		xkb_context_unref(context);
		return;
	}

	struct cg_keyboard_group *group = calloc(1, sizeof(*group));

	if (!group) {
		xkb_keymap_unref(keymap);
		xkb_context_unref(context);
		return;
	}

	group->seat = seat;
	group->is_virtual = false;
	group->wlr_group = wlr_keyboard_group_create();
	if (!group->wlr_group) {
		free(group);
		xkb_keymap_unref(keymap);
		xkb_context_unref(context);
		return;
	}

	group->wlr_group->data = group;
	wlr_keyboard_set_keymap(&group->wlr_group->keyboard, keymap);
	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard, EMBED_REPEAT_RATE, EMBED_REPEAT_DELAY);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wl_list_insert(&seat->keyboard_groups, &group->link);
	wl_signal_add(&group->wlr_group->keyboard.events.key, &group->key);
	group->key.notify = handle_keyboard_group_key;
	wl_signal_add(&group->wlr_group->keyboard.events.modifiers, &group->modifiers);
	group->modifiers.notify = handle_keyboard_group_modifiers;

	seat->embed_keys = group->wlr_group;
	wlr_seat_set_keyboard(seat->seat, &group->wlr_group->keyboard);

	/*
	 * POINTER CONSTRAINTS ARE ADVERTISED ONLY WHERE THEY ARE HONOURED.
	 * This end can stop its own cursor and tell the parent to stop drawing
	 * its arrow; a cage on a terminal of its own has a real device whose
	 * motion it does not intercept, so a lock announced there would be a
	 * client told its grab took while the pointer kept moving.
	 */
	seat->pointer_constraints = wlr_pointer_constraints_v1_create(seat->server->wl_display);
	if (seat->pointer_constraints) {
		seat->new_constraint.notify = handle_new_constraint;
		wl_signal_add(&seat->pointer_constraints->events.new_constraint, &seat->new_constraint);
	}

	update_capabilities(seat);
}

static void
handle_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->pointer->base, event->x, event->y, &lx, &ly);

	double dx = lx - seat->cursor->x;
	double dy = ly - seat->cursor->y;

	wlr_cursor_warp_absolute(seat->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(seat, event->time_msec, dx, dy, dx, dy);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_motion_relative(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_motion_relative);
	struct wlr_pointer_motion_event *event = data;

	wlr_cursor_move(seat->cursor, &event->pointer->base, event->delta_x, event->delta_y);
	process_cursor_motion(seat, event->time_msec, event->delta_x, event->delta_y, event->unaccel_dx,
			      event->unaccel_dy);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
drag_icon_update_position(struct cg_drag_icon *drag_icon)
{
	struct wlr_drag_icon *wlr_icon = drag_icon->wlr_drag_icon;
	struct cg_seat *seat = drag_icon->seat;
	struct wlr_touch_point *point;

	switch (wlr_icon->drag->grab_type) {
	case WLR_DRAG_GRAB_KEYBOARD:
		return;
	case WLR_DRAG_GRAB_KEYBOARD_POINTER:
		drag_icon->lx = seat->cursor->x;
		drag_icon->ly = seat->cursor->y;
		break;
	case WLR_DRAG_GRAB_KEYBOARD_TOUCH:
		point = wlr_seat_touch_get_point(seat->seat, wlr_icon->drag->touch_id);
		if (!point) {
			return;
		}
		drag_icon->lx = seat->touch_lx;
		drag_icon->ly = seat->touch_ly;
		break;
	}

	wlr_scene_node_set_position(&drag_icon->scene_tree->node, drag_icon->lx, drag_icon->ly);
}

static void
handle_drag_icon_destroy(struct wl_listener *listener, void *data)
{
	struct cg_drag_icon *drag_icon = wl_container_of(listener, drag_icon, destroy);

	wl_list_remove(&drag_icon->link);
	wl_list_remove(&drag_icon->destroy.link);
	wlr_scene_node_destroy(&drag_icon->scene_tree->node);
	free(drag_icon);
}

static void
handle_request_start_drag(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat->seat, event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag, event->serial);
		return;
	}

	struct wlr_touch_point *point;
	if (wlr_seat_validate_touch_grab_serial(seat->seat, event->origin, event->serial, &point)) {
		wlr_seat_start_touch_drag(seat->seat, event->drag, event->serial, point);
		return;
	}

	// TODO: tablet grabs
	wlr_log(WLR_DEBUG, "Ignoring start_drag request: could not validate pointer/touch serial %" PRIu32,
		event->serial);
	wlr_data_source_destroy(event->drag->source);
}

static void
handle_start_drag(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;
	struct wlr_drag_icon *wlr_drag_icon = wlr_drag->icon;
	if (wlr_drag_icon == NULL) {
		return;
	}

	struct cg_drag_icon *drag_icon = calloc(1, sizeof(struct cg_drag_icon));
	if (!drag_icon) {
		return;
	}
	drag_icon->seat = seat;
	drag_icon->wlr_drag_icon = wlr_drag_icon;
	drag_icon->scene_tree = wlr_scene_subsurface_tree_create(&seat->server->scene->tree, wlr_drag_icon->surface);
	if (!drag_icon->scene_tree) {
		free(drag_icon);
		return;
	}

	drag_icon->destroy.notify = handle_drag_icon_destroy;
	wl_signal_add(&wlr_drag_icon->events.destroy, &drag_icon->destroy);

	wl_list_insert(&seat->drag_icons, &drag_icon->link);

	drag_icon_update_position(drag_icon);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct cg_seat *seat = wl_container_of(listener, seat, destroy);
	wl_list_remove(&seat->destroy.link);
	wl_list_remove(&seat->cursor_motion_relative.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);
	wl_list_remove(&seat->touch_down.link);
	wl_list_remove(&seat->touch_up.link);
	wl_list_remove(&seat->touch_motion.link);
	wl_list_remove(&seat->touch_frame.link);
	wl_list_remove(&seat->request_set_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);
	wl_list_remove(&seat->request_set_primary_selection.link);
	if (seat->pointer_constraints) {
		wl_list_remove(&seat->new_constraint.link);
	}

	struct cg_keyboard_group *group, *group_tmp;
	wl_list_for_each_safe (group, group_tmp, &seat->keyboard_groups, link) {
		wlr_keyboard_group_destroy(group->wlr_group);
		free(group);
	}
	struct cg_pointer *pointer, *pointer_tmp;
	wl_list_for_each_safe (pointer, pointer_tmp, &seat->pointers, link) {
		handle_pointer_destroy(&pointer->destroy, NULL);
	}
	struct cg_touch *touch, *touch_tmp;
	wl_list_for_each_safe (touch, touch_tmp, &seat->touch, link) {
		handle_touch_destroy(&touch->destroy, NULL);
	}
	wl_list_remove(&seat->new_input.link);

	wlr_xcursor_manager_destroy(seat->xcursor_manager);
	if (seat->cursor) {
		wlr_cursor_destroy(seat->cursor);
	}
	free(seat);
}

struct cg_seat *
seat_create(struct cg_server *server, struct wlr_backend *backend)
{
	struct cg_seat *seat = calloc(1, sizeof(struct cg_seat));
	if (!seat) {
		wlr_log(WLR_ERROR, "Cannot allocate seat");
		return NULL;
	}

	seat->seat = wlr_seat_create(server->wl_display, "seat0");
	if (!seat->seat) {
		wlr_log(WLR_ERROR, "Cannot allocate seat0");
		free(seat);
		return NULL;
	}
	seat->server = server;
	seat->destroy.notify = handle_destroy;
	wl_signal_add(&seat->seat->events.destroy, &seat->destroy);

	seat->cursor = wlr_cursor_create();
	if (!seat->cursor) {
		wlr_log(WLR_ERROR, "Unable to create cursor");
		wl_list_remove(&seat->destroy.link);
		free(seat);
		return NULL;
	}
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	if (!seat->xcursor_manager) {
		seat->xcursor_manager = wlr_xcursor_manager_create(NULL, XCURSOR_SIZE);
		if (!seat->xcursor_manager) {
			wlr_log(WLR_ERROR, "Cannot create XCursor manager");
			wlr_cursor_destroy(seat->cursor);
			wl_list_remove(&seat->destroy.link);
			free(seat);
			return NULL;
		}
	}

	seat->cursor_motion_relative.notify = handle_cursor_motion_relative;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion_relative);
	seat->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute, &seat->cursor_motion_absolute);
	seat->cursor_button.notify = handle_cursor_button;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
	seat->cursor_axis.notify = handle_cursor_axis;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
	seat->cursor_frame.notify = handle_cursor_frame;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

	seat->touch_down.notify = handle_touch_down;
	wl_signal_add(&seat->cursor->events.touch_down, &seat->touch_down);
	seat->touch_up.notify = handle_touch_up;
	wl_signal_add(&seat->cursor->events.touch_up, &seat->touch_up);
	seat->touch_motion.notify = handle_touch_motion;
	wl_signal_add(&seat->cursor->events.touch_motion, &seat->touch_motion);
	seat->touch_frame.notify = handle_touch_frame;
	wl_signal_add(&seat->cursor->events.touch_frame, &seat->touch_frame);

	seat->request_set_cursor.notify = handle_request_set_cursor;
	wl_signal_add(&seat->seat->events.request_set_cursor, &seat->request_set_cursor);
	seat->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&seat->seat->events.request_set_selection, &seat->request_set_selection);
	seat->request_set_primary_selection.notify = handle_request_set_primary_selection;
	wl_signal_add(&seat->seat->events.request_set_primary_selection, &seat->request_set_primary_selection);

	wl_list_init(&seat->keyboards);
	wl_list_init(&seat->keyboard_groups);
	wl_list_init(&seat->pointers);
	wl_list_init(&seat->touch);

	seat->new_input.notify = handle_new_input;
	wl_signal_add(&backend->events.new_input, &seat->new_input);

	server->new_virtual_keyboard.notify = handle_virtual_keyboard;
	server->new_virtual_pointer.notify = handle_virtual_pointer;

	wl_list_init(&seat->drag_icons);
	seat->request_start_drag.notify = handle_request_start_drag;
	wl_signal_add(&seat->seat->events.request_start_drag, &seat->request_start_drag);
	seat->start_drag.notify = handle_start_drag;
	wl_signal_add(&seat->seat->events.start_drag, &seat->start_drag);

	return seat;
}

void
seat_destroy(struct cg_seat *seat)
{
	if (!seat) {
		return;
	}

	wl_list_remove(&seat->request_start_drag.link);
	wl_list_remove(&seat->start_drag.link);

	struct cg_keyboard_group *keyboard_group, *keyboard_group_tmp;
	wl_list_for_each_safe (keyboard_group, keyboard_group_tmp, &seat->keyboard_groups, link) {
		keyboard_group_destroy(keyboard_group);
	}

	// Destroying the wlr seat will trigger the destroy handler on our seat,
	// which will in turn free it.
	wlr_seat_destroy(seat->seat);
}

struct cg_view *
seat_get_focus(struct cg_seat *seat)
{
	struct wlr_surface *prev_surface = seat->seat->keyboard_state.focused_surface;
	if (!prev_surface) {
		return NULL;
	}
	return view_from_wlr_surface(prev_surface);
}

void
seat_set_focus(struct cg_seat *seat, struct cg_view *view)
{
	struct cg_server *server = seat->server;
	struct wlr_seat *wlr_seat = seat->seat;
	struct cg_view *prev_view = seat_get_focus(seat);

	if (!view || prev_view == view) {
		return;
	}

#if CAGE_HAS_XWAYLAND
	/*
	 * THE GUESS IS FOR AN OVERRIDE-REDIRECT SURFACE AND FOR NOTHING ELSE.
	 * wlroots answers false for every window type a menu, a tooltip, a
	 * splash or a UTILITY carries, and a managed toplevel of any of those
	 * types is still a window the parent may hand the keyboard to — a GIMP
	 * dock under Xwayland is _NET_WM_WINDOW_TYPE_UTILITY. Dropping the
	 * parent's instruction there leaves the guest with no keyboard focus at
	 * all while the parent believes a window has it.
	 */
	if (view->type == CAGE_XWAYLAND_VIEW) {
		struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
		struct wlr_xwayland_surface *xs = xwayland_view->xwayland_surface;

		if (xs->override_redirect && !wlr_xwayland_surface_override_redirect_wants_focus(xs)) {
			return;
		}
	}
#endif

	if (prev_view) {
		view_activate(prev_view, false);
	}

	/* Move the view to the front, but only if it isn't a
	   fullscreen view. */
	if (!view_is_primary(view)) {
		wl_list_remove(&view->link);
		wl_list_insert(&server->views, &view->link);
	}

	view_activate(view, true);
	char *title = view_get_title(view);
	struct cg_output *output;
	wl_list_for_each (output, &server->outputs, link) {
		output_set_window_title(output, title);
	}
	free(title);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wlr_seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(wlr_seat, view->wlr_surface, keyboard->keycodes, keyboard->num_keycodes,
					       &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(wlr_seat, view->wlr_surface, NULL, 0, NULL);
	}

	process_cursor_motion(seat, -1, 0, 0, 0, 0);
}

void
seat_center_cursor(struct cg_seat *seat)
{
	struct cg_server *server = seat->server;
	struct wlr_box layout_box;

	/*
	 * THE CENTRE OF THE ANCHOR AND NOT OF THE UNION, when this cage is a
	 * window. Windows sit in spans that are wider than they are, so the
	 * centre of everything is usually between two of them — over no output
	 * at all, where nothing is hovered until the parent sends a position.
	 */
	if (server->embed.embedded && server->embed.anchor) {
		wlr_output_layout_get_box(server->output_layout, server->embed.anchor->wlr_output,
					  &layout_box);
		wlr_cursor_warp(seat->cursor, NULL, layout_box.x + layout_box.width / 2,
				layout_box.y + layout_box.height / 2);
		return;
	}

	/* Place the cursor in the center of the output layout. */
	wlr_output_layout_get_box(server->output_layout, NULL, &layout_box);
	wlr_cursor_warp(seat->cursor, NULL, layout_box.width / 2, layout_box.height / 2);
}

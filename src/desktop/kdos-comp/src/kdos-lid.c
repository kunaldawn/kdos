// SPDX-License-Identifier: GPL-2.0-only
/*
 * The laptop lid, graft-shaped like kdos-idle.c beside it. There is no
 * acpid on KDOS and nothing else sees the switch: libinput delivers it
 * as a wlr_switch, labwc's seat has no case for that device type, and a
 * closed lid did precisely nothing.
 *
 * The policy is one key, `lid_close = off|lock|suspend`:
 *   off      screen off only — the clamshell-with-external-monitor case
 *   lock     lock, then screen off
 *   suspend  hand it to kdos-power, whose job the lock-before-suspend
 *            handshake already is
 * Opening the lid powers outputs back on and counts as activity; it
 * never unlocks.
 *
 * This file takes its devices from its OWN listener on the backend's
 * new_input signal rather than a hook in seat.c: a switch is not a seat
 * capability and joins no wl_seat, so there is nothing for the seat to
 * own. labwc's "unsupported input device" log line still fires — one
 * INFO line beats a hook in a file this fork otherwise leaves alone.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/util/log.h>

#include "common/spawn.h"
#include "kdos.h"
#include "labwc.h"
#include "session-lock.h"

struct kdos_lid {
	struct wl_list link;
	struct wlr_switch *sw;
	struct wl_listener toggle;
	struct wl_listener destroy;
};

static struct wl_list lids = { .prev = &lids, .next = &lids };
static struct wl_listener lid_new_input;
static bool lid_listening;
static bool lid_closed;

static void
handle_lid_toggle(struct wl_listener *listener, void *data)
{
	struct wlr_switch_toggle_event *ev = data;
	(void)listener;
	if (ev->switch_type != WLR_SWITCH_TYPE_LID) {
		return;
	}

	if (ev->switch_state == WLR_SWITCH_STATE_ON) {
		/* debounce: two switches (or a repeated event) must not
		 * spawn two suspends */
		if (lid_closed) {
			return;
		}
		lid_closed = true;
		switch (kdos_conf.lid_close) {
		case KDOS_LID_SUSPEND:
			wlr_log(WLR_INFO, "lid closed — suspending "
				"(lid_close = suspend)");
			spawn_async_no_shell("kdos-power suspend");
			break;
		case KDOS_LID_LOCK:
			wlr_log(WLR_INFO, "lid closed — locking "
				"(lid_close = lock)");
			if (!server.session_lock_manager
					|| !server.session_lock_manager->locked) {
				spawn_async_no_shell("kdos-lock");
			}
			kdos_idle_outputs_power(false);
			break;
		case KDOS_LID_OFF:
			wlr_log(WLR_INFO, "lid closed — outputs off "
				"(lid_close = off)");
			kdos_idle_outputs_power(false);
			break;
		}
	} else {
		if (!lid_closed) {
			return;
		}
		lid_closed = false;
		wlr_log(WLR_INFO, "lid opened");
		kdos_idle_outputs_power(true);
		kdos_idle_activity();
	}
}

static void
handle_lid_destroy(struct wl_listener *listener, void *data)
{
	struct kdos_lid *lid = wl_container_of(listener, lid, destroy);
	(void)data;
	wl_list_remove(&lid->toggle.link);
	wl_list_remove(&lid->destroy.link);
	wl_list_remove(&lid->link);
	free(lid);
}

static void
handle_lid_new_input(struct wl_listener *listener, void *data)
{
	struct wlr_input_device *device = data;
	(void)listener;
	if (device->type != WLR_INPUT_DEVICE_SWITCH) {
		return;
	}

	struct kdos_lid *lid = calloc(1, sizeof(*lid));
	if (!lid) {
		return;
	}
	lid->sw = wlr_switch_from_input_device(device);
	lid->toggle.notify = handle_lid_toggle;
	wl_signal_add(&lid->sw->events.toggle, &lid->toggle);
	lid->destroy.notify = handle_lid_destroy;
	wl_signal_add(&device->events.destroy, &lid->destroy);
	wl_list_insert(&lids, &lid->link);
	wlr_log(WLR_INFO, "lid: watching switch device %s",
		device->name ? device->name : "(unnamed)");
}

/* Same shape as the idle VM gate, and re-run from kdos_conf_reload(). */
void
kdos_lid_reconfigure(void)
{
	if (!kdos_conf.lid_configured && kdos_in_vm()) {
		kdos_conf.lid_close = KDOS_LID_OFF;
	}
}

void
kdos_lid_init(void)
{
	kdos_lid_reconfigure();

	lid_new_input.notify = handle_lid_new_input;
	wl_signal_add(&server.backend->events.new_input, &lid_new_input);
	lid_listening = true;

	static const char *const names[] = { "off", "lock", "suspend" };
	wlr_log(WLR_INFO, "lid_close = %s%s", names[kdos_conf.lid_close],
		!kdos_conf.lid_configured && kdos_in_vm()
			? " (a VM, and no lid_close in comp.conf)" : "");
}

void
kdos_lid_finish(void)
{
	struct kdos_lid *lid, *tmp;
	wl_list_for_each_safe(lid, tmp, &lids, link) {
		wl_list_remove(&lid->toggle.link);
		wl_list_remove(&lid->destroy.link);
		wl_list_remove(&lid->link);
		free(lid);
	}
	wl_list_init(&lids);
	if (lid_listening) {
		wl_list_remove(&lid_new_input.link);
		lid_listening = false;
	}
}

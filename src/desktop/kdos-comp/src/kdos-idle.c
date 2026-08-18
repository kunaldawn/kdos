// SPDX-License-Identifier: GPL-2.0-only
/*
 * The idle POLICY — dim, then lock, then outputs off — ported from the
 * pre-fork kdos-comp. labwc's idle.c already owns the protocol half
 * (ext-idle-notify + idle-inhibit); this file is only the policy, and
 * it lives in the compositor because every step needs something only
 * the compositor has: the scene graph to dim, the output state to
 * blank, the input path to rearm on.
 *
 * Rules carried over:
 * - Stages are cumulative and each is measured from the LAST ACTIVITY,
 *   not from the previous stage: dim 300 / lock 600 / off 900 means
 *   what it says.
 * - Activity ends the dim and powers outputs back on. It NEVER
 *   unlocks — only a password does that.
 * - An inhibitor stops the policy dead rather than pausing it.
 * - OFF BY DEFAULT IN A VM (DMI sys_vendor): a blanked screen over VNC
 *   is indistinguishable from a crashed compositor and already cost an
 *   afternoon. Any idle_* key in comp.conf overrides — including a 0.
 *
 * Hooks in upstream files: one call in idle_manager_notify_activity()
 * (the single funnel every input path already goes through), and one
 * in each idle-inhibitor handler.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include <wlr/config.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "common/spawn.h"
#include "kdos.h"
#include "labwc.h"
#include "output.h"
#include "session-lock.h"

/*
 * The dim is a translucent black rect, not a gamma change: gamma is
 * per-output, needs a DRM capability headless/nested backends lack,
 * and cannot be undone if the compositor dies with it applied.
 */
#define KDOS_DIM_ALPHA 0.55f

/* One heads-up this long before the LOCK stage — a lock with no warning
 * eats the sentence someone was typing. */
#define KDOS_LOCK_WARN_S 10

static struct wlr_scene_rect *dim_rect;
static struct wl_event_source *idle_timer;
static int idle_stage;		/* next stage to fire: 0 dim, 1 lock, 2 off */
static int ninhibit;
static bool warn_armed;		/* the timer's next fire is the warning */
static bool lock_warned;	/* one warning per idle spell */

static void
raise_locks_above_dim(void)
{
	/* the dim must never cover a lock surface's password prompt */
	struct output *o;
	wl_list_for_each(o, &server.outputs, link) {
		wlr_scene_node_raise_to_top(&o->session_lock_tree->node);
	}
}

static void
dim_set(bool on)
{
	if (!on) {
		if (dim_rect) {
			wlr_scene_node_destroy(&dim_rect->node);
			dim_rect = NULL;
		}
		return;
	}
	if (dim_rect) {
		return;
	}

	struct wlr_box box = { 0 };
	wlr_output_layout_get_box(server.output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0) {
		return;
	}

	const float shade[4] = { 0.0f, 0.0f, 0.0f, KDOS_DIM_ALPHA };
	dim_rect = wlr_scene_rect_create(&server.scene->tree,
		box.width, box.height, shade);
	if (!dim_rect) {
		return;
	}
	wlr_scene_node_set_position(&dim_rect->node, box.x, box.y);
	wlr_scene_node_raise_to_top(&dim_rect->node);
	raise_locks_above_dim();
}

/*
 * `enabled` on a wlr_output is the only portable "off" there is. A failed
 * commit is LOGGED — an output that will not come back on is the worst
 * silent failure this file can have — and a failed re-enable is retried on
 * the next activity, because the user's next keypress is exactly when a
 * still-black screen is being stared at.
 */
static bool power_reassert;

static void
outputs_power(bool on)
{
	struct output *o;
	wl_list_for_each(o, &server.outputs, link) {
		struct wlr_output_state st;
		wlr_output_state_init(&st);
		wlr_output_state_set_enabled(&st, on);
		if (!wlr_output_commit_state(o->wlr_output, &st)) {
			wlr_log(WLR_ERROR, "idle: cannot power %s %s",
				o->wlr_output->name, on ? "on" : "off");
			if (on) {
				power_reassert = true;
			}
		}
		wlr_output_state_finish(&st);
	}
}

void
kdos_idle_outputs_power(bool on)
{
	outputs_power(on);
}

/* ── the timer ──────────────────────────────────────────────────── */

static int
next_deadline(void)
{
	const int at[] = { kdos_conf.idle_dim_s, kdos_conf.idle_lock_s,
		kdos_conf.idle_off_s };
	for (int stage = idle_stage; stage < 3; stage++) {
		if (at[stage] > 0) {
			return at[stage];
		}
	}
	return 0;
}

static void
arm(void)
{
	warn_armed = false;
	if (!idle_timer) {
		return;
	}
	int next = next_deadline();
	/* an inhibitor stops the policy dead rather than pausing it */
	if (next <= 0 || ninhibit > 0) {
		wl_event_source_timer_update(idle_timer, 0);
		return;
	}

	/* stages measure from last activity; the delay to the next one
	 * is the difference between their thresholds */
	int reached = 0;
	const int at[] = { kdos_conf.idle_dim_s, kdos_conf.idle_lock_s,
		kdos_conf.idle_off_s };
	for (int i = 0; i < idle_stage; i++) {
		if (at[i] > reached) {
			reached = at[i];
		}
	}

	/* the stage the timer is running toward */
	int fire_stage = idle_stage;
	while (fire_stage < 3 && at[fire_stage] <= 0) {
		fire_stage++;
	}
	/* the warning is a stop on the way to LOCK, not a stage: it moves
	 * no state and activity between it and the lock cancels nothing
	 * but the warning itself. `reached` measures from the point arm()
	 * ran, so a fired warning IS a reached threshold — without that,
	 * the rearm after it pushed the lock a whole interval out. */
	if (fire_stage == 1) {
		if (!lock_warned && next - reached > KDOS_LOCK_WARN_S) {
			next -= KDOS_LOCK_WARN_S;
			warn_armed = true;
		} else if (lock_warned
				&& at[1] - KDOS_LOCK_WARN_S > reached) {
			reached = at[1] - KDOS_LOCK_WARN_S;
		}
	}

	int delay = next - reached;
	if (delay < 1) {
		delay = 1;
	}
	wl_event_source_timer_update(idle_timer, delay * 1000);
}

static int
idle_tick(void *data)
{
	(void)data;
	const int at[] = { kdos_conf.idle_dim_s, kdos_conf.idle_lock_s,
		kdos_conf.idle_off_s };
	int stage = idle_stage;

	if (warn_armed) {
		warn_armed = false;
		lock_warned = true;
		/* through the notification daemon the compositor already
		 * supervises; gdbus is the same route kdos-appbox takes, a
		 * fixed string with nothing interpolated into it */
		if (!server.session_lock_manager
				|| !server.session_lock_manager->locked) {
			wlr_log(WLR_INFO, "idle: lock in %ds", KDOS_LOCK_WARN_S);
			spawn_async_no_shell("gdbus call --timeout 2 --session "
				"--dest org.freedesktop.Notifications "
				"--object-path /org/freedesktop/Notifications "
				"--method org.freedesktop.Notifications.Notify "
				"KDOS 0 distributor-logo-kdos 'Locking soon' "
				"'The session locks in 10 seconds — any input keeps it open' "
				"[] {} 8000");
		}
		arm();
		return 0;
	}

	/* skip switched-off stages so `idle_off` alone works */
	while (stage < 3 && at[stage] <= 0) {
		stage++;
	}
	if (stage >= 3) {
		return 0;
	}

	/* logged, every stage: "the screen went dark and I do not know
	 * why" is otherwise unanswerable */
	switch (stage) {
	case 0:
		wlr_log(WLR_INFO, "idle: dimming after %ds",
			kdos_conf.idle_dim_s);
		dim_set(true);
		break;
	case 1:
		/* already locked (Super+L and walked away) is not a
		 * reason to spawn a second lock client */
		if (!server.session_lock_manager
				|| !server.session_lock_manager->locked) {
			wlr_log(WLR_INFO, "idle: locking after %ds",
				kdos_conf.idle_lock_s);
			spawn_async_no_shell("kdos-lock");
		}
		break;
	case 2:
		wlr_log(WLR_INFO, "idle: outputs off after %ds",
			kdos_conf.idle_off_s);
		outputs_power(false);
		break;
	}

	idle_stage = stage + 1;
	arm();
	return 0;
}

void
kdos_idle_activity(void)
{
	/* a re-enable that failed at wake time gets another go now */
	if (power_reassert) {
		power_reassert = false;
		outputs_power(true);
	}

	if (idle_stage == 0 && !lock_warned) {
		/* the common case by a mile: rearm and nothing else */
		arm();
		return;
	}

	/* waking up: outputs first — the user is staring at black */
	wlr_log(WLR_DEBUG, "idle: activity at stage %d", idle_stage);
	if (idle_stage > 2) {
		outputs_power(true);
		/* a commit can lie by succeeding late; anything still dark
		 * is retried on the NEXT activity */
		struct output *o;
		wl_list_for_each(o, &server.outputs, link) {
			if (!o->wlr_output->enabled) {
				power_reassert = true;
			}
		}
	}
	dim_set(false);
	idle_stage = 0;
	lock_warned = false;
	arm();
	/* deliberately NOT unlocking — a password ends the lock */
}

void
kdos_idle_inhibit(bool add)
{
	if (add) {
		ninhibit++;
	} else if (--ninhibit < 0) {
		ninhibit = 0;
	}
	arm();
}

/* ── init ───────────────────────────────────────────────────────── */

/*
 * A virtual machine by the firmware's own account; nested/headless
 * backends (no wlr_session) are also never something a user sits at.
 */
static bool
in_a_vm(void)
{
#if WLR_HAS_SESSION
	if (!server.session) {
		return true;
	}
#else
	return true;
#endif
	char vendor[128] = { 0 };
	FILE *f = fopen("/sys/class/dmi/id/sys_vendor", "r");
	if (f) {
		if (!fgets(vendor, sizeof(vendor), f)) {
			vendor[0] = '\0';
		}
		fclose(f);
	}
	return strstr(vendor, "QEMU") || strstr(vendor, "Bochs")
		|| strstr(vendor, "VirtualBox") || strstr(vendor, "VMware")
		|| strstr(vendor, "innotek")
		|| strstr(vendor, "Microsoft Corporation")	/* Hyper-V */
		|| strstr(vendor, "Parallels")
		|| strstr(vendor, "Amazon EC2")
		|| strstr(vendor, "Google");			/* GCE */
}

bool
kdos_in_vm(void)
{
	return in_a_vm();
}

/* The VM gate, factored so a SIGHUP re-parse gets the same answer. */
static void
idle_apply_vm_gate(void)
{
	if (!kdos_conf.idle_configured && in_a_vm()) {
		kdos_conf.idle_dim_s = 0;
		kdos_conf.idle_lock_s = 0;
		kdos_conf.idle_off_s = 0;
		wlr_log(WLR_INFO, "idle timers off (a VM, and no idle_* in "
			"comp.conf) — a blanked screen is indistinguishable "
			"from a crash over VNC");
	}
}

void
kdos_idle_init(void)
{
	idle_apply_vm_gate();

	idle_timer = wl_event_loop_add_timer(
		wl_display_get_event_loop(server.wl_display), idle_tick, NULL);
	arm();

	if (kdos_conf.idle_dim_s || kdos_conf.idle_lock_s
			|| kdos_conf.idle_off_s) {
		wlr_log(WLR_INFO, "idle: dim %ds, lock %ds, outputs off %ds "
			"(0 = never)", kdos_conf.idle_dim_s,
			kdos_conf.idle_lock_s, kdos_conf.idle_off_s);
	}
}

/*
 * comp.conf was re-read. The stage the session is at survives — a reload
 * during a dim must not wake the screen — but the deadlines are the new
 * ones and the warning gets to fire again for them.
 */
void
kdos_idle_reconfigure(void)
{
	idle_apply_vm_gate();
	lock_warned = false;
	arm();
	wlr_log(WLR_INFO, "idle: dim %ds, lock %ds, outputs off %ds "
		"(0 = never)", kdos_conf.idle_dim_s,
		kdos_conf.idle_lock_s, kdos_conf.idle_off_s);
}

/*
 * The layout changed under a live dim: the rect was sized for the old
 * layout and a new output would come up undimmed beside a dimmed one.
 * Runs from output_update_for_layout_change(), beside the wallpaper.
 */
void
kdos_idle_arrange(void)
{
	if (!dim_rect) {
		return;
	}
	struct wlr_box box = { 0 };
	wlr_output_layout_get_box(server.output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0) {
		return;
	}
	wlr_scene_rect_set_size(dim_rect, box.width, box.height);
	wlr_scene_node_set_position(&dim_rect->node, box.x, box.y);
	wlr_scene_node_raise_to_top(&dim_rect->node);
	raise_locks_above_dim();
}

void
kdos_idle_finish(void)
{
	if (idle_timer) {
		wl_event_source_remove(idle_timer);
		idle_timer = NULL;
	}
}

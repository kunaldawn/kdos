/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-comp — idle: ext-idle-notify, idle-inhibit, and the policy
 *
 * Three separate things that are usually one program and should not be:
 *
 *   - `ext-idle-notify-v1` tells CLIENTS how long the seat has been idle. That
 *     is what a chat client uses to go "away", and it is the compositor's to
 *     report because only the compositor sees the input.
 *   - `idle-inhibit-unstable-v1` lets a client say "not now" — a video player
 *     playing fullscreen. Honouring it is the difference between a desktop and
 *     a screen that blanks halfway through a film.
 *   - The POLICY — dim, then lock, then power the outputs off — is ours, and it
 *     is here rather than in a separate idle daemon because every step of it
 *     needs something only the compositor has: the scene graph to dim, the
 *     output state to blank, and the input timestamps to reset on.
 *
 * OFF BY DEFAULT UNDER QEMU, and that is a debugging decision, not a
 * preference: the debug rig already learned that a screen-blanking idle daemon
 * looks EXACTLY like a crashed compositor over VNC. A developer watching the
 * session boot in a VM would otherwise spend the afternoon debugging a working
 * desktop. A comp.conf that sets any idle key overrides this — including
 * setting one to 0, which is how you say "I mean it".
 *
 * The stages are cumulative and each one is armed from the LAST activity, not
 * from the previous stage: dim at 300, lock at 600, off at 900 means what it
 * says, and reordering the config cannot produce a lock that never fires
 * because the dim rearmed the timer.
 */

#include <stdio.h>
#include <string.h>

#include "kdos-comp.h"

/* The dim is a translucent black rect over the whole layout, not a gamma
 * change: gamma is per-output, needs a DRM capability the headless and nested
 * backends do not have, and cannot be undone if the compositor dies with it
 * applied. A scene node goes away with the scene. */
#define KC_DIM_ALPHA 0.55f

static void dim_set(struct kc_server *s, bool on)
{
	if (!on) {
		if (s->idle_dim_rect) {
			wlr_scene_node_destroy(&s->idle_dim_rect->node);
			s->idle_dim_rect = NULL;
		}
		return;
	}
	if (s->idle_dim_rect)
		return;

	struct wlr_box box = {0};
	wlr_output_layout_get_box(s->output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0)
		return;

	const float shade[4] = { 0.0f, 0.0f, 0.0f, KC_DIM_ALPHA };
	/*
	 * In the LOCK tree, above every window and every panel but BELOW the
	 * lock surfaces — a dim that a panel could draw over would look like a
	 * rendering bug, and one that covered the lock screen would hide the
	 * password prompt.
	 */
	s->idle_dim_rect = wlr_scene_rect_create(s->layer_lock, box.width,
						box.height, shade);
	if (!s->idle_dim_rect)
		return;
	wlr_scene_node_set_position(&s->idle_dim_rect->node, box.x, box.y);
	wlr_scene_node_lower_to_bottom(&s->idle_dim_rect->node);
}

/* DPMS. `enabled` on a wlr_output is the only portable "off" there is, and it
 * is a committed state like any other. */
static void outputs_power(struct kc_server *s, bool on)
{
	struct kc_output *o;
	wl_list_for_each(o, &s->outputs, link) {
		struct wlr_output_state st;
		wlr_output_state_init(&st);
		wlr_output_state_set_enabled(&st, on);
		wlr_output_commit_state(o->wlr_output, &st);
		wlr_output_state_finish(&st);
	}
}

/* ── the timer ─────────────────────────────────────────────────────────── */

/* Seconds until the next stage that is both configured and not yet reached, or
 * 0 when there is nothing left to do. */
static int next_deadline(const struct kc_server *s)
{
	const int at[] = { s->idle_dim_s, s->idle_lock_s, s->idle_off_s };
	for (int stage = s->idle_stage; stage < 3; stage++)
		if (at[stage] > 0)
			return at[stage];
	return 0;
}

static void arm(struct kc_server *s)
{
	if (!s->idle_timer)
		return;
	int next = next_deadline(s);
	/*
	 * An inhibitor stops the policy dead rather than pausing it: a video
	 * player that holds an inhibitor for two hours should not find the
	 * screen locking one second after it lets go.
	 */
	if (next <= 0 || s->ninhibit > 0) {
		wl_event_source_timer_update(s->idle_timer, 0);
		return;
	}

	/* Each stage is measured from the last activity, so the delay to the
	 * next one is the difference between their thresholds. */
	static const int NONE = 0;
	int reached = NONE;
	const int at[] = { s->idle_dim_s, s->idle_lock_s, s->idle_off_s };
	for (int i = 0; i < s->idle_stage; i++)
		if (at[i] > reached)
			reached = at[i];
	int delay = next - reached;
	if (delay < 1)
		delay = 1;
	wl_event_source_timer_update(s->idle_timer, delay * 1000);
}

static int idle_tick(void *data)
{
	struct kc_server *s = data;
	const int at[] = { s->idle_dim_s, s->idle_lock_s, s->idle_off_s };
	int stage = s->idle_stage;

	/* Skip past stages that are switched off, so `idle_off` alone works
	 * without an `idle_dim` in front of it. */
	while (stage < 3 && at[stage] <= 0)
		stage++;
	if (stage >= 3)
		return 0;

	/* Logged, every stage. "The screen went dark and I do not know why" is
	 * otherwise unanswerable, and the answer is one line in the log the user
	 * already has. */
	switch (stage) {
	case 0:
		wlr_log(WLR_INFO, "idle: dimming after %ds", s->idle_dim_s);
		dim_set(s, true);
		break;
	case 1:
		/* Already locked (the user pressed Super+L and walked away) is
		 * not a reason to spawn a second lock client — the compositor
		 * refuses those, but spawning one per idle period would leave a
		 * trail of processes that all immediately failed. */
		if (!kc_locked(s)) {
			static const char *const argv[] = { "kdos-lock", NULL };
			wlr_log(WLR_INFO, "idle: locking after %ds",
				s->idle_lock_s);
			kc_spawn(argv);
		}
		break;
	case 2:
		wlr_log(WLR_INFO, "idle: outputs off after %ds", s->idle_off_s);
		outputs_power(s, false);
		break;
	}

	s->idle_stage = stage + 1;
	arm(s);
	return 0;
}

void kc_idle_activity(struct kc_server *s)
{
	if (s->idle_notifier)
		wlr_idle_notifier_v1_notify_activity(s->idle_notifier, s->seat);

	if (s->idle_stage == 0) {
		/* The common case by a mile: every keystroke and every pointer
		 * motion lands here, so it does nothing but rearm. */
		arm(s);
		return;
	}

	/* Waking up. The outputs come back before anything else, because the
	 * user is looking at a black screen waiting for exactly that. */
	wlr_log(WLR_DEBUG, "idle: activity at stage %d", s->idle_stage);
	if (s->idle_stage > 2)
		outputs_power(s, true);
	dim_set(s, false);
	s->idle_stage = 0;
	arm(s);
	/*
	 * Deliberately NOT unlocking. Activity ends the dim and the blank; the
	 * lock is ended by a password and nothing else.
	 */
}

/* ── idle-inhibit ──────────────────────────────────────────────────────── */

struct kc_inhibitor {
	struct kc_server *server;
	struct wl_listener destroy;
};

static void inhibitor_destroy(struct wl_listener *l, void *data)
{
	struct kc_inhibitor *in = wl_container_of(l, in, destroy);
	struct kc_server *s = in->server;
	(void)data;

	if (--s->ninhibit < 0)
		s->ninhibit = 0;
	if (s->ninhibit == 0 && s->idle_notifier)
		wlr_idle_notifier_v1_set_inhibited(s->idle_notifier, false);
	wl_list_remove(&in->destroy.link);
	free(in);
	arm(s);
}

static void new_inhibitor(struct wl_listener *l, void *data)
{
	struct kc_server *s = wl_container_of(l, s, new_inhibitor);
	struct wlr_idle_inhibitor_v1 *wlr_in = data;

	struct kc_inhibitor *in = calloc(1, sizeof(*in));
	if (!in)
		return;
	in->server = s;
	in->destroy.notify = inhibitor_destroy;
	wl_signal_add(&wlr_in->events.destroy, &in->destroy);

	s->ninhibit++;
	if (s->idle_notifier)
		wlr_idle_notifier_v1_set_inhibited(s->idle_notifier, true);
	arm(s);
}

/* ── init ──────────────────────────────────────────────────────────────── */

/*
 * A virtual machine, by the firmware's own account. `sys_vendor` is what the
 * kernel exposes of DMI and reads `QEMU` on qemu/kvm and `Bochs`/`SeaBIOS` on
 * the older machine types; the nested and headless backends are also never
 * anything a user is sitting in front of.
 */
static bool in_a_vm(const struct kc_server *s)
{
	if (!s->session)
		return true;	/* headless or nested: no seat to go idle */

	char vendor[128] = {0};
	FILE *f = fopen("/sys/class/dmi/id/sys_vendor", "r");
	if (f) {
		if (!fgets(vendor, sizeof(vendor), f))
			vendor[0] = '\0';
		fclose(f);
	}
	return strstr(vendor, "QEMU") || strstr(vendor, "Bochs") ||
	       strstr(vendor, "VirtualBox") || strstr(vendor, "VMware") ||
	       strstr(vendor, "innotek");
}

void kc_idle_init(struct kc_server *s)
{
	s->idle_notifier = wlr_idle_notifier_v1_create(s->display);
	s->idle_inhibit = wlr_idle_inhibit_v1_create(s->display);
	if (s->idle_inhibit) {
		s->new_inhibitor.notify = new_inhibitor;
		wl_signal_add(&s->idle_inhibit->events.new_inhibitor,
			      &s->new_inhibitor);
	}

	if (!s->idle_configured && in_a_vm(s)) {
		s->idle_dim_s = s->idle_lock_s = s->idle_off_s = 0;
		wlr_log(WLR_INFO, "idle timers off (a VM, and no idle_* in "
				  "comp.conf) — a blanked screen is "
				  "indistinguishable from a crash over VNC");
	}

	s->idle_timer = wl_event_loop_add_timer(
		wl_display_get_event_loop(s->display), idle_tick, s);
	arm(s);

	if (s->idle_dim_s || s->idle_lock_s || s->idle_off_s)
		wlr_log(WLR_INFO, "idle: dim %ds, lock %ds, outputs off %ds "
			"(0 = never)", s->idle_dim_s, s->idle_lock_s,
			s->idle_off_s);
}

void kc_idle_free(struct kc_server *s)
{
	if (s->idle_timer) {
		wl_event_source_remove(s->idle_timer);
		s->idle_timer = NULL;
	}
	if (s->idle_inhibit)
		wl_list_remove(&s->new_inhibitor.link);
}

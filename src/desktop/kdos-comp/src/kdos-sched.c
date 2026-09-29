// SPDX-License-Identifier: GPL-2.0-only
/*
 * RENDER-LATE FRAME SCHEDULING — `max_render_time = <ms>` in comp.conf.
 *
 * By default an output composites the moment its frame event arrives, which
 * on a DRM output is just after the previous frame was shown: the composite
 * then waits in the display for most of a refresh, and a client buffer that
 * arrives a moment after the composite waits a whole refresh more. With a
 * budget set, the composite is held back until `max_render_time` before the
 * next predicted vertical blank, so whatever clients commit in between is in
 * it. At 60 Hz that brings such a commit to the screen up to one refresh
 * sooner (an estimate; the rig has no display to measure it on).
 *
 * THE BUDGET IS THE USER'S. It must cover the composite and the phosphor
 * pass on this machine's GPU, and nothing here measures that: kdos-frames'
 * render_ms is CPU time up to the GL flush, not GPU completion, so it cannot
 * size a budget. Too small a budget misses the blank; the frame is then
 * shown a refresh late, and `kdos stutter` reports the miss. That is why the
 * default is off.
 *
 * WHERE IT DOES NOTHING — the frame composites at once, as it does off:
 * - an output that is not DRM (a nested window, headless): its frame events
 *   are not the display's blanks, so there is nothing to predict;
 * - no presentation within the last refresh (the first frame after the
 *   desktop was idle): a prediction from an old timestamp drifts with the two
 *   clocks, and a wrong one costs a whole refresh;
 * - no known refresh rate;
 * - variable refresh on: the blank follows the commit, so waiting only waits;
 * - a frame that may tear: it is flipped at once by design;
 * - less than a millisecond to wait, the timer's resolution.
 *
 * THE WAIT. While the timer runs the output's frame_pending is held true, as
 * a commit would hold it, so a client's damage does not schedule another
 * frame event in between: it is picked up by the composite the timer starts.
 * The timer lowers the flag again before compositing. A commit that reaches
 * the output some other way during the wait (a mode set, variable refresh
 * switched on for a fullscreen window, the shutdown animation) carries its
 * own frame event, so the timer is disarmed rather than left to composite on
 * top of a flip that is still pending.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#include <wlr/config.h>
#include <wlr/types/wlr_output.h>
#if WLR_HAS_DRM_BACKEND
#include <wlr/backend/drm.h>
#elif !defined(wlr_output_is_drm)	/* testing/fixtures/sched supplies one */
#define wlr_output_is_drm(output) (false)
#endif

#include "labwc.h"
#include "output.h"
#include "kdos.h"

struct ks_output {
	struct wl_list link;			/* outputs */
	struct output *output;
	struct wl_event_source *timer;
	bool armed;
	int64_t last_present_ns, refresh_ns;
	struct wl_listener present;
	struct wl_listener commit;
	struct wl_listener destroy;
};

static struct wl_list outputs = { &outputs, &outputs };
static bool finished;

int64_t
kdos_sched_delay_ns(int64_t now_ns, int64_t last_present_ns,
		int64_t refresh_ns, int64_t budget_ns)
{
	if (refresh_ns <= 0 || last_present_ns <= 0) {
		return 0;
	}
	int64_t blank = last_present_ns + refresh_ns;
	if (blank <= now_ns) {
		return 0;
	}
	int64_t delay = blank - now_ns - budget_ns;
	return delay > 0 ? delay : 0;
}

static struct ks_output *
ks_get(struct output *output)
{
	struct ks_output *ks;
	wl_list_for_each(ks, &outputs, link) {
		if (ks->output == output) {
			return ks;
		}
	}
	return NULL;
}

static void
disarm(struct ks_output *ks)
{
	if (ks->armed) {
		wl_event_source_timer_update(ks->timer, 0);
		ks->armed = false;
	}
}

static int
handle_timer(void *data)
{
	struct ks_output *ks = data;

	if (!ks->armed) {
		return 0;
	}
	ks->armed = false;
	/* held since the defer; the composite's own commit raises it again */
	ks->output->wlr_output->frame_pending = false;
	kdos_output_repaint(ks->output);
	return 0;
}

static void
handle_present(struct wl_listener *listener, void *data)
{
	struct ks_output *ks = wl_container_of(listener, ks, present);
	struct wlr_output_event_present *ev = data;

	if (!ev->presented) {
		return;
	}
	ks->last_present_ns = (int64_t)ev->when.tv_sec * 1000000000LL
		+ ev->when.tv_nsec;
	ks->refresh_ns = ev->refresh;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct ks_output *ks = wl_container_of(listener, ks, commit);

	(void)data;
	if (!ks->armed) {
		return;
	}
	/* Any commit to an enabled DRM output requests its own page-flip
	 * event, buffer or not (variable refresh switched on for a
	 * fullscreen window is one without): a composite on top of it would
	 * be refused as a second pending flip. A commit that disabled the
	 * output raises no flag of its own, so the one the wait held is
	 * lowered here or the output never asks for a frame again. */
	disarm(ks);
	if (!ks->output->wlr_output->enabled) {
		ks->output->wlr_output->frame_pending = false;
	}
}

static void
ks_free(struct ks_output *ks)
{
	if (ks->timer) {
		wl_event_source_remove(ks->timer);
	}
	wl_list_remove(&ks->present.link);
	wl_list_remove(&ks->commit.link);
	wl_list_remove(&ks->destroy.link);
	wl_list_remove(&ks->link);
	free(ks);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct ks_output *ks = wl_container_of(listener, ks, destroy);

	(void)data;
	ks_free(ks);
}

void
kdos_sched_output_add(struct output *output)
{
	if (finished || !wlr_output_is_drm(output->wlr_output)) {
		return;
	}
	struct ks_output *ks = calloc(1, sizeof(*ks));
	if (!ks) {
		return;
	}
	ks->output = output;
	ks->timer = wl_event_loop_add_timer(
		wl_display_get_event_loop(server.wl_display), handle_timer, ks);
	if (!ks->timer) {
		free(ks);
		return;
	}
	ks->present.notify = handle_present;
	wl_signal_add(&output->wlr_output->events.present, &ks->present);
	ks->commit.notify = handle_commit;
	wl_signal_add(&output->wlr_output->events.commit, &ks->commit);
	ks->destroy.notify = handle_destroy;
	wl_signal_add(&output->wlr_output->events.destroy, &ks->destroy);
	wl_list_insert(&outputs, &ks->link);
}

bool
kdos_sched_defer(struct output *output)
{
	if (kdos_conf.max_render_time <= 0) {
		return false;
	}
	struct ks_output *ks = ks_get(output);
	if (!ks) {
		return false;
	}
	/* a frame event while armed is a flip this file did not start;
	 * the decision is made again from this one */
	disarm(ks);

	struct wlr_output *wo = output->wlr_output;
	if (wo->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED
			|| output_get_tearing_allowance(output)) {
		return false;
	}
	int64_t refresh = ks->refresh_ns;
	if (refresh <= 0 && wo->refresh > 0) {
		refresh = 1000000000000LL / wo->refresh;	/* mHz */
	}
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int64_t now = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
	int64_t delay = kdos_sched_delay_ns(now, ks->last_present_ns, refresh,
		(int64_t)kdos_conf.max_render_time * 1000000LL);
	/* whole milliseconds, rounded down: early is safe, late is a miss */
	int ms = (int)(delay / 1000000LL);
	if (ms < 1) {
		return false;
	}
	wo->frame_pending = true;
	wl_event_source_timer_update(ks->timer, ms);
	ks->armed = true;
	return true;
}

void
kdos_sched_finish(void)
{
	finished = true;
	struct ks_output *ks, *tmp;
	wl_list_for_each_safe(ks, tmp, &outputs, link) {
		if (ks->armed) {
			disarm(ks);
			ks->output->wlr_output->frame_pending = false;
		}
		ks_free(ks);
	}
}

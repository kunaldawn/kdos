/*
 * Render-late frame scheduling (kdos-sched.c) against wlroots' real output
 * signals and a real event loop.
 *
 * What can go wrong is not the subtraction but the handshake with wlroots: a
 * deferred frame must hold the output's frame_pending up for the wait (or a
 * client's damage starts a second frame event in the middle of it) and lower
 * it before the composite (or the output never asks for a frame again); a
 * commit that arrives some other way during the wait must disarm the timer (or
 * the timer composites over a flip still pending); and every output it cannot
 * predict must composite at once. None of that is visible to a compiler, and
 * the rig has no display with a vertical blank.
 *
 * So this compiles kdos-sched.c itself (#included) with the stub headers
 * motioncheck uses, stands a wlr_output up by hand — its three signals and
 * the fields the file reads — and plays presentations, commits and timeouts
 * through a wl_event_loop. The refresh is 200 ms rather than a display's, so
 * a loaded machine cannot make a millisecond timer miss its check.
 *
 *   schedcheck     exit 0 when every check holds, 1 naming each that does not
 *
 * Not shipped. testing/selftest.sh compiles it where wlroots-0.20 is
 * installed and nothing else does.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/util/log.h>

/* the DRM test, answered by this file */
#define wlr_output_is_drm(output) schedcheck_is_drm(output)
struct wlr_output;
bool schedcheck_is_drm(struct wlr_output *output);

#include "kdos-sched.c"

struct server server;
struct kdos_conf kdos_conf;

static struct wlr_output *drm_output;
static bool tearing;
static int repaints;
static bool pending_at_repaint;

bool
schedcheck_is_drm(struct wlr_output *output)
{
	return output == drm_output;
}

bool
output_is_usable(struct output *output)
{
	(void)output;
	return true;
}

bool
output_get_tearing_allowance(struct output *output)
{
	(void)output;
	return tearing;
}

void
kdos_output_repaint(struct output *output)
{
	repaints++;
	pending_at_repaint = output->wlr_output->frame_pending;
}

static int fails, checks;

#define CHECK(cond, ...) do {						\
	checks++;							\
	if (!(cond)) {							\
		fails++;						\
		printf("FAIL %d: ", __LINE__);				\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)

#define MS	1000000LL
#define REFRESH	(200 * MS)

static int64_t
mono(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void
make_output(struct wlr_output *wo, struct output *out)
{
	memset(wo, 0, sizeof(*wo));
	wl_signal_init(&wo->events.present);
	wl_signal_init(&wo->events.commit);
	wl_signal_init(&wo->events.destroy);
	memset(out, 0, sizeof(*out));
	out->wlr_output = wo;
}

/* A presentation `ago_ns` before now, reporting `refresh_ns`. */
static void
present(struct wlr_output *wo, int64_t ago_ns, int refresh_ns)
{
	int64_t when = mono() - ago_ns;
	struct wlr_output_event_present ev = {
		.output = wo,
		.presented = true,
		.when = { .tv_sec = when / 1000000000LL,
			.tv_nsec = when % 1000000000LL },
		.refresh = refresh_ns,
	};
	wl_signal_emit_mutable(&wo->events.present, &ev);
}

/* A commit leaving the output `enabled`. As in wlroots' apply step, one to
 * an enabled output raises frame_pending whether or not it carries a buffer
 * (DRM asks every such commit for a page-flip event); one that disables the
 * output leaves the flag as it was. */
static void
commit(struct wlr_output *wo, uint32_t committed, bool enabled)
{
	struct wlr_output_state state = { .committed = committed };
	struct wlr_output_event_commit ev = { .output = wo, .state = &state };
	wo->enabled = enabled;
	if (enabled) {
		wo->frame_pending = true;
	}
	wl_signal_emit_mutable(&wo->events.commit, &ev);
}

/* Run the loop for `ms`, or until a repaint. */
static void
run(struct wl_event_loop *loop, int ms)
{
	int start = repaints;
	int64_t end = mono() + ms * MS;
	while (repaints == start && mono() < end) {
		wl_event_loop_dispatch(loop, (int)((end - mono()) / MS) + 1);
	}
}

int
main(void)
{
	wlr_log_init(WLR_ERROR, NULL);
	server.wl_display = wl_display_create();
	wl_list_init(&server.outputs);
	struct wl_event_loop *loop = wl_display_get_event_loop(
		server.wl_display);
	struct wlr_output wo;
	struct output out;

	/* The arithmetic. */
	CHECK(kdos_sched_delay_ns(1000, 990, 16, 4) == 2,
		"6 ns to the blank minus a budget of 4 is not 2");
	CHECK(kdos_sched_delay_ns(1000, 980, 16, 4) == 0,
		"a blank already past is waited for");
	CHECK(kdos_sched_delay_ns(1000, 990, 16, 10) == 0,
		"a budget past the blank is waited for");
	CHECK(kdos_sched_delay_ns(1000, 990, 0, 4) == 0
		&& kdos_sched_delay_ns(1000, 0, 16, 4) == 0,
		"an unknown refresh or presentation is waited for");

	make_output(&wo, &out);
	drm_output = &wo;
	kdos_sched_output_add(&out);
	CHECK(ks_get(&out) != NULL, "a DRM output got no timer");

	/* Off, and nothing presented yet: at once. */
	kdos_conf.max_render_time = 0;
	present(&wo, 2 * MS, REFRESH);
	CHECK(!kdos_sched_defer(&out), "deferred with the key off");
	kdos_conf.max_render_time = 4;
	struct ks_output *ks = ks_get(&out);
	int64_t keep = ks->last_present_ns;
	ks->last_present_ns = 0;
	CHECK(!kdos_sched_defer(&out), "deferred with nothing presented");
	ks->last_present_ns = keep;

	/* Deferred: the flag held up for the wait, lowered before the
	 * composite, which comes no earlier than the budget allows. */
	present(&wo, 2 * MS, REFRESH);
	int64_t t0 = mono();
	CHECK(kdos_sched_defer(&out), "not deferred with 194 ms to wait");
	CHECK(wo.frame_pending, "frame_pending not held during the wait");
	run(loop, 1000);
	int64_t waited = (mono() - t0) / MS;
	CHECK(repaints == 1, "%d composites after the wait, not 1", repaints);
	CHECK(!pending_at_repaint, "frame_pending still up at the composite");
	CHECK(waited >= 150, "the composite came after %lld ms, not about "
		"194", (long long)waited);

	/* Any commit during the wait disarms the timer: its own flip
	 * brings the next frame event. With a buffer ... */
	wo.enabled = true;
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (2)");
	commit(&wo, WLR_OUTPUT_STATE_BUFFER, true);
	CHECK(!ks->armed, "a commit with a buffer left the timer armed");
	run(loop, 300);
	CHECK(repaints == 1, "a disarmed timer composited");

	/* ... without one (variable refresh switched on for a fullscreen
	 * window): the flip it queued keeps the flag up ... */
	wo.frame_pending = false;
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (2b)");
	commit(&wo, WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED, true);
	CHECK(!ks->armed, "a commit without a buffer left the timer armed");
	CHECK(wo.frame_pending, "a buffer-less commit's flip lost its flag");
	run(loop, 300);
	CHECK(repaints == 1, "a timer composited over a buffer-less flip");

	/* ... and one that disables the output lowers the flag the wait
	 * held, since no flip will. */
	wo.frame_pending = false;
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (2c)");
	commit(&wo, WLR_OUTPUT_STATE_ENABLED, false);
	CHECK(!ks->armed, "disabling the output left the timer armed");
	CHECK(!wo.frame_pending, "disabling the output left frame_pending "
		"up");
	run(loop, 300);
	CHECK(repaints == 1, "a disabled output's timer composited");
	wo.enabled = true;

	/* A frame event while armed decides again: one composite, not
	 * two. */
	wo.frame_pending = false;
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (3)");
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred again");
	run(loop, 1000);
	run(loop, 300);
	CHECK(repaints == 2, "%d composites for two frame events, not 1 "
		"more", repaints);

	/* ... and one it then refuses leaves no timer behind: the caller
	 * composites at once, and the old wait must not composite again. */
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (4)");
	tearing = true;
	CHECK(!kdos_sched_defer(&out), "deferred a frame that may tear (1)");
	tearing = false;
	wo.frame_pending = false;
	run(loop, 300);
	CHECK(repaints == 2, "a refused frame event left its timer running");

	/* Everything it cannot predict composites at once. */
	present(&wo, 300 * MS, REFRESH);
	CHECK(!kdos_sched_defer(&out), "deferred on a stale presentation");
	present(&wo, 150 * MS, REFRESH);
	kdos_conf.max_render_time = 60;
	CHECK(!kdos_sched_defer(&out), "deferred with the budget past "
		"the blank");
	kdos_conf.max_render_time = 4;
	present(&wo, 2 * MS, REFRESH);
	wo.adaptive_sync_status = WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;
	CHECK(!kdos_sched_defer(&out), "deferred with variable refresh on");
	wo.adaptive_sync_status = WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED;
	tearing = true;
	CHECK(!kdos_sched_defer(&out), "deferred a frame that may tear");
	tearing = false;
	CHECK(!wo.frame_pending && !ks->armed,
		"a refused defer left the flag or the timer up");

	/* No refresh in the presentation: the mode's, in mHz. */
	present(&wo, 2 * MS, 0);
	wo.refresh = 5000;
	CHECK(kdos_sched_defer(&out), "the mode's refresh was not used");
	run(loop, 1000);
	CHECK(repaints == 3, "the mode-timed composite did not come");

	/* Not a DRM output: no timer, at once. */
	struct wlr_output wo2;
	struct output out2;
	make_output(&wo2, &out2);
	kdos_sched_output_add(&out2);
	present(&wo2, 2 * MS, REFRESH);
	CHECK(!ks_get(&out2) && !kdos_sched_defer(&out2),
		"a nested or headless output was deferred");

	/* The output goes mid-wait: the timer goes with it. */
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (5)");
	wl_signal_emit_mutable(&wo.events.destroy, &wo);
	CHECK(!ks_get(&out), "the record outlived its output");
	run(loop, 300);
	CHECK(repaints == 3, "a destroyed output's timer composited");

	/* Shutdown mid-wait: disarmed, the flag lowered, and no output
	 * added afterwards gets a timer. */
	make_output(&wo, &out);
	kdos_sched_output_add(&out);
	present(&wo, 2 * MS, REFRESH);
	CHECK(kdos_sched_defer(&out), "not deferred (6)");
	kdos_sched_finish();
	CHECK(!wo.frame_pending, "the shutdown left frame_pending up");
	run(loop, 300);
	CHECK(repaints == 3, "a timer composited after the shutdown");
	make_output(&wo2, &out2);
	drm_output = &wo2;
	kdos_sched_output_add(&out2);
	CHECK(!ks_get(&out2), "an output added after the shutdown got a "
		"timer");

	wl_display_destroy(server.wl_display);
	if (fails) {
		printf("schedcheck: %d FAILED\n", fails);
	} else {
		printf("schedcheck: %d checks passed\n", checks);
	}
	return fails != 0;
}

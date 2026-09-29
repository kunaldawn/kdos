// SPDX-License-Identifier: GPL-2.0-only
/*
 * PEEK — hold the pointer on Show Desktop and the windows fade to reveal
 * what is behind them.
 *
 * Windows 7's Aero Peek, and its designers' argument for it is the one that
 * applies here too: a thumbnail is a guess at what a window contains, and
 * "the answer is simply to show the actual window — complete with its real
 * content, real size and real location". The desktop is the thing being
 * looked at, so the windows get out of the way rather than being replaced by
 * a picture of anything.
 *
 * It lives in the compositor because it is the only thing that can do it:
 * the panel knows the pointer is dwelling, and the scene graph is here.
 *
 * NOT A MINIMISE. labwc already has Show Desktop (show-desktop.h) and that is
 * a state change — it iconifies, and the windows come back only when
 * something restores them. This is transient and owns no state a client can
 * observe: nothing is minimised, nothing is unfocused, no toplevel state
 * changes, so a peek that is interrupted by a crash or a lost connection
 * leaves the session exactly as it was except for an alpha value.
 *
 * Alpha rather than hiding the nodes, for the same reason: a hidden node
 * stops being hit-testable and stops being damaged, and both of those are
 * state. An opacity is a number the next frame overwrites.
 */

#include <wlr/types/wlr_scene.h>

#include "labwc.h"
#include "view.h"
#include "kdos.h"

/* What a peeked window fades to. Not zero: a window that vanishes entirely
 * makes the desktop look empty rather than looked-through, and the outlines
 * are what say the windows are still there. */
#define PEEK_ALPHA 0.12f

/* How long the fade takes each way, eased like every compositor fade
 * (kdos-motion.c); `motion = no` makes it one frame. */
#define PEEK_NS (150 * 1000000LL)

static bool peeking;

/* The fade in flight: from `from` at `start_ns` towards `to`. `shown` is
 * the alpha the windows carry now, which is where a reversal starts from —
 * a pointer that leaves half way through fades back from half way. */
static float from = 1.0f, to = 1.0f, shown = 1.0f;
static int64_t start_ns;
static bool running;

/*
 * Every mapped view, including the ones on other workspaces: their scene
 * trees are disabled, and kdos_motion_set_alpha() walks disabled trees too.
 * Filtering them out would mean a second idea about which views exist, and
 * getting THAT wrong is a window that comes back from a peek still
 * transparent — a workspace switch during a peek would otherwise leave the
 * old workspace's windows at the peek alpha.
 */
static void
apply(float alpha)
{
	struct view *view;

	shown = alpha;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		if (view->scene_tree) {
			kdos_motion_set_alpha(&view->scene_tree->node, alpha);
		}
	}
}

void
kdos_peek_set(bool on)
{
	if (peeking == on) {
		return;
	}
	peeking = on;
	/* a window mid-transition would fight the peek for its alpha, and
	 * keep an offset the peek knows nothing about */
	kdos_winmotion_settle_all();
	from = shown;
	to = on ? PEEK_ALPHA : 1.0f;
	start_ns = kdos_frames_now();
	running = true;
	/* the first step equals where it starts and damages nothing, so
	 * the first tick has to be asked for */
	kdos_motion_kick();
}

bool
kdos_peek_holds(void)
{
	return peeking || running || shown != 1.0f;
}

/* From kdos_motion_tick(), at the top of every output frame. */
void
kdos_peek_tick(int64_t now_ns)
{
	if (!running) {
		return;
	}
	bool done;
	float alpha = kdos_motion_ease(start_ns, PEEK_NS, from, to, now_ns,
		&done);
	apply(alpha);
	if (done) {
		running = false;
	}
}

/*
 * Called from the compositor's own teardown and from anywhere a peek must not
 * outlive its reason. A peek is transient by definition, and the one way it
 * could become permanent is the panel dying between the on and the off.
 * Immediate, not eased: nothing will tick after it.
 */
void
kdos_peek_finish(void)
{
	peeking = false;
	running = false;
	if (shown != 1.0f) {
		apply(1.0f);
	}
}

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

static bool peeking;

static void
set_alpha(struct wlr_scene_buffer *buffer, int sx, int sy, void *data)
{
	float *alpha = data;

	(void)sx;
	(void)sy;
	wlr_scene_buffer_set_opacity(buffer, *alpha);
}

/*
 * Every mapped view, including the ones on other workspaces — their scene
 * trees are simply not enabled, so setting an opacity on them costs a walk
 * and changes nothing visible. Filtering them out would mean a second idea
 * about which views exist, and getting THAT wrong is a window that comes back
 * from a peek still transparent.
 */
void
kdos_peek_set(bool on)
{
	float alpha = on ? PEEK_ALPHA : 1.0f;
	struct view *view;

	if (peeking == on) {
		return;
	}
	peeking = on;

	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		if (!view->scene_tree) {
			continue;
		}
		wlr_scene_node_for_each_buffer(&view->scene_tree->node,
			set_alpha, &alpha);
	}
}

/*
 * Called from the compositor's own teardown and from anywhere a peek must not
 * outlive its reason. A peek is transient by definition, and the one way it
 * could become permanent is the panel dying between the on and the off.
 */
void
kdos_peek_finish(void)
{
	kdos_peek_set(false);
}

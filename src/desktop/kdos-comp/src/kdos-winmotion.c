// SPDX-License-Identifier: GPL-2.0-only
/*
 * WINDOW TRANSITIONS — with `window_motion = yes` (and `motion`, the desktop's
 * reduce-motion switch, on) a window fades in and rises a few pixels into
 * place as it maps or comes back from being minimised, fades and sinks away
 * as it unmaps or is minimised, and a workspace switch slides the new
 * workspace's windows in from the side the switch goes towards while the old
 * one's slide out the other way.
 *
 * SKIPPABLE BY CONSTRUCTION. A transition is a picture of a change that has
 * already happened: the window is mapped, unmapped, minimised or on the other
 * workspace the moment the event arrives, focus has moved, and input goes
 * where the window is. Nothing waits for a transition to end, so a key
 * pressed during one acts on the new state, and a second event on the same
 * window starts from wherever the first one had got to.
 *
 * POSITION AND OPACITY ONLY. A window is a tree of client buffers and
 * decoration rectangles; a buffer can be drawn at another size, a rectangle
 * cannot, so a scale would pull the decorations apart from the contents. The
 * transitions move and fade the whole tree instead (kdos-motion.c), and a
 * closing window is a snapshot of it — its buffers and its border rectangles,
 * copied, holding their own buffer locks and refusing the pointer.
 *
 * A live window's border rectangles do not fade: a scene rectangle has no
 * opacity, only a colour its owner rewrites as focus changes. The outline is
 * there at once and the contents fade in inside it, which is also what peek
 * shows (kdos-peek.c): the outline says where the window is.
 *
 * Everything here is compositor-side: no client is asked to redraw, and cells,
 * dumps and goldens never see any of it. OFF by default, because a fading
 * window hides nothing beneath it: everything under it is composited for the
 * length of the transition.
 */

#include <wlr/types/wlr_scene.h>

#include "labwc.h"
#include "node.h"
#include "view.h"
#include "workspaces.h"
#include "kdos.h"

/* An open is the layer surfaces' open plus a little, because a window is
 * bigger and moves; a close is shorter, because what closes is dismissed. */
#define WIN_OPEN_NS	(150 * 1000000LL)
#define WIN_CLOSE_NS	(120 * 1000000LL)

/* How far a window rises into place, or sinks away: enough to read as
 * movement, too little to reach whatever is placed next to it. */
#define WIN_RISE	12

/* How far a workspace switch moves windows sideways. */
#define WIN_SHIFT	48

static bool
enabled(void)
{
	return kdos_conf.motion && kdos_conf.window_motion
		&& !kdos_peek_holds();
}

/* A fullscreen window does not slide: its edge would pull away from the
 * screen's edge and show a strip of what is beneath. It still fades. */
static int
rise(struct view *view)
{
	return view->fullscreen ? 0 : WIN_RISE;
}

static int
shift(struct view *view, int dir)
{
	return view->fullscreen ? 0 : dir * WIN_SHIFT;
}

void
kdos_winmotion_visibility(struct view *view, bool visible)
{
	if (!view->scene_tree) {
		return;
	}
	if (visible) {
		/* Back while its own close still fades (a minimise undone
		 * at once): the open carries on from the ghost. */
		float from = 0.0f;
		int dx = 0, dy = rise(view);
		kdos_motion_tree_reclaim(view->scene_tree, &from, &dx, &dy);
		if (enabled()) {
			kdos_motion_tree_open(view->scene_tree, from, dx, dy,
				WIN_OPEN_NS);
		}
		return;
	}

	/* Whatever `window_motion` says now, an open still running on the
	 * hidden tree is ended, or the window comes back offset. */
	float from = kdos_motion_tree_settle(view->scene_tree);
	if (!enabled()) {
		return;
	}
	/*
	 * Only a window that was on screen: its workspace is the one shown.
	 * The view's own tree has just been switched off, so the test is on
	 * its parent.
	 */
	struct wlr_scene_tree *parent = view->scene_tree->node.parent;
	int lx, ly;
	if (!parent || !wlr_scene_node_coords(&parent->node, &lx, &ly)) {
		return;
	}
	/*
	 * The contents are copied through wlroots' own unmap: the surface's
	 * subsurface tree (a child of content_tree for an xdg window,
	 * content_tree itself for an X window) is switched off by a listener
	 * of wlroots' that may have run before this one. A shaded window's
	 * contents were not showing, and stay out.
	 */
	struct wlr_scene_tree *forced = view->shaded ? NULL : view->content_tree;
	kdos_motion_tree_close(view->scene_tree, forced,
		&view->scene_tree->node, true, from, 0, rise(view),
		WIN_CLOSE_NS);
}

/* The workspace's position in the list, for which way a switch goes. */
static int
workspace_index(struct workspace *ws)
{
	int i = 0;
	struct workspace *w;
	wl_list_for_each(w, &server.workspaces.all, link) {
		if (w == ws) {
			return i;
		}
		i++;
	}
	return -1;
}

/* The view a child of a workspace's view tree is, or NULL for anything else
 * there — a snapshot of a closing window has no descriptor. */
static struct view *
tree_view(struct wlr_scene_node *node)
{
	struct node_descriptor *desc = node->data;
	if (!desc || desc->type != LAB_NODE_VIEW) {
		return NULL;
	}
	struct view *view = desc->view;
	if (!view || !view->mapped || view->minimized || !view->scene_tree) {
		return NULL;
	}
	return view;
}

void
kdos_winmotion_workspace(struct workspace *from, struct workspace *to)
{
	if (!from || !to || from == to || !enabled()) {
		return;
	}
	/* Towards a later workspace the new one comes in from the right. */
	int dir = workspace_index(to) > workspace_index(from) ? 1 : -1;
	struct wlr_scene_node *node;

	/*
	 * The three view trees in scene order, bottom first: the enum's
	 * numbering is not their stacking order.
	 */
	static const int order[] = { VIEW_LAYER_ALWAYS_ON_BOTTOM,
		VIEW_LAYER_NORMAL, VIEW_LAYER_ALWAYS_ON_TOP };

	/*
	 * The old workspace's windows leave as snapshots stacked directly
	 * below the new workspace, each one placed below the new workspace
	 * and so above the one before it: bottom of the stack first keeps
	 * their order. Omnipresent windows have already moved to `to`, and
	 * go nowhere.
	 */
	for (int i = 0; i < 3; i++) {
		wl_list_for_each(node, &from->view_trees[order[i]]->children,
				link) {
			struct view *view = tree_view(node);
			if (!view) {
				continue;
			}
			float alpha = kdos_motion_tree_settle(view->scene_tree);
			kdos_motion_tree_close(view->scene_tree, NULL,
				&to->tree->node, false, alpha,
				shift(view, -dir), 0, WIN_OPEN_NS);
		}
	}

	/* The new workspace's windows slide in and fade up, except the ones
	 * that were already on screen: the omnipresent windows and a window
	 * being dragged across. One caught mid-transition carries on from
	 * the alpha it had; one whose ghost from the switch away is still
	 * leaving (a switch straight back) carries on from the ghost's alpha
	 * and offset, and the ghost goes. */
	for (int i = 0; i < 3; i++) {
		wl_list_for_each(node, &to->view_trees[order[i]]->children,
				link) {
			struct view *view = tree_view(node);
			if (!view || view->visible_on_all_workspaces
					|| view == server.grabbed_view) {
				continue;
			}
			float alpha = kdos_motion_tree_settle(view->scene_tree);
			int dx = shift(view, dir), dy = 0;
			if (alpha >= 1.0f && !kdos_motion_tree_reclaim(
					view->scene_tree, &alpha, &dx, &dy)) {
				alpha = 0.0f;
			}
			kdos_motion_tree_open(view->scene_tree, alpha, dx, dy,
				WIN_OPEN_NS);
		}
	}
}

void
kdos_winmotion_settle_all(void)
{
	struct view *view;
	wl_list_for_each(view, &server.views, link) {
		if (view->scene_tree) {
			kdos_motion_tree_settle(view->scene_tree);
		}
	}
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Window grouping — Haiku's Stack & Tile, the "stack" half.
 *
 * A group is a list of views and one of them is showing; the others are
 * minimized, and the showing one's titlebar carries a tab per member.
 * `AddToTabGroup` stacks the focused window onto the one behind it,
 * `RemoveFromTabGroup` takes it back out, `NextInTabGroup` steps the
 * tabs from the keyboard, and clicking a tab raises that member.
 *
 * Four decisions, and each is the reason a piece of this is small:
 *
 * - **A hidden member is a MINIMIZED view**, not a scene tree we
 *   disabled behind labwc's back. That is show-desktop.c's pattern
 *   (`minimize_views()` is `view_minimize()` in a loop) and it is what
 *   makes the rest fall out for free: the scene node goes, focus skips
 *   it, and wlr-foreign-toplevel already reports minimized — so the
 *   panel stays truthful with no second graft. Disabling the tree
 *   ourselves would have left every one of those to re-implement.
 * - **Members share the showing member's geometry**, applied through
 *   `view_move_resize()` — the same call placement uses — at join and
 *   again whenever the shown member changes. A stack whose windows are
 *   different sizes is a stack that flickers.
 * - **The tab strip is rebuilt, never patched.** It hangs off the
 *   titlebar's own subtrees and is thrown away and redrawn on every
 *   `ssd_update_title()`, which is also the call that fires on a
 *   resize. Nothing in it caches geometry, so nothing in it can
 *   disagree with the titlebar it is drawn on.
 * - **A tab's node descriptor names the MEMBER's view.** labwc's
 *   context resolution walks up to the nearest descriptor and hands its
 *   view to the mousebinding, so the shipped `Title Press -> Focus` is
 *   already "raise this member" and this file adds no input code at
 *   all.
 *
 * Known gap, stated rather than hidden: dragging a tab moves the member
 * it names rather than tearing it out of the group. Tear-off is the
 * other half of Stack & Tile and is not implemented.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "common/font.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "foreign-toplevel/foreign.h"
#include "kdos.h"
#include "labwc.h"
#include "node.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

struct kg_member {
	struct wl_list link;		/* struct kg_group.members */
	struct kg_group *group;
	struct view *view;
	struct wl_listener destroy;	/* view->events.destroy */
	/* the member's own tab strip, one per SSD active state; owned by
	 * the titlebar subtree and therefore dropped rather than freed
	 * when the titlebar goes */
	struct wlr_scene_tree *tabs[2];
};

struct kg_group {
	struct wl_list link;		/* kg_groups */
	struct wl_list members;		/* struct kg_member.link, tab order */
	struct view *shown;
};

static struct wl_list kg_groups;
static bool kg_ready;
/* view_minimize() re-enters the focus path, which re-enters us */
static bool kg_activating;

static void
kg_init(void)
{
	if (!kg_ready) {
		wl_list_init(&kg_groups);
		kg_ready = true;
	}
}

static struct kg_member *
kg_member_of(struct view *view)
{
	if (!kg_ready || !view) {
		return NULL;
	}
	struct kg_group *g;
	struct kg_member *m;
	wl_list_for_each(g, &kg_groups, link) {
		wl_list_for_each(m, &g->members, link) {
			if (m->view == view) {
				return m;
			}
		}
	}
	return NULL;
}

int
kdos_group_size(struct view *view)
{
	struct kg_member *m = kg_member_of(view);
	return m ? wl_list_length(&m->group->members) : 0;
}

/* ── the tab strip ──────────────────────────────────────────────── */

static void
kg_tabs_drop(struct kg_member *m)
{
	for (int a = 0; a < 2; a++) {
		if (m->tabs[a]) {
			wlr_scene_node_destroy(&m->tabs[a]->node);
			m->tabs[a] = NULL;
		}
	}
}

void
kdos_group_ssd_clear(struct view *view)
{
	struct kg_member *m = kg_member_of(view);
	if (!m) {
		return;
	}
	/* the titlebar tree took the children with it; only the handles
	 * are ours to drop */
	m->tabs[0] = NULL;
	m->tabs[1] = NULL;
}

/*
 * One tab per member across the title area. Rebuilt whole on every call
 * — see the header comment; a cached strip is a strip that disagrees
 * with the titlebar after a resize.
 */
void
kdos_group_ssd_update(struct ssd *ssd)
{
	if (!ssd || !ssd->titlebar.tree) {
		return;
	}
	struct view *view = ssd->view;
	struct kg_member *self = kg_member_of(view);
	if (!self) {
		return;
	}
	/* only the showing member wears the strip; the rest are minimized
	 * and have no titlebar on screen anyway */
	if (self->group->shown != view) {
		return;
	}
	kg_tabs_drop(self);

	int nr = wl_list_length(&self->group->members);
	if (nr < 2) {
		return;
	}

	struct theme *theme = rc.theme;
	int width = view->current.width;
	int height = theme->titlebar_height;
	/*
	 * The strip occupies the title area. Its bounds are the same
	 * padding the buttons leave, computed the plain way rather than
	 * read out of ssd->state: a wrong guess here draws over a button,
	 * and the button positions are what this must not collide with.
	 */
	int pad = theme->window_titlebar_padding_width;
	int button = theme->window_button_width + theme->window_button_spacing;
	/* The box chip reserves the same room here that get_title_offsets
	 * gives it, or the first tab is drawn on top of it. */
	int left = pad + button * rc.nr_title_buttons_left
		+ kdos_boxchip_width(view);
	int right = pad + button * rc.nr_title_buttons_right;
	int strip_w = width - left - right;
	if (strip_w < nr * 8) {
		return;	/* no room for tabs; the plain title stays */
	}
	int tab_w = strip_w / nr;

	for (int a = 0; a < 2; a++) {
		struct ssd_titlebar_subtree *subtree = &ssd->titlebar.subtrees[a];
		if (!subtree->tree) {
			continue;
		}
		/* the plain title would sit under the tabs */
		if (subtree->title) {
			wlr_scene_node_set_enabled(
				&subtree->title->scene_buffer->node, false);
		}
		self->tabs[a] = lab_wlr_scene_tree_create(subtree->tree);
		wlr_scene_node_set_position(&self->tabs[a]->node, left, 0);

		const float *fg = theme->window[a].label_text_color;
		struct font *font = a ? &rc.font_activewindow
			: &rc.font_inactivewindow;
		/*
		 * A colour derived from the one the theme gave us: the
		 * separator must read as "this tab" without inventing a
		 * palette entry the themerc never declared. Every channel
		 * is scaled, not just alpha — theme colours are
		 * PRE-MULTIPLIED (theme.c's copy_color_scaled keeps RGB at
		 * or below alpha), and rgb above alpha is not a colour any
		 * renderer agrees about.
		 */
		float rule[4] = { fg[0] * 0.35f, fg[1] * 0.35f,
			fg[2] * 0.35f, fg[3] * 0.35f };

		int x = 0;
		int i = 0;
		struct kg_member *m;
		wl_list_for_each(m, &self->group->members, link) {
			struct wlr_scene_tree *tab =
				lab_wlr_scene_tree_create(self->tabs[a]);
			wlr_scene_node_set_position(&tab->node, x, 0);
			/*
			 * The descriptor names the MEMBER, which is what
			 * turns the shipped `Title Press -> Focus` into
			 * "raise this member" with no input code here.
			 */
			node_descriptor_create(&tab->node, LAB_NODE_TITLE,
				m->view, /*data*/ NULL);

			/* an invisible hit area, as the OSD items use */
			struct wlr_scene_rect *hit = lab_wlr_scene_rect_create(
				tab, tab_w, height, (float[4]) {0});
			wlr_scene_node_set_position(&hit->node, 0, 0);

			if (i > 0) {
				struct wlr_scene_rect *sep =
					lab_wlr_scene_rect_create(tab, 1,
						height, rule);
				wlr_scene_node_set_position(&sep->node, 0, 0);
			}
			if (m->view == self->group->shown) {
				struct wlr_scene_rect *mark =
					lab_wlr_scene_rect_create(tab,
						tab_w, 2, fg);
				wlr_scene_node_set_position(&mark->node, 0,
					height - 2);
			}

			struct scaled_font_buffer *label =
				scaled_font_buffer_create(tab);
			const float ignored_bg[4] = {0, 0, 0, 0};
			scaled_font_buffer_update(label,
				m->view->title ? m->view->title : "",
				tab_w - 4, font, fg, ignored_bg);
			wlr_scene_node_set_position(&label->scene_buffer->node,
				2, (height - label->height) / 2);

			x += tab_w;
			i++;
		}
	}
}

/* ── the group ──────────────────────────────────────────────────── */

static void
kg_apply_geometry(struct kg_group *g, struct view *to)
{
	if (!g->shown || g->shown == to || !to) {
		return;
	}
	struct wlr_box geo = g->shown->current;
	if (geo.width <= 0 || geo.height <= 0) {
		return;
	}
	view_move_resize(to, geo);
}

static void
kg_member_free(struct kg_member *m)
{
	kg_tabs_drop(m);
	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->link);
	free(m);
}

static void
kg_group_dissolve(struct kg_group *g)
{
	/* view_minimize() re-enters the focus path, which re-enters
	 * kdos_group_activate() — on a group that is half torn down */
	bool was = kg_activating;
	kg_activating = true;
	struct kg_member *m, *tmp;
	wl_list_for_each_safe(m, tmp, &g->members, link) {
		struct view *v = m->view;
		if (v != g->shown) {
			view_minimize(v, false);
		}
		kg_member_free(m);
		/* the strip is gone; the plain title has to come back, and
		 * only a reload re-enables the node kdos_group_ssd_update()
		 * hid under it */
		view_reload_ssd(v);
	}
	kg_activating = was;
	wl_list_remove(&g->link);
	free(g);
}

/* A group of one is not a group: the last member left behind would keep
 * a tab strip of one tab and a record nobody can see. Returns true when
 * the group is gone, so a caller cannot touch it afterwards. */
static bool
kg_prune(struct kg_group *g)
{
	if (wl_list_length(&g->members) >= 2) {
		return false;
	}
	kg_group_dissolve(g);
	return true;
}

/*
 * The strip names EVERY member, so a membership change invalidates the
 * showing member's titlebar even though nothing about that view changed
 * — and a tab left naming a member that has been freed is a descriptor
 * pointing at freed memory, which labwc dereferences on plain pointer
 * MOTION over the titlebar. Only a reload rebuilds it (ssd_update_title
 * is the sole caller of kdos_group_ssd_update, and neither a sibling's
 * death nor a promotion goes through one).
 */
static void
kg_shown_reload(struct kg_group *g)
{
	if (g->shown) {
		view_reload_ssd(g->shown);
	}
}

static void
handle_member_destroy(struct wl_listener *listener, void *data)
{
	struct kg_member *m = wl_container_of(listener, m, destroy);
	struct kg_group *g = m->group;
	bool was_shown = g->shown == m->view;

	kg_member_free(m);
	if (was_shown) {
		g->shown = NULL;
		struct kg_member *first;
		if (!wl_list_empty(&g->members)) {
			first = wl_container_of(g->members.next, first, link);
			g->shown = first->view;
			view_minimize(first->view, false);
		}
	}
	if (!kg_prune(g)) {
		kg_shown_reload(g);
	}
	kdos_foreign_workspace_sync();
}

static struct kg_member *
kg_member_add(struct kg_group *g, struct view *view)
{
	struct kg_member *m = znew(*m);
	m->group = g;
	m->view = view;
	m->destroy.notify = handle_member_destroy;
	wl_signal_add(&view->events.destroy, &m->destroy);
	wl_list_append(&g->members, &m->link);
	return m;
}

void
kdos_group_add(struct view *view)
{
	kg_init();
	if (!view || !view->mapped) {
		return;
	}
	if (kg_member_of(view)) {
		wlr_log(WLR_INFO, "group: %s is already in a tab group",
			view->app_id);
		return;
	}

	/*
	 * The window behind this one on the same workspace is the anchor:
	 * "stack me onto that". server.views is front-to-back with the
	 * active view at the front, so the next match is the one under it.
	 */
	struct view *anchor = view_next(&server.views, view,
		LAB_VIEW_CRITERIA_CURRENT_WORKSPACE);
	while (anchor && (anchor->minimized || anchor == view)) {
		anchor = view_next(&server.views, anchor,
			LAB_VIEW_CRITERIA_CURRENT_WORKSPACE);
	}
	if (!anchor) {
		wlr_log(WLR_INFO, "group: nothing behind %s to stack onto",
			view->app_id);
		return;
	}

	struct kg_member *am = kg_member_of(anchor);
	struct kg_group *g;
	if (am) {
		g = am->group;
	} else {
		g = znew(*g);
		wl_list_init(&g->members);
		wl_list_insert(&kg_groups, &g->link);
		kg_member_add(g, anchor);
		g->shown = anchor;
	}
	kg_member_add(g, view);

	/* the joiner takes the group's geometry and the group's front */
	kg_apply_geometry(g, view);
	kg_activating = true;
	struct kg_member *m;
	wl_list_for_each(m, &g->members, link) {
		if (m->view != view) {
			view_minimize(m->view, true);
		}
	}
	kg_activating = false;
	g->shown = view;
	view_reload_ssd(view);
	kdos_foreign_workspace_sync();
}

void
kdos_group_remove(struct view *view)
{
	struct kg_member *m = kg_member_of(view);
	if (!m) {
		return;
	}
	struct kg_group *g = m->group;
	bool was_shown = g->shown == view;

	kg_member_free(m);
	if (was_shown && !wl_list_empty(&g->members)) {
		struct kg_member *first =
			wl_container_of(g->members.next, first, link);
		g->shown = first->view;
		view_minimize(first->view, false);
	}
	if (!kg_prune(g)) {
		/* the promotion above went through the focus path, which
		 * finds g->shown already set and returns before its own
		 * reload */
		kg_shown_reload(g);
	}
	view_reload_ssd(view);
	kdos_foreign_workspace_sync();
}

void
kdos_group_next(struct view *view, bool reverse)
{
	struct kg_member *m = kg_member_of(view);
	if (!m) {
		return;
	}
	struct wl_list *link = reverse ? m->link.prev : m->link.next;
	if (link == &m->group->members) {
		link = reverse ? link->prev : link->next;
	}
	if (link == &m->link) {
		return;
	}
	struct kg_member *next = wl_container_of(link, next, link);
	desktop_focus_view(next->view, /*raise*/ true);
}

/*
 * Called from the focus path: whichever member just got focus becomes
 * the shown one and the rest go back to minimized. Re-entrant, because
 * view_minimize() runs the focus path again.
 */
void
kdos_group_activate(struct view *view)
{
	struct kg_member *m = kg_member_of(view);
	if (!m || kg_activating) {
		return;
	}
	struct kg_group *g = m->group;
	if (g->shown == view) {
		return;
	}

	kg_activating = true;
	kg_apply_geometry(g, view);
	g->shown = view;
	struct kg_member *other;
	wl_list_for_each(other, &g->members, link) {
		if (other->view != view && !other->view->minimized) {
			view_minimize(other->view, true);
		}
	}
	kg_activating = false;

	view_reload_ssd(view);
	kdos_foreign_workspace_sync();
}

void
kdos_group_finish(void)
{
	if (!kg_ready) {
		return;
	}
	struct kg_group *g, *tmp;
	wl_list_for_each_safe(g, tmp, &kg_groups, link) {
		struct kg_member *m, *mtmp;
		wl_list_for_each_safe(m, mtmp, &g->members, link) {
			kg_member_free(m);
		}
		wl_list_remove(&g->link);
		free(g);
	}
	kg_ready = false;
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * App-first Alt-Tab. One row per APPLICATION, not per window:
 *
 *   Firefox ×4 — Bug 1234 - Mozilla Firefox     <- selected group
 *   GIMP ×2
 *   Terminal
 *
 * Tab (and Left/Right) steps between groups; Down/Up steps between the
 * windows of the selected group, and the selected row grows the title of
 * the window that would be focused. Releasing the modifier focuses that
 * window, which for a group merely tabbed past is its most recently used
 * one.
 *
 * Three notes:
 *
 * - There is exactly ONE OSD item per group, and its `view` is the
 *   group's currently selected member, kept in step at update() time
 *   BEFORE cycle_osd_scroll_update() runs — the scroll helper looks the
 *   selected view up in the item list and ASSERTS when it is not there.
 *   That invariant is why groups are rows and members never are.
 * - Only the selected row's text is rewritten on a step, so the layout
 *   is fixed for the life of the switcher: nothing appears, disappears
 *   or moves under the hand, which is what "Down expands" costs when a
 *   plugin interface has init() and update() and no relayout.
 * - The group name comes from cycle_osd_field_get_content() with a
 *   desktop-entry field, so it is the same name the other OSDs show and
 *   the sfdo lookup lives in one place. Grouping is by app_id, and a
 *   view with no app_id is its own group — collapsing every
 *   unidentified window onto one row would make the count a lie.
 */
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/font.h"
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "cycle.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "theme.h"
#include "view.h"

struct cycle_osd_apps_item {
	struct cycle_osd_item base;
	struct wlr_scene_tree *normal_tree, *active_tree;
	/* the selected row's label carries the member title, so it is
	 * rewritten rather than rebuilt when the selection moves */
	struct scaled_font_buffer *active_text;
	int text_width;
};

/* Two views are the same application when their app_ids match and are
 * not empty. An empty app_id identifies nothing, so it groups with
 * nothing. */
static bool
same_app(struct view *a, struct view *b)
{
	if (a == b) {
		return true;
	}
	if (string_null_or_empty(a->app_id) || string_null_or_empty(b->app_id)) {
		return false;
	}
	return !strcmp(a->app_id, b->app_id);
}

/* The first member of a group in cycle-list order — under the default
 * focus ordering that is the group's most recently used window. */
static struct view *
group_leader(struct view *view)
{
	struct view *v;
	wl_list_for_each(v, &server.cycle.views, cycle_link) {
		if (same_app(v, view)) {
			return v;
		}
	}
	return view;
}

static int
group_count(struct view *view)
{
	int n = 0;
	struct view *v;
	wl_list_for_each(v, &server.cycle.views, cycle_link) {
		if (same_app(v, view)) {
			n++;
		}
	}
	return n;
}

static void
add_app_label(struct buf *buf, struct view *view, struct view *member)
{
	struct cycle_osd_field field = {
		.content = LAB_FIELD_DESKTOP_ENTRY_NAME,
	};
	cycle_osd_field_get_content(&field, buf, view);

	int n = group_count(view);
	if (n > 1) {
		buf_add_fmt(buf, " ×%d", n);
	}
	if (member && n > 1 && !string_null_or_empty(member->title)) {
		buf_add(buf, " — ");
		buf_add(buf, member->title);
	}
}

/* ── stepping, called from cycle.c ───────────────────────────────── */

struct view *
cycle_osd_apps_step_group(struct view *from, enum lab_cycle_dir dir)
{
	if (!from || wl_list_empty(&server.cycle.views)) {
		return from;
	}
	/*
	 * Walk the cycle list until the app changes. Forwards, the first
	 * member of a different group IS that group's leader; backwards it
	 * is the group's last member, so the leader is looked up.
	 */
	struct wl_list *link = &from->cycle_link;
	for (int guard = wl_list_length(&server.cycle.views); guard > 0; guard--) {
		link = (dir == LAB_CYCLE_DIR_BACKWARD) ? link->prev : link->next;
		if (link == &server.cycle.views) {
			continue;	/* the head is not a view */
		}
		struct view *v = wl_container_of(link, v, cycle_link);
		if (!same_app(v, from)) {
			return group_leader(v);
		}
	}
	return from;	/* one application, one group */
}

struct view *
cycle_osd_apps_step_member(struct view *from, enum lab_cycle_dir dir)
{
	if (!from || wl_list_empty(&server.cycle.views)) {
		return from;
	}
	struct view *prev = NULL, *first = NULL, *last = NULL, *next = NULL;
	bool seen = false;
	struct view *v;
	wl_list_for_each(v, &server.cycle.views, cycle_link) {
		if (!same_app(v, from)) {
			continue;
		}
		if (!first) {
			first = v;
		}
		last = v;
		if (seen && !next && v != from) {
			next = v;
		}
		if (v == from) {
			seen = true;
		} else if (!seen) {
			prev = v;
		}
	}
	if (dir == LAB_CYCLE_DIR_BACKWARD) {
		return prev ? prev : last;
	}
	return next ? next : first;
}

/* ── drawing ────────────────────────────────────────────────────── */

static struct scaled_font_buffer *
add_row_text(struct wlr_scene_tree *parent, const char *text, int x, int y,
		int width, const float *fg, const float *bg)
{
	struct scaled_font_buffer *fb = scaled_font_buffer_create(parent);
	scaled_font_buffer_update(fb, text ? text : "", width, &rc.font_osd,
		fg, bg);
	wlr_scene_node_set_position(&fb->scene_buffer->node, x, y);
	return fb;
}

static void
cycle_osd_apps_init(struct cycle_osd_output *osd_output)
{
	struct output *output = osd_output->output;
	struct theme *theme = rc.theme;
	struct window_switcher_classic_theme *st = &theme->osd_window_switcher_classic;
	int padding = theme->osd_border_width + st->padding;

	struct wlr_box output_box;
	wlr_output_layout_get_box(server.output_layout, output->wlr_output,
		&output_box);

	int w = st->width;
	if (st->width_is_percent) {
		w = output_box.width * st->width / 100;
	}

	/* One row per group, in the order the groups first appear */
	struct wl_array leaders;
	wl_array_init(&leaders);
	struct view *view;
	wl_list_for_each(view, &server.cycle.views, cycle_link) {
		if (group_leader(view) == view) {
			struct view **slot = wl_array_add(&leaders, sizeof(*slot));
			if (slot) {
				*slot = view;
			}
		}
	}
	int nr_groups = (int)(leaders.size / sizeof(struct view *));
	int nr_visible = (output_box.height - 2 * padding) / st->item_height;
	if (nr_visible < 1) {
		nr_visible = 1;
	}
	if (nr_visible > nr_groups) {
		nr_visible = nr_groups;
	}
	int h = nr_visible * st->item_height + 2 * padding;

	osd_output->tree = lab_wlr_scene_tree_create(output->cycle_osd_tree);

	float *fg = theme->osd_label_text_color;
	float *bg = theme->osd_bg_color;
	float *active_bg = st->item_active_bg_color;
	float *active_border = st->item_active_border_color;

	struct lab_scene_rect_options bg_opts = {
		.border_colors = (float *[1]) { theme->osd_border_color },
		.nr_borders = 1,
		.border_width = theme->osd_border_width,
		.bg_color = bg,
		.width = w,
		.height = h,
	};
	lab_scene_rect_create(osd_output->tree, &bg_opts);

	int text_w = w - 2 * padding - 2 * st->item_active_border_width
		- 2 * st->item_padding_x;
	if (text_w <= 0) {
		wlr_log(WLR_ERROR, "not enough space for the app switcher");
		goto out;
	}

	osd_output->items_tree = lab_wlr_scene_tree_create(osd_output->tree);

	int y = padding;
	struct view **leader;
	wl_array_for_each(leader, &leaders) {
		struct cycle_osd_apps_item *item = znew(*item);
		wl_list_append(&osd_output->items, &item->base.link);
		item->base.view = *leader;
		item->text_width = text_w;
		item->base.tree = lab_wlr_scene_tree_create(osd_output->items_tree);
		node_descriptor_create(&item->base.tree->node,
			LAB_NODE_CYCLE_OSD_ITEM, NULL, item);

		item->normal_tree = lab_wlr_scene_tree_create(item->base.tree);
		item->active_tree = lab_wlr_scene_tree_create(item->base.tree);
		wlr_scene_node_set_enabled(&item->active_tree->node, false);

		struct lab_scene_rect_options hi_opts = {
			.border_colors = (float *[1]) { active_border },
			.nr_borders = 1,
			.border_width = st->item_active_border_width,
			.bg_color = active_bg,
			.width = w - 2 * padding,
			.height = st->item_height,
		};
		struct lab_scene_rect *hi = lab_scene_rect_create(
			item->active_tree, &hi_opts);
		wlr_scene_node_set_position(&hi->tree->node, padding, y);

		/* hitbox for mouse clicks, as the other OSDs do */
		struct wlr_scene_rect *hitbox = lab_wlr_scene_rect_create(
			item->base.tree, w - 2 * padding, st->item_height,
			(float[4]) {0});
		wlr_scene_node_set_position(&hitbox->node, padding, y);

		struct buf label = BUF_INIT;
		add_app_label(&label, *leader, /*member*/ NULL);
		int x = padding + st->item_active_border_width + st->item_padding_x;
		int ty = y + (st->item_height - font_height(&rc.font_osd)) / 2;
		add_row_text(item->normal_tree, label.data, x, ty, text_w, fg, bg);
		item->active_text = add_row_text(item->active_tree, label.data,
			x, ty, text_w, fg, active_bg);
		buf_reset(&label);

		y += st->item_height;
	}

	struct wlr_box bar = {
		.x = w - padding - SCROLLBAR_W,
		.y = padding,
		.width = SCROLLBAR_W,
		.height = h - 2 * padding,
	};
	cycle_osd_scroll_init(osd_output, bar, st->item_height,
		/*nr_cols*/ 1, nr_groups, nr_visible, active_border, active_bg);

out:
	wl_array_release(&leaders);
	wlr_scene_node_set_position(&osd_output->tree->node,
		output_box.x + (output_box.width - w) / 2,
		output_box.y + (output_box.height - h) / 2);
}

static void
cycle_osd_apps_update(struct cycle_osd_output *osd_output)
{
	struct view *selected = server.cycle.selected_view;
	struct cycle_osd_apps_item *item;

	/*
	 * Point the selected group's item at the member the user is on
	 * BEFORE the scroll helper looks the selected view up in this
	 * list — it asserts when it cannot find it.
	 */
	wl_list_for_each(item, &osd_output->items, base.link) {
		if (selected && same_app(item->base.view, selected)) {
			item->base.view = selected;
		}
	}

	cycle_osd_scroll_update(osd_output);

	struct theme *theme = rc.theme;
	struct window_switcher_classic_theme *st =
		&theme->osd_window_switcher_classic;
	wl_list_for_each(item, &osd_output->items, base.link) {
		bool active = selected && same_app(item->base.view, selected);
		wlr_scene_node_set_enabled(&item->normal_tree->node, !active);
		wlr_scene_node_set_enabled(&item->active_tree->node, active);
		if (!active || !item->active_text) {
			continue;
		}
		struct buf label = BUF_INIT;
		add_app_label(&label, item->base.view, selected);
		scaled_font_buffer_update(item->active_text, label.data,
			item->text_width, &rc.font_osd,
			theme->osd_label_text_color, st->item_active_bg_color);
		buf_reset(&label);
	}
}

struct cycle_osd_impl cycle_osd_apps_impl = {
	.init = cycle_osd_apps_init,
	.update = cycle_osd_apps_update,
};

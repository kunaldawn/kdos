// SPDX-License-Identifier: GPL-2.0-only
#include "placement.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include "common/mem.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "ssd.h"
#include "view.h"
#include "kwm.h"

/*
 * The search itself is libkwm's — one implementation, shared with kdos-con.
 * What stays here is the half only a compositor can do: walking its own views
 * and asking the decoration how thick it is.
 */
bool
placement_find_best(struct view *view, struct wlr_box *geometry)
{
	assert(view);

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		return false;
	}

	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	struct border margin = ssd_get_margin(view->ssd);

	int cap = 0;
	struct view *v;
	for_each_view(v, &server.views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || v->output != output) {
			continue;
		}
		cap++;
	}

	/*
	 * Every window already on this output, as absolute edges ALREADY
	 * INFLATED BY ITS OWN DECORATION. libkwm cannot ask how thick a frame
	 * is, and the effective height is what makes a shaded window occupy
	 * only its titlebar.
	 */
	KwmBox *ex = cap > 0 ? xzalloc((size_t)cap * sizeof(*ex)) : NULL;
	int n = 0;

	if (ex) {
		for_each_view(v, &server.views,
				LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
			if (v == view || v->output != output) {
				continue;
			}
			struct border vm = ssd_get_margin(v->ssd);

			ex[n].left = v->pending.x - vm.left;
			ex[n].top = v->pending.y - vm.top;
			ex[n].right = v->pending.x + vm.right + v->pending.width;
			ex[n].bottom = v->pending.y + vm.bottom
				+ view_effective_height(v, /* use_pending */ true);
			n++;
		}
	}

	KwmRect u = { usable.x, usable.y, usable.width, usable.height };
	KwmBorder m = { margin.top, margin.right, margin.bottom, margin.left };
	KwmRect r = kwm_place(u, rc.gap, m,
		geometry->width, geometry->height, ex, n);

	free(ex);

	geometry->x = r.x;
	geometry->y = r.y;
	return true;
}

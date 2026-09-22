/* libkwm — tiling geometry and the edge primitives. See kwm.h. */

#include "kwm.h"

unsigned
kwm_tile_next(unsigned cur, unsigned edge, int combine, int across,
	      int *move_output)
{
	if (move_output)
		*move_output = 0;

	/*
	 * Only a single cardinal edge combines. Anything else — a corner asked
	 * for directly, CENTER, NONE — is taken as given, and a view already
	 * tiled to CENTER is excluded from combining entirely.
	 */
	if (!kwm_edge_is_cardinal(edge) || cur == KWM_EDGE_CENTER)
		return edge;

	unsigned inverse = kwm_edge_invert(edge);
	unsigned axis = edge | inverse;
	unsigned parallel = cur & axis;
	unsigned orthogonal = cur & ~axis;

	if (across && cur == edge) {
		/*
		 * Snapping again towards an edge already occupied crosses to
		 * the next screen and lands against the far side of it. The
		 * caller must leave the view alone if there is no screen there.
		 */
		if (move_output)
			*move_output = 1;
		return inverse;
	}

	if (combine && parallel == inverse && orthogonal != KWM_EDGE_NONE) {
		/* A quarter loses the component it is snapping away from. */
		return orthogonal;
	}

	if (combine && parallel == KWM_EDGE_NONE) {
		/* A half gains an orthogonal component and becomes a quarter. */
		return cur | edge;
	}

	/*
	 * Everything else takes the request unchanged — including a quarter
	 * snapped towards the edge it already occupies, which discards its
	 * orthogonal component and collapses to a half.
	 */
	return edge;
}

KwmRect
kwm_tile_geom(KwmRect usable, int gap, KwmBorder margin, unsigned tiled)
{
	int x1 = gap;
	int y1 = gap;
	int x2 = usable.w - gap;
	int y2 = usable.h - gap;

	if (tiled & KWM_EDGE_RIGHT)
		x1 = (usable.w + gap) / 2;
	if (tiled & KWM_EDGE_LEFT)
		x2 = (usable.w - gap) / 2;
	if (tiled & KWM_EDGE_BOTTOM)
		y1 = (usable.h + gap) / 2;
	if (tiled & KWM_EDGE_TOP)
		y2 = (usable.h - gap) / 2;

	KwmRect r;
	r.x = x1 + usable.x + margin.left;
	r.y = y1 + usable.y + margin.top;
	r.w = (x2 - x1) - margin.left - margin.right;
	r.h = (y2 - y1) - margin.top - margin.bottom;
	return r;
}

int
kwm_edge_best(int next, int edge, int decreasing)
{
	if (!KWM_BOUNDED(next))
		return KWM_BOUNDED(edge) ? edge : next;

	if (!KWM_BOUNDED(edge))
		return next;

	if (decreasing)
		return next > edge ? next : edge;
	return next < edge ? next : edge;
}

int
kwm_edge_between(int cur, int tgt, int other)
{
	if (cur == tgt)
		return 0;

	if (tgt <= other && other < cur)
		return 1;
	if (cur < other && other <= tgt)
		return 1;
	return 0;
}

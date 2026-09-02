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

/*
 * Where the line from (x1,y1) to (x2,y2) sits at x. A vertical run has no
 * single answer, so it takes the midpoint — which is what makes a zero-length
 * move miss every obstacle rather than hitting all of them.
 */
static double
interpolate(int x, int x1, double y1, int x2, double y2)
{
	int run = x2 - x1;
	if (run == 0)
		return 0.5 * (y1 + y2);

	return y1 + (x - x1) * ((y2 - y1) / (double)run);
}

int
kwm_edge_sweeps(KwmEdge cur, KwmEdge tgt, KwmEdge obstacle)
{
	double lo = interpolate(obstacle.offset,
		cur.offset, cur.min, tgt.offset, tgt.min);
	double hi = interpolate(obstacle.offset,
		cur.offset, cur.max, tgt.offset, tgt.max);

	if (obstacle.max <= lo)
		return 0;
	if (obstacle.min >= hi)
		return 0;

	return kwm_edge_between(cur.offset, tgt.offset, obstacle.offset);
}

KwmRect
kwm_fit(KwmRect want, KwmRect work)
{
	KwmRect r = want;

	/* Shrink only when it genuinely cannot fit, and then to the area
	 * itself rather than to some fraction of it. */
	if (r.w > work.w) {
		r.w = work.w;
		r.x = work.x;
	}
	if (r.h > work.h) {
		r.h = work.h;
		r.y = work.y;
	}

	/*
	 * Then move. The far edge is clamped BEFORE the near one, so a window
	 * that is off both ends of a work area smaller than itself ends up at
	 * the origin rather than off the near edge.
	 */
	if (r.x + r.w > work.x + work.w)
		r.x = work.x + work.w - r.w;
	if (r.y + r.h > work.y + work.h)
		r.y = work.y + work.h - r.h;
	if (r.x < work.x)
		r.x = work.x;
	if (r.y < work.y)
		r.y = work.y;

	return r;
}

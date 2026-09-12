/* libkwm — the neighbour-edge search. See kwm.h.
 *
 * Ported from kdos-comp's edges.c. What stayed behind is everything that walks
 * the compositor: the view list, the output list, and the scene-graph pass that
 * works out which edges are visible. What is here is the arithmetic those three
 * feed, which is the half both desktops need.
 */

#include "kwm.h"

static int
is_lesser(unsigned dir)
{
	return dir == KWM_EDGE_LEFT || dir == KWM_EDGE_TOP;
}

KwmEdge
kwm_edge_of(KwmBox b, unsigned dir, int pad)
{
	KwmEdge e = { 0, 0, 0 };

	switch (dir) {
	case KWM_EDGE_LEFT:
		e.offset = kwm_clip_sub(b.left, pad);
		e.min = b.top;
		e.max = b.bottom;
		break;
	case KWM_EDGE_RIGHT:
		e.offset = kwm_clip_add(b.right, pad);
		e.min = b.top;
		e.max = b.bottom;
		break;
	case KWM_EDGE_TOP:
		e.offset = kwm_clip_sub(b.top, pad);
		e.min = b.left;
		e.max = b.right;
		break;
	case KWM_EDGE_BOTTOM:
		e.offset = kwm_clip_add(b.bottom, pad);
		e.min = b.left;
		e.max = b.right;
		break;
	}

	return e;
}

/*
 * An edge the caller has said is not visible is pushed out of bounds rather
 * than dropped, so it loses every comparison in kwm_edge_best without the
 * search needing a special case for it.
 */
static KwmEdge
edge_if_visible(KwmBox b, unsigned dir, int pad, unsigned visible)
{
	KwmEdge e = kwm_edge_of(b, dir, pad);

	if (!(visible & dir))
		e.offset = is_lesser(dir) ? INT_MIN : INT_MAX;

	return e;
}

static int *
slot(KwmBox *b, unsigned dir)
{
	switch (dir) {
	case KWM_EDGE_LEFT:
		return &b->left;
	case KWM_EDGE_RIGHT:
		return &b->right;
	case KWM_EDGE_TOP:
		return &b->top;
	default:
		return &b->bottom;
	}
}

void
kwm_edge_init(KwmBox *best)
{
	best->top = INT_MIN;
	best->right = INT_MAX;
	best->bottom = INT_MAX;
	best->left = INT_MIN;
}

static void
one_region_edge(KwmBox *best, KwmBox cur, KwmBox tgt, KwmRegion region,
		unsigned dir, int gap, KwmEdgeValidator v, void *user)
{
	/*
	 * Only the ALIGNED edge is padded by the gap. The moving box already
	 * carries its own padding, so padding the opposing edge as well would
	 * count the gap twice.
	 */
	v(slot(best, dir),
	  kwm_edge_of(cur, dir, 0),
	  kwm_edge_of(tgt, dir, 0),
	  edge_if_visible(region.box, kwm_edge_invert(dir), 0, region.visible),
	  edge_if_visible(region.box, dir, gap, region.visible),
	  is_lesser(dir), user);
}

void
kwm_edge_regions(KwmBox *best, KwmBox cur, KwmBox tgt,
		 const KwmRegion *regions, int n, int gap,
		 KwmEdgeValidator v, void *user)
{
	static const unsigned dirs[] = {
		KWM_EDGE_LEFT, KWM_EDGE_RIGHT, KWM_EDGE_TOP, KWM_EDGE_BOTTOM
	};

	for (int i = 0; i < n; i++)
		for (unsigned d = 0; d < 4; d++)
			one_region_edge(best, cur, tgt, regions[i],
					dirs[d], gap, v, user);
}

void
kwm_edge_output(KwmBox *best, KwmBox cur, KwmBox tgt, KwmRect usable,
		KwmEdgeValidator v, void *user)
{
	static const KwmBox unbounded = {
		INT_MIN, INT_MAX, INT_MAX, INT_MIN
	};
	static const unsigned dirs[] = {
		KWM_EDGE_LEFT, KWM_EDGE_RIGHT, KWM_EDGE_TOP, KWM_EDGE_BOTTOM
	};

	KwmBox out;
	out.top = usable.y;
	out.right = usable.x + usable.w;
	out.bottom = usable.y + usable.h;
	out.left = usable.x;

	/*
	 * An output is treated as four half-planes, each sharing one edge of it
	 * and extending away from it, and only the opposing edge of each is
	 * ever finite. Validating the aligned edge as well would compare four
	 * infinities for nothing.
	 */
	for (unsigned d = 0; d < 4; d++)
		v(slot(best, dirs[d]),
		  kwm_edge_of(cur, dirs[d], 0),
		  kwm_edge_of(tgt, dirs[d], 0),
		  kwm_edge_of(out, dirs[d], 0),
		  kwm_edge_of(unbounded, dirs[d], 0),
		  is_lesser(dirs[d]), user);
}

void
kwm_edge_check(int *best, KwmEdge cur, KwmEdge tgt, KwmEdge oppose,
	       KwmEdge align, int lesser, void *user)
{
	(void)lesser;
	(void)user;

	if (cur.offset == tgt.offset)
		return;

	int decreasing = tgt.offset < cur.offset;

	if (kwm_edge_between(cur.offset, tgt.offset, oppose.offset))
		*best = kwm_edge_best(*best, oppose.offset, decreasing);

	if (kwm_edge_between(cur.offset, tgt.offset, align.offset))
		*best = kwm_edge_best(*best, align.offset, decreasing);
}

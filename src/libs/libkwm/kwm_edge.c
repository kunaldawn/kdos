/* libkwm — the neighbour-edge search. See kwm.h.
 *
 * Ported from kdos-comp's edges.c. What stayed behind is everything that walks
 * the compositor: the view list, the output list, and the scene-graph pass that
 * works out which edges are visible. What is here is the arithmetic those three
 * feed, which is the half that can be asserted without one.
 */

#include "kwm.h"

void
kwm_edge_check(int *best, KwmEdge cur, KwmEdge tgt, KwmEdge oppose,
	       KwmEdge align)
{
	if (cur.offset == tgt.offset)
		return;

	int decreasing = tgt.offset < cur.offset;

	if (kwm_edge_between(cur.offset, tgt.offset, oppose.offset))
		*best = kwm_edge_best(*best, oppose.offset, decreasing);

	if (kwm_edge_between(cur.offset, tgt.offset, align.offset))
		*best = kwm_edge_best(*best, align.offset, decreasing);
}

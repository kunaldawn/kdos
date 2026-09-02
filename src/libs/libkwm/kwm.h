/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkwm — the window model, and only the model
 *
 * Placement, tiling, the neighbour-edge search, the focus stack, workspace
 * semantics and output ordering. Two desktops obey it: kdos-comp draws windows
 * in pixels and kdos-con draws them in cells, and a defect in any rule here is
 * therefore ONE fix rather than two that drift.
 *
 * NOTHING BUT libkbase, AND NO MATHS LIBRARY. kdos-comp compiles libkbase and
 * libkcolor into a static archive and feeds it to meson; giving this library a
 * real `-l` moves that archive with it. The edge sweep interpolates with
 * doubles, which is plain arithmetic and calls nothing — do not reach for
 * anything that would need -lm.
 *
 * THIS LIBRARY KNOWS NOTHING ABOUT WINDOWS. It is handed rectangles and told
 * what is being asked; what a window IS, which output it is on, whether a
 * client accepted its size and whether it is maximised all stay with the
 * caller. That is what lets a compositor and a cell grid share it.
 *
 * The contract is testing/fixtures/wm/geometry.txt, every row of which cites
 * the line of kdos-comp it was derived from. This library reproduces that
 * table. Where a rule here disagrees with a row there, the row is right.
 * ---------------------------------
 */

#ifndef KWM_H
#define KWM_H

#include <limits.h>

/*
 * Laid out to match KRect (libktui) and struct border (kdos-comp) field for
 * field, so a caller converts with a four-field copy the compiler removes.
 * Reordering either breaks that at every call site at once — and silently,
 * because struct border is top/right/bottom/left and nothing warns.
 */
typedef struct {
	int x, y, w, h;
} KwmRect;

typedef struct {
	int top, right, bottom, left;
} KwmBorder;

/*
 * The bit VALUES match enum lab_edge, because kdos-comp passes its own enum
 * straight in. Changing one without the other is a silent geometry bug.
 */
enum {
	KWM_EDGE_NONE = 0,
	KWM_EDGE_TOP = 1 << 0,
	KWM_EDGE_BOTTOM = 1 << 1,
	KWM_EDGE_LEFT = 1 << 2,
	KWM_EDGE_RIGHT = 1 << 3,
	KWM_EDGE_CENTER = 1 << 4,

	KWM_EDGES_CARDINAL = KWM_EDGE_TOP | KWM_EDGE_BOTTOM
		| KWM_EDGE_LEFT | KWM_EDGE_RIGHT
};

/* ────────────────────────────────────────────────────────────────────────
 * Tiling
 *
 * Two steps, because the compositor keeps them apart and the split is the
 * useful one: what the tiled state BECOMES is a decision over a bitmask, and
 * what that state LOOKS like is arithmetic over a rectangle. Nothing about a
 * window enters either.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * The tiled state after snapping `cur` towards `edge`.
 *
 * CALL ONLY WHEN THE VIEW IS NOT MAXIMISED. A maximised view is unmaximised
 * first and takes `edge` unchanged; that check is about view state, which this
 * library cannot see, so it stays with the caller.
 *
 * `*move_output` is set when the caller must move the view to the output
 * adjacent in `edge`'s direction — and the caller must leave the view ALONE if
 * there is no usable output there, rather than applying the returned state.
 *
 * The transition splits `cur` into the component parallel to the snap axis and
 * the component orthogonal to it. A half plus an orthogonal edge is a quarter;
 * a quarter snapped against its own parallel component is the half that
 * remains; and A QUARTER SNAPPED TOWARDS THE EDGE IT ALREADY OCCUPIES COLLAPSES
 * TO A HALF, because the parallel component is then neither the inverse nor
 * absent and no branch matches. That last one looks like a bug and is not.
 */
unsigned kwm_tile_next(unsigned cur, unsigned edge, int combine, int across,
		       int *move_output);

/*
 * The rectangle a tiled state occupies inside `usable`, inset by `margin`.
 *
 * THE TWO HALVES OF AN AXIS COME FROM DIFFERENT EXPRESSIONS — (size + gap) / 2
 * and (size - gap) / 2 — which is what puts a WHOLE gap between two tiled
 * windows rather than half a gap each. An odd dimension therefore gives the
 * right or bottom half one extra pixel; that is the behaviour, not a rounding
 * error to be corrected.
 *
 * A state matching none of the four cardinal bits — CENTER, or NONE — is the
 * whole usable area inset by the gap. CENTER is maximise-shaped, not centred.
 */
KwmRect kwm_tile_geom(KwmRect usable, int gap, KwmBorder margin, unsigned tiled);

/*
 * Has a press-and-move become a drag?
 *
 * MEASURED IN CELLS, so a drag begins when the pointer or finger leaves the
 * cell it went down in. Both desktops must agree, or the same gesture picks a
 * file up on one and merely selects it on the other. A threshold in pixels is a
 * number the console cannot see.
 */
static inline int kwm_drag_threshold(int dx, int dy)
{
	return dx != 0 || dy != 0;
}

static inline int kwm_edge_is_cardinal(unsigned e)
{
	return e == KWM_EDGE_TOP || e == KWM_EDGE_BOTTOM
		|| e == KWM_EDGE_LEFT || e == KWM_EDGE_RIGHT;
}

static inline unsigned kwm_edge_invert(unsigned e)
{
	unsigned r = KWM_EDGE_NONE;
	if (e & KWM_EDGE_TOP)
		r |= KWM_EDGE_BOTTOM;
	if (e & KWM_EDGE_BOTTOM)
		r |= KWM_EDGE_TOP;
	if (e & KWM_EDGE_LEFT)
		r |= KWM_EDGE_RIGHT;
	if (e & KWM_EDGE_RIGHT)
		r |= KWM_EDGE_LEFT;
	return r;
}

/* ────────────────────────────────────────────────────────────────────────
 * The neighbour-edge search
 *
 * One question — moving this edge in this direction, which other edge does it
 * meet first — and three commands are built on it: move to the next edge, grow
 * to the next edge, shrink to the next edge.
 *
 * INT_MIN and INT_MAX mean "no edge here" and propagate through the arithmetic
 * rather than wrapping, which is why the addition saturates.
 * ──────────────────────────────────────────────────────────────────────── */

#define KWM_BOUNDED(a) ((a) < INT_MAX && (a) > INT_MIN)

/* A line segment: a position along one axis, and its extent along the other. */
typedef struct {
	int offset;
	int min, max;
} KwmEdge;

static inline int kwm_clip_add(int a, int b)
{
	if (b > 0)
		return a >= (INT_MAX - b) ? INT_MAX : a + b;
	if (b < 0)
		return a <= (INT_MIN - b) ? INT_MIN : a + b;
	return a;
}

static inline int kwm_clip_sub(int a, int b)
{
	if (b > 0)
		return a <= (INT_MIN + b) ? INT_MIN : a - b;
	if (b < 0)
		return a >= (INT_MAX + b) ? INT_MAX : a - b;
	return a;
}

/*
 * The better of two candidate stopping points. A bounded edge always beats an
 * unbounded one; between two bounded edges the nearest in the direction of
 * travel wins, which is the larger going down and the smaller going up.
 */
int kwm_edge_best(int next, int edge, int decreasing);

/*
 * Whether `other` lies between where a moving edge is and where it is going.
 * The TARGET is inclusive and the CURRENT position is exclusive, in both
 * directions: an edge you are already sitting on is not a snap point, and an
 * edge exactly at the target is.
 */
int kwm_edge_between(int cur, int tgt, int other);

/*
 * A box in ABSOLUTE edge coordinates, which is a different thing from a
 * KwmBorder of thicknesses even though kdos-comp uses `struct border` for
 * both. Two types here, so a caller cannot pass one where the other belongs.
 */
typedef struct {
	int top, right, bottom, left;
} KwmBox;

/*
 * A region a moving edge may stop against: its box, and which of its own edges
 * are actually visible. An edge that is covered by another window is not a
 * thing you can snap to, and `visible` is how the caller says so — working that
 * out needs the scene graph, so it stays with the caller.
 */
typedef struct {
	KwmBox box;
	unsigned visible;
} KwmRegion;

/*
 * Decides whether a region edge is a stopping point, and updates `*best`.
 *
 * `user` is not decoration: kdos-comp's validators read rc.gap and the two
 * edge-strength settings out of a global, and this library owns no globals.
 * Snapping and pointer resistance want DIFFERENT answers from the same search
 * — resistance has resist and attract zones and snapping does not — which is
 * why this is a callback rather than a rule written in here.
 */
typedef void (*KwmEdgeValidator)(int *best, KwmEdge cur, KwmEdge tgt,
				 KwmEdge oppose, KwmEdge align, int lesser,
				 void *user);

/* The four "no edge here" bounds a search starts from. */
void kwm_edge_init(KwmBox *best);

/* One edge of a box, offset outward by `pad`. */
KwmEdge kwm_edge_of(KwmBox b, unsigned dir, int pad);

/*
 * The stopping points a box moving from `cur` to `tgt` meets among `regions`.
 *
 * An OPPOSING edge keeps the gap and an ALIGNED edge does not, which is why
 * only the aligned one is padded: the first is two windows placed beside each
 * other and the second is two windows lined up.
 */
void kwm_edge_regions(KwmBox *best, KwmBox cur, KwmBox tgt,
		      const KwmRegion *regions, int n, int gap,
		      KwmEdgeValidator v, void *user);

/* The same, against the edges of an output the box is inside. */
void kwm_edge_output(KwmBox *best, KwmBox cur, KwmBox tgt, KwmRect usable,
		     KwmEdgeValidator v, void *user);

/*
 * The validator snapping uses: an edge counts when it lies between where the
 * moving edge is and where it is going. Pointer resistance supplies its own.
 */
void kwm_edge_check(int *best, KwmEdge cur, KwmEdge tgt, KwmEdge oppose,
		    KwmEdge align, int lesser, void *user);

/*
 * Whether an edge sweeping from `cur` to `tgt` passes through `obstacle`.
 *
 * The moving segment sweeps a quadrilateral, so the test is against the
 * quadrilateral's extent at the obstacle's own offset, found by interpolation.
 * An integer midpoint is a different answer and a different window position.
 */
int kwm_edge_sweeps(KwmEdge cur, KwmEdge tgt, KwmEdge obstacle);

/* ────────────────────────────────────────────────────────────────────────
 * Placement
 *
 * Where a new window lands. NOT a cascade: an irregular grid is built by
 * extending every existing window's edges to infinity, each interval is counted
 * for how many windows cover it, and the candidate is convolved across the grid
 * in four directions. The first position with no overlap at all ends the search.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * `ex` are the existing windows on the same output, as absolute edges ALREADY
 * INFLATED BY THEIR OWN DECORATION. This library cannot ask how thick a frame
 * is, and two callers measure it differently.
 *
 * `margin` and `gap` are the candidate's; the returned rectangle is where its
 * CONTENT goes, inset by both, exactly as the default corner is.
 *
 * With nothing else on the output — or if the grid cannot be allocated — the
 * answer is that default corner, which is the same degradation the compositor
 * already has.
 */
KwmRect kwm_place(KwmRect usable, int gap, KwmBorder margin,
		  int want_w, int want_h, const KwmBox *ex, int n);

/*
 * Bring a rectangle back inside a work area.
 *
 * MOVES BEFORE IT SHRINKS. A window pushed off the edge by a smaller screen
 * should come back the same size, because the screen may grow again and a
 * window that was silently made smaller never grows back. Only one that cannot
 * fit at all is shrunk, and then to exactly the area.
 */
KwmRect kwm_fit(KwmRect want, KwmRect work);

/* ────────────────────────────────────────────────────────────────────────
 * Rings
 *
 * Window cycling and workspace switching are both a walk around a ring with a
 * sentinel between the last item and the first. The compositor keeps them as
 * linked lists and the console keeps them as arrays; the RULE is the same one,
 * so it lives here and neither writes it twice.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * The next index in a ring of `n`, `dir` positive for forward. The sentinel is
 * skipped, so this always wraps and never returns `cur` unless n is 1.
 * Returns -1 for an empty ring or an index outside it.
 */
int kwm_ring_next(int n, int cur, int dir);

/*
 * The nearest workspace to `cur` that has something on it, searching in one
 * direction and wrapping AT MOST ONCE. -1 when there is none.
 *
 * Occupancy is an INPUT, not something derived here. The compositor counts
 * views that are not omnipresent; the panel counts windows that are not
 * minimised, because the workspace protocol reports active, urgent and hidden
 * but never "there is something here". Those are two different rules with two
 * different right answers, and a library that picked one would be wrong on the
 * other side.
 */
int kwm_ws_adjacent(const unsigned char *occupied, int n, int cur,
		    int reverse, int wrap);

#endif /* KWM_H */

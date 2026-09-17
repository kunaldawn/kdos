/* kdos-con — the window list, and compositing it into the grid. See con.h. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "con.h"

Session S;

Win *win_find(int id)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->id == id)
			return w;
	return NULL;
}

Win *win_focused(void)
{
	return win_find(S.focus);
}

/*
 * Run-or-raise's search: the next window running `prog` after window `after`.
 *
 * THE LIST ORDER IS THE STACK, so "next" here is the next one further down —
 * pressing a chord repeatedly walks a program's windows front to back and then
 * round, which is the order the eye already has for them.
 *
 * ONLY THE HEAD OF A FAMILY ANSWERS, and `after` is resolved up to one. Every
 * window of one guest carries the same program name, so a dialog, a dock and a
 * splash all match the chord — and the dialog is the one in front, so a search
 * that took the topmost match would hand the chord a question instead of the
 * document it is about, and a second press would never move. A raise carries
 * the whole family up anyway, so the head is the member that reaches all of
 * them.
 *
 * MINIMISED WINDOWS AND OTHER WORKSPACES COUNT. `reachable()` below is the
 * CYCLE's rule and would be wrong here: a run-or-raise that skipped a
 * minimised editor would start a second one, and the whole point of the chord
 * is that there is one.
 *
 * Panels, the background layer and the saver never do: they are chrome, they
 * carry no `prog`, and an empty `prog` matches nothing — which is also what
 * stops a surface that named no app id from answering for every chord.
 */
Win *win_find_prog(const char *prog, int after)
{
	Win *first = NULL, *a = win_find(after);
	int depth = 0, seen;

	if (!prog || !*prog)
		return NULL;
	while (a && a->owner && depth++ < 4) {
		Win *own = win_find(a->owner);

		if (!own)
			break;
		a = own;
	}
	seen = a == NULL;
	for (Win *w = S.wins; w; w = w->next) {
		if (w->panel || w->background || w->overlay || w->owner ||
		    strcmp(w->prog, prog))
			continue;
		if (!first)
			first = w;
		if (seen)
			return w;
		if (w == a)
			seen = 1;
	}
	/* Past the end is back to the top — and when `after` named a window
	 * that is gone, the first match is still the right answer. */
	return first;
}

/*
 * THE MODAL QUESTION STANDING OVER THIS WINDOW, or NULL.
 *
 * A MODAL IS MODAL TO ITS APPLICATION AND NOT TO THE MACHINE. It blocks the
 * one window it belongs to — that window cannot be raised, focused or closed
 * while it is up — and nothing else on the desktop: the terminal beside it,
 * the panel and every other application carry on, because a dialog that stops
 * the whole session is a dialog a crashed application takes the machine down
 * with.
 *
 * A MINIMISED MODAL BLOCKS NOTHING. Somebody put it out of the way on purpose
 * and a window that could not then be used would be a window with no way back
 * to it at all.
 *
 * AND NEITHER DOES ONE OFF ITS OWNER'S DESK. A question the person cannot see
 * blocking a window they are looking at is a window that has stopped answering
 * with nothing on the screen to say why, and the flash would land where they
 * are not looking. Every path that moves one of the pair moves the other, so
 * the test is the rule the rest of this file keeps, written where the block is
 * decided rather than in each of them.
 */
Win *win_modal_for(int id)
{
	Win *o = win_find(id);

	if (!id || !o)
		return NULL;
	for (Win *w = S.wins; w; w = w->next)
		if (w->modal && w->owner == id && !w->minimised && !w->hidden &&
		    (w->sticky || o->sticky || w->workspace == o->workspace))
			return w;
	return NULL;
}

/* Move one window to the front of the list, which is the top of the stack. */
static Win *list_front(int id)
{
	Win **pp = &S.wins;

	while (*pp && (*pp)->id != id)
		pp = &(*pp)->next;
	if (!*pp)
		return NULL;

	Win *w = *pp;

	*pp = w->next;
	w->next = S.wins;
	S.wins = w;
	return w;
}

/* The same move to the TAIL, which is the bottom of the stack. A lower is a
 * raise read backwards and the list is the one place either of them writes. */
static void list_back(int id)
{
	Win **pp = &S.wins;
	Win *w;

	while (*pp && (*pp)->id != id)
		pp = &(*pp)->next;
	if (!*pp)
		return;

	w = *pp;
	*pp = w->next;
	w->next = NULL;
	for (pp = &S.wins; *pp; pp = &(*pp)->next)
		;
	*pp = w;
}

/*
 * THE WINDOW AT THE TOP OF AN OWNERSHIP CHAIN.
 *
 * A raise and a lower both act on the whole family and therefore on its head,
 * whichever member was named: a dialog dragged through the stack on its own
 * ends up on the wrong side of the window it is asking about, which is an
 * application that looks frozen either way round.
 *
 * THE DEPTH IS BOUNDED BECAUSE OWNERSHIP NEED NOT BE. A guest names the owner
 * and nothing this side can promise the chain has no cycle in it; a bound is
 * the one answer that cannot become a session that stops drawing.
 */
static Win *family_head(Win *w)
{
	int depth = 0;

	while (w && w->owner && depth++ < 4) {
		Win *own = win_find(w->owner);

		if (!own)
			break;
		w = own;
	}
	return w;
}

/*
 * AND WHATEVER THIS WINDOW OWNS COMES WITH IT.
 *
 * A dialog left behind the window it is asking about is a dialog a person
 * cannot see and an application that looks frozen. The list is the stack, so
 * moving each child to the front AFTER its owner puts it above the owner, and
 * a child's own children above it.
 *
 * THE DEPTH IS BOUNDED BECAUSE OWNERSHIP NEED NOT BE. A guest names the owner
 * and nothing this side can promise the chain has no cycle in it; a bound is
 * the one answer that cannot become a session that stops drawing.
 */
static void raise_owned(int id, int depth)
{
	int kids[16];
	int n = 0;

	if (depth > 4)
		return;
	/*
	 * COLLECTED IN THE ORDER THEY ARE TO END IN AND MOVED IN REVERSE. The
	 * list is the stack and each move is to the front, so the last one
	 * moved is the one on top: fronting them in the order they were found
	 * would turn the family over on every raise, and four docks would swap
	 * places each time their image window came up. The modal is collected
	 * first so it ends above its siblings — a question a person has to
	 * answer must not open behind the dialog beside it — and the rest keep
	 * the order they already had.
	 */
	for (int pass = 0; pass < 2; pass++)
		for (Win *o = S.wins; o && n < 16; o = o->next)
			if (o->owner == id && (o->modal == 0) == pass)
				kids[n++] = o->id;
	for (int i = n - 1; i >= 0; i--) {
		if (!list_front(kids[i]))
			continue;
		raise_owned(kids[i], depth + 1);
	}
}

/*
 * A DIALOG GOES WHERE THE WINDOW IT BELONGS TO GOES, AND IS PUT AWAY WITH IT.
 *
 * A question left on a workspace its owner has left is a frame belonging to
 * nothing: an owned window carries no taskbar row of its own, so there is
 * nothing on that desk to reach it by — and a modal stranded there goes on
 * blocking an owner the person is looking at somewhere else. The same walk
 * answers both verbs: `ws` is the workspace to move the family to or -1 to
 * leave it where it is, and `min` the minimised flag to apply or -1.
 *
 * A STICKY CHILD IS NOT MOVED. It is on every workspace already, so there is
 * nowhere to send it and no desk it can be stranded on.
 *
 * THE DEPTH IS BOUNDED for raise_owned()'s reason: the chain is a guest's to
 * name and not this side's to trust.
 */
static void owned_apply(int id, int ws, int min, int depth)
{
	if (depth > 4)
		return;
	for (Win *o = S.wins; o; o = o->next) {
		if (o->owner != id)
			continue;
		if (ws >= 0 && !o->sticky)
			o->workspace = ws;
		if (min >= 0)
			o->minimised = min;
		owned_apply(o->id, ws, min, depth + 1);
	}
}

/*
 * The list IS the stack, front first. Raising is a move to the front rather
 * than a z-index, so there is one answer to what is on top and no way for two
 * windows to claim the same depth.
 *
 * A RAISE AIMED AT THE OWNER OF A MODAL LANDS ON THE MODAL, and flashes it.
 * The ring, the directional search, a number chord and a click on the window
 * all end here, so this is the one place the rule has to be written — and the
 * flash is the answer, because a click that did nothing at all reads as a
 * desktop that has stopped rather than as an application waiting to be
 * answered.
 */
void win_raise(int id)
{
	Win *w = win_find(id);
	Win *m = win_modal_for(id);
	int root;

	if (!w)
		return;
	/*
	 * FROM THE HEAD OF THE FAMILY DOWN, whichever member was named. The
	 * whole family comes up together and the modal ends on top of it, so a
	 * dialog raised on its own brings the window it is asking about up
	 * under it rather than leaving it behind whatever the person was
	 * looking at before.
	 */
	root = family_head(w)->id;
	list_front(root);
	raise_owned(root, 0);
	/*
	 * AND THE WINDOW THAT WAS NAMED ENDS ON TOP OF THE FAMILY. The walk
	 * above brings the family up in its own order, which says nothing
	 * about which member was asked for: fronting the named window last,
	 * with its own children over it again, is what makes a click on one of
	 * four docks bring that dock out rather than the one that happened to
	 * be in front of it.
	 */
	if (id != root) {
		list_front(id);
		raise_owned(id, 0);
	}
	/* The focus stays on the window that was named, or on the question
	 * standing over it. */
	if (m) {
		m->bell_until = con_now_ms() + CON_FLASH_MS;
		S.focus = m->id;
	} else {
		S.focus = id;
	}

	/*
	 * RAISING A GUEST IS A VT SWITCH. It has no cells to bring to the front
	 * — it is on a terminal of its own — so the only thing "front" can mean
	 * for one is the screen showing it.
	 */
	if (w->kind == WIN_VT)
		vt_show(w);
}

/* What the ring can step to — declared here because a lower hands the focus to
 * the frontmost window the ring would reach, and one predicate is what makes
 * that sentence true. Defined with the ring below. */
static int reachable(const Win *w);

/*
 * AND THE FAMILY GOES DOWN DEEPEST-FIRST.
 *
 * Every move is to the TAIL, so the LAST window moved is the bottom one: the
 * order is a grandchild, then its parent, then the owner, which is
 * raise_owned()'s order read backwards. Moving a parent before its children
 * would bury the children under it — the dialog on the wrong side of the
 * window it is asking about, which is the one thing a family move exists to
 * prevent.
 *
 * THE MODAL MOVES FIRST so it ends highest of its siblings, for the reason
 * raise_owned() collects it first: a question a person has to answer must not
 * end up behind the dialog beside it.
 *
 * THE DEPTH IS BOUNDED for raise_owned()'s reason: the chain is a guest's to
 * name and not this side's to trust.
 */
static void lower_owned(int id, int depth)
{
	int kids[16];
	int n = 0;

	if (depth > 4)
		return;
	for (int pass = 0; pass < 2; pass++)
		for (Win *o = S.wins; o && n < 16; o = o->next)
			if (o->owner == id && (o->modal == 0) == pass)
				kids[n++] = o->id;
	for (int i = 0; i < n; i++) {
		lower_owned(kids[i], depth + 1);
		list_back(kids[i]);
	}
}

/*
 * TO THE BACK. The inverse of win_raise, and the only verb that reaches the
 * window underneath one of the same size: every step of the ring RAISES, so a
 * pair of windows fully overlapped stays in the order it is in however many
 * times it is stepped.
 *
 * FROM THE HEAD OF THE FAMILY, as a raise is: a dialog sent to the back on its
 * own would be a question behind the window asking it.
 *
 * THE FOCUS FOLLOWS THE FRONT, AND ONLY WHEN IT WAS ON THE FAMILY THAT MOVED.
 * A keyboard left on a window now covered by another is one whose keys land
 * where nobody is looking — and lowering a window a person is NOT typing in
 * must not take the keyboard off the one they are, which is the rule
 * win_minimise() keeps for the same reason. Nothing is raised to arrange it:
 * the frontmost window the ring can reach is already in front of the one that
 * has just gone to the back.
 */
void win_lower(Win *w)
{
	Win *f;

	if (!w)
		return;
	w = family_head(w);
	lower_owned(w->id, 0);
	list_back(w->id);

	f = win_find(S.focus);
	if (f && family_head(f) == w) {
		for (Win *o = S.wins; o; o = o->next)
			if (reachable(o)) {
				S.focus = o->id;
				break;
			}
	}
	ktui_draw_invalidate();
}

/*
 * What is left for windows once every exclusive zone is taken out. A panel
 * that reserves one genuinely SHRINKS the area rather than covering it, which
 * is the whole difference between a panel and a window that happens to be at
 * the bottom.
 */
/*
 * FIT EVERY WINDOW TO THE WORK AREA AS IT NOW IS.
 *
 * The rules the resize path applies, for the same reasons: a full window is
 * the whole grid rather than the area a bar left over, a tiled one keeps its
 * slot, and everything else goes through `kwm_fit` so a surface's own minimum
 * is honoured in one place.
 *
 * PANELS ARE LEFT ALONE. A panel is placed against an EDGE and the work area
 * is what it carved out — fitting one into that area would push it off its own
 * edge. The resize path re-docks them first, which is a different job from
 * this one: here the grid has not moved and only the area has.
 */
void win_refit(void)
{
	KwmRect area = win_workarea();

	for (Win *w = S.wins; w; w = w->next) {
		if (w->panel)
			continue;
		/*
		 * THE DESKTOP IS WHATEVER THE BARS LEFT, and it is assigned
		 * rather than fitted: `kwm_fit` moves a rectangle and clamps
		 * it and never GROWS one, so an icon layer run through it
		 * would keep the size it had when it attached for ever — and
		 * the size it had when it attached is whatever the grid was
		 * before the first view answered.
		 */
		if (w->background) {
			w->geom = area;
			win_resized(w);
			continue;
		}
		/*
		 * AN ANCHORED OVERLAY IS RE-ANCHORED, NOT FITTED. `kwm_fit`
		 * moves a rectangle the least it can to get it inside the
		 * area, which for a corner surface throws the corner away —
		 * and a surface that attached before the first view was
		 * anchored against the 80x24 fallback grid. That is what put
		 * the welcome card in the top-left corner on top of the
		 * desktop icons on every boot: centred, correctly, in a grid
		 * a quarter of the size of the screen it ended up on.
		 */
		if (w->overlay && w->surf) {
			win_place_corner(w, w->geom.w, w->geom.h,
					 kcon_surface_corner(w->surf),
					 kcon_surface_margin_x(w->surf),
					 kcon_surface_margin_y(w->surf));
			win_resized(w);
			continue;
		}
		if (w->full) {
			w->geom.x = 0;
			w->geom.y = 0;
			w->geom.w = S.cols;
			w->geom.h = S.rows;
		} else if (w->tiled) {
			w->geom = win_tile_rect(w->tiled);
		} else {
			w->geom = kwm_fit(w->geom, area, w->min_w, w->min_h);
		}
		win_resized(w);
	}
}

KwmRect win_workarea(void)
{
	int top = 0, bottom = S.rows, left = 0, right = S.cols;

	for (Win *w = S.wins; w; w = w->next) {
		/* A HIDDEN PANEL RESERVES NOTHING. It is out of the draw loop
		 * and out of the hit test already; a zone it went on holding
		 * would be a strip of desktop no window could reach and
		 * nothing occupies. */
		if (!w->panel || !w->exclusive || w->hidden)
			continue;

		switch (w->panel_edge) {
		case KDISP_EDGE_TOP:
			if (w->geom.h > top)
				top = w->geom.h;
			break;
		case KDISP_EDGE_BOTTOM:
			if (S.rows - w->geom.h < bottom)
				bottom = S.rows - w->geom.h;
			break;
		case KDISP_EDGE_LEFT:
			if (w->geom.w > left)
				left = w->geom.w;
			break;
		default:
			if (S.cols - w->geom.w < right)
				right = S.cols - w->geom.w;
			break;
		}
	}

	/* The built-in taskbar only exists while no shell has attached one. */
	if (!panel_have_shell())
		bottom -= panel_rows();

	KwmRect r = { left, top, right - left, bottom - top };

	if (r.w < 1)
		r.w = 1;
	if (r.h < 1)
		r.h = 1;
	return r;
}

/* Where a docked panel sits, from the edge and thickness it asked for. */
void win_dock(Win *w)
{
	int t = w->geom.h;

	switch (w->panel_edge) {
	case KDISP_EDGE_TOP:
		w->geom.x = 0;
		w->geom.y = 0;
		w->geom.w = S.cols;
		break;
	case KDISP_EDGE_BOTTOM:
		w->geom.x = 0;
		w->geom.y = S.rows - t;
		w->geom.w = S.cols;
		break;
	case KDISP_EDGE_LEFT:
		w->geom.x = 0;
		w->geom.y = 0;
		w->geom.h = S.rows;
		break;
	default:
		w->geom.x = S.cols - w->geom.w;
		w->geom.y = 0;
		w->geom.h = S.rows;
		break;
	}
}

/*
 * THE FRAME AROUND A CONTENT RECT, and this is the direction the cost runs for
 * a FREELY PLACED window: `geom` is the content, and the border is added
 * OUTSIDE it. A window landed by win_place(), dragged, or put at a named
 * rectangle by win_place_at() therefore answers a thicker border by taking
 * more of the desk, and its program keeps every cell of its content.
 *
 * win_tile_rect() runs the same margin the other way for a window whose OUTER
 * rectangle is fixed, and there the thickness comes out of the program. The
 * two together are the whole rule; there is no third direction.
 *
 * AND THE OUTER RECTANGLE IS WHAT THE GRID HOLDS. win_fit_area() deflates the
 * grid by these same numbers, so a freely placed window keeps its content
 * until it no longer fits and is then shrunk like any other — this is the
 * rectangle that has to be on the screen, and `geom` is not it.
 */
KwmRect win_frame(const Win *w)
{
	KwmRect r = w->geom;

	r.x -= CON_FRAME_X;
	r.y -= CON_FRAME_Y;
	r.w += CON_FRAME_X * 2;
	r.h += CON_FRAME_Y * 2;
	return r;
}

/*
 * IS THE SESSION DRAWING THIS ONE'S BORDER.
 *
 * ONE EXPRESSION, because four answers to it drift: the fit below keeps the
 * border on the grid, win_covered_at() counts the cells it occludes, the draw
 * loop paints it and win_grab_at() hands a press inside it to the resize. A
 * window that is framed for one of them and bare for another is a band drawn
 * where nothing is grabbed, or grabbed where nothing is drawn.
 *
 * A panel is docked, a layer is a menu or a toast, the icon layer IS the
 * desktop, and a fullscreen window asked for the screen — the lock and the
 * saver by the same flag. Each is part of the desktop rather than something
 * sitting on it, and none of them gets chrome.
 */
static int win_framed(const Win *w)
{
	return !(w->panel || w->full || w->overlay || w->background);
}

/*
 * THE AREA A WINDOW'S GEOMETRY IS FITTED INTO, and for a framed window it is
 * the grid MINUS the border.
 *
 * `geom` IS THE CONTENT AND WHAT MUST STAY ON THE GRID IS THE FRAME. A content
 * rect fitted to the grid's own edge puts everything win_frame() adds outside
 * it: the left rule, both corners on that side and the band win_grab_at()
 * answers all land on a negative column, and what is drawn is a window with no
 * edge to take hold of. Deflating by exactly what win_frame() inflates by is
 * the one place the two cannot drift apart.
 *
 * THE WHOLE GRID AND NOT THE WORK AREA. A fullscreen window is deliberately
 * over a panel's exclusive zone, and it is also the one class with no border
 * to keep; fitting it to the work area here would take the panel's rows back
 * off it.
 *
 * AN AXIS TOO SHORT TO HOLD A BORDER AND A CELL IS LEFT WHOLE, and ONLY that
 * axis. Deflating a four-column grid by two columns each side leaves an area of
 * negative width, and kwm_fit() answers one with a window of negative width —
 * drawn nowhere at all, which is worse than a border off the edge. The other
 * axis is not in that bargain: a grid narrow enough to refuse a border across
 * can still hold one down, and refusing both would put a border off an edge
 * that had room for it.
 */
static KwmRect win_fit_area(const Win *w)
{
	KwmRect a = { 0, 0, S.cols, S.rows };

	if (!win_framed(w))
		return a;
	if (S.cols >= 2 * CON_FRAME_X + 1) {
		a.x = CON_FRAME_X;
		a.w = S.cols - 2 * CON_FRAME_X;
	}
	if (S.rows >= 2 * CON_FRAME_Y + 1) {
		a.y = CON_FRAME_Y;
		a.h = S.rows - 2 * CON_FRAME_Y;
	}
	return a;
}

/*
 * EVERY PLACEMENT ENDS HERE. A named layout, a restored session, the overlap
 * search, a remembered rectangle, a drag, a snap, a tile, the drop-down and a
 * refit after the grid changed all set `geom` and then run it through this —
 * so the minimum a surface asked for and the border it has to keep are applied
 * in one place rather than at nine call sites.
 *
 * A session that has not been given a size yet has no area to fit to, and
 * fitting to a zero one would set every window to zero cells.
 */
static void win_fit(Win *w)
{
	if (S.cols > 0 && S.rows > 0)
		w->geom = kwm_fit(w->geom, win_fit_area(w), w->min_w,
				  w->min_h);
}

/*
 * Where a new window lands. The existing windows are handed over as absolute
 * edges already inflated by their own frame, because libkwm cannot ask how
 * thick one is.
 */
void win_place(Win *w, int want_w, int want_h)
{
	KwmBox ex[64];
	int n = 0;

	/*
	 * WHERE THIS PROGRAM'S WINDOW WAS, IF IT IS REMEMBERED. Here and not at
	 * the call sites: the lookup sits behind the `floating` test below, so
	 * the only windows that reach it are the ones that open by the overlap
	 * search — a layer is placed by win_place_corner() and a restored
	 * session by win_place_at(), and the one overlay that arrives here is
	 * a guest's splash, which is floating and so is centred and looked up
	 * for nothing. That is what keeps a menu or a splash from inheriting
	 * the rectangle a person keeps their document at.
	 */
	/*
	 * A FLOAT OPENS WHERE THE EYE IS AND IS NOT LOOKED UP. Being placed in
	 * the middle at a size it asked for is the whole of what the entry
	 * requested; a remembered rectangle from a previous run would answer a
	 * different question, and answer it first.
	 */
	if (w->floating) {
		KwmRect a = win_workarea();
		Win *own = w->owner ? win_find(w->owner) : NULL;
		int cw = want_w > 0 ? want_w : a.w;
		int ch = want_h > 0 ? want_h : a.h;

		if (cw > a.w)
			cw = a.w;
		if (ch > a.h)
			ch = a.h;
		/*
		 * A WINDOW THAT BELONGS TO ANOTHER OPENS CENTRED ON THAT ONE,
		 * not in the middle of the screen: a dialog belongs to the
		 * window that raised it, and a person whose eye is on a
		 * document at the left of a wide screen should not have to
		 * find the question about it in the middle.
		 *
		 * CLAMPED INTO THE WORK AREA AFTERWARDS, because the owner may
		 * be at an edge or larger than the area itself — a dialog half
		 * off the screen is one whose buttons cannot be clicked.
		 */
		if (own) {
			KwmRect f = win_frame(own);

			w->geom.x = f.x + (f.w - cw) / 2;
			w->geom.y = f.y + (f.h - ch) / 2;
			if (w->geom.x < a.x + CON_FRAME_X)
				w->geom.x = a.x + CON_FRAME_X;
			if (w->geom.y < a.y + CON_FRAME_Y)
				w->geom.y = a.y + CON_FRAME_Y;
			if (w->geom.x + cw > a.x + a.w - CON_FRAME_X)
				w->geom.x = a.x + a.w - CON_FRAME_X - cw;
			if (w->geom.y + ch > a.y + a.h - CON_FRAME_Y)
				w->geom.y = a.y + a.h - CON_FRAME_Y - ch;
		} else {
			w->geom.x = a.x + (a.w - cw) / 2;
			w->geom.y = a.y + (a.h - ch) / 2;
		}
		w->geom.w = cw;
		w->geom.h = ch;
		win_resized(w);
		return;
	}

	/* A REMEMBERED RECTANGLE IS STILL FITTED. geo_recall() fits what it
	 * read into the WORK AREA, which is a rectangle for the content and
	 * says nothing about the border standing outside it; and this road
	 * returns to a caller that reads `geom` and opens a pty at it, with no
	 * win_resized() between. */
	if (geo_recall(w)) {
		win_fit(w);
		return;
	}

	for (Win *o = S.wins; o && n < 64; o = o->next) {
		if (o == w || o->minimised || o->hidden)
			continue;
		/* A STICKY WINDOW IS AN OBSTACLE ON EVERY WORKSPACE, because it
		 * is drawn on every one: asking which workspace it recorded
		 * would place a new window underneath the scratchpad on all of
		 * them but the one it happens to name. */
		if (!o->sticky && o->workspace != w->workspace)
			continue;

		KwmRect f = win_frame(o);

		ex[n].left = f.x;
		ex[n].top = f.y;
		ex[n].right = f.x + f.w;
		ex[n].bottom = f.y + f.h;
		n++;
	}

	/* top, right, bottom, left — the struct's own order, and the frame is
	 * thicker across than down, so four copies of one number would hand
	 * libkwm a margin the frame does not have on either axis. */
	KwmBorder m = { CON_FRAME_Y, CON_FRAME_X, CON_FRAME_Y, CON_FRAME_X };
	KwmRect area = win_workarea();
	KwmRect g = kwm_place(area, S.gap, m, want_w, want_h, ex, n);

	w->geom.x = g.x;
	w->geom.y = g.y;
	w->geom.w = want_w;
	w->geom.h = want_h;
	/* THE SEARCH IS ASKED, NOT OBEYED. kwm_place() carries the margin into
	 * where it looks and still answers the least-overlapping rectangle it
	 * found, which on a grid with no room left is one whose margin hangs
	 * off an edge — and the margin is this window's border. */
	win_fit(w);
}

/*
 * WHERE A LAYER ASKED TO SIT.
 *
 * An overlay is not placed by the minimal-overlap search: a menu belongs to the
 * button that opened it, a toast belongs in the corner, and a search that put
 * either wherever there happened to be room is a surface a person has to hunt
 * for. `corner` is `enum kdisp_corner` and the margins are the distances from
 * the two edges that corner names, in cells — the same field libkwl reads as
 * pixels, which is the same number in a caller because `kdisp_cell_w()` answers
 * 1 on this transport.
 *
 * CLAMPED TO THE WORK AREA, NEVER PLACED OFF IT. A margin is what the caller
 * would like and the grid is what exists; a bottom-anchored menu taller than
 * the screen above the taskbar is drawn from the top of the work area rather
 * than from a negative row.
 */
void win_place_corner(Win *w, int want_w, int want_h, int corner, int mx,
		      int my)
{
	KwmRect a = win_workarea();
	int x, y;

	if (want_w > a.w)
		want_w = a.w;
	if (want_h > a.h)
		want_h = a.h;
	if (mx < 0)
		mx = 0;
	if (my < 0)
		my = 0;

	switch (corner) {
	case KDISP_CORNER_TOP_RIGHT:
		x = a.x + a.w - want_w - (mx ? mx : 1);
		y = a.y + (my ? my : 1);
		break;
	case KDISP_CORNER_TOP_LEFT:
		x = a.x + mx;
		y = a.y + my;
		break;
	case KDISP_CORNER_BOTTOM_LEFT:
		x = a.x + mx;
		y = a.y + a.h - want_h - my;
		break;
	case KDISP_CORNER_BOTTOM_CENTER:
		x = a.x + (a.w - want_w) / 2;
		y = a.y + a.h - want_h - (my ? my : 1);
		break;
	default:	/* KDISP_CORNER_CENTER */
		x = a.x + (a.w - want_w) / 2;
		y = a.y + (a.h - want_h) / 2;
		break;
	}

	if (x + want_w > a.x + a.w)
		x = a.x + a.w - want_w;
	if (y + want_h > a.y + a.h)
		y = a.y + a.h - want_h;
	if (x < a.x)
		x = a.x;
	if (y < a.y)
		y = a.y;

	w->geom.x = x;
	w->geom.y = y;
	w->geom.w = want_w;
	w->geom.h = want_h;
}

/*
 * THE RECTANGLE A TILED STATE ASKS FOR, which is not always a tile.
 *
 * `kwm_tile_geom` halves an axis for each edge snapped on it, so a state
 * holding BOTH edges of an axis collapses that axis to nothing — left and
 * right together put x1 and x2 on the same midpoint, and the window comes back
 * with a NEGATIVE width and is drawn nowhere. The window model defines no such
 * tile: the compositor's transition never produces an opposing pair, so the
 * contract fixture has no row for one and libkwm is right to answer as it
 * does. Maximise is this program's own state, and the work area is what it
 * means.
 *
 * AND HERE THE BORDER'S THICKNESS COMES OUT OF THE PROGRAM. The outer
 * rectangle is the work area or a division of it, so the margin is SUBTRACTED
 * from a rectangle that is already fixed and the content this returns is
 * 2 * CON_FRAME_X columns and 2 * CON_FRAME_Y rows smaller than the space the
 * window fills. Every maximised, snapped and tiled window is on this path, as
 * is the scratchpad's drop-down through scratch_shape(), so raising either
 * number takes cells from all of them at once — only a freely placed window
 * pays for its border outside its content.
 */
KwmRect win_tile_rect(unsigned tiled)
{
	KwmRect a = win_workarea();
	/* top, right, bottom, left. */
	KwmBorder m = { CON_FRAME_Y, CON_FRAME_X, CON_FRAME_Y, CON_FRAME_X };

	if (tiled != KWM_EDGES_CARDINAL)
		return kwm_tile_geom(a, S.gap, m, tiled);

	a.x += CON_FRAME_X;
	a.y += CON_FRAME_Y;
	a.w -= CON_FRAME_X * 2;
	a.h -= CON_FRAME_Y * 2;
	if (a.w < 1)
		a.w = 1;
	if (a.h < 1)
		a.h = 1;
	return a;
}

void win_snap(Win *w, unsigned edge, int combine)
{
	if (!w)
		return;

	int move_output = 0;
	unsigned next = kwm_tile_next(w->tiled, edge, combine,
				      /* across */ 0, &move_output);

	/* One output for now, so a request to cross to the next screen has
	 * nowhere to go and the window is left alone — which is what the
	 * compositor does when there is no adjacent output either. */
	if (move_output)
		return;

	if (!w->tiled)
		w->restore = w->geom;

	w->tiled = next;
	w->geom = win_tile_rect(next);
	win_resized(w);
}

/*
 * A WINDOW AT A REMEMBERED RECTANGLE.
 *
 * `win_place` searches for somewhere to put a new window; this puts one
 * exactly where it was, which is what a restored session means. The fit is
 * still applied — a session saved on a bigger screen would otherwise place a
 * window where nothing can reach it, and the screen a session comes back on is
 * whatever is attached now.
 */
void win_place_at(Win *w, int x, int y, int cw, int ch)
{
	if (!w || cw < 1 || ch < 1)
		return;
	w->geom.x = x;
	w->geom.y = y;
	w->geom.w = cw;
	w->geom.h = ch;
	win_resized(w);
}

/*
 * Tell the window it changed size. A terminal reflows its grid and a surface
 * is configured; both are the same event to the window and neither may be
 * skipped, because a program that was not told draws the old size into the new
 * rectangle and the difference is never repainted.
 */
void win_resized(Win *w)
{
	/* THROUGH win_fit, which is where the surface's minimum and the
	 * border's claim on the grid are both applied. */
	win_fit(w);

	if (w->kind == WIN_TERM && w->term)
		kvt_term_resize(w->term, w->geom.w, w->geom.h);
	else if (w->kind == WIN_SURFACE && w->surf)
		kcon_surface_configure(w->surf, w->geom.w, w->geom.h);
	else if (w->kind == WIN_EMBED)
		embed_resized(w);
}

/*
 * Maximise is the four-edge state, so an untile from it returns where an untile
 * from any other tile does and there is no second restore rectangle to keep in
 * step with the first.
 */
/*
 * OUT OF EVERY TILE, BACK TO THE RECTANGLE THE FIRST ONE WAS TAKEN FROM.
 *
 * Maximise is the four-edge tile and a snap is one of the others, so there is
 * ONE restore rectangle and one way out of it — a second untile written
 * somewhere else would be a second thing to keep in step with `restore`.
 */
static void win_untile(Win *w)
{
	w->tiled = KWM_EDGE_NONE;
	w->geom = w->restore;
	win_resized(w);
}

void win_maximise(Win *w)
{
	if (!w)
		return;
	if (w->tiled == KWM_EDGES_CARDINAL) {
		win_untile(w);
		return;
	}
	if (!w->tiled)
		w->restore = w->geom;
	w->tiled = KWM_EDGES_CARDINAL;
	w->geom = win_tile_rect(w->tiled);
	win_resized(w);
}

/*
 * Fullscreen takes the WHOLE grid, panel included, and drops the frame — the
 * one case where a window is allowed over a docked panel's exclusive zone,
 * because a program that asked for the screen and got the screen minus a row
 * has been told a size that is not the one it is showing.
 *
 * FULLSCREEN IS ORTHOGONAL TO THE TILE, which is why the tile flag is left
 * alone across it and `restore` is written only from an untiled rectangle:
 * `restore` is what an untile returns to, so a fullscreen that put a tile
 * rect in it would leave the later untile returning the window to the tile it
 * is already in — and close writes that to the geometry table, so the
 * original rectangle is gone from the next session too. Leaving fullscreen
 * RE-DERIVES the tile instead of replaying a rectangle, so a grid that
 * changed underneath — a panel docked, the output resized — still lands the
 * window in its tile.
 */
void win_fullscreen(Win *w)
{
	if (!w)
		return;
	if (w->full) {
		w->full = 0;
		w->geom = w->tiled ? win_tile_rect(w->tiled) : w->restore;
		win_resized(w);
		return;
	}
	if (!w->tiled)
		w->restore = w->geom;
	w->full = 1;
	w->geom.x = 0;
	w->geom.y = 0;
	w->geom.w = S.cols;
	w->geom.h = S.rows;
	win_resized(w);
}

void win_minimise(Win *w)
{
	if (!w || w->panel)
		return;
	/*
	 * A WINDOW WITH NO ROW OF ITS OWN IS NOT MINIMISABLE. The taskbar row
	 * is the way back from a minimise, and a dialog, a dock and a splash
	 * are listed under the window they belong to rather than carrying one
	 * — so putting one away would leave it drawn nowhere, cycled past and
	 * in no bar, reachable by nothing on the desktop. A question is
	 * answered or closed, not put away.
	 */
	if (w->no_task)
		return;
	w->minimised = 1;
	/* AND WHAT IT OWNS GOES WITH IT, or the dialogs stay drawn with the
	 * window they are asking about gone from under them. */
	owned_apply(w->id, -1, 1, 0);

	Win *f = win_focused();

	/*
	 * THE FOCUS CANNOT STAY ON A WINDOW THAT IS DRAWN NOWHERE — it may be
	 * one of the dialogs that went with this one.
	 *
	 * AND NOTHING ELSE MOVES IT. Minimising a window a person is not
	 * typing in must not take the keyboard off the one they are: the chip,
	 * `Super+n` on a window that is not in front, and the window list's
	 * `m` all reach this with somebody else focused, and a cycle run
	 * unconditionally would hand the keyboard to whatever the ring offers
	 * next.
	 */
	if (f && f->minimised) {
		S.focus = 0;
		win_cycle(1);
	}
}

/*
 * BRING ONE BACK, focused and on top.
 *
 * A minimise with no restore is a one-way door in a desktop's most ordinary
 * verb: the window is drawn nowhere, cycled past, and not hit-testable, so
 * nothing a person can do reaches it again. It also comes back onto the
 * workspace they are on rather than the one it left — a window restored onto
 * a workspace nobody is looking at has not been restored.
 */
void win_restore(Win *w)
{
	int depth = 0;

	if (!w)
		return;
	/* FROM THE HEAD OF THE FAMILY, as the minimise was: a dialog has no
	 * row of its own and went away under the window it belongs to, so
	 * that window's row is the way back for both of them. Bounded for
	 * raise_owned()'s reason. */
	while (w->owner && depth++ < 4) {
		Win *own = win_find(w->owner);

		if (!own)
			break;
		w = own;
	}
	if (!w->minimised)
		return;
	w->minimised = 0;
	w->workspace = S.workspace;
	/* What went away with it comes back with it, onto the same workspace:
	 * a dialog left minimised has no row of its own to be restored by. */
	owned_apply(w->id, S.workspace, 0, 0);
	/* The focus is win_raise's to set, and it is not always this window:
	 * a restore of a window a modal is standing over lands on the modal,
	 * which is the whole of what a modal means. */
	win_raise(w->id);
	ktui_draw_invalidate();
}

/*
 * The most recently minimised window on this workspace, or the last one
 * anywhere when this workspace has none. The list is in stacking order, so the
 * first match walking down is the one that went away last — which is the one a
 * person means by "bring it back".
 */
Win *win_last_minimised(void)
{
	Win *any = NULL;

	for (Win *w = S.wins; w; w = w->next) {
		if (!w->minimised || w->panel)
			continue;
		/* A sticky window is here whichever workspace this is. */
		if (w->sticky || w->workspace == S.workspace)
			return w;
		if (!any)
			any = w;
	}
	return any;
}

/* What the window list holds — declared here because the chord below brings
 * back exactly the rows that list marks as away, and one predicate is what
 * makes that sentence true. Defined with the list it draws. */
static int listable(const Win *w);

/*
 * EVERY WINDOW PUT AWAY ON THIS WORKSPACE, BACK IN ONE PRESS. The chord above
 * walks the stack one window per press, which is the right answer for "bring
 * back the one I just put away" and the wrong one for a desk cleared window by
 * window: the screen is blank while you walk it and nothing on it says how many
 * presses are left.
 *
 * THE SET IS `listable()`, MINIMISED — the window list's own rows, so what this
 * brings back is what a person counts on that list. A second rule spelling out
 * "put away, on this workspace, with a row of its own" would be a second thing
 * to keep in agreement with the list. It is also what holds the sweep to this
 * workspace: a restore moves a window to the workspace it is restored onto, so
 * one that took the other workspaces too would empty them onto this one, which
 * is a rearrangement nobody asked for and no chord undoes.
 *
 * ONE AT A TIME FROM THE BACK, RE-READING THE STACK, because each restore
 * raises and a raise reorders the very list this walks. The backmost first
 * leaves the one that went away last on top, which is where the single-window
 * chord would have left it. Nothing is collected into an array on the way: a
 * fixed one is a desk with more windows put away than it holds, restored to
 * that many and silent about the rest.
 *
 * IT ENDS BECAUSE EVERY CANDIDATE IS THE HEAD OF ITS FAMILY. win_restore()
 * climbs to the head and returns where the head is already back; an owned
 * window carries `no_task` and `listable()` drops it. So each pass clears one
 * window's `minimised` and the set is one shorter.
 */
void win_restore_all(void)
{
	for (;;) {
		Win *back = NULL;

		for (Win *w = S.wins; w; w = w->next)
			if (w->minimised && listable(w))
				back = w;
		if (!back)
			break;
		win_restore(back);
	}
}

/*
 * SWITCH TO A WORKSPACE, and it is the one place that does.
 *
 * The chord and the pager cell reach the same code: two paths that each set
 * the field themselves is two chances for one of them to forget the focus, and
 * a switch that leaves the focus on a window the person can no longer see
 * sends their next keystroke somewhere invisible.
 */
void win_workspace(int ws)
{
	if (ws < 0 || ws >= S.nworkspace || ws == S.workspace)
		return;
	S.workspace = ws;
	/* Nothing here is focused until the cycle picks the topmost window
	 * that is: the old focus belongs to a workspace nobody is looking at. */
	S.focus = 0;
	win_cycle(1);
	ktui_draw_invalidate();
}

/*
 * Send to another workspace and follow nothing: the window leaves and the view
 * stays where it is, which is what makes this the move rather than a switch.
 */
void win_send(Win *w, int ws)
{
	/* A WINDOW ON NO WORKSPACE CANNOT BE SENT TO ONE. The scratchpad is on
	 * all of them, so the chord would record a number nothing reads and
	 * drop the focus from a window still on the screen. */
	if (!w || w->panel || w->sticky || ws < 0 || ws >= S.nworkspace)
		return;
	/*
	 * THE FAMILY MOVES AS ONE, FROM ITS HEAD. A dialog sent on its own is
	 * a frame on a desk with no row to reach it by, and a modal sent on
	 * its own goes on blocking an owner the person can still see — so the
	 * window named is resolved up to the one it belongs to and that is
	 * what moves. Bounded for raise_owned()'s reason.
	 */
	int depth = 0;

	while (w->owner && depth++ < 4) {
		Win *own = win_find(w->owner);

		if (!own || own->panel || own->sticky)
			break;
		w = own;
	}
	w->workspace = ws;
	owned_apply(w->id, ws, -1, 0);

	Win *f = win_focused();

	/* The focus cannot stay on a window that has left the workspace being
	 * looked at — it may be a dialog that went with the window named. */
	if (f && !f->sticky && f->workspace != S.workspace)
		S.focus = 0;
}

/*
 * ── THE SCRATCHPAD ──────────────────────────────────────────────────────
 *
 * One window a session may keep over everything, shown and hidden by one
 * chord, on whatever workspace is being looked at.
 *
 * TWO FLAGS AND NOT A FOURTH WINDOW KIND. `sticky` and `hidden` are ordinary
 * fields any window may carry, so the scratchpad is a window that has been
 * MARKED rather than a window that was opened differently — which is what
 * lets the second chord hand the role to something already running, and what
 * keeps every other rule in this file working on it unchanged.
 *
 * HIDDEN IS NOT MINIMISED. A minimise leaves a taskbar row, because the row is
 * the way back; the scratchpad has no row while it is away and the chord is
 * the way back. Both states answer the show, so a scratchpad somebody
 * minimised by hand is not a window that needs two different keys.
 */
Win *win_scratch(void)
{
	Win *w = S.scratch ? win_find(S.scratch) : NULL;

	if (!w)
		S.scratch = 0;
	return w;
}

/*
 * THE DROP-DOWN SHAPE: the full width of the work area, the top half of its
 * height. Applied on every show rather than remembered, because the grid can
 * be resized while the scratchpad is away and a remembered rectangle would
 * bring it back partly off the screen — or, on a screen that had shrunk, not
 * onto it at all.
 */
static void scratch_shape(Win *w)
{
	KwmRect a = win_workarea();
	int h = a.h / 2;

	/* A work area too short to halve gives the whole of it: half of three
	 * rows is a window with no content row at all once the frame is
	 * taken. */
	if (h < 2 * CON_FRAME_Y + 1)
		h = a.h;
	/* A SHOW ALWAYS PRODUCES A SHAPE. win_place_at refuses a content
	 * rectangle under one cell, which on a work area three cells across
	 * would leave the scratchpad wherever it happened to be — visible, and
	 * in the one place the chord did not put it. */
	if (a.w < 2 * CON_FRAME_X + 1 || h < 2 * CON_FRAME_Y + 1) {
		w->geom = a;
		win_resized(w);
		return;
	}
	w->tiled = KWM_EDGE_NONE;
	w->full = 0;
	/* `geom` is the CONTENT and the frame stands outside it, so it is the
	 * FRAME that spans the width and the content is inset by the border's
	 * thickness on each axis. */
	win_place_at(w, a.x + CON_FRAME_X, a.y + CON_FRAME_Y,
		     a.w - 2 * CON_FRAME_X, h - 2 * CON_FRAME_Y);
	w->restore = w->geom;
}

void win_scratch_show(Win *w)
{
	if (!w)
		return;
	w->hidden = 0;
	w->minimised = 0;
	w->sticky = 1;
	scratch_shape(w);
	/* The focus is win_raise's to set, and it is not always this window: a
	 * show of a window a modal is standing over lands on the modal, which
	 * is the whole of what a modal means. */
	win_raise(w->id);
	ktui_draw_invalidate();
}

void win_scratch_hide(Win *w)
{
	if (!w)
		return;
	w->hidden = 1;
	/* The focus cannot stay on a window that is drawn nowhere: the next
	 * keystroke would go somewhere invisible. The cycle picks whatever is
	 * on the workspace being looked at, and nothing when it is empty.
	 *
	 * AND ONLY WHEN THIS WINDOW HELD IT. Putting the scratchpad away while
	 * typing in something else must leave the keyboard where it is. */
	if (S.focus == w->id) {
		S.focus = 0;
		win_cycle(1);
	}
	ktui_draw_invalidate();
}

void win_scratch_mark(Win *w)
{
	Win *old = win_scratch();

	/* Chrome is not something a person switches to, so it is not something
	 * they can hand this role to either. */
	if (!w || w->panel || w->overlay || w->background || w == S.lock ||
	    w == S.saver)
		return;
	/*
	 * NOR A TAB OF A STACK, and it is the same rule stackable() holds from
	 * the other side. The scratchpad's chord shows and hides its window
	 * through the very `hidden` flag a stack puts its other tabs away
	 * with, so a window both mechanisms own is one they disagree about:
	 * the scratchpad un-hides it while the stack believes another tab is
	 * up, and two tabs of one rectangle are on screen at once.
	 */
	if (win_stack_n(w) > 1)
		return;
	if (old && old != w) {
		/* BACK ONTO THE WORKSPACE BEING LOOKED AT, not the one it was
		 * on when it was marked: a window that had been sticky was on
		 * no workspace at all, and one handed back to a workspace
		 * nobody is looking at has been taken away rather than
		 * returned. */
		old->sticky = 0;
		old->hidden = 0;
		old->workspace = S.workspace;
	}
	S.scratch = w->id;
	w->sticky = 1;
	w->hidden = 0;
	ktui_draw_invalidate();
}

/*
 * ── THE STACK ──────────────────────────────────────────────────────────
 *
 * TWO WINDOWS IN ONE RECTANGLE, one of them on screen. The head is an ordinary
 * entry in S.wins holding the geometry and every other member is `hidden`,
 * which is the whole of the mechanism: a hidden window sleeps its guest, keeps
 * no taskbar row, is stepped past by the ring, claims no cells for a hit test
 * and carries no window-list row. Five behaviours, no new code for any of them.
 *
 * STACKING AND NOT TILING. A tile GROUP — two windows side by side that move
 * and size together — reuses none of this: `tiled` is a per-window bitmask
 * resolved against the WORK AREA rather than against a neighbour, and
 * win_tile_all() clears it afterwards precisely so that an arrangement is not
 * a state. There is nowhere for a group to live and nothing for it to be made
 * out of, so it is not built here.
 *
 * AND THE POINTER DOES NOT MAKE ONE. A title-bar drag is a translation with no
 * drop target and no hit test against another window, so a stack made by
 * dragging could be proved only by a rig photograph; the chords can be proved
 * by a golden, which is why the chords are what exist.
 */
#define STACK_MAX 32

/*
 * CAN THIS WINDOW BE A TAB.
 *
 * Chrome cannot: a panel, a layer, the lock and the saver are not things a
 * person switches between, so they are not things a person can fold into one
 * frame either — and a window with no taskbar row of its own is a dialog or a
 * dock, which belongs to the window that raised it rather than beside it.
 *
 * NOR THE SCRATCHPAD, and that one is load-bearing: its chord shows and hides
 * it through the same `hidden` flag the stack puts its members away with, so a
 * scratchpad folded into a stack would be a window two mechanisms disagree
 * about the visibility of.
 *
 * A GUEST ON A TERMINAL OF ITS OWN OWNS NO CELLS HERE, so a tab naming one
 * would promise a frame that is not on this screen.
 */
static int stackable(const Win *w)
{
	if (!w || w->panel || w->overlay || w->background || w->no_task)
		return 0;
	if (w == S.lock || w == S.saver || w->kind == WIN_VT)
		return 0;
	if (S.scratch && w->id == S.scratch)
		return 0;
	return 1;
}

/*
 * EVERY TAB OF THIS WINDOW'S STACK, IN ID ORDER — which is the strip's order.
 *
 * NOT S.wins' ORDER. That list is the z-order and win_stack_show() brings the
 * incoming member to the front of it, so a strip drawn by walking the list
 * would put the tabs in a different sequence after every switch. An id only
 * ever goes up, so id order is the one order a person can point at twice.
 *
 * Answers 0 for a window in no stack, which is what the frame tests before it
 * draws a strip at all.
 */
static int stack_set(const Win *w, Win **set, int max)
{
	int n = 0;

	if (!w || !w->stack)
		return 0;
	for (Win *o = S.wins; o && n < max; o = o->next) {
		int i;

		if (o->stack != w->stack)
			continue;
		for (i = n; i > 0 && set[i - 1]->id > o->id; i--)
			set[i] = set[i - 1];
		set[i] = o;
		n++;
	}
	return n;
}

int win_stack_n(const Win *w)
{
	Win *set[STACK_MAX];

	return stack_set(w, set, STACK_MAX);
}

int win_stack_index(const Win *w)
{
	Win *set[STACK_MAX];
	int n = stack_set(w, set, STACK_MAX);

	for (int i = 0; i < n; i++)
		if (set[i] == w)
			return i + 1;
	return 0;
}

/*
 * A MEMBER ONTO THE HEAD'S RECTANGLE AND THE HEAD'S STATE.
 *
 * win_place_at() IS NOT OPTIONAL EVEN FOR A MEMBER NOBODY CAN SEE: it routes
 * through win_resized(), which reflows a terminal and configures a guest, and
 * a member still configured to its old size draws the old size into the new
 * rectangle the moment it is brought up. The tile state travels with it for
 * the same reason `restore` does — a Super+arrow after a tab switch is
 * measured from what the stack is in, not from what the incoming tab was in
 * before it joined.
 */
static void stack_place_as(Win *m, const Win *hd)
{
	m->workspace = hd->workspace;
	m->sticky = hd->sticky;
	m->tiled = hd->tiled;
	m->full = hd->full;
	m->restore = hd->restore;
	win_place_at(m, hd->geom.x, hd->geom.y, hd->geom.w, hd->geom.h);
}

void win_stack_join(Win *a, Win *b)
{
	Win *hd;
	int old;

	if (!a || !b || a == b || !stackable(a) || !stackable(b))
		return;
	/* Already the same stack: there is nothing to join, and a second pass
	 * would re-point the members at a head that is not on screen. */
	if (a->stack && a->stack == b->stack)
		return;

	/* B KEEPS OR TAKES THE HEAD, because b is the window left on screen
	 * and the head is what names the stack. */
	hd = b->stack ? win_find(b->stack) : b;
	if (!hd)
		hd = b;
	hd->stack = hd->id;
	old = a->stack;

	/* AND WHAT `a` WAS ALREADY CARRYING COMES WITH IT. Joining a stack to
	 * a stack is one frame with every tab of both, not a tab whose own
	 * members are left pointing at a window that is now hidden — that is
	 * the same stranding win_drop() below exists to prevent. */
	for (Win *w = S.wins; w; w = w->next) {
		if (w == hd)
			continue;
		if (w != a && (!old || w->stack != old))
			continue;
		w->stack = hd->id;
		stack_place_as(w, hd);
		w->hidden = 1;
	}
	/* The head takes the keyboard: the window that has just been hidden
	 * cannot keep it, and it is the tab the chord left on screen. */
	win_raise(hd->id);
	ktui_draw_invalidate();
}

void win_stack_show(Win *m)
{
	Win *hd;
	int old, took, keep;

	if (!m || !m->stack)
		return;
	hd = win_find(m->stack);
	if (!hd || hd == m)
		return;

	old = hd->id;
	took = S.focus == old;
	keep = S.focus;

	hd->hidden = 1;
	m->hidden = 0;
	stack_place_as(m, hd);
	/* THE STACK IS NAMED BY WHICHEVER TAB IS ON SCREEN, so every member is
	 * re-pointed — the head's own entry included, which is what makes
	 * `m->stack == m->id` true of the new one. */
	for (Win *w = S.wins; w; w = w->next)
		if (w->stack == old)
			w->stack = m->id;

	/*
	 * THE INCOMING TAB COMES TO THE FRONT, AND TAKES THE KEYBOARD ONLY IF
	 * THE OUTGOING ONE HELD IT. A stack stepped while somebody is typing
	 * in another window must leave the keyboard where it is, and a focus
	 * left on the tab that went away would send the next keystroke to a
	 * window drawn nowhere. It is win_scratch_hide's rule with the answer
	 * that hide cannot give: there the window going away leaves nothing on
	 * screen to hand the focus to, so the ring is asked; here the tab
	 * coming up is exactly what the chord asked for.
	 */
	win_raise(m->id);
	if (!took)
		S.focus = keep;
	ktui_draw_invalidate();
}

void win_stack_step(Win *w, int dir)
{
	Win *set[STACK_MAX];
	int n, cur = -1;

	if (!w || !w->stack)
		return;
	n = stack_set(w, set, STACK_MAX);
	if (n < 2)
		return;
	for (int i = 0; i < n; i++)
		if (set[i]->id == w->stack)
			cur = i;
	if (cur < 0)
		return;
	win_stack_show(set[(cur + (dir < 0 ? n - 1 : 1)) % n]);
}

void win_stack_with_next(Win *w)
{
	int i, n = 0;
	Win *o;

	if (!w)
		return;
	if (!stackable(w)) {
		con_notice("this window cannot be a tab");
		return;
	}
	/* THE RING AND NOT THE LIST, because the ring is the order the title
	 * bars and the taskbar rows number windows in: "the next window" has
	 * to mean the same thing to the chord as it does to the number a
	 * person can see. */
	for (Win *x = S.wins; x; x = x->next)
		if (reachable(x))
			n++;
	i = win_index(w);
	if (i < 1 || n < 2) {
		con_notice("no other window to stack this one with");
		return;
	}
	/* PAST THE ONES THAT CANNOT BE A TAB. The scratchpad and a guest on a
	 * terminal of its own are in the ring and are not stackable, so a
	 * chord that took the very next entry would be a chord that did
	 * nothing with two windows open and the scratchpad between them. */
	for (int k = 1; k < n; k++) {
		o = win_nth((i + k - 1) % n + 1);
		if (o && o != w && stackable(o)) {
			win_stack_join(o, w);
			return;
		}
	}
	con_notice("no other window to stack this one with");
}

void win_stack_unstack(Win *w)
{
	Win *set[STACK_MAX];
	int n = stack_set(w, set, STACK_MAX);
	Win *hd = n ? win_find(w->stack) : NULL;

	if (n < 2)
		return;
	for (int i = 0; i < n; i++) {
		Win *m = set[i];

		m->stack = 0;
		/* The tab on screen is already where the person put it. */
		if (m == hd)
			continue;
		/*
		 * AND IT COMES BACK ONTO THE DESK THE STACK IS ON. A hidden
		 * tab keeps whatever workspace it had when it was folded in,
		 * and win_send() moves the OWNER family and not the stack — so
		 * a stack sent to another workspace leaves its tabs behind,
		 * and unstacking there would put a window on a desk nobody is
		 * looking at with nothing on screen to say where it went.
		 */
		if (hd) {
			m->workspace = hd->workspace;
			m->sticky = hd->sticky;
		}
		/*
		 * ONE AT A TIME, UN-HIDDEN BEFORE IT IS PLACED. win_place()
		 * excludes the windows already on the desk and geo_recall()
		 * declines an origin one of them is sitting on, so a member
		 * placed after its siblings lands somewhere they are not —
		 * un-hiding the lot first would show the search none of them
		 * and pile every tab onto one rectangle.
		 */
		m->hidden = 0;
		win_place(m, m->geom.w, m->geom.h);
	}
	ktui_draw_invalidate();
}

/*
 * WHETHER THIS POINT IS ONE OF THE CELLS THE SURFACE SAID IT ANSWERS.
 *
 * A surface declares an input region — `kdisp_input_cells()` — and four of
 * them declare an EMPTY one: the tooltip, the toast stack, the candidate
 * window and the screen saver are all drawn over the desktop and all take
 * nothing, because the thing under them is what a click is aimed at. A hit
 * test that ignored it gave the topmost rectangle every click, so a tooltip
 * that opened over the Start button swallowed the click on it and the menu
 * never opened.
 *
 * Everything that is not a libkcon surface — a terminal, a guest, the session's
 * own chrome — answers everywhere, which is also what a surface that never
 * called it reports.
 */
static int win_takes_point(const Win *w, int x, int y)
{
	int n;

	if (w->kind != WIN_SURFACE || !w->surf)
		return 1;
	n = kcon_surface_input_n(w->surf);
	if (n < 0)
		return 1;
	for (int i = 0; i < n; i++) {
		KRect r;

		if (!kcon_surface_input_at(w->surf, i, &r))
			continue;
		if (x >= w->geom.x + r.x && x < w->geom.x + r.x + r.w &&
		    y >= w->geom.y + r.y && y < w->geom.y + r.y + r.h)
			return 1;
	}
	return 0;
}

Win *win_at(int x, int y)
{
	for (Win *w = S.wins; w; w = w->next) {
		/* A guest owns no cells: nothing on this grid is it, so a
		 * pointer never lands on one. */
		if (w->kind == WIN_VT)
			continue;
		/* A saver claims no cells either, however many it covers: a
		 * click goes through to what is underneath, which is what lets
		 * the idle policy see the activity that takes it away. */
		if (w == S.saver)
			continue;
		/*
		 * A BACKGROUND CLAIMS NOTHING EITHER. It is the desktop's icon
		 * layer and it covers the whole grid, so hit-testing it before
		 * the windows would take every click on the desktop.
		 *
		 * An overlay is NOT excluded: a menu is there to be clicked,
		 * and it is first in the list because it is drawn last.
		 */
		if (w->background)
			continue;
		/* Nor on a surface that says it has nothing to show: it is
		 * drawn nowhere, and a rectangle that swallows clicks without
		 * drawing anything is worse than one that does. */
		if (w->kind == WIN_SURFACE && w->surf &&
		    kcon_surface_hidden(w->surf))
			continue;
		/* A hidden window claims no cells either — it is drawn nowhere,
		 * and a rectangle that swallowed clicks without drawing would
		 * be a hole in the desktop where the scratchpad last was. */
		if (w->minimised || w->hidden)
			continue;
		if (!w->sticky && w->workspace != S.workspace)
			continue;

		KwmRect f = win_frame(w);

		if (x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h &&
		    win_takes_point(w, x, y))
			return w;
	}
	return NULL;
}

/*
 * HOW FAR ALONG A SIDE THIS LONG IS STILL ITS CORNER. `thick` is the border's
 * thickness ACROSS this arm — CON_FRAME_X for an arm running along the top or
 * bottom row, CON_FRAME_Y for one running down a side column.
 *
 * Clamped to a third of the side as well as to CON_GRAB_CORNER: a frame twelve
 * cells wide with four cells of corner at each end has four cells of side left
 * to drag, and one eight cells wide would have none at all — every press on it
 * would take both axes and the window could not be resized in one.
 *
 * AND NEVER SHORTER THAN THE BAND IT CROSSES. The corner is the block where
 * the two bands meet, so an arm shorter than `thick` leaves cells inside that
 * block taking a single axis: on a two-column border, a press one cell in from
 * the corner that changes the width and not the height. The floor outranks the
 * third, because a frame too small to hold both arms is one whose title row is
 * a handful of cells — and Super+left drag moves a window from anywhere inside
 * it.
 */
static int corner_arm(int side, int thick)
{
	int a = side / 3;

	if (a > CON_GRAB_CORNER)
		a = CON_GRAB_CORNER;
	if (a < thick)
		a = thick;
	return a < 1 ? 1 : a;
}

/*
 * HOW FAR FROM EACH END OF A SIDE A PRESS TAKES BOTH AXES, for the frame `w`
 * is drawn at.
 *
 * ASKED RATHER THAN RECOMPUTED. `win_grab_at` decides what a press arms from
 * these two numbers, and the grip lights what a press would arm — so a second
 * copy of the arithmetic is a light that promises a grab the router will not
 * give. Exported for that one caller.
 */
void win_grab_arms(const Win *w, int *ax, int *ay)
{
	KwmRect f = win_frame((Win *)w);

	*ax = corner_arm(f.w, CON_FRAME_X);
	*ay = corner_arm(f.h, CON_FRAME_Y);
}

/*
 * THE TITLE ROW, END TO END, CHIPS INCLUDED.
 *
 * WHAT THE RIGHT AND THE MIDDLE BUTTON ARE ANSWERED FROM, and it is the whole
 * band rather than the span between the chips: a row where two of the three
 * buttons meant one thing over the name and nothing over `↓ ■ X` is a row a
 * person has to aim at to find out what it does. The chips answer the LEFT
 * button and only that one, so nothing is taken from them.
 *
 * A WINDOW WITH NO FRAME HAS NO TITLE ROW — win_grab_at()'s list and for its
 * reason: the frame rectangle of a panel, a fullscreen window or a layer is
 * still the content inflated by the border, so a test that did not refuse them
 * would find a title row on a popup menu.
 */
int win_on_title(const Win *w, int x, int y)
{
	KwmRect f;

	if (!w || !win_framed(w))
		return 0;
	f = win_frame((Win *)w);
	return x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + CON_FRAME_Y;
}

/*
 * WHICH EDGES A RESIZE FROM INSIDE THE WINDOW TAKES: the nearest in each axis,
 * so a press near a corner takes both and one in the middle of a side takes
 * that side alone. A press in the exact middle takes the bottom-right corner,
 * which is what a hand expects when nothing else is nearer.
 */
static unsigned near_edges(const Win *w, int x, int y)
{
	unsigned e = KWM_EDGE_NONE;
	int third_w = w->geom.w / 3, third_h = w->geom.h / 3;

	if (third_w < 1)
		third_w = 1;
	if (third_h < 1)
		third_h = 1;

	if (x < w->geom.x + third_w)
		e |= KWM_EDGE_LEFT;
	else if (x >= w->geom.x + w->geom.w - third_w)
		e |= KWM_EDGE_RIGHT;
	if (y < w->geom.y + third_h)
		e |= KWM_EDGE_TOP;
	else if (y >= w->geom.y + w->geom.h - third_h)
		e |= KWM_EDGE_BOTTOM;

	return e ? e : (KWM_EDGE_RIGHT | KWM_EDGE_BOTTOM);
}

/*
 * WHAT A PRESS ON THIS WINDOW ARMS, and which edges a resize is to move.
 *
 * EITHER BUTTON RESIZES FROM THE SIDE AND BOTTOM BORDERS. The right button is
 * not the one a hand reaches for on a border, and a left drag along a frame's
 * own rule that did nothing reads as a window that cannot be resized at all.
 * Inside the border there is nothing else for a left press to mean: every cell
 * of it belongs to the window manager, not to whatever is in the window.
 *
 * THE WHOLE BAND ANSWERS, NOT ITS OUTERMOST CELL. The border is CON_FRAME_X
 * columns and CON_FRAME_Y rows thick and a press anywhere in it is a resize
 * from that side — a thickness a hand can find that only the outer cell acted
 * on would be a wider picture of the same unhittable target.
 *
 * THE ENDS OF EVERY SIDE ARE A CORNER and take both axes, for CON_GRAB_CORNER
 * cells. The block where the two bands actually cross is two cells, and a
 * corner nobody can land on means every resize is one axis at a time — so the
 * arms down the side columns are also where the vertical tolerance lives, the
 * top and bottom bands being one row each.
 *
 * THE TITLE ROW IS THE LEFT BUTTON'S ALONE. Left on the row moves the window
 * and left on the corner arms at either end resizes from that corner, which is
 * what makes the top two corners reachable at all on a row that is otherwise
 * the one handle the window has. THE OTHER TWO BUTTONS ARM NOTHING THERE: the
 * right one opens the window menu and the middle one lowers the window, and a
 * button that also armed a drag would start one on the way to the menu. It is
 * the grip that makes that a rule rather than a preference — it is lit by
 * asking this function with the left button standing in for the press that has
 * not happened, so a right press that resized from the top edge would be a
 * resize the window never said was there.
 *
 * SUPER IS THE WAY IN FROM ANYWHERE: left moves and middle resizes, so a
 * window that is all content is movable without hunting for its one draggable
 * row.
 *
 * AND A BARE RIGHT PRESS INSIDE BELONGS TO WHATEVER OWNS THE CELLS. Resizing
 * from anywhere inside is right for a window whose content is the session's to
 * interpret and wrong for one that is a program's: a right click inside a
 * terminal or an embedded application that armed a resize is a context menu
 * unreachable in every graphical application on this desktop.
 */
int win_grab_at(const Win *w, int x, int y, int btn, int mods, unsigned *edges)
{
	KwmRect f;
	unsigned e = KWM_EDGE_NONE;
	int l, r, t, b, ax, ay;
	int super = (mods & KT_MOD_SUPER) != 0;

	*edges = KWM_EDGE_NONE;
	/*
	 * A WINDOW WITH NO FRAME HAS NO FRAME TO TAKE HOLD OF, and the list is
	 * `win_draw_all`'s own: a panel is docked and dragging it would move
	 * the work area out from under every other window; a fullscreen window
	 * has no border; a layer — a menu, a toast — and the icon layer are
	 * part of the desktop rather than things sitting on it. Their frame
	 * rectangle is still inflated by the border, so a grab that did not
	 * refuse them would resize a popup menu from a border nobody can see.
	 */
	if (!w || !win_framed(w))
		return WIN_GRAB_NONE;
	/*
	 * A DETENT IS NOT A DRAG. A wheel tick is delivered as a press with no
	 * release to match it, so a grab armed by one would own the pointer
	 * until some later click let go of it — every motion after a scroll
	 * over a border would resize the window that was scrolled over.
	 */
	if (btn != KT_MB_LEFT && btn != KT_MB_MIDDLE && btn != KT_MB_RIGHT)
		return WIN_GRAB_NONE;

	f = win_frame(w);
	if (x < f.x || x >= f.x + f.w || y < f.y || y >= f.y + f.h)
		return WIN_GRAB_NONE;

	/*
	 * A BAND AND NOT A CELL. The border is CON_FRAME_X columns across and
	 * CON_FRAME_Y rows down, and every cell of it is the window manager's;
	 * a test that named only the outermost one would leave the rest of a
	 * thick border doing nothing, which is the same window a hand cannot
	 * get hold of with a thicker rule drawn round it.
	 *
	 * THE TWO CANNOT OVERLAP. win_place_at refuses a content rectangle
	 * under one cell, so the narrowest frame is 2 * CON_FRAME_X + 1 across
	 * and 2 * CON_FRAME_Y + 1 down, and a cell is never both sides at once.
	 */
	l = x < f.x + CON_FRAME_X;
	r = x >= f.x + f.w - CON_FRAME_X;
	t = y < f.y + CON_FRAME_Y;
	b = y >= f.y + f.h - CON_FRAME_Y;
	/*
	 * AN ARM IS MEASURED ALONG ITS OWN SIDE AND FLOORED BY THE OTHER BAND'S
	 * THICKNESS: the run along the top and bottom rows is as long as the
	 * side columns are wide, and the run down the side columns is at least
	 * as deep as the top and bottom rows are tall. Anything less is a
	 * corner block with a cell in it that takes one axis.
	 */
	ax = corner_arm(f.w, CON_FRAME_X);
	ay = corner_arm(f.h, CON_FRAME_Y);

	if (l || r) {
		e |= l ? KWM_EDGE_LEFT : KWM_EDGE_RIGHT;
		if (y < f.y + ay)
			e |= KWM_EDGE_TOP;
		else if (y >= f.y + f.h - ay)
			e |= KWM_EDGE_BOTTOM;
	}
	if (t || b) {
		e |= t ? KWM_EDGE_TOP : KWM_EDGE_BOTTOM;
		if (x < f.x + ax)
			e |= KWM_EDGE_LEFT;
		else if (x >= f.x + f.w - ax)
			e |= KWM_EDGE_RIGHT;
	}

	if (super) {
		if (btn == KT_MB_LEFT)
			return WIN_GRAB_MOVE;
		*edges = e ? e : near_edges(w, x, y);
		return WIN_GRAB_RESIZE;
	}

	if (e) {
		/* The title row, its corner arms included: see above. Nothing
		 * but the left button takes hold of it. */
		if (t && !b) {
			if (btn != KT_MB_LEFT)
				return WIN_GRAB_NONE;
			if (!(e & (KWM_EDGE_LEFT | KWM_EDGE_RIGHT)))
				return WIN_GRAB_MOVE;
		}
		*edges = e;
		return WIN_GRAB_RESIZE;
	}

	if (btn == KT_MB_RIGHT && w->kind != WIN_TERM && w->kind != WIN_EMBED) {
		*edges = near_edges(w, x, y);
		return WIN_GRAB_RESIZE;
	}
	return WIN_GRAB_NONE;
}

/*
 * A WINDOW ON THE DESK IN FRONT OF THE PERSON. What an arrangement moves and
 * what show-desktop puts away, and the first half of what the focus can reach.
 *
 * A saver is never in it: one that could be given the focus is one that can be
 * left on screen with a window behind it taking the keyboard. Nor is a layer —
 * a toast, a menu and the icon layer are not things a person has open — and
 * nor is a panel, which is docked rather than placed.
 */
static int on_this_desk(const Win *w)
{
	if (!w || w->minimised || w->hidden)
		return 0;
	/* A STICKY WINDOW IS ON NO WORKSPACE AND SO ON EVERY ONE. Asking which
	 * workspace it is on is the one question that must not be put to it:
	 * the scratchpad is reached from whichever one a person is looking at,
	 * and a ring that skipped it there would leave the key as the only way
	 * to the window it just put on the screen. */
	if (!w->sticky && w->workspace != S.workspace)
		return 0;
	if (w == S.saver || w->overlay || w->background || w->panel)
		return 0;
	return 1;
}

/*
 * A WINDOW A PERSON CAN MOVE THE FOCUS TO. The ring, the number chord, the
 * directional search and the swap all mean the same set, and four copies of
 * the rule is four chances for one of them to stop on a tooltip.
 *
 * THE WINDOW LIST IS THE ONE CALLER THAT MEANS A WIDER SET, because it is a
 * taskbar rather than a step — `listable()` holds the minimised windows this
 * rule drops, and a row for one is the way back to it.
 *
 * IT IS ONE RULE NARROWER THAN THE DESK. A window a modal is standing over is
 * not one a raise can land on: the raise is redirected to the question, so a
 * ring entry for it is one the ring never advances past — every step lands on
 * the modal again and the window after it is never reached. An ARRANGEMENT
 * still moves it, because a window left out of a tile because somebody has a
 * save dialog open is a window sitting across the grid everything else was
 * folded into.
 */
static int reachable(const Win *w)
{
	if (!on_this_desk(w))
		return 0;
	if (win_modal_for(w->id))
		return 0;
	return 1;
}

/* Alt-Tab, over the windows on this workspace, through libkwm's ring. */
void win_cycle(int dir)
{
	int ids[64];
	int n = 0, cur = -1;

	for (Win *w = S.wins; w && n < 64; w = w->next) {
		if (!reachable(w))
			continue;
		if (w->id == S.focus)
			cur = n;
		ids[n++] = w->id;
	}

	if (n < 1)
		return;
	/*
	 * AN EMPTY FOCUS LANDS ON THE FRONT WINDOW RATHER THAN STEPPING FROM
	 * IT. This is also the call a minimise, a workspace switch and a
	 * scratchpad hide hand the keyboard on with, so it has to answer with
	 * nothing focused — and a step taken from a ring position that does
	 * not exist hands the keyboard to the window BEHIND the one the person
	 * is looking straight at. A focus on something the ring does not reach
	 * is the same case: the front window is where a fresh one starts.
	 */
	if (cur < 0) {
		win_raise(ids[0]);
		return;
	}
	/* Alt-Tab on a single window is a no-op, and the ring has nowhere to
	 * step to. */
	if (n < 2)
		return;

	int next = kwm_ring_next(n, cur, dir);

	if (next >= 0)
		win_raise(ids[next]);
}

/*
 * ── THE RING, BY NUMBER ─────────────────────────────────────────────────
 *
 * Every window on this workspace carries its position in the Alt-Tab ring,
 * drawn in its title bar and in its taskbar row, and `Super+Alt+N` raises the
 * one that number names. The index is the RING'S rather than an id of its own:
 * it is the order cycling already walks, it renumbers when a window closes,
 * and a second numbering would be a second thing to keep in agreement.
 *
 * The text desks this comes from — Turbo Vision's Alt+N, DESQview's Open
 * Window codes — all numbered windows, and for the same reason: on a keyboard
 * a number is the shortest way to say which one.
 */
Win *win_nth(int n)
{
	int i = 0;

	if (n < 1)
		return NULL;
	for (Win *w = S.wins; w; w = w->next) {
		if (!reachable(w))
			continue;
		if (++i == n)
			return w;
	}
	return NULL;
}

int win_index(const Win *w)
{
	int i = 0;

	if (!w)
		return 0;
	for (Win *o = S.wins; o; o = o->next) {
		if (!reachable(o))
			continue;
		i++;
		if (o == w)
			return i;
	}
	return 0;
}

/*
 * ── ARRANGEMENTS ────────────────────────────────────────────────────────
 *
 * Tile fills the work area with a grid of ceil(sqrt(n)) columns, the last row
 * absorbing whatever does not divide; cascade offsets each window by one title
 * row and two columns from the last, at two thirds of the work area. Both are
 * what the Window menu of every text desk of the era did, and both are what a
 * person with five terminals open actually wants.
 *
 * NEITHER IS A TILE STATE. `tiled` is cleared, so a Super+arrow afterwards
 * snaps from the rectangle the arrangement made rather than from a half the
 * window is not in — an arrangement is where the windows are put, and a snap
 * state is a claim about which edges they hold.
 *
 * A FULLSCREEN WINDOW IS LEFT ALONE by both, and so is the scratchpad. Each
 * was put there on purpose and holds a shape of its own; folding either into a
 * grid of five would be undoing a request nobody withdrew, and the drop-down
 * shape in particular is the whole of what its chord promises.
 */
static int arrange_set(Win **out, int max)
{
	int n = 0;

	for (Win *w = S.wins; w && n < max; w = w->next)
		if (on_this_desk(w) && !w->full && !w->sticky)
			out[n++] = w;
	return n;
}

void win_tile_all(void)
{
	Win *set[32];
	int n = arrange_set(set, 32);

	if (n < 1)
		return;

	KwmRect a = win_workarea();
	int cols = 1;

	while (cols * cols < n)
		cols++;

	int rows = (n + cols - 1) / cols;
	int cw = a.w / cols, ch = a.h / rows;

	if (cw < 2 * CON_FRAME_X + 5 || ch < 2 * CON_FRAME_Y + 4)
		return;		/* a grid nothing could be read in */

	for (int i = 0; i < n; i++) {
		int r = i / cols, c = i % cols;
		int wide = (r == rows - 1) ? n - r * cols : cols;
		/* The last row spreads across the width rather than leaving a
		 * gap where the grid ran out: a lone window sitting in the
		 * left third under four others reads as a mistake. */
		int lw = a.w / wide;
		KwmRect g;

		/*
		 * ONE CELL SHORT ON EACH FAR EDGE, FOR THE SHADOW. A window's
		 * drop shadow is a column to its right and a row below it, so
		 * tiles that touched put every window's shadow across its
		 * neighbour's TITLE BAR — a grid whose titles cannot be read,
		 * which is most of what a tiled grid is for.
		 */
		g.x = (r == rows - 1 ? a.x + c * lw : a.x + c * cw) +
		      CON_FRAME_X;
		g.y = a.y + r * ch + CON_FRAME_Y;
		g.w = (r == rows - 1 ? lw : cw) - 2 * CON_FRAME_X - 1;
		g.h = ch - 2 * CON_FRAME_Y - 1;

		set[i]->tiled = 0;
		set[i]->restore = set[i]->geom;
		set[i]->geom = kwm_fit(g, a, set[i]->min_w, set[i]->min_h);
		win_resized(set[i]);
	}
}

void win_cascade(void)
{
	Win *set[32];
	int n = arrange_set(set, 32);

	if (n < 1)
		return;

	KwmRect a = win_workarea();
	int cw = a.w * 2 / 3, ch = a.h * 2 / 3;

	if (cw < 8 || ch < 5)
		return;

	for (int i = 0; i < n; i++) {
		KwmRect g;
		/* The staircase wraps rather than walking off the bottom
		 * right: with more windows than steps the later ones start
		 * again from the top left, which is what every desk that had
		 * this did once the screen ran out. */
		int steps = (a.h - ch) / 2;
		int k = steps > 0 ? i % steps : 0;

		g.x = a.x + CON_FRAME_X + k * 2;
		g.y = a.y + CON_FRAME_Y + k;
		g.w = cw;
		g.h = ch;
		set[i]->tiled = 0;
		set[i]->restore = set[i]->geom;
		set[i]->geom = kwm_fit(g, a, set[i]->min_w, set[i]->min_h);
		win_resized(set[i]);
		win_raise(set[i]->id);
	}
}

/*
 * ── SHOW THE DESKTOP ────────────────────────────────────────────────────
 *
 * Norton Commander's Ctrl+O hid both panels to show the shell behind them;
 * this hides every window to show the icons. The SET is remembered rather than
 * "unminimise everything": a window that was already minimised before the
 * chord was pressed was minimised on purpose, and bringing it back would be
 * the desktop undoing something a person asked for.
 *
 * WHETHER THE DESKTOP IS SHOWING IS ASKED OF THE WINDOWS, NOT LATCHED. Every
 * other way back — the restore chords, a window list row, a taskbar row —
 * empties the remembered set without this function running, so a flag saying
 * "showing" would survive a desk that is full again and spend the next press
 * restoring nothing. The set is cleared by the pass that walks it either way,
 * so a chord that found nothing to bring back goes on to hide.
 */
void win_show_desktop(void)
{
	static int hidden[64];
	static int nhidden;
	int back = 0;

	for (int i = 0; i < nhidden; i++) {
		Win *w = win_find(hidden[i]);

		if (w && w->minimised) {
			w->minimised = 0;
			win_raise(w->id);
			back++;
		}
	}
	nhidden = 0;
	if (back)
		return;

	for (Win *w = S.wins; w && nhidden < 64; w = w->next) {
		/* EVERY WINDOW ON THE DESK, not only the ones the ring
		 * reaches: one a modal is standing over is a window the person
		 * can see, and a show-desktop that left it there would clear
		 * the desk around it. */
		if (!on_this_desk(w))
			continue;
		hidden[nhidden++] = w->id;
		w->minimised = 1;
	}
	if (nhidden > 0) {
		S.focus = 0;
		ktui_draw_invalidate();
	}
}

/*
 * THE NEAREST WINDOW IN ONE DIRECTION, or NULL.
 *
 * A candidate has to START past where the focused window starts — its own
 * leading edge, not its trailing one, so a window that merely overlaps a
 * little is still to the right of one it overlaps — and it has to SHARE ROWS
 * (or columns) with it. Without the overlap test, Super+Alt+Right on a
 * two-column layout lands on whatever happens to be furthest down the other
 * column, which is not what the arrow was pointing at. Nothing overlapping
 * means the focus does not move; Alt-Tab is the way to a window this search
 * cannot see, and a focus that jumped somewhere unrelated would be worse than
 * one that stayed.
 *
 * `kwm_edge_best` picks between two candidates: the smallest leading edge
 * going right or down, the largest going left or up.
 */
Win *win_dir(unsigned dir)
{
	Win *cur = win_focused(), *best = NULL;
	int decreasing = dir == KWM_EDGE_LEFT || dir == KWM_EDGE_TOP;
	int horiz = dir == KWM_EDGE_LEFT || dir == KWM_EDGE_RIGHT;
	int edge = decreasing ? INT_MIN : INT_MAX;

	if (!cur || !reachable(cur))
		return NULL;

	for (Win *w = S.wins; w; w = w->next) {
		if (w == cur || !reachable(w))
			continue;

		int lead = horiz ? w->geom.x : w->geom.y;
		int from = horiz ? cur->geom.x : cur->geom.y;

		if (decreasing ? lead >= from : lead <= from)
			continue;

		/* The spans across the direction of travel, which must meet. */
		int a0 = horiz ? cur->geom.y : cur->geom.x;
		int a1 = a0 + (horiz ? cur->geom.h : cur->geom.w);
		int b0 = horiz ? w->geom.y : w->geom.x;
		int b1 = b0 + (horiz ? w->geom.h : w->geom.w);

		if (b1 <= a0 || b0 >= a1)
			continue;

		int pick = kwm_edge_best(edge, lead, decreasing);

		if (pick != edge || !best) {
			edge = pick;
			best = w;
		}
	}
	return best;
}

/*
 * TWO WINDOWS TRADE RECTANGLES, and the focus follows the window rather than
 * the place. A swap that left the focus where it was would move the window out
 * from under the person's own keystrokes.
 *
 * A maximised or fullscreen window is fitted to the grid rather than placed,
 * so trading its rectangle would give the other window a size nothing asked
 * for; both flags travel with it.
 *
 * SO DOES `restore`, WHICH IS PART OF THE SAME STATE. It is where an untile
 * puts a window back, and a tile flag that moved without it would send the
 * receiving window to a rectangle it was never at — a maximised window
 * swapped, then unmaximised, landing on the other one's old place.
 */
void win_swap(Win *a, Win *b)
{
	if (!a || !b || a == b)
		return;

	KwmRect g = a->geom, rest = a->restore;
	int full = a->full, tiled = a->tiled;

	a->geom = b->geom;
	a->restore = b->restore;
	a->full = b->full;
	a->tiled = b->tiled;
	b->geom = g;
	b->restore = rest;
	b->full = full;
	b->tiled = tiled;
	win_resized(a);
	win_resized(b);
	ktui_draw_invalidate();
}

/*
 * THE NEXT WORKSPACE THAT HAS SOMETHING ON IT, wrapping once.
 *
 * Occupancy is counted here rather than in libkwm because "there is something
 * here" is this desktop's own rule: a minimised window still holds its
 * workspace — it has a taskbar row and comes back to where it was — and a
 * panel, a toast and the icon layer are not somebody's work.
 *
 * Stepping past an empty workspace is the point. With nine of them and two in
 * use, an arrow that stopped on every empty one in between would be an arrow
 * nobody presses twice.
 */
void win_workspace_step(int reverse)
{
	unsigned char occupied[9] = { 0 };
	int n = S.nworkspace;

	if (n > (int)sizeof(occupied))
		n = (int)sizeof(occupied);

	for (Win *w = S.wins; w; w = w->next) {
		if (w->panel || w->overlay || w->background || w == S.saver)
			continue;
		/* A sticky window occupies no workspace, so it makes none of
		 * them a stop: an arrow that landed on an empty workspace
		 * because the scratchpad was open would step nowhere useful. */
		if (w->sticky || w->hidden)
			continue;
		if (w->workspace >= 0 && w->workspace < n)
			occupied[w->workspace] = 1;
	}

	int ws = kwm_ws_adjacent(occupied, n, S.workspace, reverse, 1);

	if (ws >= 0)
		win_workspace(ws);
}

/*
 * THE REMOVAL ITSELF, asking nobody. win_close() below asks; this is what
 * happens once the answer has arrived, or once there is nobody left to ask.
 *
 * THE LOCK AND THE SAVER ARE CLEARED HERE AND NOT WHERE THE ASKING IS. A
 * request to close is not a departure: a lock program that has been asked to go
 * is still drawing the lock until it does, and a session that dropped the
 * pointer at the request would composite the desktop underneath it in the
 * meantime. Only `S.locked` outlives the window, and only an explicit dismissal
 * from the lock client clears that.
 */
void win_drop(Win *w)
{
	if (!w)
		return;

	/*
	 * THE RECTANGLE IS KEPT FIRST, BEFORE THE ROLES BELOW ARE CLEARED.
	 *
	 * A window leaves through six paths — a chord, its own client, a
	 * reaped guest, the garbage collector, the end of the session — and
	 * this is the one they all funnel into, which is why the record is
	 * written here and not in win_close(): for three of the four window
	 * kinds that only ASKS the client to go, and a record written there
	 * would be written again when the window actually left.
	 *
	 * THE ORDER IS THE WHOLE OF IT. What may be remembered is decided by
	 * role, and the lock and the saver are named by S.lock and S.saver —
	 * so clearing those first would offer a lock screen's rectangle to the
	 * next window of the same program.
	 */
	geo_record(w);

	if (S.lock == w)
		S.lock = NULL;
	if (S.saver == w)
		S.saver = NULL;
	/* The scratchpad went with it. Cleared HERE rather than where the
	 * chord looks, because a window is dropped from six places and only
	 * one of them is a person asking for it. */
	if (S.scratch == w->id)
		S.scratch = 0;

	embed_free(w);
	term_free(w);

	Win **pp = &S.wins;

	while (*pp && *pp != w)
		pp = &(*pp)->next;
	if (*pp)
		*pp = w->next;

	/*
	 * AND WHAT BELONGED TO IT KEEPS ONE ROW BETWEEN THEM. An application
	 * that outlives the window a person opened is still one application,
	 * so the orphan nearest the front takes the owner's place and the rest
	 * are re-parented onto it: giving every dock and dialog its own row
	 * would turn one entry into six the moment an image window closed,
	 * which is the bar this whole rule exists to stop growing. An owner id
	 * naming nothing is what must not be left behind — that is a window
	 * listed nowhere and blocked by nothing, which a person can see and
	 * cannot reach.
	 *
	 * MODALITY DOES NOT SURVIVE: there is no window left for it to block.
	 * A splash never becomes the heir — it is drawn over everything, is in
	 * no ring and has no frame to close it by.
	 */
	Win *heir = NULL;

	for (Win *o = S.wins; o; o = o->next) {
		if (o->owner != w->id)
			continue;
		o->modal = 0;
		if (!heir && !o->overlay) {
			heir = o;
			o->owner = 0;
			o->no_task = 0;
		} else {
			o->owner = heir ? heir->id : 0;
		}
	}

	/*
	 * AND A STACK THAT LOSES THE TAB ON SCREEN LOSES EVERY OTHER TAB WITH
	 * IT. The members are `hidden`: no taskbar row, no ring step, no hit
	 * rectangle and no window-list row, so a member still pointing at a
	 * head that has gone is a window a person can reach by nothing at all.
	 * The frontmost survivor takes the head's rectangle and its place at
	 * the head, and the rest re-point at it — the owner loop above,
	 * applied to the other relation.
	 *
	 * A STACK OF ONE IS NOT A STACK, whichever member left: the survivor's
	 * field is cleared, so the frame stops drawing a strip and the window
	 * is an ordinary one again.
	 */
	if (w->stack) {
		int old = w->stack;
		Win *first = NULL;
		int n = 0;

		for (Win *o = S.wins; o; o = o->next)
			if (o->stack == old) {
				if (!first)
					first = o;
				n++;
			}
		if (first && old == w->id) {
			first->hidden = 0;
			stack_place_as(first, w);
			for (Win *o = S.wins; o; o = o->next)
				if (o->stack == old)
					o->stack = n > 1 ? first->id : 0;
		} else if (first && n < 2) {
			first->stack = 0;
		}
	}

	if (S.focus == w->id)
		S.focus = S.wins ? S.wins->id : 0;
	free(w);
}

void win_close(Win *w)
{
	if (!w)
		return;

	/*
	 * A WINDOW WITH A MODAL QUESTION OVER IT CANNOT BE CLOSED, and the
	 * flash is what says so. The application has stopped answering about
	 * that window until the dialog is answered, so a close routed to it is
	 * a request nothing ever replies to and a frame button that does
	 * nothing at all — and for a terminal or a surface it would be worse
	 * than nothing, because those close immediately and would take the
	 * question with them.
	 */
	Win *m = win_modal_for(w->id);

	if (m) {
		win_raise(w->id);
		return;
	}

	if (w->kind == WIN_TERM && w->term) {
		kvt_term_close(w->term);
		w->term = NULL;
	} else if (w->kind == WIN_VT && w->vt_pid > 0) {
		/*
		 * ASKED TO GO, NOT REMOVED. The entry stays in the list until
		 * the compositor is actually gone, because a taskbar that
		 * dropped an application the moment somebody clicked close is
		 * one that lies about a program still saving a file. vt_reap
		 * takes it out.
		 */
		vt_close(w);
		return;
	} else if (w->kind == WIN_EMBED && embed_alive(w)) {
		/*
		 * ASKED TO GO, NOT REMOVED, AND THE ASK NAMES THIS TOPLEVEL
		 * ALONE. The guest decides what a close means — a save prompt
		 * is a window being used — so the entry stays until the cage
		 * says the toplevel actually unmapped. The process behind it is
		 * untouched: an application whose last window closed is an
		 * application with no window, which is what the shared session
		 * bus is for.
		 */
		embed_close(w);
		return;
	} else if (w->kind == WIN_SURFACE && w->surf) {
		/*
		 * THE SAME RULE AGAIN, and here it is load-bearing rather than
		 * merely honest. A surface stays in the server's list until its
		 * client actually disconnects, so an entry removed at the
		 * request is an entry adopt_surfaces() builds again on the very
		 * next pass — as a fresh window, and for a role the session
		 * configures on adopt as a fresh LOCK or a fresh saver. win_gc
		 * takes it out when the client has gone.
		 */
		kcon_surface_close(w->surf);
		return;
	}

	win_drop(w);
}

/*
 * A surface whose client went away leaves a window with nothing behind it.
 * A TERMINAL does not: its window stays, showing how its program finished,
 * until somebody dismisses it.
 */
void win_gc(void)
{
	Win *w = S.wins;

	while (w) {
		Win *next = w->next;

		if (w->kind == WIN_SURFACE && w->surf) {
			int alive = 0;

			for (int i = 0; i < kcon_server_count(S.server); i++) {
				KconSurface *f = kcon_server_at(S.server, i);

				if (kcon_surface_kind(f) == KCON_KIND_VIEW)
					continue;
				if (f == w->surf)
					alive = 1;
			}
			if (!alive) {
				w->surf = NULL;
				win_drop(w);
			}
		}
		w = next;
	}
}

/* ── drawing ─────────────────────────────────────────────────────────── */

/* NOT `const Win *`. A terminal holding a frame open is composed from a buffer
 * the window itself keeps, and keeping it current is a write — see
 * term_cells(). */
static void draw_content(Win *w)
{
	const KtuiCell *src = NULL;
	int sw = 0, sh = 0;
	static KtuiCell buf[512 * 256];

	if (w->kind == WIN_TERM && w->term) {

		sw = w->geom.w;
		sh = w->geom.h;
		if (sw * sh > (int)(sizeof(buf) / sizeof(buf[0])))
			return;
		/* The live grid, or the last whole frame while the program
		 * inside is holding one open with DECSET 2026. A terminal that
		 * never brackets anything answers with `buf` every time. */
		src = term_cells(w, buf, sw, sh);
	} else if (w->kind == WIN_SURFACE && w->surf) {
		src = kcon_surface_cells(w->surf);
		sw = kcon_surface_cols(w->surf);
		sh = kcon_surface_rows(w->surf);
	} else if (w->kind == WIN_EMBED) {
		/* Its cells name sprites the session already sent to every
		 * view. Nothing here is a pixel. */
		embed_draw(w);
		return;
	}

	if (!src)
		return;

	for (int y = 0; y < w->geom.h && y < sh; y++)
		for (int x = 0; x < w->geom.w && x < sw; x++) {
			const KtuiCell *c = &src[y * sw + x];
			uint32_t ch = c->ch;

			/*
			 * A SPRITE CELL NAMES THE SURFACE'S OWN SLOT, and two
			 * surfaces both using slot 0 is the normal case. The
			 * session slot was assigned when the picture arrived;
			 * rewriting the cell here is what stops one window's
			 * picture appearing in another's. A slot the surface
			 * never sent maps to -1 and becomes the fallback
			 * codepoint rather than somebody else's image.
			 */
			if (w->kind == WIN_SURFACE && KTUI_IS_SPRITE(ch)) {
				int gs = kcon_surface_map_slot(w->surf,
					(int)KTUI_SPRITE_SLOT(ch));

				if (gs < 0)
					ch = ' ';
				else
					ch = (ch & ~(0xffffu << 8)) |
					     ((uint32_t)gs << 8);
			}

			/* The cell is copied WHOLE and only its codepoint is
			 * ever rewritten: a window's own colour — a literal a
			 * program named, an underline's colour — has no slot
			 * to be reduced into and must reach the frame with
			 * the cell it belongs to. */
			KtuiCell out = *c;

			out.ch = ch;
			/* The hovered link's whole run, the id being what says
			 * where the address ends. */
			if (w->kind == WIN_TERM && w->hover &&
			    kvt_term_link_at(w->term, (unsigned int)x,
					     (unsigned int)y) == w->hover)
				out.attr |= KT_A_UNDERLINE;
			ktui_draw_put(w->geom.x + x, w->geom.y + y, &out);
		}
}

/*
 * A LOCKED SCREEN WITH NO LOCK CLIENT. The session stayed locked because the
 * program that was drawing the lock died, and this is what is left: the
 * machine is not usable and it says so, rather than showing the desktop to
 * whoever is standing there.
 */
void win_lock_draw(void)
{
	KRect all = krect(0, 0, S.cols, S.rows);
	const char *msg = "LOCKED — the lock screen is not running.";
	const char *how = "Switch to another terminal to recover this session.";

	ktui_draw_fill(all, KT_BG);
	ktui_draw_text((S.cols - (int)strlen(msg)) / 2, S.rows / 2 - 1,
		       S.cols, msg, KT_WARN, KT_BG, 0);
	ktui_draw_text((S.cols - (int)strlen(how)) / 2, S.rows / 2 + 1,
		       S.cols, how, KT_DIM, KT_BG, 0);
}

/*
 * WHERE EACH FRAME'S BUTTONS LANDED, in draw order — back to front, so the
 * last match is the frame on top and that is the one a click belongs to.
 */
typedef struct {
	int x0, x1, y, kind, id;
} WinBtnHit;

static WinBtnHit btn_hits[96];
static int nbtn_hits;

/*
 * THE CHIP A PRESS IS HELD ON. Zero is none; the kind is meaningless then.
 *
 * KEPT ACROSS THE DRAG so that a hand which slips off a chip and comes back
 * still acts on the release. What is not kept is the LIGHT: `draw_buttons`
 * shows the armed state only while the pointer is also over the chip, so a
 * hand moved away shows a chip that will not fire, which is the answer to
 * "how do I take this back".
 */
static int btn_armed_id, btn_armed_kind;

void win_button_arm(int id, int kind)
{
	if (btn_armed_id == id && btn_armed_kind == kind)
		return;
	btn_armed_id = id;
	btn_armed_kind = kind;
	ktui_draw_invalidate();
}

int win_button_armed(int *id)
{
	*id = btn_armed_id;
	return btn_armed_id ? btn_armed_kind : WIN_BTN_NONE;
}

void win_button_disarm(void)
{
	if (!btn_armed_id)
		return;
	btn_armed_id = 0;
	btn_armed_kind = WIN_BTN_NONE;
	ktui_draw_invalidate();
}

int win_button_at(int x, int y, int *id)
{
	*id = 0;
	for (int i = nbtn_hits - 1; i >= 0; i--)
		if (btn_hits[i].y == y && x >= btn_hits[i].x0 &&
		    x <= btn_hits[i].x1) {
			*id = btn_hits[i].id;
			return btn_hits[i].kind;
		}
	return WIN_BTN_NONE;
}

/*
 * WHERE THE POINTER IS, in cells, or off the grid for nowhere. Recorded on
 * every pointer event the router sees, including the leave libkwl reports as
 * an off-grid position: a highlight nothing retracts stays lit for the rest of
 * the session, and a lit button nobody is pointing at is a lie about where the
 * next press will land.
 */
static int ptr_cx = -1, ptr_cy = -1;

void win_ptr_at(int x, int y)
{
	int was_id = 0, now_id = 0;
	int was = win_button_at(ptr_cx, ptr_cy, &was_id);
	int now = win_button_at(x, y, &now_id);

	ptr_cx = x;
	ptr_cy = y;
	/*
	 * THE ONLY THING THAT MOVED IS THE POINTER, so the repaint is asked
	 * for here or the chip lights when some other window happens to
	 * redraw — which on a still desktop is never.
	 *
	 * AND ONLY WHEN THE CHIP UNDER IT CHANGED. Nothing else on the screen
	 * reads this position, so a repaint per cell of motion would be the
	 * whole grid re-sent for every centimetre of a hand crossing an empty
	 * desktop.
	 */
	if (was != now || was_id != now_id)
		ktui_draw_invalidate();
}

/*
 * OSC 133'S PROMPT MARKS, ON THE FRAME'S LEFT BORDER.
 *
 * A terminal has no gutter — every column belongs to the child — so this is
 * drawn on a column of the frame, which is the window manager's. A window with
 * no frame gets nothing rather than a character of the shell's overwritten.
 *
 * The colour carries the meaning: a bullet in the error slot is a command that
 * failed, in the accent one that did not, and a dot where nothing has finished
 * at that prompt yet.
 */
static void draw_marks(Win *w)
{
	if (w->kind != WIN_TERM || !w->term)
		return;
	for (int i = 0; i < w->geom.h; i++) {
		int status = -1;

		if (!kvt_term_mark_at(w->term, (unsigned int)i, &status))
			continue;
		/*
		 * ON THE BORDER CELL BESIDE THE TEXT, which is the innermost
		 * column of the left band — `r.x` is the outermost, and a mark
		 * there sits CON_FRAME_X cells from the line it marks and
		 * breaks the frame's own rule to do it. A border one column
		 * thick makes the two the same cell.
		 */
		ktui_draw_text(w->geom.x - 1, w->geom.y + i, 1,
			       status < 0 ? ktui_glyph[KT_G_DOT]
					  : ktui_glyph[KT_G_BULLET],
			       status < 0 ? KT_DIM
					  : status ? KT_ERR : KT_ACCENT,
			       KT_SURFACE, KT_A_NONE);
	}
}

/*
 * WHERE THE BUTTON RUN STARTS on a frame this wide, and which of the three are
 * on it — the index of the first, so `3 - *first` is how many.
 *
 * THE ONE PLACE EITHER IS WORKED OUT. The title is cut to end before the run
 * and the chips are painted from it, so two answers to "where do the buttons
 * go" is a title cut to the wrong column on exactly the frames nobody looks
 * at.
 *
 * A CHIP IS TWO CELLS, so the run is two per button, and it may not reach the
 * frame's own left corner or the cell beside it: a frame with no rule left of
 * its buttons has nowhere to put a title at all.
 *
 * WHAT GOES WHEN THERE IS NOT ROOM FOR THREE. Close is the one a window cannot
 * be got rid of without, so it is the last to go; minimise is the first,
 * because the taskbar row does the same job and is always there. A frame that
 * kept its title and dropped every button instead leaves the smallest window
 * the desktop can make — the one a resize can always reach — with no way to
 * close it but the keyboard.
 */
static int btn_run(const Win *w, KRect r, int *first)
{
	/*
	 * NO MINIMISE ON A WINDOW WITH NO TASKBAR ROW. The row is the way back
	 * from a minimise and a dialog, a dock or a splash is listed under the
	 * window it belongs to, so the button would be one that puts a window
	 * where nothing on the desktop can reach it.
	 */
	int lo = w->no_task ? 1 : 0;
	/*
	 * ONE COLUMN MORE THAN THE CHIPS, because the group is separated from
	 * the title rule by a gap: a chip whose plate begins where the rule
	 * ends reads as the rule thickening, not as a button starting. The
	 * gap is drawn by draw_buttons() in the frame's own colours, so it is
	 * the border that gives the column up and not the title.
	 */
	int n = (r.w - 4) / CON_CHIP_W;

	if (n > 3 - lo)
		n = 3 - lo;
	if (n < 0)
		n = 0;
	*first = 3 - n;
	return r.x + r.w - 1 - n * CON_CHIP_W;
}

/*
 * THE COLOURS OF ONE CHIP, and every one of them a slot.
 *
 * A BUTTON DRAWN IN THE BORDER'S OWN SLOT IS NOT A BUTTON. Give the three the
 * colour `ktui_draw_box` is handed in the same call and the group reads as a
 * run of border rather than as three things to press — the border's rule shows
 * through the gap between each pair and joins them — and on an unfocused frame
 * that slot is KT_DIM, which measures 1.45:1 against KT_SURFACE across the
 * seven schemes, under any threshold at which a glyph can be read at all.
 *
 * A CHIP CARRIES ITS MEANING IN ITS FILL. The urgent slot for the one that
 * destroys the window and the mid fill for the two that do not, so close is
 * told apart from its neighbours before it is read; the dim fill on an
 * unfocused frame, which is the quietest ground a chip can have and still put
 * KT_TEXT on it at better than 8:1.
 *
 * AND THE ACCENT UNDER THE POINTER, for all three. It is this desktop's slot
 * for the thing that is live, it clears 10:1 against the glyph in every
 * scheme, and it is the only step that is unmistakable on a chip that is
 * already red: what the button does is taught by its resting colour, and the
 * highlight has one job, which is to say the press will land here.
 *
 * THE INK IS DARK ON EVERY BRIGHT FILL, and it has to be: KT_TEXT measures
 * 1.10:1 against KT_ACCENT and 2.17:1 against KT_ERR, so a bright glyph on a
 * lit or a red chip is a chip with nothing drawn on it. KT_SURFACE clears 3.4:1
 * on the mid fill, 5.2:1 on the red one and 10.4:1 under the pointer.
 *
 * WHICH MAKES THE CHIP'S MARK A SHAPE AND NOT A CONTRAST. The dark slots of
 * this palette are one colour to the eye — KT_BG measures 1.00:1 to 1.20:1
 * against KT_SURFACE across the seven schemes — so dark ink cannot be told
 * from the window body below the title row by its colour, whichever of them it
 * is drawn in. What tells them apart is the plate AROUND the mark, so a chip
 * glyph has to be one the fill encloses on all four sides: `draw_buttons`
 * holds that rule with the glyphs that satisfy it.
 */
static void btn_slots(int kind, int focused, int hot, int armed, int *fg,
		      int *bg)
{
	*fg = KT_SURFACE;
	if (armed) {
		/*
		 * HELD DOWN IS THE HOVER PAIR THE OTHER WAY UP. The plate goes
		 * to the body's own slot and the mark takes the lit one, so the
		 * chip reads as pushed IN rather than as lit brighter — and it
		 * needs no ninth colour to do it. KT_ACCENT on KT_SURFACE
		 * measures the same 10.49:1 the hover pair does, because it is
		 * the same pair.
		 */
		*bg = KT_SURFACE;
		*fg = KT_ACCENT;
	} else if (hot)
		*bg = KT_ACCENT;
	else if (!focused) {
		*bg = KT_DIM;
		*fg = KT_TEXT;
	} else if (kind == WIN_BTN_CLOSE)
		*bg = KT_ERR;
	else
		*bg = KT_MID;
}

/*
 * A TITLE CUT TO THE COLUMNS IT MAY HAVE, in place.
 *
 * `ktui_draw_box` lays a title out from the frame's third column and closes it
 * with a space, and the buttons are painted over that same row afterwards: a
 * title long enough to reach them ends inside a chip, in the chip's colours
 * and mid-word. The cut is made before the box sees the title, so what is on
 * the row is a whole title or a shortened one — never a button with a letter
 * of somebody's window name in it.
 *
 * COLUMNS AND NOT BYTES. A title is whatever the program set, so it is UTF-8
 * and a byte count cuts a character in half; half a character is the `?` every
 * unmapped codepoint becomes.
 */
static void title_cut(char *s, int cols)
{
	const char *p = s;
	int used = 0;

	if (cols < 0)
		cols = 0;
	for (;;) {
		uint32_t cp = 0;
		const char *next;
		int cw;

		if (!*p)
			return;
		next = ktui_utf8_next(p, &cp);
		cw = ktui_wcwidth(cp);
		if (cw < 0)
			cw = 0;
		if (used + cw > cols)
			break;
		used += cw;
		p = next;
	}
	s[p - s] = '\0';
}

/*
 * `↓ ■ X` at the right of the title row, each one a three-cell chip: the
 * chip's own fill, the mark, the fill again.
 *
 * THREE AND NOT TWO. See CON_CHIP_W: the mark is read by the plate AROUND it,
 * because the palette's dark slots are one colour to the eye, and only an odd
 * width can put plate on both sides of it. The extra cell is also the
 * difference between a target a mouse must be aimed at and one a finger can
 * land on.
 *
 * INSIDE THE VT TIER. The console font is 512 glyphs and renders anything it
 * does not carry as a blank, so a hollow square would be an invisible button
 * on `tty1`. `↓` and `■` are both on the font's list and both come from the
 * glyph table, which hands a terminal with no UTF-8 `v` and `#`; `X` is ASCII.
 *
 * EVERY MARK IS INK THE FILL ENCLOSES — a shape with plate above it, below it
 * and either side, and that plate is the whole of the boundary. On a focused
 * frame `btn_slots` draws the mark in KT_SURFACE, which is the slot the frame
 * body under the title row is filled with, so the strip of plate between the
 * ink and the cell's floor is the only thing dividing the two; an unfocused
 * chip carries KT_TEXT on KT_DIM, which clears 8.3:1 against both and needs
 * no such margin. It is a threshold, not an absolute: measured in the
 * console's ter-kdos32n, `↓`, `■` and `X` are 68, 108 and 80 lit pixels of
 * 512 on rows 6-25, 10-21 and 6-25 of 32, keeping six clear rows or more
 * above and below; `_` is 24 pixels on rows 27 and 28, three rows off the
 * floor, and it is the one shape this fill cannot hold — at that clearance
 * the chip reads as the plate ending early rather than as a glyph.
 *
 * AND THE MARK SAYS WHAT THE BUTTON DOES: the window goes DOWN to the taskbar
 * row, fills the screen as a block, or is struck out. Down is this desktop's
 * direction mark — the same `↓` the bottom border lights with when a press
 * there would drag that edge down.
 *
 * Drawn here rather than by `ktui_draw_box`: that function has thirty-two call
 * sites across twenty-five files, including the installer and the build tool,
 * and giving it buttons would put them on every box in the tree and move
 * goldens that have nothing to do with this desktop. Buttons are a property of
 * a managed window, so the window manager draws them.
 *
 * THE HIT BOX IS THE WHOLE CHIP. A one-cell target is the failure a person
 * feels: a mouse has to be aimed at it and a finger cannot land on it at all,
 * and the miss goes to the title row underneath, which arms a move. What is
 * painted and what answers a press are the same two columns, so a chip is
 * hit wherever it can be seen.
 */
static void draw_buttons(Win *w, KRect r, int focused)
{
	/*
	 * THE ARROW AND THE SQUARE COME FROM THE GLYPH TABLE, not written
	 * literally: the table picks per tier, so the console font's own
	 * glyphs are used where they exist and a terminal without UTF-8 gets
	 * the tier's stand-in rather than the '?' every unmapped codepoint
	 * becomes.
	 *
	 * `X` is ASCII and needs no such care.
	 */
	const struct { const char *g; int kind; } b[] = {
		{ ktui_glyph[KT_G_DOWN], WIN_BTN_MIN },
		{ ktui_glyph[KT_G_SQUARE], WIN_BTN_MAX },
		{ "X", WIN_BTN_CLOSE }
	};
	int first, x = btn_run(w, r, &first);
	/*
	 * A CHIP LIGHTS ONLY ON THE FRAME THE POINTER CAN ACTUALLY REACH.
	 * Frames are painted back to front, so one under another window is
	 * painted and then covered anyway; one lit on a frame the pointer is
	 * not over would be a highlight promising a press that lands
	 * somewhere else.
	 */
	int lit = win_at(ptr_cx, ptr_cy) == w && ptr_cy == r.y;

	if (first >= 3 ||
	    nbtn_hits + (3 - first) > (int)(sizeof btn_hits / sizeof *btn_hits))
		return;

	/*
	 * THE GAP BEFORE THE GROUP, in the frame's own colours and not a
	 * chip's, so the run reads as border, space, then three buttons. A
	 * rule running into the first plate is what makes the group look like
	 * more border.
	 *
	 * THE COLOURS ARE READ BACK AND THE CHARACTER ALONE IS REPLACED: this
	 * cell IS frame, and the frame is drawn in slots this function is not
	 * handed — the bell rings a window by swapping its fill and its rule,
	 * so a gap that picked its own pair would be the one cell of the title
	 * row that did not ring.
	 */
	int cw = 0, chh = 0;
	const KtuiCell *cells = ktui_draw_cells(&cw, &chh);

	if (cells && x - 1 >= 0 && r.y >= 0 && x - 1 < cw && r.y < chh) {
		const KtuiCell *c = &cells[(size_t)r.y * cw + (x - 1)];

		ktui_draw_cell(x - 1, r.y, ' ', c->fg, c->bg, KT_A_NONE);
	}

	for (int i = first; i < 3; i++) {
		int hot = lit && ptr_cx >= x && ptr_cx <= x + CON_CHIP_W - 1;
		int armed = hot && btn_armed_id == w->id &&
			    btn_armed_kind == b[i].kind;
		int fg, bg;

		btn_slots(b[i].kind, focused, hot, armed, &fg, &bg);
		ktui_draw_cell(x, r.y, ' ', fg, bg, KT_A_NONE);
		ktui_draw_text(x + 1, r.y, 1, b[i].g, fg, bg, KT_A_NONE);
		/*
		 * THE PLATE EITHER SIDE IS WRITTEN AND NOT READ BACK. Those
		 * cells hold the rule `ktui_draw_box` ran along the title row,
		 * and a chip that kept that character puts a length of border
		 * inside a button: on a focused frame it is the double rule,
		 * which is most of the ink the chip would carry and is drawn
		 * in the same slot as the mark between them. Spaces in the
		 * chip's own fill are what make the three cells one plate with
		 * one mark on it.
		 */
		ktui_draw_cell(x + 2, r.y, ' ', fg, bg, KT_A_NONE);
		btn_hits[nbtn_hits].x0 = x;
		btn_hits[nbtn_hits].x1 = x + CON_CHIP_W - 1;
		btn_hits[nbtn_hits].y = r.y;
		btn_hits[nbtn_hits].kind = b[i].kind;
		btn_hits[nbtn_hits].id = w->id;
		nbtn_hits++;
		x += CON_CHIP_W;
	}
}

/*
 * THE TAB STRIP, ALONG THE TITLE ROW, AND ONLY ON A WINDOW WITH MORE THAN ONE
 * TAB.
 *
 * A WINDOW IN NO STACK DRAWS NOTHING HERE, which is not merely an economy: a
 * strip on an ordinary frame would move every committed frame golden in the
 * tree, and the whole of what a stack adds has to be visible only where a
 * person made one.
 *
 * IT IS THE TITLE ROW'S TEXT AND NOT AN EXTRA ROW. The strip runs from the
 * frame's third column to the gap draw_buttons() leaves before the chips —
 * `[r.x + 2, btn_run() - 1)` — which is the same run the title occupies, so a
 * stacked frame costs no cells at all. win_draw_all() drops the title for that
 * reason; the ring number goes into the live tab, which is the only member of
 * a stack the ring can reach, because every other one is hidden.
 *
 * TWO SLOT PAIRS AND NO THIRD. The live tab is KT_SURFACE on KT_ACCENT — the
 * pair a chip takes under the pointer — and a resting one is KT_TEXT on
 * KT_DIM, which clears 8.3:1. The pair does NOT dim with the frame: the strip
 * answers which tab is up and the frame answers which window has the keyboard,
 * and a strip that went flat on an unfocused frame would leave a person unable
 * to read what a stack will show when they click on it.
 *
 * THE PLATE AROUND THE TEXT IS WHAT SEPARATES ONE TAB FROM THE NEXT, exactly
 * as it is on a chip: the palette's dark slots are one colour to the eye, so a
 * tab is read by its fill and not by its ink. Each tab is filled whole and the
 * name is written one column in, which leaves a column of plate on each side
 * of it.
 *
 * THE REMAINDER GOES TO THE TABS ON THE LEFT, so the strip fills its run
 * exactly. A run divided with a gap left over is a strip with a length of
 * border in the middle of it, which reads as two strips.
 *
 * AND WHEN THE TABS OUTNUMBER THE COLUMNS IT COLLAPSES TO A COUNTER. Below
 * CON_TAB_MIN per tab the names are initials and the strip has stopped saying
 * anything; ` 2/4 ` says which tab of how many, which is the one thing still
 * worth a cell. The rest of the run is left as the rule the box drew, so the
 * counter reads as one plate on a title row rather than as a shrunken strip.
 */
static void draw_tabs(Win *w, KRect r)
{
	Win *set[STACK_MAX];
	int n = stack_set(w, set, STACK_MAX);
	int first, x0 = r.x + 2, x1 = btn_run(w, r, &first) - 1;
	int avail = x1 - x0;

	if (n < 2 || avail < 4)
		return;

	if (avail / n < CON_TAB_MIN) {
		char c[16];
		int len;

		snprintf(c, sizeof(c), " %d/%d ", win_stack_index(w), n);
		len = ktui_utf8_width(c);
		if (len > avail)
			len = avail;
		ktui_draw_text(x0, r.y, len, c, KT_SURFACE, KT_ACCENT,
			       KT_A_NONE);
		return;
	}

	for (int i = 0, x = x0; i < n; i++) {
		Win *m = set[i];
		int tw = avail / n + (i < avail % n);
		int live = m->id == w->stack;
		int fg = live ? KT_SURFACE : KT_TEXT;
		int bg = live ? KT_ACCENT : KT_DIM;
		/* THE RING NUMBER ON THE LIVE TAB ALONE, and it falls out
		 * rather than being decided here: win_index() answers 0 for a
		 * hidden window, and every tab but the live one is hidden. */
		int idx = win_index(m);
		char t[160];

		if (idx > 0 && idx < 10)
			snprintf(t, sizeof(t), "%d:%s", idx, m->title);
		else
			snprintf(t, sizeof(t), "%s", m->title);
		for (int c = 0; c < tw; c++)
			ktui_draw_cell(x + c, r.y, ' ', fg, bg, KT_A_NONE);
		ktui_draw_text(x + 1, r.y, tw - 2, t, fg, bg, KT_A_NONE);
		x += tw;
	}
}

/*
 * IS ANYBODY LOOKING AT THIS WINDOW.
 *
 * THE ONE PLACE THE LIST IS WRITTEN, because two answers to it drift: the
 * draw loop below skips what it may not paint, and embed_pump() decides from
 * the same question whether a guest keeps rendering and keeps spending the
 * display queue. A guest left awake for a window on another workspace, behind
 * the lock or under the saver costs the window being looked at exactly the
 * bandwidth it takes.
 *
 * The lock and the saver are drawn INSTEAD OF the desktop rather than over it,
 * so while either is up nothing else is on a screen at all.
 */
int win_is_on_screen(const Win *w)
{
	if (!w)
		return 0;
	if (S.locked)
		return w == S.lock && !w->minimised;
	if (S.saver && !S.saver->minimised)
		return w == S.saver;
	/* A guest is on another terminal entirely; the taskbar is the only
	 * place it appears on this one. */
	if (w->kind == WIN_VT)
		return 0;
	/*
	 * A SURFACE THAT SAYS IT HAS NOTHING TO SHOW IS DRAWN NOWHERE. An
	 * overlay — a candidate window, a stack of toasts — is up for a
	 * fraction of the time its program is running, and one that could not
	 * say so would leave an empty box on the desktop for the rest of the
	 * session.
	 */
	if (w->kind == WIN_SURFACE && w->surf && kcon_surface_hidden(w->surf))
		return 0;
	/*
	 * A panel is on a workspace of its own — every one of them, and so is
	 * a layer: a toast that belonged to the workspace it was raised on
	 * would be invisible to somebody who had just switched away from it. A
	 * sticky window is on every one for the same reason and by the same
	 * test, and a hidden one on none.
	 */
	if (w->minimised || w->hidden)
		return 0;
	if (!w->panel && !w->overlay && !w->background && !w->sticky &&
	    w->workspace != S.workspace)
		return 0;
	return 1;
}

/*
 * WHICH OF THE THREE LAYERS A WINDOW IS DRAWN IN — see the draw loop, whose
 * three passes this names. One expression and not two: a second reading of it
 * would decide occlusion by an order the walk does not paint in, which is a
 * mark laid over a menu on exactly the frames nobody looks at.
 */
static int win_layer(const Win *w)
{
	return w->background ? 0 : w->overlay ? 2 : 1;
}

int win_covered_at(const Win *w, int x, int y)
{
	int wl;
	/* The list is front-first, so everything walked before `w` is drawn
	 * after it and everything after it is drawn before. A higher layer is
	 * in front wherever it sits in the list, because the layers are three
	 * passes and not a stacking order. */
	int above = 1;

	if (!w)
		return 0;
	wl = win_layer(w);
	for (const Win *v = S.wins; v; v = v->next) {
		KwmRect r;

		if (v == w) {
			above = 0;
			continue;
		}
		if (win_layer(v) < wl || (win_layer(v) == wl && !above))
			continue;
		if (!win_is_on_screen(v))
			continue;
		/* THE RECTANGLE THE WALK ACTUALLY PAINTS. A panel, a layer, a
		 * fullscreen window and the icon layer are drawn with no frame
		 * at all, so counting a frame's cell round them would refuse a
		 * cell nothing was ever drawn on. */
		r = win_framed(v) ? win_frame(v) : v->geom;
		if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
			return 1;
	}
	return 0;
}

void win_draw_all(void)
{
	nbtn_hits = 0;

	/*
	 * WHILE LOCKED, ONLY THE LOCK IS DRAWN. Not the windows under it and
	 * not the panel: a lock screen composited over a desktop shows the
	 * desktop wherever the lock surface has a transparent cell, and every
	 * cell it has not written is one.
	 */
	if (S.locked) {
		if (S.lock && !S.lock->minimised)
			draw_content(S.lock);
		else
			win_lock_draw();
		return;
	}

	/*
	 * AND WHILE A SAVER IS UP, ONLY THE SAVER, for the same reason: a cell
	 * it has not written would show the desktop it exists to cover. It is
	 * drawn INSTEAD OF the grid rather than over it, so the window list,
	 * the panel and the terminals underneath are not painted at all — a
	 * machine nobody is looking at should not be animating two pictures.
	 *
	 * Below the lock, never above: an idle machine reaches the saver first
	 * and the lock after it, and a saver drawn over a lock prompt would
	 * hide the password field.
	 */
	if (S.saver && !S.saver->minimised) {
		draw_content(S.saver);
		return;
	}

	/*
	 * BACK TO FRONT, because the list is front-first: a z-ordered copy is
	 * the whole of compositing when a pixel is eight bytes.
	 */
	Win *order[128];
	int n = 0;

	for (Win *w = S.wins; w && n < 128; w = w->next)
		order[n++] = w;

	/*
	 * THREE PASSES, BECAUSE THERE ARE THREE LAYERS. The stacking order
	 * inside each is the list's own; between them it is fixed, and it has
	 * to be: a menu that a window could be raised above is a menu that
	 * disappears behind the thing it was opened from, and desktop icons
	 * drawn last would cover every window on the screen.
	 */
	for (int layer = 0; layer < 3; layer++) {
		for (int i = n - 1; i >= 0; i--) {
			Win *w = order[i];

			if (win_layer(w) != layer)
				continue;

			/* It is drawn here or it is drawn nowhere, and what
			 * counts as nowhere is win_is_on_screen's — the same
			 * answer the embedded guests are put to sleep by. */
			if (!win_is_on_screen(w))
				continue;

			/*
			 * NO FRAME AND NO SHADOW on a panel, a layer or a
			 * fullscreen window. Each is part of the desktop rather
			 * than something sitting on it, and a chrome-wrapped
			 * toast is the clearest case: it would arrive with a
			 * title bar and a close button nobody asked for.
			 */
			if (!win_framed(w)) {
				draw_content(w);
				continue;
			}

			KwmRect f = win_frame(w);
			KRect r = krect(f.x, f.y, f.w, f.h);
			int focused = w->id == S.focus;
			/*
			 * THE VISIBLE BELL IS THE CHROME, INVERTED. The
			 * content is the program's and must not be repainted
			 * in colours it did not choose — a flash that covered
			 * the text would hide the line that rang — so the
			 * frame and the title carry it, which is also where a
			 * person's eye already is when they are told a window
			 * wants them.
			 */
			int rung = w->bell_until > con_now_ms();
			/*
			 * THE RING POSITION, IN THE TITLE. `Super+Alt+N` names
			 * a window by it, and a number a person cannot see is
			 * a number they cannot use. It is the ring's own index
			 * rather than an id: it renumbers when a window closes,
			 * which is what Alt-Tab does too, so the two cannot
			 * disagree about what "the second window" means.
			 */
			char titled[160];
			int idx = win_index(w);
			int bfirst;

			/*
			 * A STACKED FRAME CARRIES A STRIP INSTEAD OF A NAME.
			 * Both want the same run of cells, and a name beside
			 * four tabs is a name cut to nothing — the live tab
			 * carries this window's title and its ring number, so
			 * nothing is lost but the duplicate.
			 */
			if (win_stack_n(w) > 1)
				titled[0] = '\0';
			else if (idx > 0 && idx < 10)
				snprintf(titled, sizeof(titled), "%d:%s", idx,
					 w->title);
			else
				snprintf(titled, sizeof(titled), "%s",
					 w->title);
			/*
			 * AND CUT TO WHERE THE BUTTONS BEGIN. `ktui_draw_box`
			 * writes the title from the third column and closes it
			 * with a space, so the space has to land left of the
			 * run: `btn_run` is asked rather than the width
			 * guessed, because it is the same call that puts the
			 * chips there.
			 */
			title_cut(titled, btn_run(w, r, &bfirst) - r.x - 4);

			ktui_draw_shadow(r);
			ktui_draw_fill(r, rung ? KT_ACCENT : KT_SURFACE);
			ktui_draw_box(r, titled,
				      rung ? KT_SURFACE :
				      focused ? KT_ACCENT : KT_DIM,
				      rung ? KT_ACCENT : KT_SURFACE,
				      /* dbl */ focused || rung);
			draw_buttons(w, r, focused);
			draw_tabs(w, r);
			draw_marks(w);
			draw_content(w);
		}
	}

	win_list_draw();
	/* AND THE WINDOW MENU OVER BOTH. It is a popup a person opened a
	 * moment ago and is holding the pointer over; anything drawn on top of
	 * it is a row they cannot read and cannot aim at. */
	win_menu_draw();
	/* The mark is drawn over everything, including the list: it is a
	 * selection of what is on the screen, and the screen is what has just
	 * been composed. */
	con_mark_draw();
}

/*
 * ── THE WINDOW LIST ─────────────────────────────────────────────────────
 *
 * Turbo Vision's Alt+0. Every window on this workspace the taskbar would hold,
 * numbered, `Enter` to raise or restore, `Delete` to close, `m` to put away or
 * bring back, `Escape` to leave.
 *
 * THE NUMBER ON A ROW IS THE ROW'S OWN, AND IT IS NOT THE RING NUMBER. The
 * digit that picks a row is the digit printed beside it — the one promise a
 * numbered list has to keep — and the ring cannot supply it: `reachable()`
 * holds no minimised window, so win_index() answers 0 for every row this list
 * exists to offer, and a minimised taskbar row carries no number for the same
 * reason. `Super+Alt+N` and the title bars mean the ring's number instead, and
 * the two run apart wherever the two sets do: a minimised window and a window
 * a modal stands over are rows the ring does not step to, and a dialog is a
 * ring entry with no row.
 *
 * DRAWN BY THE SESSION AND NOT BY A SURFACE, for now. `kdos-teams` is the
 * program that shows a window list, and it reads the list over the management
 * protocol that Task 6.4 routes through `libkdisp`; until that lands it draws
 * nothing here. A list the session paints out of the list it already holds is
 * fifty lines and works today — and it goes when the surface can do it, rather
 * than staying as a second window list nobody maintains.
 */
static int list_open, list_sel;

/*
 * WHAT THE LIST HOLDS, AND IT IS NOT THE RING'S SET. `reachable()` is the
 * right rule for a ring — a step onto a window drawn nowhere lands on a blank
 * screen — and the wrong one here: this list is the taskbar in a box, and the
 * bar's rule is that A MINIMISED WINDOW KEEPS ITS ROW, because the row is the
 * way back. The list is what a session with no shell has instead of a bar, so
 * a list built from the ring's predicate would answer "no windows" to a person
 * whose windows are every one of them put away, leaving the chord as the only
 * route back.
 *
 * Otherwise it is the bar's set exactly: one application is one row, a layer
 * and a docked panel are not things a person switches between, and a hidden
 * window's chord is its way back rather than a row.
 *
 * A WINDOW A MODAL STANDS OVER IS IN IT, which is where this parts from the
 * ring. The ring must skip one — every step lands back on the question and the
 * window after it is never reached — but a row is not a step: `Enter` on it
 * goes through `win_raise()`, which redirects to the question and flashes it,
 * exactly as a click on the taskbar row does. The question itself carries no
 * row of its own, so a list that dropped the owner as well would answer "no
 * windows" to an application with a save dialog open.
 */
static int listable(const Win *w)
{
	if (!w || w->panel || w->overlay || w->background || w->no_task)
		return 0;
	if (w == S.saver || w == S.lock || w->hidden)
		return 0;
	if (w->kind == WIN_SURFACE && w->surf && kcon_surface_hidden(w->surf))
		return 0;
	if (!w->sticky && w->workspace != S.workspace)
		return 0;
	return 1;
}

/* Both halves of the list work from one set, in stacking order: a draw and a
 * key that built theirs separately would number the rows differently the first
 * time one of them grew a test the other did not. */
static int list_set(Win **set, int max)
{
	int n = 0;

	for (Win *w = S.wins; w && n < max; w = w->next)
		if (listable(w))
			set[n++] = w;
	return n;
}

/*
 * WHAT A ROW DOES IS THE WINDOW'S STATE TO DECIDE, which is the taskbar row's
 * rule as well: a window that is away comes back, and one that is here comes to
 * the front. Two keys for the two halves would make a person read the mark
 * before they could press anything.
 */
static void list_pick(Win *w)
{
	if (w->minimised)
		win_restore(w);
	else
		win_raise(w->id);
}

void win_list_toggle(void)
{
	list_open = !list_open;
	list_sel = 0;
	ktui_draw_invalidate();
}

int win_list_active(void)
{
	return list_open;
}

void win_list_draw(void)
{
	Win *set[32];
	int n = 0;

	if (!list_open)
		return;
	n = list_set(set, 32);

	int rows = (n ? n : 1) + 4;
	/* Wide enough for the footer below, which is the widest thing in the
	 * box: a box sized to the rows would cut the line that says what the
	 * keys do. */
	int cols = 60;
	KwmRect a = win_workarea();

	if (cols > a.w)
		cols = a.w;
	if (rows > a.h)
		rows = a.h;

	KRect r = krect(a.x + (a.w - cols) / 2, a.y + (a.h - rows) / 2, cols,
			rows);

	if (list_sel >= n)
		list_sel = n ? n - 1 : 0;

	ktui_draw_shadow(r);
	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, "Windows", KT_ACCENT, KT_SURFACE, 1);

	if (!n) {
		ktui_draw_text(r.x + 2, r.y + 2, cols - 4, "no windows",
			       KT_MID, KT_SURFACE, KT_A_NONE);
	} else {
		for (int i = 0; i < n && 2 + i < rows - 2; i++) {
			char row[80];
			int on = i == list_sel;
			int away = set[i]->minimised;

			/*
			 * A WINDOW THAT IS AWAY SAYS SO IN ITS FIRST COLUMN,
			 * with the mark its own minimise chip carries and in
			 * the fill a taskbar row uses for it. Rows that all
			 * looked alike would make `Enter` mean two things with
			 * nothing on screen saying which.
			 */
			snprintf(row, sizeof(row), " %s %2d  %.46s",
				 away ? ktui_glyph[KT_G_DOWN] : " ", i + 1,
				 set[i]->title);
			if (on)
				ktui_draw_fill(krect(r.x + 1, r.y + 1 + i,
						     cols - 2, 1), KT_ACCENT);
			ktui_draw_text(r.x + 1, r.y + 1 + i, cols - 2, row,
				       on ? KT_BG : away ? KT_DIM : KT_TEXT,
				       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
		}
	}
	ktui_draw_text(r.x + 2, r.y + rows - 2, cols - 4,
		       "Enter raise/restore  Del close  m minimise/restore  Esc",
		       KT_MID, KT_SURFACE, KT_A_NONE);
}

/* True when the key was the list's. It owns the keyboard while it is up. */
int win_list_key(int key)
{
	Win *set[32];
	int n = 0;

	if (!list_open)
		return 0;
	n = list_set(set, 32);
	if (list_sel >= n)
		list_sel = n ? n - 1 : 0;

	switch (key) {
	case KT_K_ESC:
		list_open = 0;
		break;
	case KT_K_UP:
		if (list_sel > 0)
			list_sel--;
		break;
	case KT_K_DOWN:
		if (list_sel + 1 < n)
			list_sel++;
		break;
	case KT_K_ENTER:
		if (n)
			list_pick(set[list_sel]);
		list_open = 0;
		break;
	case KT_K_DEL:
		if (n)
			win_close(set[list_sel]);
		break;
	case 'm':
		/* THE KEY THAT PUTS A WINDOW AWAY BRINGS IT BACK. A list that
		 * could do only the first half is the one-way door this list
		 * exists to be the way out of. */
		if (!n)
			break;
		if (set[list_sel]->minimised)
			win_restore(set[list_sel]);
		else
			win_minimise(set[list_sel]);
		break;
	default:
		/* A digit picks directly, which is the whole point of a
		 * numbered list. */
		if (key >= '1' && key <= '9' && key - '1' < n) {
			list_pick(set[key - '1']);
			list_open = 0;
		}
		break;
	}
	ktui_draw_invalidate();
	return 1;
}

/*
 * ── THE WINDOW MENU ─────────────────────────────────────────────────────
 *
 * EVERY FRAME VERB IN ONE LIST, WITH THE CHORD BESIDE IT.
 *
 * WHAT A POINTER CAN OTHERWISE REACH IS THE THREE CHIPS AND A ROW TO DRAG.
 * Fullscreen, lower, the scratchpad mark and send-to-workspace are chords and
 * nothing else, and the shipped `taskbar = windows` draws the window rows
 * instead of the function-key row that names any of them — so a person who has
 * not read the book cannot find one of them at all.
 *
 * THE CHORD ON EACH ROW IS READ OUT OF THE BIND TABLE, never written here.
 * The menu exists to teach the keyboard, and a menu teaching a chord
 * `keys.conf` has moved would be worse than no menu: the pointer verb would
 * still work and the key it named would do something else.
 *
 * DRAWN WITH THE TOOLKIT'S OWN MENU and not a second one. `KtuiMenu` already
 * holds a column of labels with a caret, an accelerator letter per row, a
 * disabled slot and a hit test that walks the same rows the draw walks; a
 * session that grew its own would be the third copy of that widget in the
 * tree. It is a POPUP with no bar — a bar across the top of the desktop would
 * be a menu belonging to no window.
 *
 * THE SECOND PANE IS THE WORKSPACE LIST. `win_send` needs a number and a row
 * cannot ask for one, so the row opens the pane that names them at the same
 * cell the first pane was at.
 */
enum {
	WM_RESTORE = 1, WM_REARRANGE, WM_MIN, WM_MAX, WM_FULL, WM_LOWER,
	WM_SCRATCH, WM_SEND, WM_CLOSE,
	/* A workspace by number: the id CARRIES the workspace, so nine rows
	 * are one row's worth of code. Above every verb above it. */
	WM_WS = 100
};

enum { WM_PANE_MAIN = 0, WM_PANE_WS = 1 };

static KtuiMenu wmenu;
/* THE WINDOW THE ROWS NAME, by id: a window can close while its menu is up,
 * and a pointer held here would be a verb run on freed memory. */
static int wmenu_id;
/* The release of the press that picked a row. See win_menu_ptr(). */
static int wmenu_eat;

/*
 * THE STRINGS THE ROWS PRINT, HELD FOR AS LONG AS THE MENU IS UP. A
 * `KtuiMenuItem` keeps the pointer it is given and the draw reads it every
 * frame, so a chord formatted onto the stack of whatever opened the menu would
 * be a row printing whatever is on that stack now.
 */
static char wm_chord[10][32];
static char wm_ws_label[9][24];
static char wm_ws_chord[9][32];

static KtuiMenuItem wm_item[] = {
	{ "&Restore",		WM_RESTORE,	NULL, 1 },
	{ "&Move or size",	WM_REARRANGE,	NULL, 1 },
	{ "Mi&nimise",		WM_MIN,		NULL, 1 },
	{ "Ma&ximise",		WM_MAX,		NULL, 1 },
	{ "&Fullscreen",	WM_FULL,	NULL, 1 },
	{ "&Lower",		WM_LOWER,	NULL, 1 },
	{ "&Scratchpad",	WM_SCRATCH,	NULL, 1 },
	{ "Send &to workspace",	WM_SEND,	NULL, 1 },
	{ NULL,			0,		NULL, 0 },
	{ "&Close",		WM_CLOSE,	NULL, 1 }
};

static KtuiMenuItem wm_ws_item[9];

static KtuiMenuPane wm_pane[] = {
	{ NULL, wm_item, (int)(sizeof wm_item / sizeof wm_item[0]) },
	{ NULL, wm_ws_item, 0 }
};

/*
 * WHICH ROW PRINTS WHICH ACTION'S CHORD. Parallel to `wm_item` because the two
 * are read in one walk: a lookup keyed on the id would be a second table to
 * keep in step with the first. NULL is the rule, which prints nothing; the
 * empty string is a verb this desktop binds no chord to, and prints nothing
 * either rather than inventing one.
 */
static const char *wm_by[] = {
	"", "rearrange", "minimise", "maximise", "fullscreen", "lower",
	"scratchpad-mark", "", NULL, "close"
};

/*
 * WHAT THIS WINDOW CAN AND CANNOT BE ASKED, AND WHAT EACH ROW COSTS TO SAY.
 *
 * Read once per open rather than per draw: the chords come out of a file and
 * the rows are a walk of the window list, and neither changes while a menu is
 * down under somebody's hand.
 *
 * A ROW THAT DOES NOTHING IS GREYED AND NOT HIDDEN. A menu whose rows moved
 * with the window's state would put Close where Fullscreen was between one
 * press and the next, and the whole reason a menu is worth having over a chord
 * is that its rows stay where a hand left them.
 */
static void wm_arm(Win *w)
{
	int n = (int)(sizeof wm_item / sizeof wm_item[0]);
	/*
	 * RESTORE IS WHATEVER THIS WINDOW'S STATE MAKES IT, and it prints the
	 * chord that does THAT — the fullscreen toggle on a fullscreen window
	 * and the maximise toggle on a maximised one. A row naming one chord
	 * for all three states would name a chord that does something else in
	 * two of them.
	 */
	const char *by = w->full		     ? "fullscreen" :
			 w->tiled == KWM_EDGES_CARDINAL ? "maximise" : "";

	for (int i = 0; i < n; i++) {
		const char *act = i == 0 ? by : wm_by[i];

		if (!act) {
			wm_item[i].accel = NULL;
			continue;
		}
		keys_chord_for(act, wm_chord[i], sizeof wm_chord[i]);
		wm_item[i].accel = wm_chord[i][0] ? wm_chord[i] : NULL;
	}

	wm_item[0].enabled = w->full || w->tiled || w->minimised;
	wm_item[1].enabled = !w->full;
	/* The taskbar row is the way back from a minimise, so a window with no
	 * row of its own cannot be put away — win_minimise()'s rule. */
	wm_item[2].enabled = !w->no_task && !w->panel && !w->minimised;
	wm_item[3].enabled = !w->full;
	wm_item[4].enabled = 1;
	wm_item[5].enabled = 1;
	/* The scratchpad and a stack both own `hidden`, so a tab may not be
	 * offered the mark win_scratch_mark() would refuse it. */
	wm_item[6].enabled = !w->panel && win_stack_n(w) < 2;
	wm_item[7].enabled = !w->sticky && !w->panel && S.nworkspace > 1;
	wm_item[9].enabled = 1;

	wm_pane[WM_PANE_WS].n = S.nworkspace > 9 ? 9 : S.nworkspace;
	for (int i = 0; i < wm_pane[WM_PANE_WS].n; i++) {
		/*
		 * NO ACCELERATOR MARK ON A DIGIT. `ktui_menu_accel_of` reads
		 * a LETTER after the `&` and answers 0 for anything else, so
		 * a marked digit would be an underline advertising a key the
		 * menu does not answer. The rows are picked with the arrows,
		 * with Enter and with the pointer, and the chord beside each
		 * one is what the keyboard reaches them by.
		 */
		snprintf(wm_ws_label[i], sizeof wm_ws_label[i],
			 "Workspace %d", i + 1);
		/*
		 * THE DIGIT CHORDS ARE NOT IN THE BIND TABLE and cannot be:
		 * nine digits and nine shifted digits would be eighteen rows
		 * saying one thing, so `keys_action` answers them in a branch
		 * of its own. The NAME still comes from the one formatter, so
		 * a row here reads exactly as the key card prints it.
		 */
		keys_chord_name('1' + i, KT_MOD_SUPER | KT_MOD_SHIFT,
				wm_ws_chord[i], sizeof wm_ws_chord[i]);
		wm_ws_item[i].label = wm_ws_label[i];
		wm_ws_item[i].id = WM_WS + i;
		wm_ws_item[i].accel = wm_ws_chord[i];
		wm_ws_item[i].enabled = i != w->workspace;
	}
}

void win_menu_open(Win *w, int x, int y)
{
	/*
	 * CLEAR OF THE BAR, WHICH THE WIDGET CANNOT KNOW ABOUT. `ktui_menu_draw`
	 * clamps the pane onto the GRID, and the taskbar is drawn after every
	 * window — so a menu opened on a window whose title row is near the
	 * bottom would have its last rows painted over by the bar. The work
	 * area is what the bar left, and the pane is as tall as its rows plus
	 * its box.
	 */
	KwmRect a = win_workarea();
	int h = (int)(sizeof wm_item / sizeof wm_item[0]) + 2;

	/*
	 * A MENU IS FOR A WINDOW A PERSON HAS OPEN. A docked panel and a layer
	 * — a toast, a popup, the icon layer — are parts of the desktop rather
	 * than things sitting on it, and every row here would name something
	 * that must not happen to one.
	 *
	 * A FULLSCREEN WINDOW IS NOT IN THAT LIST AND MUST NOT BE, which is
	 * where this parts from `win_framed`: a window with no frame left to
	 * point at is exactly the one whose menu is the way out.
	 */
	if (!w || w->panel || w->overlay || w->background)
		return;
	if (y + h > a.y + a.h)
		y = a.y + a.h - h;
	if (y < a.y)
		y = a.y;
	wmenu.pane = wm_pane;
	wmenu.npane = (int)(sizeof wm_pane / sizeof wm_pane[0]);
	wmenu_id = w->id;
	wm_arm(w);
	ktui_menu_open(&wmenu, WM_PANE_MAIN, x, y);
	ktui_draw_invalidate();
}

/*
 * A WINDOW THAT WENT AWAY TAKES ITS MENU WITH IT. The rows name one window and
 * nothing else, so a menu left standing over a rectangle that is gone is a
 * list of verbs with nothing to run them on. Asked at the instant the question
 * matters and never cached, which is the rule every other liveness test in
 * this tree keeps.
 */
int win_menu_active(void)
{
	if (ktui_menu_active(&wmenu) && !win_find(wmenu_id))
		ktui_menu_close(&wmenu);
	return ktui_menu_active(&wmenu);
}

void win_menu_draw(void)
{
	if (!win_menu_active())
		return;
	ktui_menu_draw(&wmenu);
}

static void wm_act(Win *w, int id)
{
	if (id >= WM_WS) {
		win_send(w, id - WM_WS);
		return;
	}
	switch (id) {
	case WM_RESTORE:
		/* THE ONE THE ROW WAS ENABLED FOR, in the order wm_arm() reads
		 * the state in: a window that is fullscreen AND tiled is out
		 * of fullscreen first, because that is the state it is
		 * showing. */
		if (w->full)
			win_fullscreen(w);
		else if (w->minimised)
			win_restore(w);
		else if (w->tiled)
			win_untile(w);
		break;
	case WM_REARRANGE:
		con_rearrange(w);
		break;
	case WM_MIN:
		win_minimise(w);
		break;
	case WM_MAX:
		win_maximise(w);
		break;
	case WM_FULL:
		win_fullscreen(w);
		break;
	case WM_LOWER:
		win_lower(w);
		break;
	case WM_SCRATCH:
		win_scratch_mark(w);
		break;
	case WM_CLOSE:
		win_close(w);
		break;
	default:
		break;
	}
	ktui_draw_invalidate();
}

static void wm_pick(int id)
{
	Win *w = win_find(wmenu_id);

	if (!w)
		return;
	if (id == WM_SEND) {
		/* THE SECOND PANE OPENS WHERE THE FIRST ONE WAS. `wmenu.x` and
		 * `wmenu.y` are the origin the DRAW clamped onto the grid, so
		 * the list lands on the rows the hand is already over rather
		 * than at the cell the menu was asked for. */
		wm_arm(w);
		ktui_menu_open(&wmenu, WM_PANE_WS, wmenu.x, wmenu.y);
		return;
	}
	wm_act(w, id);
}

/* True when the key was the menu's. It owns the keyboard while it is up, for
 * the window list's reason: its arrows move the caret, and a chord firing
 * underneath would snap a window while somebody was choosing a verb for it. */
int win_menu_key(const KtuiEvent *ev)
{
	int id = 0;

	if (!win_menu_active())
		return 0;
	if (ktui_menu_event(&wmenu, ev, &id) == KTUI_MENU_PICKED)
		wm_pick(id);
	ktui_draw_invalidate();
	return 1;
}

/* True when the pointer event was the menu's. */
int win_menu_ptr(const KtuiEvent *ev)
{
	int id = 0, sel, open, r;

	/*
	 * THE RELEASE OF THE PRESS THAT PICKED IS SWALLOWED. A row runs on the
	 * PRESS, as every menu in this tree does, so the menu is already down
	 * when the button comes up — and a release let through lands on
	 * whatever the menu was covering as a button-up that window never
	 * heard go down.
	 */
	if (wmenu_eat && ev->btn == KT_MB_LEFT) {
		if (ev->press == KT_MP_RELEASE) {
			wmenu_eat = 0;
			return 1;
		}
		/* AND A NEW PRESS SPENDS IT UNUSED. A release can go missing —
		 * the pointer leaves the screen, a mode takes it — and a debt
		 * left standing would swallow the release of somebody else's
		 * later click, which is a chip armed and never fired. */
		if (ev->press == KT_MP_PRESS)
			wmenu_eat = 0;
	}
	if (!win_menu_active())
		return 0;
	/*
	 * DISPATCHED ON THE BUTTON AND NEVER ON THE PRESS KIND. A wheel detent
	 * is delivered as a press with no release to match it, and the widget
	 * picks a row on a press — so a tick over the menu would run whichever
	 * row it happened to be under. The left button picks; every other
	 * press puts the menu away, which is what a press outside it does in
	 * any case.
	 */
	if (ev->press == KT_MP_PRESS && ev->btn != KT_MB_LEFT) {
		ktui_menu_close(&wmenu);
		ktui_draw_invalidate();
		return 1;
	}
	sel = wmenu.sel;
	open = wmenu.open;
	r = ktui_menu_event(&wmenu, ev, &id);
	if (r == KTUI_MENU_PICKED) {
		wmenu_eat = 1;
		wm_pick(id);
	}
	/* A REDRAW ONLY WHERE SOMETHING MOVED. The pointer sends an event per
	 * cell crossed and the caret follows it, so an unconditional
	 * invalidate here is a commit per motion for a menu that has not
	 * changed. */
	if (r == KTUI_MENU_PICKED || wmenu.sel != sel || wmenu.open != open)
		ktui_draw_invalidate();
	return 1;
}

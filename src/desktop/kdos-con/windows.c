/* kdos-con — the window list, and compositing it into the grid. See con.h. */

#include <limits.h>
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
 * The list IS the stack, front first. Raising is a move to the front rather
 * than a z-index, so there is one answer to what is on top and no way for two
 * windows to claim the same depth.
 */
void win_raise(int id)
{
	Win **pp = &S.wins;

	while (*pp && (*pp)->id != id)
		pp = &(*pp)->next;
	if (!*pp)
		return;

	Win *w = *pp;

	*pp = w->next;
	w->next = S.wins;
	S.wins = w;
	S.focus = id;

	/*
	 * RAISING A GUEST IS A VT SWITCH. It has no cells to bring to the front
	 * — it is on a terminal of its own — so the only thing "front" can mean
	 * for one is the screen showing it.
	 */
	if (w->kind == WIN_VT)
		vt_show(w);
}

/*
 * What is left for windows once every exclusive zone is taken out. A panel
 * that reserves one genuinely SHRINKS the area rather than covering it, which
 * is the whole difference between a panel and a window that happens to be at
 * the bottom.
 */
KwmRect win_workarea(void)
{
	int top = 0, bottom = S.rows, left = 0, right = S.cols;

	for (Win *w = S.wins; w; w = w->next) {
		if (!w->panel || !w->exclusive)
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

KwmRect win_frame(const Win *w)
{
	KwmRect r = w->geom;

	r.x -= CON_FRAME;
	r.y -= CON_FRAME;
	r.w += CON_FRAME * 2;
	r.h += CON_FRAME * 2;
	return r;
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

	for (Win *o = S.wins; o && n < 64; o = o->next) {
		if (o == w || o->minimised || o->workspace != w->workspace)
			continue;

		KwmRect f = win_frame(o);

		ex[n].left = f.x;
		ex[n].top = f.y;
		ex[n].right = f.x + f.w;
		ex[n].bottom = f.y + f.h;
		n++;
	}

	KwmBorder m = { CON_FRAME, CON_FRAME, CON_FRAME, CON_FRAME };
	KwmRect area = win_workarea();
	KwmRect g = kwm_place(area, S.gap, m, want_w, want_h, ex, n);

	w->geom.x = g.x;
	w->geom.y = g.y;
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
 */
KwmRect win_tile_rect(unsigned tiled)
{
	KwmRect a = win_workarea();
	KwmBorder m = { CON_FRAME, CON_FRAME, CON_FRAME, CON_FRAME };

	if (tiled != KWM_EDGES_CARDINAL)
		return kwm_tile_geom(a, S.gap, m, tiled);

	a.x += CON_FRAME;
	a.y += CON_FRAME;
	a.w -= CON_FRAME * 2;
	a.h -= CON_FRAME * 2;
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
 * Tell the window it changed size. A terminal reflows its grid and a surface
 * is configured; both are the same event to the window and neither may be
 * skipped, because a program that was not told draws the old size into the new
 * rectangle and the difference is never repainted.
 */
void win_resized(Win *w)
{
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
void win_maximise(Win *w)
{
	if (!w)
		return;
	if (w->tiled == KWM_EDGES_CARDINAL) {
		w->tiled = KWM_EDGE_NONE;
		w->geom = w->restore;
		win_resized(w);
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
 */
void win_fullscreen(Win *w)
{
	if (!w)
		return;
	if (w->full) {
		w->full = 0;
		w->geom = w->restore;
		win_resized(w);
		return;
	}
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
	w->minimised = 1;
	if (S.focus == w->id)
		S.focus = 0;
	win_cycle(1);
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
	if (!w || !w->minimised)
		return;
	w->minimised = 0;
	w->workspace = S.workspace;
	win_raise(w->id);
	S.focus = w->id;
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
		if (w->workspace == S.workspace)
			return w;
		if (!any)
			any = w;
	}
	return any;
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
	if (!w || w->panel || ws < 0 || ws >= S.nworkspace)
		return;
	w->workspace = ws;
	if (S.focus == w->id)
		S.focus = 0;
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
		if (w->minimised || w->workspace != S.workspace)
			continue;

		KwmRect f = win_frame(w);

		if (x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h)
			return w;
	}
	return NULL;
}

/*
 * A WINDOW A PERSON CAN MOVE THE FOCUS TO. The ring, the directional search
 * and the swap all mean the same set, and three copies of the rule is three
 * chances for one of them to stop on a tooltip.
 *
 * A saver is never in it: one that could be given the focus is one that can be
 * left on screen with a window behind it taking the keyboard. Nor is a layer —
 * a toast, a menu and the icon layer are not things a person has open — and
 * nor is a panel, which is docked rather than placed.
 */
static int reachable(const Win *w)
{
	if (!w || w->minimised || w->workspace != S.workspace)
		return 0;
	if (w == S.saver || w->overlay || w->background || w->panel)
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

	if (n < 2)
		return;
	if (cur < 0)
		cur = 0;

	int next = kwm_ring_next(n, cur, dir);

	if (next >= 0)
		win_raise(ids[next]);
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

	if (S.lock == w)
		S.lock = NULL;
	if (S.saver == w)
		S.saver = NULL;

	embed_free(w);

	Win **pp = &S.wins;

	while (*pp && *pp != w)
		pp = &(*pp)->next;
	if (*pp)
		*pp = w->next;

	if (S.focus == w->id)
		S.focus = S.wins ? S.wins->id : 0;
	free(w);
}

void win_close(Win *w)
{
	if (!w)
		return;

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
		/* The same rule a guest on a terminal follows: asked to go, not
		 * removed. embed_reap takes the entry out once the compositor
		 * holding the application is actually gone. */
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

static void draw_content(const Win *w)
{
	const KtuiCell *src = NULL;
	int sw = 0, sh = 0;
	static KtuiCell buf[512 * 256];

	if (w->kind == WIN_TERM && w->term) {
		sw = w->geom.w;
		sh = w->geom.h;
		if (sw * sh > (int)(sizeof(buf) / sizeof(buf[0])))
			return;
		kvt_term_render(w->term, buf, sw, sh);
		src = buf;
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

			ktui_draw_cell(w->geom.x + x, w->geom.y + y,
				       ch, c->fg, c->bg, c->attr);
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
 * `_ ■ X` at the right of the title row.
 *
 * INSIDE THE VT TIER. The console font is 512 glyphs and renders anything it
 * does not carry as a blank, so a hollow square would be an invisible button
 * on `tty1` — `■` is on the font's list and `_` and `X` are ASCII.
 *
 * Drawn here rather than by `ktui_draw_box`: that function has thirty-two call
 * sites across twenty-five files, including the installer and the build tool,
 * and giving it buttons would put them on every box in the tree and move
 * goldens that have nothing to do with this desktop. Buttons are a property of
 * a managed window, so the window manager draws them.
 */
static void draw_buttons(Win *w, KRect r, int focused)
{
	/*
	 * THE SQUARE COMES FROM THE GLYPH TABLE, not written literally: the
	 * table picks per tier, so the console font's square is used where it
	 * exists and a terminal without UTF-8 gets the tier's own stand-in
	 * rather than the '?' every unmapped codepoint becomes.
	 *
	 * `_` and `X` are ASCII and need no such care.
	 */
	const struct { const char *g; int kind; } b[] = {
		{ "_", WIN_BTN_MIN },
		{ ktui_glyph[KT_G_SQUARE], WIN_BTN_MAX },
		{ "X", WIN_BTN_CLOSE }
	};
	int x = r.x + r.w - 7;

	/* A frame too narrow for its title and three buttons gets the title:
	 * a button nobody can read is not a button. */
	if (r.w < 16 || nbtn_hits + 3 > 96)
		return;

	for (int i = 0; i < 3; i++) {
		ktui_draw_text(x, r.y, 2, b[i].g,
			       focused ? KT_ACCENT : KT_DIM, KT_SURFACE,
			       KT_A_NONE);
		btn_hits[nbtn_hits].x0 = x;
		btn_hits[nbtn_hits].x1 = x;
		btn_hits[nbtn_hits].y = r.y;
		btn_hits[nbtn_hits].kind = b[i].kind;
		btn_hits[nbtn_hits].id = w->id;
		nbtn_hits++;
		x += 2;
	}
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
			int wl = w->background ? 0 : w->overlay ? 2 : 1;

			if (wl != layer)
				continue;

			/* A guest is on another terminal entirely; the taskbar
			 * is the only place it appears on this one. */
			if (w->kind == WIN_VT)
				continue;
			/*
			 * A SURFACE THAT SAYS IT HAS NOTHING TO SHOW IS DRAWN
			 * NOWHERE. An overlay — a candidate window, a stack of
			 * toasts — is up for a fraction of the time its program
			 * is running, and one that could not say so would leave
			 * an empty box on the desktop for the rest of the
			 * session.
			 */
			if (w->kind == WIN_SURFACE && w->surf &&
			    kcon_surface_hidden(w->surf))
				continue;
			/* A panel is on a workspace of its own — every one of
			 * them, and so is a layer: a toast that belonged to the
			 * workspace it was raised on would be invisible to
			 * somebody who had just switched away from it. */
			if (w->minimised ||
			    (!w->panel && !w->overlay && !w->background &&
			     w->workspace != S.workspace))
				continue;

			/*
			 * NO FRAME AND NO SHADOW on a panel, a layer or a
			 * fullscreen window. Each is part of the desktop rather
			 * than something sitting on it, and a chrome-wrapped
			 * toast is the clearest case: it would arrive with a
			 * title bar and a close button nobody asked for.
			 */
			if (w->panel || w->full || w->overlay ||
			    w->background) {
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

			ktui_draw_shadow(r);
			ktui_draw_fill(r, rung ? KT_ACCENT : KT_SURFACE);
			ktui_draw_box(r, w->title,
				      rung ? KT_SURFACE :
				      focused ? KT_ACCENT : KT_DIM,
				      rung ? KT_ACCENT : KT_SURFACE,
				      /* dbl */ focused || rung);
			draw_buttons(w, r, focused);
			draw_content(w);
		}
	}
}

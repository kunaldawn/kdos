/* kdos-con — the window list, and compositing it into the grid. See con.h. */

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

	KwmBorder m = { CON_FRAME, CON_FRAME, CON_FRAME, CON_FRAME };
	KwmRect g = kwm_tile_geom(win_workarea(), S.gap, m, next);

	w->geom = g;
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
 * Maximise is the four-edge tile, so an untile from it returns where an untile
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

	KwmBorder m = { CON_FRAME, CON_FRAME, CON_FRAME, CON_FRAME };

	w->geom = kwm_tile_geom(win_workarea(), S.gap, m, w->tiled);
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

/* Alt-Tab, over the windows on this workspace, through libkwm's ring. */
void win_cycle(int dir)
{
	int ids[64];
	int n = 0, cur = -1;

	for (Win *w = S.wins; w && n < 64; w = w->next) {
		if (w->minimised || w->workspace != S.workspace)
			continue;
		/* Never in the cycle: a saver that could be given the focus is
		 * a saver that can be left on screen with a window behind it
		 * taking the keyboard. */
		if (w == S.saver)
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

void win_draw_all(void)
{
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

	for (int i = n - 1; i >= 0; i--) {
		Win *w = order[i];

		/* A guest is on another terminal entirely; the taskbar is the
		 * only place it appears on this one. */
		if (w->kind == WIN_VT)
			continue;
		/*
		 * A SURFACE THAT SAYS IT HAS NOTHING TO SHOW IS DRAWN NOWHERE.
		 * An overlay — a candidate window, a stack of toasts — is up
		 * for a fraction of the time its program is running, and one
		 * that could not say so would leave an empty box on the desktop
		 * for the rest of the session.
		 */
		if (w->kind == WIN_SURFACE && w->surf &&
		    kcon_surface_hidden(w->surf))
			continue;
		/* A panel is on a workspace of its own — every one of them. */
		if (w->minimised || (!w->panel && w->workspace != S.workspace))
			continue;

		/* A panel has no frame and casts no shadow: it is part of the
		 * desktop rather than something sitting on it. */
		if (w->panel || w->full) {
			draw_content(w);
			continue;
		}

		KwmRect f = win_frame(w);
		KRect r = krect(f.x, f.y, f.w, f.h);
		int focused = w->id == S.focus;

		ktui_draw_shadow(r);
		ktui_draw_fill(r, KT_SURFACE);
		ktui_draw_box(r, w->title,
			      focused ? KT_ACCENT : KT_DIM, KT_SURFACE,
			      /* dbl */ focused);
		draw_content(w);
	}
}

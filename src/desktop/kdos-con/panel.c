/* kdos-con — the taskbar. See con.h.
 *
 * Drawn here only until a shell attaches. kdos-shell's panel is the real one
 * and speaks the same management model over libkcon; this exists so a session
 * with no shell installed is still usable rather than a bare grid.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "con.h"

int panel_rows(void)
{
	return 1;
}

/*
 * THE HIT MAP, RECORDED AS THE ROW IS DRAWN.
 *
 * Not re-derived from a column afterwards: a title is truncated to what fits,
 * the clock is right-aligned, and the window list is walked backwards — so a
 * second calculation of where each element ended up is a second thing to get
 * wrong, and a click that lands one entry off is worse than one that lands
 * nowhere. Whoever paints an element records the span it actually covered.
 */
static PanelHit hits[PANEL_HITS];
static int nhits;

static void hit_add(int x0, int x1, int kind, int arg)
{
	if (nhits >= PANEL_HITS || x1 < x0)
		return;
	hits[nhits].x0 = x0;
	hits[nhits].x1 = x1;
	hits[nhits].kind = kind;
	hits[nhits].arg = arg;
	nhits++;
}

/*
 * What is under a point on the panel row, or PANEL_HIT_NONE. The row itself is
 * not a window, so `win_at()` cannot answer this and the caller asks here
 * first whenever the pointer is on it.
 */
int panel_hit(int x, int y, int *arg)
{
	*arg = 0;
	if (y != S.rows - 1)
		return PANEL_HIT_NONE;
	for (int i = 0; i < nhits; i++)
		if (x >= hits[i].x0 && x <= hits[i].x1) {
			*arg = hits[i].arg;
			return hits[i].kind;
		}
	return PANEL_HIT_NONE;
}

/*
 * True once a shell has docked a panel of its own. kdos-shell's panel is the
 * real one and speaks the same model over libkcon; the one below exists so a
 * session with no shell installed is usable rather than a bare grid.
 */
/*
 * THE SHELL'S PANEL, not any docked surface.
 *
 * A slit is a panel too — it docks and takes an exclusive zone — and treating
 * every docked surface as "the bar is covered" left a desktop with a column of
 * gadgets and NO TASKBAR at all: no Start, no window list, no clock, no pager.
 * The one surface that replaces this bar is the one that IS a bar, and it says
 * so with its app id.
 */
int panel_have_shell(void)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->panel && !strcmp(w->app_id, "kdos-shell"))
			return 1;
	return 0;
}

void panel_draw(void)
{
	if (panel_have_shell())
		return;

	int y = S.rows - 1;
	KRect bar = krect(0, y, S.cols, 1);

	ktui_draw_fill(bar, KT_SURFACE);

	nhits = 0;

	int x = 0;
	int x0 = x;

	x += ktui_draw_text(x, y, S.cols - x, " Start ", KT_BG, KT_ACCENT,
			    KT_A_NONE);
	hit_add(x0, x - 1, PANEL_HIT_START, 0);
	x += ktui_draw_text(x, y, S.cols - x, " ", KT_MID, KT_SURFACE,
			    KT_A_NONE);

	/*
	 * The window list, in the order the windows were created rather than
	 * the stacking order — a taskbar whose entries jump about as you click
	 * them is one you cannot aim at.
	 */
	Win *order[64];
	int n = 0;

	for (Win *w = S.wins; w && n < 64; w = w->next) {
		/* A surface saying it has nothing to show is in no taskbar
		 * either: an entry for a window nobody can see is one that
		 * cannot be raised. */
		if (w->kind == WIN_SURFACE && w->surf &&
		    kcon_surface_hidden(w->surf))
			continue;
		/*
		 * A LAYER IS NOT A WINDOW. A row for a toast or a tooltip is a
		 * row that appears for a second and takes the one beside it
		 * with it when it goes, and a row for the icon layer is a row
		 * for the desktop itself.
		 */
		if (w->overlay || w->background)
			continue;
		/*
		 * A MINIMISED WINDOW KEEPS ITS ROW. The row is the way back:
		 * it is drawn nowhere else, cycled past and not hit-testable
		 * on the desktop, so dropping it from the bar as well would
		 * leave the chord as the only route to it.
		 */
		if (w->workspace == S.workspace)
			order[n++] = w;
	}

	/* The window list stops where the pager and the clock begin: a title
	 * that ran under them would be drawn over and the hit map would name a
	 * span the eye cannot see. */
	int reserved = 7 + S.nworkspace * 3;

	for (int i = n - 1; i >= 0 && x < S.cols - reserved - 2; i--) {
		Win *w = order[i];
		int focused = w->id == S.focus;
		char label[24];

		/* An explicit precision, not a wide field trimmed by luck: a
		 * taskbar entry truncates ON PURPOSE, and saying so here is
		 * better than turning the warning off for the whole file. */
		/*
		 * A GUEST IS MARKED BY THE TERMINAL IT IS ON, because that is
		 * the one thing about it a person needs: it is not on this
		 * screen, and the number is how they get to it without the
		 * taskbar.
		 */
		if (w->kind == WIN_VT)
			snprintf(label, sizeof(label), " [vt%d] %.13s ", w->vt,
				 w->title);
		else
			snprintf(label, sizeof(label), " %.20s ", w->title);
		int rx0 = x;

		x += ktui_draw_text(x, y, S.cols - x - reserved, label,
				    focused ? KT_BG : KT_TEXT,
				    focused ? KT_ACCENT
					    : w->minimised ? KT_DIM
							   : KT_SURFACE,
				    KT_A_NONE);
		hit_add(rx0, x - 1, PANEL_HIT_WIN, w->id);
	}

	/*
	 * THE PAGER, left of the clock.
	 *
	 * Without it a workspace switch is a screen whose windows vanished,
	 * with nothing saying where they went or where you now are.
	 *
	 * OCCUPANCY IS COUNTED HERE and not asked of `libkwm`: this bar counts
	 * windows that are not minimised, and the compositor's own pager counts
	 * views that are not omnipresent. They are right answers to two
	 * different questions, and a library that answered one of them would be
	 * wrong for the other.
	 */
	int pager_w = S.nworkspace * 3;
	int px = S.cols - 1 - 6 - pager_w;

	for (int i = 0; i < S.nworkspace && px >= x; i++) {
		int occupied = 0;
		char cell[8];

		for (Win *o = S.wins; o; o = o->next)
			if (o->workspace == i && !o->minimised && !o->panel) {
				occupied = 1;
				break;
			}

		/* Clamped, not merely formatted: `sessions` is a configuration
		 * key and a pager cell is three columns wide whatever it says.
		 * A workspace past nine has no digit and is not drawn. */
		snprintf(cell, sizeof(cell), " %d ", (i + 1) % 10);

		int cx0 = px;

		px += ktui_draw_text(px, y, S.cols - px, cell,
				     i == S.workspace ? KT_BG
						      : occupied ? KT_TEXT
								 : KT_DIM,
				     i == S.workspace ? KT_ACCENT : KT_SURFACE,
				     KT_A_NONE);
		hit_add(cx0, px - 1, PANEL_HIT_WS, i);
	}

	/* The clock, right-aligned. A fixed string when the session is being
	 * dumped, because a golden with a time in it fails every minute. */
	char clock[16];

	if (getenv("KDOS_CON_DUMP"))
		snprintf(clock, sizeof(clock), "00:00");
	else {
		time_t t = time(NULL);
		struct tm tmv;

		localtime_r(&t, &tmv);
		snprintf(clock, sizeof(clock), "%02d:%02d", tmv.tm_hour,
			 tmv.tm_min);
	}

	ktui_draw_text_right(0, y, S.cols - 1, clock, KT_MID, KT_SURFACE,
			     KT_A_NONE);
	/* Right-aligned, so its span is measured from the right edge rather
	 * than from where the drawing happened to stop. */
	hit_add(S.cols - 1 - (int)strlen(clock), S.cols - 1, PANEL_HIT_CLOCK,
		0);
}

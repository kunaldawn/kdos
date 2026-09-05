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

/* "HH:MM" and a column of air. Named because the function-key row measures
 * against it and the clock draws into it; two figures for one width is how a
 * row ends up overwriting the thing it was measured to avoid. */
#define PANEL_CLOCK_W 6
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
/*
 * IS THE BAR IN FUNCTION-KEY MODE? `taskbar = fkeys` in con.conf.
 *
 * Read once and kept: con.conf documents itself as read when the session
 * starts, so a value re-read per frame would answer differently either side of
 * an edit and the bar would change shape under somebody's hand.
 */
int panel_fkeys(void)
{
	static int mode = -1;

	if (mode < 0)
		mode = !strcmp(kcon_conf_str("taskbar", "windows"), "fkeys");
	return mode;
}

int panel_have_shell(void)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->panel && !strcmp(w->app_id, "kdos-shell"))
			return 1;
	return 0;
}

/*
 * The left column of the first element of a kind, or -1.
 *
 * Read out of the hit map rather than recomputed, for the reason the map exists
 * at all: a title is truncated and the clock is right-aligned, so a second
 * calculation of where an element ended up is a second thing to get wrong — and
 * this one decides where a menu opens, so being one column out is visible.
 */
int panel_span_x0(int kind)
{
	for (int i = 0; i < nhits; i++)
		if (hits[i].kind == kind)
			return hits[i].x0;
	return -1;
}

void panel_draw(void)
{
	if (panel_have_shell())
		return;

	int y = S.rows - 1;
	KRect bar = krect(0, y, S.cols, 1);

	ktui_draw_fill(bar, KT_SURFACE);

	nhits = 0;

	/*
	 * WHILE A WINDOW IS BEING MOVED FROM THE KEYBOARD, THE BAR SAYS SO.
	 *
	 * The mode takes every key, so a person who pressed the chord by
	 * mistake has no way to find out what happened unless the desktop
	 * tells them — and the one row that is always on screen is this one.
	 * No hit map is recorded: nothing on this row is clickable while the
	 * mode is on, and a hit map naming spans that do nothing would be a
	 * map that lies.
	 */
	if (con_rearranging()) {
		ktui_draw_text(0, y, S.cols,
			       " arrows move   Shift+arrows size   "
			       "Ctrl+arrows by eight   Return keep   "
			       "Esc revert", KT_BG, KT_ACCENT, KT_A_NONE);
		return;
	}
	if (con_paste_armed()) {
		ktui_draw_text(0, y, S.cols,
			       " that paste would RUN — press the chord again "
			       "within five seconds to mean it", KT_BG,
			       KT_WARN, KT_A_NONE);
		return;
	}
	if (con_marking()) {
		ktui_draw_text(0, y, S.cols,
			       " arrows place the corner   "
			       "Shift+arrows extend   Ctrl+arrows by eight   "
			       "Return copy   Esc cancel", KT_BG, KT_ACCENT,
			       KT_A_NONE);
		return;
	}

	int x = 0;
	int x0 = x;

	x += ktui_draw_text(x, y, S.cols - x, " Start ", KT_BG, KT_ACCENT,
			    KT_A_NONE);
	hit_add(x0, x - 1, PANEL_HIT_START, 0);
	x += ktui_draw_text(x, y, S.cols - x, " ", KT_MID, KT_SURFACE,
			    KT_A_NONE);

	/*
	 * ── THE FUNCTION-KEY ROW ────────────────────────────────────────
	 *
	 * Norton Commander's bottom row named F1 to F10 and what each did, and
	 * a person's hands learned it in a week. `taskbar = fkeys` puts that
	 * row here instead of the window list.
	 *
	 * IT NAMES THE CHORD; IT DOES NOT BIND THE KEY. A bare F7 belongs to
	 * whatever is in the focused window — that is the whole reason a
	 * terminal desktop can carry a function-key row at all — so every cell
	 * is a POINTER target that fires the `Super+F<n>` chord, and the label
	 * carries the `Super` so nobody learns the wrong key from it.
	 *
	 * EVERY CELL NAMES A CHORD THAT EXISTS. The ten below are bound in
	 * `keys.conf`, and eight of them in `rc.xml` too; tile and rearrange
	 * are the console's alone because labwc has no equivalent action, and
	 * the row says so by naming them anyway — this is the console's bar.
	 */
	if (panel_fkeys()) {
		static const struct {
			int n;
			const char *label;
		} fk[] = {
			{ 1, "Keys" },	   { 2, "Wins" },  { 3, "Audio" },
			{ 4, "Net" },	   { 5, "BT" },	   { 6, "Devs" },
			{ 7, "Find" },	   { 8, "Tile" },  { 9, "Move" },
			{ 10, "Menu" },
		};
		int nfk = (int)(sizeof(fk) / sizeof(fk[0]));
		/* The clock's own width, kept clear. A cell drawn into it
		 * would be a label the clock overwrites, which is a target a
		 * person aims at and misses. */
		int right = S.cols - PANEL_CLOCK_W;

		/*
		 * `Super` ONCE, not ten times. The row has to say which key it
		 * means — the chord is `Super+F<n>` and a bare `F7` belongs to
		 * the program in the window — and ten copies of the word do
		 * not fit in eighty columns beside ten labels.
		 */
		x += ktui_draw_text(x, y, right - x, "Super+", KT_MID,
				    KT_SURFACE, KT_A_NONE);

		for (int i = 0; i < nfk; i++) {
			char cell[24];
			int cx0 = x;
			int w = snprintf(cell, sizeof(cell), "%d%s ", fk[i].n,
					 fk[i].label);

			/* A CELL IS DRAWN WHOLE OR NOT AT ALL. Half a label is
			 * a target that names the wrong chord, so a row too
			 * narrow for the tenth ends at the ninth. */
			if (x + w > right)
				break;
			/* The number in the accent and the verb beside it, so
			 * the digit a hand is looking for is the part that
			 * stands out of a row of ten. */
			x += ktui_draw_text(x, y, right - x, cell, KT_BG,
					    KT_ACCENT, KT_A_NONE);
			/* The trailing space is the gap, and it is not part of
			 * the target: a click landing between two cells should
			 * do nothing rather than the wrong one. */
			hit_add(cx0, x - 2, PANEL_HIT_FKEY, fk[i].n);
		}
		goto clock;
	}

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
		/*
		 * AND THE RING NUMBER, the same one the title bar draws, so
		 * `Super+Alt+N` can be aimed from the bar as well as from the
		 * frame. A guest on a terminal of its own is in no ring — it
		 * is not on this screen — so it keeps its `[vtN]` mark and
		 * gets no number.
		 */
		int idx = win_index(w);

		if (w->kind == WIN_VT)
			snprintf(label, sizeof(label), " [vt%d] %.13s ", w->vt,
				 w->title);
		else if (idx > 0 && idx < 10)
			snprintf(label, sizeof(label), " %d:%.18s ", idx,
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

clock:
	/* The clock, right-aligned. A fixed string when the session is being
	 * dumped, because a golden with a time in it fails every minute. */
	{
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
}

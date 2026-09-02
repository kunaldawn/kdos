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
 * True once a shell has docked a panel of its own. kdos-shell's panel is the
 * real one and speaks the same model over libkcon; the one below exists so a
 * session with no shell installed is usable rather than a bare grid.
 */
int panel_have_shell(void)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->panel)
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

	int x = 0;

	x += ktui_draw_text(x, y, S.cols - x, " Start ", KT_BG, KT_ACCENT,
			    KT_A_NONE);
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
		if (w->workspace == S.workspace)
			order[n++] = w;
	}

	for (int i = n - 1; i >= 0 && x < S.cols - 12; i--) {
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
		x += ktui_draw_text(x, y, S.cols - x - 10, label,
				    focused ? KT_BG : KT_TEXT,
				    focused ? KT_ACCENT : KT_SURFACE,
				    KT_A_NONE);
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
}

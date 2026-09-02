/* kdos-con — terminal windows. See con.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "con.h"

Win *term_open(const char *const argv[])
{
	Win *w = calloc(1, sizeof(*w));

	if (!w)
		return NULL;

	w->kind = WIN_TERM;
	w->id = ++S.next_id;
	w->workspace = S.workspace;
	snprintf(w->title, sizeof(w->title), "%s", argv[0] ? argv[0] : "?");
	snprintf(w->app_id, sizeof(w->app_id), "terminal");

	KwmRect area = win_workarea();
	int want_w = area.w * 2 / 3;
	int want_h = area.h * 2 / 3;

	if (want_w < 20)
		want_w = area.w > 20 ? 20 : area.w;
	if (want_h < 6)
		want_h = area.h > 6 ? 6 : area.h;

	/* Placed before the child starts, so the pty is opened at the size the
	 * window actually got and the program never sees a resize it did not
	 * need to handle. */
	w->next = S.wins;
	S.wins = w;
	win_place(w, want_w, want_h);

	w->term = kvt_term_open(argv, w->geom.w, w->geom.h);
	if (w->term)
		kvt_term_scrollback(w->term,
				    (unsigned)kcon_conf_int("scrollback", 2000));
	if (!w->term) {
		win_close(w);
		return NULL;
	}

	S.focus = w->id;
	return w;
}

void term_pump_all(void)
{
	for (Win *w = S.wins; w; w = w->next)
		if (w->kind == WIN_TERM && w->term)
			kvt_term_pump(w->term);
}

/*
 * A key becomes the bytes a terminal program expects, which is the state
 * machine's job and not this file's: the escape an arrow produces depends on
 * application cursor mode and on the modifiers, and both live in there.
 */
int term_key(Win *w, const KtuiEvent *ev)
{
	if (!w || w->kind != WIN_TERM || !w->term || ev->type != KT_EVT_KEY)
		return 0;

	return kvt_term_key(w->term, ev->key, ev->mods);
}

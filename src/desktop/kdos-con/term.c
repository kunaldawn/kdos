/* kdos-con — terminal windows. See con.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "con.h"

/*
 * THE TITLE A PROGRAM SETS, from OSC 0 and OSC 2.
 *
 * Without this a frame and a taskbar row show the name the window was mapped
 * with for the life of the session, so three terminals are three identical
 * rows and there is no way to tell which is building and which is editing.
 *
 * THE GRID IS INVALIDATED, NOT ONLY THE FIELD. The frame and the taskbar are
 * painted from a diff against the last frame, and neither the border nor the
 * row changes on its own — so a title written into the struct and nothing else
 * is a title nobody sees until something unrelated forces a repaint.
 */
static void term_osc(struct kvt_vte *vte, const char *u8, size_t len,
		     void *data)
{
	Win *w = data;

	(void)vte;
	if (!w || len <= 2)
		return;
	if (strncmp(u8, "0;", 2) && strncmp(u8, "2;", 2))
		return;
	snprintf(w->title, sizeof(w->title), "%s", u8 + 2);
	ktui_draw_invalidate();
}

/*
 * A CHILD PUT SOMETHING ON THE CLIPBOARD, through OSC 52. It goes to the
 * session's own selection, which is the only clipboard on this desktop — so a
 * `vim` yank inside a session terminal is pastable into every other window.
 */
static void term_clip(struct kvt_vte *vte, const char *text, size_t len,
		      int primary, void *data)
{
	(void)vte;
	(void)data;
	clip_put(text, len, primary);
}

/*
 * BEL. A terminal is the only thing on this desktop that can ring, and what
 * ringing means is split: the window flashes here and every view is told, so a
 * person over ssh hears it from the terminal they are sitting at.
 */
static void term_bell(struct kvt_vte *vte, void *data)
{
	(void)vte;
	con_bell(data);
}

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
	if (w->term) {
		kvt_term_scrollback(w->term,
				    (unsigned)kcon_conf_int("scrollback", 2000));
		kvt_term_osc_cb(w->term, term_osc, w);
		kvt_term_clip_cb(w->term, term_clip, w);
		kvt_term_bell_cb(w->term, term_bell, w);
	}
	if (!w->term) {
		win_close(w);
		return NULL;
	}

	S.focus = w->id;
	return w;
}

/*
 * THE POINTER OVER A SESSION TERMINAL.
 *
 * `route_ptr()` returned early for a terminal window, so the wheel was dropped
 * and a drag selected nothing: two thousand lines of scrollback were kept and
 * nothing could reach them. The decisions are `kvt_ui_mouse`'s, shared with
 * `kdos-term` so the two cannot drift.
 *
 * A COMPLETED SELECTION GOES TO THE SESSION'S OWN PRIMARY. `ktui_clip_copy()`
 * is a documented no-op on a Linux VT, which is what a `--tty` view on tty1
 * runs in, so the session buffer is the only destination that exists there.
 */
void term_mouse(Win *w, const KtuiEvent *ev)
{
	char *text = NULL;

	if (!w || w->kind != WIN_TERM || !w->term)
		return;

	if (!kvt_ui_mouse(w->term, &w->ui, ev, kb_now_s(), &text))
		return;

	clip_put(text, strlen(text), 1);
	free(text);
	ktui_draw_invalidate();
}

/*
 * A MIDDLE CLICK PASTES THE PRIMARY, the tradition every terminal keeps. It is
 * here rather than in the routing because only a terminal window can take
 * text: a cell surface receives a paste through its own client.
 */
void term_paste(Win *w, int primary)
{
	size_t n = 0;
	const char *text;

	if (!w || w->kind != WIN_TERM || !w->term)
		return;
	text = clip_get(primary, &n);
	if (n)
		kvt_term_paste(w->term, text);
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

	/*
	 * SHIFT+PAGE LOOKS AT WHAT SCROLLED AWAY, and is one of the two chords
	 * a terminal claims for itself — the child cannot express it, because
	 * the lines are gone from its screen and only this program still has
	 * them. Two thousand lines are kept; without this nothing can reach
	 * them.
	 */
	if ((ev->mods & KT_MOD_SHIFT) &&
	    (ev->key == KT_K_PGUP || ev->key == KT_K_PGDN)) {
		kvt_term_scroll(w->term, ev->key == KT_K_PGUP ? -10 : 10);
		ktui_draw_invalidate();
		return 1;
	}

	/*
	 * ANY OTHER KEY ENDS THE SCROLLBACK VIEW. Typing into a screen that is
	 * not the one the cursor is on is how a person ends up editing a line
	 * they cannot see.
	 */
	kvt_screen_sb_reset(kvt_term_screen(w->term));

	/* Ctrl+V pastes the clipboard. The key still goes to the child, so a
	 * program that means something else by it is unaffected. */
	if (ev->key == 0x16)
		term_paste(w, 0);

	return kvt_term_key(w->term, ev->key, ev->mods);
}

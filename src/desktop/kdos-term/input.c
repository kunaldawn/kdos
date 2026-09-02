/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * Keys and the pointer.
 *
 * THE TERMINAL OWNS ALMOST NOTHING. Every key it does not claim goes to the
 * child, because a chord this program eats is a chord no program running
 * inside it can ever use — and the ones it does claim are the two the child
 * cannot express: copy, and looking at what has scrolled away.
 */

#include <string.h>

#include "term.h"

int term_key(const KtuiEvent *ev)
{
	if (!T.t || ev->type != KT_EVT_KEY)
		return 0;

	/*
	 * A key is what ends a scrollback view. Scrolling back and then typing
	 * into a screen that is not the one the cursor is on is how a person
	 * ends up editing a line they cannot see.
	 */
	kvt_screen_sb_reset(kvt_term_screen(T.t));

	return kvt_term_key(T.t, ev->key, ev->mods);
}

/*
 * A LEFT DRAG SELECTS AND A DOUBLE CLICK TAKES A WORD, and neither happens
 * once the child has asked for mouse reports — a program that tracks the mouse
 * is drawing its own idea of what is selected, and a second selection drawn on
 * top of it belongs to nobody.
 *
 * Shift is the override, as it is in every terminal: it hands the pointer back
 * to the terminal so text can be taken out of a program that captured it.
 */
void term_mouse(const KtuiEvent *ev)
{
	struct kvt_screen *sc;

	if (!T.t || ev->type != KT_EVT_MOUSE)
		return;

	sc = kvt_term_screen(T.t);

	if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
		int up = ev->btn == KT_MB_WHEEL_UP;

		/* The wheel goes to the child first: an alternate-screen
		 * program has its own idea of scrolling and the scrollback is
		 * empty while it is up. */
		if (!(ev->mods & KT_MOD_SHIFT) &&
		    kvt_term_mouse(T.t, ev->mx, ev->my,
				   up ? KVT_MOUSE_BUTTON_WHEEL_UP
				      : KVT_MOUSE_BUTTON_WHEEL_DOWN,
				   ev->mods, KVT_MOUSE_EVENT_PRESSED))
			return;
		kvt_term_scroll(T.t, up ? -3 : 3);
		return;
	}

	if (!(ev->mods & KT_MOD_SHIFT) && kvt_term_mouse_mode(T.t)) {
		int btn = ev->btn == KT_MB_MIDDLE ? KVT_MOUSE_BUTTON_MIDDLE
			  : ev->btn == KT_MB_RIGHT ? KVT_MOUSE_BUTTON_RIGHT
			  : KVT_MOUSE_BUTTON_LEFT;
		int event = ev->press == KT_MP_PRESS ? KVT_MOUSE_EVENT_PRESSED
			    : ev->press == KT_MP_RELEASE ? KVT_MOUSE_EVENT_RELEASED
			    : KVT_MOUSE_EVENT_MOVED;

		if (kvt_term_mouse(T.t, ev->mx, ev->my, btn, ev->mods, event))
			return;
	}

	if (ev->btn != KT_MB_LEFT)
		return;

	if (ev->press == KT_MP_PRESS) {
		double now = kb_now_s();

		/*
		 * A second click in the same cell inside the double-click
		 * window takes the word. Measured in CELLS rather than pixels:
		 * a cell surface has no pixels, and a person who moved half a
		 * character did not mean a new selection.
		 */
		if (now - T.click_at < 0.4 && ev->mx == T.click_x &&
		    ev->my == T.click_y) {
			kvt_screen_selection_word(sc, (unsigned)ev->mx,
						  (unsigned)ev->my);
			T.selecting = 0;
			T.click_at = 0;
			return;
		}
		T.click_x = ev->mx;
		T.click_y = ev->my;
		T.click_at = now;

		kvt_screen_selection_reset(sc);
		kvt_screen_selection_start(sc, (unsigned)ev->mx,
					   (unsigned)ev->my);
		T.sel_x = ev->mx;
		T.sel_y = ev->my;
		T.selecting = 1;
		return;
	}

	if (ev->press == KT_MP_DRAG && T.selecting) {
		kvt_screen_selection_target(sc, (unsigned)ev->mx,
					    (unsigned)ev->my);
		return;
	}

	if (ev->press == KT_MP_RELEASE && T.selecting) {
		T.selecting = 0;
		/*
		 * A press and a release in the same cell is a CLICK, and a
		 * click selects nothing. Without this every click left a
		 * one-character selection on the primary clipboard, which is
		 * what middle-click would then paste.
		 */
		if (ev->mx == T.sel_x && ev->my == T.sel_y) {
			kvt_screen_selection_reset(sc);
			return;
		}
		/*
		 * ONTO THE PRIMARY SELECTION, which is what a drag means on
		 * every X and Wayland desktop there has ever been. The
		 * clipboard is Ctrl+Shift+C and is a separate decision.
		 */
		char *text = NULL;

		if (kvt_screen_selection_copy(sc, &text) >= 0 && text) {
			kdisp_copy(text, strlen(text), 1);
			free(text);
		}
	}
}

void term_paste_pending(void)
{
	const char *text = NULL;
	size_t n = ktui_paste_take(&text);

	if (n && text)
		kvt_term_paste(T.t, text);
}

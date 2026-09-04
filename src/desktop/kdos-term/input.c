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
 * ALL OF THAT IS `kvt_ui_mouse`, in libkvt, because the console session runs
 * terminals of its own and must decide the same things. Two copies would
 * drift, and the difference would be a terminal that behaves differently
 * depending on which desktop it is on. What is left here is the one thing
 * that IS this program's: where a completed selection goes.
 */
void term_mouse(const KtuiEvent *ev)
{
	char *text = NULL;

	if (!T.t)
		return;

	if (!kvt_ui_mouse(T.t, &T.ui, ev, kb_now_s(), &text))
		return;

	/*
	 * ONTO THE PRIMARY SELECTION, which is what a drag means on every X
	 * and Wayland desktop there has ever been. The clipboard is
	 * Ctrl+Shift+C and is a separate decision.
	 */
	kdisp_copy(text, strlen(text), 1);
	free(text);
}

void term_paste_pending(void)
{
	const char *text = NULL;
	size_t n = ktui_paste_take(&text);

	if (n && text)
		kvt_term_paste(T.t, text);
}

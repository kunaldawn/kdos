/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — modals
 * ---------------------------------
 */

#include <string.h>

#include "kbase.h"
#include "ktui.h"

static struct {
	int active;
	int confirm;
	char title[64];
	char msg[512];
	char yes[24], no[24];
	void (*on_yes)(void);
	int saved_focus;
	int sel;		/* 0 the affirmative button, 1 the other   */
} md;

/* A modal takes the focus ring over completely and hands it back on close, so
 * dismissing a dialog does not silently move the caret on the page. `sel` is
 * the single truth for which button is chosen: the highlight is pointed at it
 * in ktui_modal_draw(), never held as a fixed focus id, because a surface
 * that runs no immediate-mode frame never resets the id counter and a fixed
 * id would match nothing after the first repaint. */
static void modal_open(void)
{
	md.saved_focus = ktui_focus_get();
	ktui_focus_set(0);
}

static void modal_close(void)
{
	md.active = 0;
	ktui_focus_set(md.saved_focus);
}

int ktui_modal_active(void)
{
	return md.active;
}

void ktui_modal_alert(const char *title, const char *msg)
{
	memset(&md, 0, sizeof(md));
	md.active = 1;
	kb_strlcpy(md.title, title, sizeof(md.title));
	kb_strlcpy(md.msg, msg, sizeof(md.msg));
	kb_strlcpy(md.yes, "OK", sizeof(md.yes));
	modal_open();
}

void ktui_modal_confirm(const char *title, const char *msg, const char *yes,
			const char *no, void (*on_yes)(void))
{
	memset(&md, 0, sizeof(md));
	md.active = 1;
	md.confirm = 1;
	kb_strlcpy(md.title, title, sizeof(md.title));
	kb_strlcpy(md.msg, msg, sizeof(md.msg));
	kb_strlcpy(md.yes, yes, sizeof(md.yes));
	kb_strlcpy(md.no, no, sizeof(md.no));
	md.on_yes = on_yes;
	modal_open();
}

void ktui_modal_draw(void)
{
	if (!md.active)
		return;

	int lines = 1;
	for (const char *p = md.msg; *p; p++)
		if (*p == '\n')
			lines++;

	int w = 56;
	if (w > ktui_w - 6)
		w = ktui_w - 6;
	/* Floor, not a refusal: below this the buttons have no room at any
	 * layout, so the dialog stops shrinking and lets the cell clipping
	 * take the edges on a screen that is genuinely smaller. */
	if (w < 20)
		w = 20;
	int h = lines + 6;
	int x = (ktui_w - w) / 2, y = (ktui_h - h) / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	KRect r = krect(x, y, w, h);
	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_shadow(r);
	ktui_draw_box(r, md.title, md.confirm ? KT_WARN : KT_ACCENT, KT_SURFACE, 1);

	const char *p = md.msg;
	for (int i = 0; i < lines; i++) {
		const char *nl = strchr(p, '\n');
		char buf[256];
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, p, n);
		buf[n] = 0;
		ktui_draw_text(x + 3, y + 2 + i, w - 6, buf, KT_TEXT, KT_SURFACE, 0);
		if (!nl)
			break;
		p = nl + 1;
	}

	/* Buttons stay inside the dialog rect at every width: a fixed
	 * x + w - 40 walks off the left edge below 46 columns. */
	int by = y + h - 2;
	int bw = 18;

	/* The highlight is pointed at `sel` here, at draw time, rather than
	 * held as a fixed focus id: the two buttons claim the next two ids
	 * back to back, and a surface that runs no immediate-mode frame never
	 * resets the id counter, so a fixed id would match nothing after the
	 * first repaint and Tab would move the selection with no feedback. */
	ktui_focus_set(ktui_id_base() + (md.confirm ? md.sel : 0));

	if (md.confirm) {
		int bx1 = x + w - 40, bx2 = x + w - 21;
		if (w < 42) {
			bw = (w - 2) / 2;
			bx1 = x + 1;
			bx2 = x + w - 1 - bw;
		}
		if (ktui_button(krect(bx1, by, bw, 1), md.yes, 1, 1)) {
			void (*fn)(void) = md.on_yes;
			modal_close();
			if (fn)
				fn();
		}
		if (ktui_button(krect(bx2, by, bw, 1), md.no, 1, 0))
			modal_close();
	} else {
		int bx = x + w - 21;
		if (bw > w - 2)
			bw = w - 2;
		if (bx < x + 1)
			bx = x + 1;
		if (ktui_button(krect(bx, by, bw, 1), md.yes, 1, 1))
			modal_close();
	}
}

/*
 * THE DIALOG ANSWERS THE WHOLE KEYBOARD HERE, not through the buttons. The
 * buttons' own Enter path lives inside ktui_frame_begin/ktui_frame_end, so a
 * surface that raises a modal without running the immediate-mode frame — a
 * terminal, a viewer, anything whose main loop is its own — would otherwise
 * have Esc as the only answer and could never confirm the thing it asked.
 *
 * Returns 1 for every key it took and consumes it, so a caller that DOES run
 * the frame does not also walk the focus ring on the same Tab.
 */
int ktui_modal_event(KtuiEvent *ev)
{
	if (!md.active || !ev || ev->type != KT_EVT_KEY)
		return 0;

	switch (ev->key) {
	case KT_K_ESC:
		modal_close();
		ktui_consume();
		return 1;
	case KT_K_ENTER:
	case ' ': {
		/* Read before the close: closing hands the focus ring back
		 * and the callback is free to raise the next dialog. */
		void (*fn)(void) = md.sel == 0 ? md.on_yes : NULL;

		modal_close();
		ktui_consume();
		if (fn)
			fn();
		return 1;
	}
	case KT_K_LEFT:
		md.sel = 0;
		break;
	case KT_K_RIGHT:
		md.sel = md.confirm ? 1 : 0;
		break;
	case KT_K_TAB:
	case KT_K_BTAB:
		md.sel = md.confirm ? !md.sel : 0;
		break;
	default:
		return 0;
	}
	ktui_consume();
	return 1;
}

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
} md;

/* A modal takes the focus ring over completely and hands it back on close,
 * so dismissing a dialog does not silently move the caret on the page. */
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
	int h = lines + 6;
	int x = (ktui_w - w) / 2, y = (ktui_h - h) / 2;

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

	int by = y + h - 2;
	if (md.confirm) {
		if (ktui_button(krect(x + w - 40, by, 18, 1), md.yes, 1, 1)) {
			void (*fn)(void) = md.on_yes;
			modal_close();
			if (fn)
				fn();
		}
		if (ktui_button(krect(x + w - 21, by, 18, 1), md.no, 1, 0))
			modal_close();
	} else {
		if (ktui_button(krect(x + w - 21, by, 18, 1), md.yes, 1, 1))
			modal_close();
	}
}

int ktui_modal_event(KtuiEvent *ev)
{
	if (!md.active)
		return 0;
	if (ev->type == KT_EVT_KEY && ev->key == KT_K_ESC) {
		modal_close();
		return 1;
	}
	return 0;
}

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
 * The page registry, the sidebar, and what the frame does when it is narrow.
 *
 * DEGRADATION IS BY WIDTH AND NOTHING VANISHES WITHOUT A WAY BACK. The
 * sidebar is the first thing to go and `[`/`]` still cycle; below that F1
 * lists the pages as a modal. A control that disappears with no replacement
 * is how a narrow window becomes a window with no way out of the page it
 * opened on.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

ResState R;

static int g_page;
static int g_focus_sidebar;
static int g_modal;		/* the F1 page list, for the narrow band  */
static int g_hover = -1;

/* Where the sidebar ended, so a click knows which side it landed on. */
static int g_side_w;
static int g_top;
static int g_body_x, g_body_y, g_body_w, g_body_h;

int res_page_index(const char *id)
{
	for (int i = 0; i < RP_NPAGES; i++)
		if (!strcmp(RES_PAGES[i].id, id))
			return i;
	return -1;
}

void res_page_set(int idx)
{
	if (idx >= 0 && idx < RP_NPAGES)
		g_page = idx;
}

int res_page_current(void) { return g_page; }

/*
 * The three bands. Measured in CELLS, because that is the unit the whole
 * program is in — a pixel width would mean two answers on two font sizes.
 */
enum { RES_WIDE = 100, RES_NARROW = 60 };
enum { SIDE_WIDE = 18, SIDE_ICON = 6 };

static int sidebar_width(int w)
{
	if (w >= RES_WIDE)
		return SIDE_WIDE;
	if (w >= RES_NARROW)
		return SIDE_ICON;
	return 0;
}

static void draw_sidebar(int w, int top, int h)
{
	g_side_w = sidebar_width(w);
	if (!g_side_w)
		return;

	ktui_draw_fill(krect(0, top, g_side_w, h - top), KT_SURFACE);

	for (int i = 0; i < RP_NPAGES && top + i < h; i++) {
		const ResPage *p = &RES_PAGES[i];
		int sel = (i == g_page);
		int hov = (i == g_hover);
		/*
		 * A FILL, never KT_A_REVERSE over the label: the attribute
		 * inverts only the cells the glyphs cover, so a two-word name
		 * comes out as one lit block per word.
		 */
		int bg = sel ? KT_ACCENT : hov ? KT_MID : KT_SURFACE;
		int fg = sel ? KT_SURFACE : KT_TEXT;
		ktui_draw_fill(krect(0, top + i, g_side_w, 1), bg);

		if (g_side_w >= SIDE_WIDE) {
			ktui_draw_text(2, top + i, g_side_w - 3, p->name,
				       fg, bg, 0);
		} else {
			/* The initial, highlighted — the whole name does not
			 * fit and a truncation would read as a different
			 * page. */
			char ini[2] = { p->name[0], 0 };
			ktui_draw_text(2, top + i, 1, ini, fg, bg, 0);
		}
	}
}

/* The F1 modal: the page list, for the band with no sidebar. */
static void draw_modal(int w, int h)
{
	int mw = 28, mh = RP_NPAGES + 2;
	if (mw > w - 2)
		mw = w - 2;
	if (mh > h)
		mh = h;
	int x = (w - mw) / 2, y = (h - mh) / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	ktui_draw_fill(krect(x, y, mw, mh), KT_SURFACE);
	ktui_draw_box(krect(x, y, mw, mh), "Pages", KT_ACCENT, KT_SURFACE, 1);
	for (int i = 0; i < RP_NPAGES && i + 1 < mh - 1; i++) {
		int sel = (i == g_page);
		if (sel)
			ktui_draw_fill(krect(x + 1, y + 1 + i, mw - 2, 1),
				       KT_ACCENT);
		ktui_draw_text(x + 2, y + 1 + i, mw - 4, RES_PAGES[i].name,
			       sel ? KT_SURFACE : KT_TEXT,
			       sel ? KT_ACCENT : KT_SURFACE, 0);
	}
}

void res_draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	if (w <= 0 || h <= 0)
		return;

	ktui_draw_clear();

	/*
	 * The band spans the WHOLE window and the sidebar starts under it.
	 * libkchrome draws from column 0 of the surface it is given, so a band
	 * drawn by the page — which starts at the body's origin — would paint
	 * over the sidebar beside it.
	 */
	const ResPage *pg = &RES_PAGES[g_page];
	if (pg->prepare)
		pg->prepare();
	const char *sub = pg->headline ? pg->headline() : NULL;
	g_top = kch_header(w, RC.icons ? pg->icon : NULL, pg->name, sub,
			   RC.icons);
	if (g_top < 1 || g_top >= h)
		g_top = h > 1 ? 1 : 0;

	draw_sidebar(w, g_top, h);

	g_body_x = g_side_w ? g_side_w + 1 : 0;
	g_body_y = g_top;
	g_body_w = w - g_body_x;
	g_body_h = h - g_top;
	if (g_body_w < 1)
		g_body_w = 1;

	if (g_side_w)
		ktui_draw_vline(g_side_w, g_top, h - g_top, KT_G_VL, KT_DIM,
				KT_BG);

	if (RES_PAGES[g_page].draw)
		RES_PAGES[g_page].draw(g_body_x, g_body_y, g_body_w, g_body_h);

	if (g_modal)
		draw_modal(w, h);
}

int res_frame_key(int k)
{
	if (g_modal) {
		if (k == KT_K_ESC || k == KT_K_F1) {
			g_modal = 0;
			return 0;
		}
		if (k == KT_K_UP && g_page > 0)
			g_page--;
		else if (k == KT_K_DOWN && g_page < RP_NPAGES - 1)
			g_page++;
		else if (k == '\n' || k == '\r')
			g_modal = 0;
		return 0;
	}

	switch (k) {
	case KT_K_F1:
		g_modal = 1;
		return 0;
	case '[':
		g_page = (g_page + RP_NPAGES - 1) % RP_NPAGES;
		return 0;
	case ']':
		g_page = (g_page + 1) % RP_NPAGES;
		return 0;
	case '\t':
		g_focus_sidebar = !g_focus_sidebar;
		return 0;
	case KT_K_ESC:
		/*
		 * Steps BACK one level and only then closes: a page that
		 * opened a detail view must not exit the program on the key
		 * that means "go back" everywhere else.
		 */
		if (RES_PAGES[g_page].key && RES_PAGES[g_page].key(k))
			return 0;
		return 1;
	case 'q':
		return 1;
	}

	if (g_focus_sidebar) {
		if (k == KT_K_UP && g_page > 0) {
			g_page--;
			return 0;
		}
		if (k == KT_K_DOWN && g_page < RP_NPAGES - 1) {
			g_page++;
			return 0;
		}
	}

	if (RES_PAGES[g_page].key)
		RES_PAGES[g_page].key(k);
	return 0;
}

int res_frame_click(int mx, int my, int btn)
{
	if (g_modal) {
		int mh = RP_NPAGES + 2, y = (ktui_h - mh) / 2;
		int i = my - (y < 0 ? 0 : y) - 1;
		if (i >= 0 && i < RP_NPAGES)
			g_page = i;
		g_modal = 0;
		return 0;
	}
	if (g_side_w && mx < g_side_w) {
		int i = my - g_top;
		if (i >= 0 && i < RP_NPAGES) {
			g_page = i;
			g_focus_sidebar = 1;
		}
		return 0;
	}
	if (RES_PAGES[g_page].click)
		RES_PAGES[g_page].click(mx, my, btn);
	return 0;
}

void res_frame_motion(int mx, int my)
{
	int i = my - g_top;
	g_hover = (g_side_w && mx < g_side_w && i >= 0 && i < RP_NPAGES)
		  ? i : -1;
}

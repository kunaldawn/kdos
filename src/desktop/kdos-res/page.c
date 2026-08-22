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

#include "kwl.h"
#include "res.h"

ResState R;

static int g_page;
static int g_focus_sidebar;
static int g_modal;		/* the F1 page list, for the narrow band  */

/*
 * THE ONE CONFIRM MODAL. Every verb that ends or renices somebody else's work
 * goes through it — a process row, a whole application's worth of them — and
 * it is one implementation because two would eventually disagree about what
 * they were about to do. It NAMES the subject and the action rather than
 * asking "are you sure": a dialog that does not say what it will do is one
 * people learn to dismiss without reading.
 */
static struct {
	int active;
	int sel;		/* 0 = the verb, 1 = Cancel               */
	char title[48], msg[224], yes[24];
	void (*on_yes)(void);
	KRect r_yes, r_no;
} g_conf;
static int g_hover = -1;

/* Where the sidebar ended, so a click knows which side it landed on. */
static int g_side_w;
static int g_top;
static int g_body_x, g_body_y, g_body_w, g_body_h;
/*
 * The frame's own inside — the first column and the row count the box left
 * for everything else. Recorded from what was DRAWN, because a hit test that
 * derives an origin a second time is a hit test that disagrees with the
 * picture the frame after a resize.
 */
static int g_in_x, g_in_h;

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

void res_confirm(const char *title, const char *msg, const char *yes,
		 void (*on_yes)(void))
{
	memset(&g_conf, 0, sizeof(g_conf));
	g_conf.active = 1;
	/* Cancel is preselected. The destructive button being under the caret
	 * when the dialog opens turns a reflex Enter into a kill. */
	g_conf.sel = 1;
	kb_strlcpy(g_conf.title, title, sizeof(g_conf.title));
	kb_strlcpy(g_conf.msg, msg, sizeof(g_conf.msg));
	kb_strlcpy(g_conf.yes, yes, sizeof(g_conf.yes));
	g_conf.on_yes = on_yes;
}

int res_confirm_active(void) { return g_conf.active; }

static void confirm_take(int yes)
{
	void (*fn)(void) = g_conf.on_yes;

	g_conf.active = 0;
	g_conf.on_yes = NULL;
	if (yes && fn)
		fn();
}

/* A centred label on a filled rect. libktui's ktui_button() belongs to the
 * immediate-mode frame protocol, which this program does not drive. */
static void button(KRect r, const char *label, int on, int hue)
{
	int len = (int)strlen(label);
	int off = (r.w - len) / 2;

	if (off < 0)
		off = 0;
	ktui_draw_fill(r, on ? hue : KT_DIM);
	ktui_draw_text(r.x + off, r.y, r.w - off, label,
		       on ? KT_SURFACE : KT_TEXT, on ? hue : KT_DIM, 0);
}

static void draw_confirm(int w, int h)
{
	int mw = 56;
	if (mw > w - 4)
		mw = w - 4;
	if (mw < 20)
		mw = 20;

	/* The message is wrapped on WORDS into the dialog's own width: a
	 * subject is a process name and a box name, and clipping it is
	 * clipping the half that says which one. */
	char lines[4][224];
	int nl = 0;
	const char *p = g_conf.msg;
	int avail = mw - 6;
	if (avail < 8)
		avail = 8;
	while (*p && nl < 4) {
		int take = 0, last = 0;
		while (p[take] && take < avail) {
			if (p[take] == ' ')
				last = take;
			take++;
		}
		if (p[take] && last)
			take = last;
		if (take > (int)sizeof(lines[0]) - 1)
			take = (int)sizeof(lines[0]) - 1;
		memcpy(lines[nl], p, (size_t)take);
		lines[nl][take] = 0;
		nl++;
		p += take;
		while (*p == ' ')
			p++;
	}

	int mh = nl + 4;
	if (mh > h)
		mh = h;
	int x = (w - mw) / 2, y = (h - mh) / 2;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	KRect r = krect(x, y, mw, mh);
	ktui_draw_fill(r, KT_SURFACE);
	/* WARN, not the accent: this is the one dialog in the program whose
	 * yes does something that cannot be undone. */
	ktui_draw_box(r, g_conf.title, KT_WARN, KT_SURFACE, 1);
	for (int i = 0; i < nl && y + 1 + i < y + mh - 2; i++)
		ktui_draw_text(x + 2, y + 1 + i, mw - 4, lines[i], KT_TEXT,
			       KT_SURFACE, 0);

	int bw = (mw - 5) / 2;
	if (bw < 6)
		bw = 6;
	int by = y + mh - 2;
	g_conf.r_yes = krect(x + 2, by, bw, 1);
	g_conf.r_no = krect(x + mw - 2 - bw, by, bw, 1);

	button(g_conf.r_yes, g_conf.yes, g_conf.sel == 0, KT_WARN);
	button(g_conf.r_no, "Cancel", g_conf.sel == 1, KT_ACCENT);
}

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

	ktui_draw_fill(krect(g_in_x, top, g_side_w, h - top), KT_SURFACE);

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
		ktui_draw_fill(krect(g_in_x, top + i, g_side_w, 1), bg);

		if (g_side_w >= SIDE_WIDE) {
			ktui_draw_text(g_in_x + 1, top + i, g_side_w - 2,
				       p->name, fg, bg, 0);
		} else {
			/* The initial, highlighted — the whole name does not
			 * fit and a truncation would read as a different
			 * page. */
			char ini[2] = { p->name[0], 0 };
			ktui_draw_text(g_in_x + 1, top + i, 1, ini, fg, bg, 0);
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

/*
 * THE FRAME IS THE SAME BOX EVERY OTHER KDOS SURFACE DRAWS.
 *
 * kdos-net, kdos-start, kdos-settings, the launcher and the chooser all put a
 * double-line box round themselves and hang their title on its top edge; this
 * program drew a header band floating on bare background with a one-column
 * margin either side, which is what `kch_header` leaves ROOM for — it starts
 * at column 1 precisely because it expects a frame there. So kdos-res was the
 * one KDOS window with the gap and nothing in it, and beside every other
 * surface on the desktop it read as somebody else's program.
 *
 * The box takes column 0, column w-1 and row h-1; the header band already
 * fits between them, and the body gives up its last row to the bottom edge.
 */
static void frame_inside(int w, int h, int *in_x, int *in_w, int *in_h)
{
	/* A window too small for a frame keeps the whole surface: a box drawn
	 * at 6x4 is a box with nothing inside it. */
	if (w < 12 || h < 8) {
		*in_x = 0;
		*in_w = w;
		*in_h = h;
		return;
	}
	/*
	 * AND THE FRAME IS DRAWN BY WHOEVER OWNS IT. In a window the
	 * compositor is already drawing `════ Resources ════[_][=][X]` round
	 * the outside, so a box here would be a second frame inside the first
	 * with the title written twice. On tty1 and in `--dump` nothing else
	 * is drawing one, and then this is the only frame there is — which is
	 * why the goldens still show it.
	 */
	if (kwl_decorated()) {
		*in_x = 1;
		*in_w = w - 2;
		*in_h = h - 1;
		return;
	}
	ktui_draw_box(krect(0, 0, w, h), " Resources ", KT_ACCENT, KT_BG, 1);
	*in_x = 1;
	*in_w = w - 2;
	*in_h = h - 1;
}

void res_draw_frame(void)
{
	int w = ktui_w, h = ktui_h;
	if (w <= 0 || h <= 0)
		return;

	ktui_draw_clear();

	int in_w;
	frame_inside(w, h, &g_in_x, &in_w, &g_in_h);

	/*
	 * The band spans the WHOLE window and the sidebar starts under it.
	 * libkchrome draws from column 0 of the surface it is given, so a band
	 * drawn by the page — which starts at the body's origin — would paint
	 * over the sidebar beside it.
	 */
	/*
	 * A DETAIL VIEW IS THE WHOLE FRAME, sidebar included. It is one
	 * subject, so a page list beside it would be offering to leave
	 * without saying so; Esc is the way back and the band says whose
	 * page this is.
	 */
	if (res_detail_active()) {
		int top = kch_header(w, NULL, res_detail_title(),
				     res_detail_subtitle(), 0);
		if (top < 1 || top >= g_in_h)
			top = g_in_h > 1 ? 1 : 0;
		g_side_w = 0;
		g_body_x = g_in_x;
		g_body_y = top;
		g_body_w = in_w;
		g_body_h = g_in_h - top;
		res_detail_draw(g_body_x, g_body_y, g_body_w, g_body_h);
		if (g_conf.active)
			draw_confirm(w, h);
		return;
	}

	const ResPage *pg = &RES_PAGES[g_page];
	if (pg->prepare)
		pg->prepare();
	const char *sub = pg->headline ? pg->headline() : NULL;
	g_top = kch_header(w, RC.icons ? pg->icon : NULL, pg->name, sub,
			   RC.icons);
	if (g_top < 1 || g_top >= g_in_h)
		g_top = g_in_h > 1 ? 1 : 0;

	draw_sidebar(w, g_top, g_in_h);

	g_body_x = g_in_x + (g_side_w ? g_side_w + 1 : 0);
	g_body_y = g_top;
	g_body_w = g_in_x + in_w - g_body_x;
	g_body_h = g_in_h - g_top;
	if (g_body_w < 1)
		g_body_w = 1;
	if (g_body_h < 1)
		g_body_h = 1;

	if (g_side_w)
		ktui_draw_vline(g_in_x + g_side_w, g_top, g_in_h - g_top,
				KT_G_VL, KT_DIM, KT_BG);

	if (RES_PAGES[g_page].draw)
		RES_PAGES[g_page].draw(g_body_x, g_body_y, g_body_w, g_body_h);

	if (g_modal)
		draw_modal(w, h);
	/* Last, and over everything: a dialog under the page it belongs to is
	 * a dialog nobody can read. */
	if (g_conf.active)
		draw_confirm(w, h);
}

int res_frame_key(int k)
{
	/*
	 * The confirm owns the keyboard while it is up, and it NEVER returns
	 * 1: `q` and Esc mean "not that" here, not "leave the program". A
	 * dialog whose cancel key also quits is one that loses the answer.
	 */
	if (g_conf.active) {
		switch (k) {
		case KT_K_LEFT:
		case KT_K_RIGHT:
		case '\t':
			g_conf.sel = !g_conf.sel;
			break;
		case '\n':
		case '\r':
			confirm_take(g_conf.sel == 0);
			break;
		case 'y':
		case 'Y':
			confirm_take(1);
			break;
		case KT_K_ESC:
		case 'n':
		case 'N':
		case 'q':
			confirm_take(0);
			break;
		}
		return 0;
	}

	if (res_detail_active()) {
		/* Esc steps back to the list rather than out of the program,
		 * which is the rule the comment on the page-level Esc has
		 * always described. */
		if (res_detail_key(k))
			return 0;
		if (k == KT_K_ESC) {
			res_detail_close();
			return 0;
		}
		if (k == 'q')
			return 1;
		return 0;
	}

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

/*
 * The wheel, before anything else looks at the pointer.
 *
 * It reaches the loop as a PRESS, so every test below that asks "what is under
 * the cursor" would answer a notch the way it answers a click: over the
 * sidebar that is a page change, over a list it is a selection. Neither is
 * scrolling, and scrolling is the only thing a wheel means.
 */
int res_frame_wheel(int up)
{
	if (g_conf.active || g_modal)
		return 0;
	if (res_detail_active())
		return res_detail_wheel(up);
	if (RES_PAGES[g_page].wheel)
		return RES_PAGES[g_page].wheel(up);
	return 0;
}

static int in_rect(KRect r, int mx, int my)
{
	return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
}

int res_frame_click(int mx, int my, int btn)
{
	if (g_conf.active) {
		if (in_rect(g_conf.r_yes, mx, my))
			confirm_take(1);
		else if (in_rect(g_conf.r_no, mx, my))
			confirm_take(0);
		/* A click anywhere else is neither answer and must not fall
		 * through to the page underneath, which is still drawn. */
		return 0;
	}
	if (res_detail_active())
		return res_detail_click(mx, my, btn);
	if (g_modal) {
		int mh = RP_NPAGES + 2, y = (ktui_h - mh) / 2;
		int i = my - (y < 0 ? 0 : y) - 1;
		if (i >= 0 && i < RP_NPAGES)
			g_page = i;
		g_modal = 0;
		return 0;
	}
	if (g_side_w && mx >= g_in_x && mx < g_in_x + g_side_w) {
		int i = my - g_top;
		if (i >= 0 && i < RP_NPAGES) {
			g_page = i;
			g_focus_sidebar = 1;
		}
		return 0;
	}
	/*
	 * A PAGE IS HANDED ITS OWN COORDINATES, not the screen's.
	 *
	 * Every page draws from the origin the frame gives it and knows its
	 * own row 0 is the column header, so handing it a screen row means
	 * each page subtracting an origin the frame already knows — five
	 * copies of one arithmetic, four of which are wrong the day the frame
	 * grows a border. Which is exactly what happened: `res_procs_click`
	 * tested `my <= 0` for the header row on a page whose first row was
	 * four.
	 */
	if (RES_PAGES[g_page].click)
		RES_PAGES[g_page].click(mx - g_body_x, my - g_body_y, btn);
	return 0;
}

void res_frame_release(void)
{
	if (RES_PAGES[g_page].release)
		RES_PAGES[g_page].release();
}

void res_frame_motion(int mx, int my)
{
	int i = my - g_top;

	g_hover = (g_side_w && mx >= g_in_x && mx < g_in_x + g_side_w &&
		   i >= 0 && i < RP_NPAGES) ? i : -1;
	if (g_conf.active || g_modal || res_detail_active())
		return;
	if (RES_PAGES[g_page].motion)
		RES_PAGES[g_page].motion(mx - g_body_x, my - g_body_y);
}

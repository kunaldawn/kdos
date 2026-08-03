/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — immediate-mode controls
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kinstall.h"

Ui ui;

#define MAX_HITS 512
#define MAX_IDS 512

typedef struct {
	Rect r;
	int id;
} Hit;

static Hit hits_a[MAX_HITS], hits_b[MAX_HITS];
static int na, nb;
static Hit *cur_hits = hits_a, *prev_hits = hits_b;
static int *cur_n = &na, *prev_n = &nb;

static int caret[MAX_IDS];	/* text cursor, per control id             */
static double last_click_t;
static int last_click_id = -1;

/* ──────────────────────────────────────────────────────────────────────── */

void ui_frame_begin(Event *ev)
{
	ui.ev = *ev;
	ui.consumed = 0;
	ui.clicked = -1;
	ui.dblclick = 0;
	ui.wheel = 0;
	ui.wheel_id = -1;
	ui.nfocus = 0;
	ui.maxid = 0;
	ui.hover = -1;
	ui.focus_seen = 0;

	if (ev->type == EVT_MOUSE) {
		ui.mx = ev->mx;
		ui.my = ev->my;
		for (int i = *prev_n - 1; i >= 0; i--) {
			if (!rect_hit(prev_hits[i].r, ev->mx, ev->my))
				continue;
			if (ev->btn == MB_WHEEL_UP || ev->btn == MB_WHEEL_DOWN) {
				ui.wheel = ev->btn == MB_WHEEL_UP ? -1 : 1;
				ui.wheel_id = prev_hits[i].id;
			} else if (ev->btn == MB_LEFT && ev->press == MP_PRESS) {
				ui.clicked = prev_hits[i].id;
				/* Chrome ids are not focus ids — clicking the
				 * sidebar must not throw the caret at whatever
				 * control happens to sit last on the page. */
				if (prev_hits[i].id < UI_ID_SIDEBAR)
					ui.focus = prev_hits[i].id;
				double t = now_s();
				if (last_click_id == ui.clicked && t - last_click_t < 0.4)
					ui.dblclick = 1;
				last_click_id = ui.clicked;
				last_click_t = t;
			}
			break;
		}
	}

	Hit *t = cur_hits;
	cur_hits = prev_hits;
	prev_hits = t;
	int *tn = cur_n;
	cur_n = prev_n;
	prev_n = tn;
	*cur_n = 0;
}

void ui_frame_end(void)
{
	if (ui.ev.type == EVT_KEY && !ui.consumed) {
		if (ui.ev.key == K_TAB && !(ui.ev.mods & MOD_SHIFT)) {
			ui_focus_next(1);
			ui.consumed = 1;
		} else if (ui.ev.key == K_BTAB ||
			   (ui.ev.key == K_TAB && (ui.ev.mods & MOD_SHIFT))) {
			ui_focus_next(-1);
			ui.consumed = 1;
		}
	}
	if (ui.nfocus && ui.focus >= ui.nfocus)
		ui.focus = ui.nfocus - 1;
	if (ui.focus < 0)
		ui.focus = 0;
}

int ui_id(void)
{
	int id = ui.nfocus++;
	if (id >= MAX_IDS)
		id = MAX_IDS - 1;
	return id;
}

void ui_hit(Rect r, int id)
{
	/* Remembering where the focused control ended up is what lets the
	 * page scroll itself to follow the Tab key. */
	if (id == ui.focus && id < UI_ID_SIDEBAR) {
		ui.focus_rect = r;
		ui.focus_seen = 1;
	}
	if (*cur_n >= MAX_HITS)
		return;
	cur_hits[*cur_n].r = r;
	cur_hits[*cur_n].id = id;
	(*cur_n)++;
}

int ui_focused(int id)
{
	return ui.focus == id;
}

void ui_focus_set(int id)
{
	ui.focus = id;
}

void ui_focus_next(int dir)
{
	if (!ui.nfocus)
		return;
	ui.focus = (ui.focus + dir + ui.nfocus) % ui.nfocus;
}

int ui_key(int k)
{
	if (ui.consumed || ui.ev.type != EVT_KEY || ui.ev.key != k)
		return 0;
	ui.consumed = 1;
	return 1;
}

int ui_activated(int id, Rect r)
{
	ui_hit(r, id);
	if (ui.clicked == id)
		return 1;
	if (ui.focus == id && !ui.consumed && ui.ev.type == EVT_KEY &&
	    (ui.ev.key == K_ENTER || ui.ev.key == ' ')) {
		ui.consumed = 1;
		return 1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int ui_button(Rect r, const char *label, int enabled, int primary)
{
	/* A disabled control claims no focus id, so Tab never parks on a Back
	 * button that cannot go anywhere. Enablement only flips on a page
	 * change, which resets focus anyway, so ids stay stable within a page. */
	int id = enabled ? ui_id() : -1;
	int focus = enabled && ui_focused(id);
	int hover = enabled && rect_hit(r, ui.mx, ui.my);

	int fg, bg;
	if (!enabled) {
		fg = CL_DIM;
		bg = CL_SURFACE;
	} else if (focus) {
		fg = CL_BG;
		bg = primary ? CL_ACCENT : CL_MID;
	} else if (primary) {
		fg = CL_ACCENT;
		bg = CL_SURFACE;
	} else {
		fg = hover ? CL_TEXT : CL_MID;
		bg = CL_SURFACE;
	}

	draw_fill(r, bg);
	int w = utf8_width(label);
	int x = r.x + (r.w - w) / 2;
	if (x < r.x)
		x = r.x;
	int cy = r.y + r.h / 2;
	draw_text(x, cy, r.w, label, fg, bg, 0);

	/* The focused button carries brackets rather than only a colour — on a
	 * washed-out laptop panel colour alone is not a focus indicator. */
	if (focus && r.w > 4) {
		draw_text(r.x, cy, 1, ki_glyph[G_RIGHT], fg, bg, 0);
		draw_text(r.x + r.w - 1, cy, 1, ki_glyph[G_LEFT], fg, bg, 0);
	}

	return enabled ? ui_activated(id, r) : 0;
}

int ui_check(int x, int y, int w, const char *label, int *val)
{
	int id = ui_id();
	Rect r = rect(x, y, w, 1);
	int focus = ui_focused(id);
	int fg = focus ? CL_BG : CL_TEXT;
	int bg = focus ? CL_ACCENT : CL_BG;

	draw_fill(r, bg);
	draw_text(x, y, 1, "[", focus ? CL_BG : CL_MID, bg, 0);
	draw_text(x + 1, y, 1, *val ? ki_glyph[G_SQUARE] : " ",
		  focus ? CL_BG : CL_ACCENT, bg, 0);
	draw_text(x + 2, y, 1, "]", focus ? CL_BG : CL_MID, bg, 0);
	draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	if (ui_activated(id, r)) {
		*val = !*val;
		return 1;
	}
	return 0;
}

int ui_radio(int x, int y, int w, const char *label, int *val, int on)
{
	int id = ui_id();
	Rect r = rect(x, y, w, 1);
	int focus = ui_focused(id);
	int sel = (*val == on);
	int fg = focus ? CL_BG : sel ? CL_TEXT : CL_MID;
	int bg = focus ? CL_ACCENT : CL_BG;

	draw_fill(r, bg);
	draw_text(x, y, 1, "(", focus ? CL_BG : CL_MID, bg, 0);
	draw_text(x + 1, y, 1, sel ? ki_glyph[G_BULLET] : " ",
		  focus ? CL_BG : CL_ACCENT, bg, 0);
	draw_text(x + 2, y, 1, ")", focus ? CL_BG : CL_MID, bg, 0);
	draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	if (ui_activated(id, r)) {
		*val = on;
		return 1;
	}
	return 0;
}

int ui_input(Rect r, char *buf, size_t cap, int secret, const char *placeholder)
{
	int id = ui_id();
	int focus = ui_focused(id);
	int len = (int)strlen(buf);
	int *cur = &caret[id];
	int changed = 0;

	if (*cur > len)
		*cur = len;

	if (focus && !ui.consumed && ui.ev.type == EVT_KEY) {
		int k = ui.ev.key;
		if (k == K_LEFT) {
			if (*cur > 0)
				(*cur)--;
			ui.consumed = 1;
		} else if (k == K_RIGHT) {
			if (*cur < len)
				(*cur)++;
			ui.consumed = 1;
		} else if (k == K_HOME) {
			*cur = 0;
			ui.consumed = 1;
		} else if (k == K_END) {
			*cur = len;
			ui.consumed = 1;
		} else if (k == K_BACKSPACE) {
			if (*cur > 0) {
				memmove(buf + *cur - 1, buf + *cur,
					(size_t)(len - *cur + 1));
				(*cur)--;
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == K_DEL) {
			if (*cur < len) {
				memmove(buf + *cur, buf + *cur + 1,
					(size_t)(len - *cur));
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == ('u' - 'a' + 1) && (ui.ev.mods & MOD_CTRL)) {
			buf[0] = 0;
			*cur = 0;
			changed = 1;
			ui.consumed = 1;
		} else if (k >= 0x20 && k < 0x7f && (size_t)len + 1 < cap) {
			memmove(buf + *cur + 1, buf + *cur, (size_t)(len - *cur + 1));
			buf[*cur] = (char)k;
			(*cur)++;
			changed = 1;
			ui.consumed = 1;
		}
		len = (int)strlen(buf);
	}

	int bg = CL_SURFACE;
	int fg = focus ? CL_TEXT : CL_MID;
	draw_fill(r, bg);
	draw_text(r.x, r.y, 1, focus ? ki_glyph[G_RIGHT] : " ",
		  focus ? CL_ACCENT : CL_DIM, bg, 0);

	int field = r.w - 2;
	int scroll = 0;
	if (*cur > field - 1)
		scroll = *cur - field + 1;

	if (!len && placeholder && !focus) {
		draw_text(r.x + 2, r.y, field, placeholder, CL_DIM, bg, 0);
	} else if (secret) {
		for (int i = 0; i < len - scroll && i < field; i++)
			draw_text(r.x + 2 + i, r.y, 1, ki_glyph[G_BULLET], fg, bg, 0);
	} else {
		draw_text(r.x + 2, r.y, field, buf + scroll, fg, bg, 0);
	}

	if (focus) {
		int cx = r.x + 2 + (*cur - scroll);
		if (cx < r.x + r.w) {
			uint32_t under = ' ';
			int idx = *cur;
			if (idx < len)
				under = secret ? (uint32_t)0x2022
					       : (uint32_t)(unsigned char)buf[idx];
			draw_cell(cx, r.y, under, CL_BG, CL_ACCENT, 0);
		}
	}

	ui_hit(r, id);
	if (ui.clicked == id) {
		int p = ui.mx - (r.x + 2) + scroll;
		if (p < 0)
			p = 0;
		if (p > len)
			p = len;
		*cur = p;
	}
	return changed;
}

void ui_progress(Rect r, double frac, const char *label)
{
	if (r.w < 4)
		return;
	if (frac < 0) {
		/* Indeterminate: a scanning cell rather than a filled bar, so
		 * nobody reads a stalled percentage into it. */
		draw_hline(r.x, r.y, r.w, G_SHADE, CL_DIM, CL_BG);
		int p = (int)(now_s() * 12) % (r.w * 2);
		if (p >= r.w)
			p = r.w * 2 - p - 1;
		for (int i = 0; i < 3 && p + i < r.w; i++)
			draw_text(r.x + p + i, r.y, 1, ki_glyph[G_FULL],
				  CL_ACCENT, CL_BG, 0);
	} else {
		if (frac > 1)
			frac = 1;
		int fill = (int)(frac * r.w + 0.5);
		for (int i = 0; i < r.w; i++)
			draw_text(r.x + i, r.y, 1,
				  i < fill ? ki_glyph[G_FULL] : ki_glyph[G_SHADE],
				  i < fill ? CL_ACCENT : CL_DIM, CL_BG, 0);
	}

	if (label && *label) {
		int w = utf8_width(label);
		int x = r.x + (r.w - w) / 2;
		draw_text(x, r.y, w, label, CL_BG, CL_ACCENT, A_REVERSE);
	}
}

void ui_scrollbar(Rect r, int total, int shown, int off)
{
	if (r.h < 2)
		return;
	if (total <= shown) {
		draw_vline(r.x, r.y, r.h, G_VL, CL_DIM, CL_BG);
		return;
	}
	int th = r.h * shown / total;
	if (th < 1)
		th = 1;
	int ty = (r.h - th) * off / (total - shown);
	for (int i = 0; i < r.h; i++)
		draw_text(r.x, r.y + i, 1,
			  (i >= ty && i < ty + th) ? ki_glyph[G_FULL]
						   : ki_glyph[G_VL],
			  (i >= ty && i < ty + th) ? CL_MID : CL_DIM, CL_BG, 0);
}

int ui_list(Rect r, ListState *st, int count, ListRow row, void *user, int id)
{
	int focus = ui_focused(id);
	int chosen = 0;
	int vis = r.h;

	if (st->sel >= count)
		st->sel = count - 1;
	if (st->sel < 0)
		st->sel = 0;

	if (focus && !ui.consumed && ui.ev.type == EVT_KEY) {
		int k = ui.ev.key;
		if (k == K_UP) {
			st->sel--;
			ui.consumed = 1;
		} else if (k == K_DOWN) {
			st->sel++;
			ui.consumed = 1;
		} else if (k == K_PGUP) {
			st->sel -= vis;
			ui.consumed = 1;
		} else if (k == K_PGDN) {
			st->sel += vis;
			ui.consumed = 1;
		} else if (k == K_HOME) {
			st->sel = 0;
			ui.consumed = 1;
		} else if (k == K_END) {
			st->sel = count - 1;
			ui.consumed = 1;
		} else if (k == K_ENTER || k == ' ') {
			chosen = 1;
			ui.consumed = 1;
		}
	}

	if (ui.wheel && ui.wheel_id == id) {
		st->off += ui.wheel * 3;
		ui.wheel = 0;
	}

	if (ui.clicked == id) {
		int idx = st->off + (ui.my - r.y);
		if (idx >= 0 && idx < count) {
			st->sel = idx;
			if (ui.dblclick)
				chosen = 1;
		}
	}

	if (st->sel < 0)
		st->sel = 0;
	if (st->sel >= count)
		st->sel = count ? count - 1 : 0;

	int maxoff = count - vis;
	if (maxoff < 0)
		maxoff = 0;
	if (st->off > maxoff)
		st->off = maxoff;
	if (st->off < 0)
		st->off = 0;
	if (st->sel < st->off)
		st->off = st->sel;
	if (st->sel >= st->off + vis)
		st->off = st->sel - vis + 1;

	int listw = count > vis ? r.w - 1 : r.w;
	for (int i = 0; i < vis; i++) {
		int idx = st->off + i;
		int y = r.y + i;
		if (idx >= count) {
			draw_fill(rect(r.x, y, listw, 1), CL_BG);
			continue;
		}
		int sel = idx == st->sel;
		draw_fill(rect(r.x, y, listw, 1), sel ? CL_ACCENT : CL_BG);
		row(idx, r.x, y, listw, sel, focus, user);
	}
	if (count > vis)
		ui_scrollbar(rect(r.x + r.w - 1, r.y, 1, r.h), count, vis, st->off);

	ui_hit(r, id);
	return chosen;
}

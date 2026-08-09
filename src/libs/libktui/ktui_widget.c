/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — immediate-mode controls
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "ktui.h"

/* Frame state. Private: applications used to write these fields by hand —
 * `ui.consumed = 1` in two dozen places — which made every one of them a
 * caller that could not survive a change in here. */
typedef struct {
	int focus;		/* id of the focused control               */
	int nfocus;		/* focusables seen this frame              */
	int maxid;
	int clicked;		/* control id clicked this frame, -1 none  */
	int dblclick;
	int wheel;		/* -1 up, +1 down, 0 none                  */
	int wheel_id;
	KtuiEvent ev;		/* event being dispatched this frame       */
	int consumed;
	int mx, my;		/* pointer position for hover              */
	int hover;
	KRect focus_rect;	/* where the focused control landed        */
	int focus_seen;
} KtuiUi;

static KtuiUi ui;

#define MAX_HITS 512
#define MAX_IDS 512

typedef struct {
	KRect r;
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

void ktui_frame_begin(KtuiEvent *ev)
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

	if (ev->type == KT_EVT_MOUSE) {
		ui.mx = ev->mx;
		ui.my = ev->my;
		for (int i = *prev_n - 1; i >= 0; i--) {
			if (!krect_hit(prev_hits[i].r, ev->mx, ev->my))
				continue;
			if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
				ui.wheel = ev->btn == KT_MB_WHEEL_UP ? -1 : 1;
				ui.wheel_id = prev_hits[i].id;
			} else if (ev->btn == KT_MB_LEFT && ev->press == KT_MP_PRESS) {
				ui.clicked = prev_hits[i].id;
				/* Chrome ids are not focus ids — clicking the
				 * sidebar must not throw the caret at whatever
				 * control happens to sit last on the page. */
				if (prev_hits[i].id < KTUI_ID_CHROME)
					ui.focus = prev_hits[i].id;
				double t = kb_now_s();
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

void ktui_frame_end(void)
{
	if (ui.ev.type == KT_EVT_KEY && !ui.consumed) {
		if (ui.ev.key == KT_K_TAB && !(ui.ev.mods & KT_MOD_SHIFT)) {
			ktui_focus_next(1);
			ui.consumed = 1;
		} else if (ui.ev.key == KT_K_BTAB ||
			   (ui.ev.key == KT_K_TAB && (ui.ev.mods & KT_MOD_SHIFT))) {
			ktui_focus_next(-1);
			ui.consumed = 1;
		}
	}
	if (ui.nfocus && ui.focus >= ui.nfocus)
		ui.focus = ui.nfocus - 1;
	if (ui.focus < 0)
		ui.focus = 0;
}

int ktui_id(void)
{
	int id = ui.nfocus++;
	if (id >= MAX_IDS)
		id = MAX_IDS - 1;
	return id;
}

void ktui_hit(KRect r, int id)
{
	/* Remembering where the focused control ended up is what lets the
	 * page scroll itself to follow the Tab key. */
	if (id == ui.focus && id < KTUI_ID_CHROME) {
		ui.focus_rect = r;
		ui.focus_seen = 1;
	}
	if (*cur_n >= MAX_HITS)
		return;
	cur_hits[*cur_n].r = r;
	cur_hits[*cur_n].id = id;
	(*cur_n)++;
}

int ktui_focused(int id)
{
	return ui.focus == id;
}

void ktui_focus_set(int id)
{
	ui.focus = id;
}

void ktui_focus_next(int dir)
{
	if (!ui.nfocus)
		return;
	ui.focus = (ui.focus + dir + ui.nfocus) % ui.nfocus;
}

int ktui_key(int k)
{
	if (ui.consumed || ui.ev.type != KT_EVT_KEY || ui.ev.key != k)
		return 0;
	ui.consumed = 1;
	return 1;
}

int ktui_activated(int id, KRect r)
{
	ktui_hit(r, id);
	if (ui.clicked == id)
		return 1;
	if (ui.focus == id && !ui.consumed && ui.ev.type == KT_EVT_KEY &&
	    (ui.ev.key == KT_K_ENTER || ui.ev.key == ' ')) {
		ui.consumed = 1;
		return 1;
	}
	return 0;
}

/* Chrome — a sidebar, a tab bar, a close button. Registered in a reserved id
 * range so a click on one is still recognised, but it never joins the Tab
 * ring and never drags the page scroll after it. Chrome usually draws BEFORE
 * the page, so handing it real ids from ktui_id() would push every control on
 * every page N places down the ring and leave the caret parked on a
 * decoration. Ids are caller-local: each consumer numbers its own from 0. */
void ktui_hit_chrome(KRect r, int id)
{
	ktui_hit(r, KTUI_ID_CHROME + id);
}

int ktui_chrome_clicked(int id)
{
	return ui.clicked == KTUI_ID_CHROME + id;
}

/* ──────────────────────────────────────────────────────────────────────── */

const KtuiEvent *ktui_event(void)
{
	return &ui.ev;
}

int ktui_consumed(void)
{
	return ui.consumed;
}

void ktui_consume(void)
{
	ui.consumed = 1;
}

int ktui_focus_get(void)
{
	return ui.focus;
}

int ktui_clicked(void)
{
	return ui.clicked;
}

int ktui_mouse_x(void)
{
	return ui.mx;
}

int ktui_mouse_y(void)
{
	return ui.my;
}

int ktui_wheel_take(KRect r)
{
	if (!ui.wheel || ui.wheel_id >= 0 || !krect_hit(r, ui.mx, ui.my))
		return 0;
	int w = ui.wheel;
	ui.wheel = 0;
	return w;
}

int ktui_focus_rect(KRect *out)
{
	if (!ui.focus_seen)
		return 0;
	*out = ui.focus_rect;
	return 1;
}

/* ──────────────────────────────────────────────────────────────────────── */

int ktui_button(KRect r, const char *label, int enabled, int primary)
{
	/* A disabled control claims no focus id, so Tab never parks on a Back
	 * button that cannot go anywhere. Enablement only flips on a page
	 * change, which resets focus anyway, so ids stay stable within a page. */
	int id = enabled ? ktui_id() : -1;
	int focus = enabled && ktui_focused(id);
	int hover = enabled && krect_hit(r, ui.mx, ui.my);

	int fg, bg;
	if (!enabled) {
		fg = KT_DIM;
		bg = KT_SURFACE;
	} else if (focus) {
		fg = KT_BG;
		bg = primary ? KT_ACCENT : KT_MID;
	} else if (primary) {
		fg = KT_ACCENT;
		bg = KT_SURFACE;
	} else {
		fg = hover ? KT_TEXT : KT_MID;
		bg = KT_SURFACE;
	}

	ktui_draw_fill(r, bg);
	int w = ktui_utf8_width(label);
	int x = r.x + (r.w - w) / 2;
	if (x < r.x)
		x = r.x;
	int cy = r.y + r.h / 2;
	ktui_draw_text(x, cy, r.w, label, fg, bg, 0);

	/* The focused button carries brackets rather than only a colour — on a
	 * washed-out laptop panel colour alone is not a focus indicator. */
	if (focus && r.w > 4) {
		ktui_draw_text(r.x, cy, 1, ktui_glyph[KT_G_RIGHT], fg, bg, 0);
		ktui_draw_text(r.x + r.w - 1, cy, 1, ktui_glyph[KT_G_LEFT], fg, bg, 0);
	}

	return enabled ? ktui_activated(id, r) : 0;
}

int ktui_check(int x, int y, int w, const char *label, int *val)
{
	int id = ktui_id();
	KRect r = krect(x, y, w, 1);
	int focus = ktui_focused(id);
	int fg = focus ? KT_BG : KT_TEXT;
	int bg = focus ? KT_ACCENT : KT_BG;

	ktui_draw_fill(r, bg);
	ktui_draw_text(x, y, 1, "[", focus ? KT_BG : KT_MID, bg, 0);
	ktui_draw_text(x + 1, y, 1, *val ? ktui_glyph[KT_G_SQUARE] : " ",
		  focus ? KT_BG : KT_ACCENT, bg, 0);
	ktui_draw_text(x + 2, y, 1, "]", focus ? KT_BG : KT_MID, bg, 0);
	ktui_draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	if (ktui_activated(id, r)) {
		*val = !*val;
		return 1;
	}
	return 0;
}

int ktui_radio(int x, int y, int w, const char *label, int *val, int on)
{
	int id = ktui_id();
	KRect r = krect(x, y, w, 1);
	int focus = ktui_focused(id);
	int sel = (*val == on);
	int fg = focus ? KT_BG : sel ? KT_TEXT : KT_MID;
	int bg = focus ? KT_ACCENT : KT_BG;

	ktui_draw_fill(r, bg);
	ktui_draw_text(x, y, 1, "(", focus ? KT_BG : KT_MID, bg, 0);
	ktui_draw_text(x + 1, y, 1, sel ? ktui_glyph[KT_G_BULLET] : " ",
		  focus ? KT_BG : KT_ACCENT, bg, 0);
	ktui_draw_text(x + 2, y, 1, ")", focus ? KT_BG : KT_MID, bg, 0);
	ktui_draw_text(x + 4, y, w - 4, label, fg, bg, 0);

	if (ktui_activated(id, r)) {
		*val = on;
		return 1;
	}
	return 0;
}

int ktui_input(KRect r, char *buf, size_t cap, int secret, const char *placeholder)
{
	int id = ktui_id();
	int focus = ktui_focused(id);
	int len = (int)strlen(buf);
	int *cur = &caret[id];
	int changed = 0;

	if (*cur > len)
		*cur = len;

	if (focus && !ui.consumed && ui.ev.type == KT_EVT_KEY) {
		int k = ui.ev.key;
		if (k == KT_K_LEFT) {
			if (*cur > 0)
				(*cur)--;
			ui.consumed = 1;
		} else if (k == KT_K_RIGHT) {
			if (*cur < len)
				(*cur)++;
			ui.consumed = 1;
		} else if (k == KT_K_HOME) {
			*cur = 0;
			ui.consumed = 1;
		} else if (k == KT_K_END) {
			*cur = len;
			ui.consumed = 1;
		} else if (k == KT_K_BACKSPACE) {
			if (*cur > 0) {
				memmove(buf + *cur - 1, buf + *cur,
					(size_t)(len - *cur + 1));
				(*cur)--;
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == KT_K_DEL) {
			if (*cur < len) {
				memmove(buf + *cur, buf + *cur + 1,
					(size_t)(len - *cur));
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == ('u' - 'a' + 1) && (ui.ev.mods & KT_MOD_CTRL)) {
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

	int bg = KT_SURFACE;
	int fg = focus ? KT_TEXT : KT_MID;
	ktui_draw_fill(r, bg);
	ktui_draw_text(r.x, r.y, 1, focus ? ktui_glyph[KT_G_RIGHT] : " ",
		  focus ? KT_ACCENT : KT_DIM, bg, 0);

	int field = r.w - 2;
	int scroll = 0;
	if (*cur > field - 1)
		scroll = *cur - field + 1;

	if (!len && placeholder && !focus) {
		ktui_draw_text(r.x + 2, r.y, field, placeholder, KT_DIM, bg, 0);
	} else if (secret) {
		for (int i = 0; i < len - scroll && i < field; i++)
			ktui_draw_text(r.x + 2 + i, r.y, 1, ktui_glyph[KT_G_BULLET], fg, bg, 0);
	} else {
		ktui_draw_text(r.x + 2, r.y, field, buf + scroll, fg, bg, 0);
	}

	if (focus) {
		int cx = r.x + 2 + (*cur - scroll);
		if (cx < r.x + r.w) {
			uint32_t under = ' ';
			int idx = *cur;
			if (idx < len)
				under = secret ? (uint32_t)0x2022
					       : (uint32_t)(unsigned char)buf[idx];
			ktui_draw_cell(cx, r.y, under, KT_BG, KT_ACCENT, 0);
		}
	}

	ktui_hit(r, id);
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

void ktui_progress(KRect r, double frac, const char *label)
{
	if (r.w < 4)
		return;
	if (frac < 0) {
		/* Indeterminate: a scanning cell rather than a filled bar, so
		 * nobody reads a stalled percentage into it. */
		ktui_draw_hline(r.x, r.y, r.w, KT_G_SHADE, KT_DIM, KT_BG);
		int p = (int)(kb_now_s() * 12) % (r.w * 2);
		if (p >= r.w)
			p = r.w * 2 - p - 1;
		for (int i = 0; i < 3 && p + i < r.w; i++)
			ktui_draw_text(r.x + p + i, r.y, 1, ktui_glyph[KT_G_FULL],
				  KT_ACCENT, KT_BG, 0);
	} else {
		if (frac > 1)
			frac = 1;
		int fill = (int)(frac * r.w + 0.5);
		for (int i = 0; i < r.w; i++)
			ktui_draw_text(r.x + i, r.y, 1,
				  i < fill ? ktui_glyph[KT_G_FULL] : ktui_glyph[KT_G_SHADE],
				  i < fill ? KT_ACCENT : KT_DIM, KT_BG, 0);
	}

	if (label && *label) {
		int w = ktui_utf8_width(label);
		int x = r.x + (r.w - w) / 2;
		ktui_draw_text(x, r.y, w, label, KT_BG, KT_ACCENT, KT_A_REVERSE);
	}
}

void ktui_scrollbar(KRect r, int total, int shown, int off)
{
	if (r.h < 2)
		return;
	if (total <= shown) {
		ktui_draw_vline(r.x, r.y, r.h, KT_G_VL, KT_DIM, KT_BG);
		return;
	}
	int th = r.h * shown / total;
	if (th < 1)
		th = 1;
	int ty = (r.h - th) * off / (total - shown);
	for (int i = 0; i < r.h; i++)
		ktui_draw_text(r.x, r.y + i, 1,
			  (i >= ty && i < ty + th) ? ktui_glyph[KT_G_FULL]
						   : ktui_glyph[KT_G_VL],
			  (i >= ty && i < ty + th) ? KT_MID : KT_DIM, KT_BG, 0);
}

int ktui_list(KRect r, KtuiList *st, int count, KtuiListRow row, void *user, int id)
{
	int focus = ktui_focused(id);
	int chosen = 0;
	int vis = r.h;

	if (st->sel >= count)
		st->sel = count - 1;
	if (st->sel < 0)
		st->sel = 0;

	if (focus && !ui.consumed && ui.ev.type == KT_EVT_KEY) {
		int k = ui.ev.key;
		if (k == KT_K_UP) {
			st->sel--;
			ui.consumed = 1;
		} else if (k == KT_K_DOWN) {
			st->sel++;
			ui.consumed = 1;
		} else if (k == KT_K_PGUP) {
			st->sel -= vis;
			ui.consumed = 1;
		} else if (k == KT_K_PGDN) {
			st->sel += vis;
			ui.consumed = 1;
		} else if (k == KT_K_HOME) {
			st->sel = 0;
			ui.consumed = 1;
		} else if (k == KT_K_END) {
			st->sel = count - 1;
			ui.consumed = 1;
		} else if (k == KT_K_ENTER || k == ' ') {
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
			ktui_draw_fill(krect(r.x, y, listw, 1), KT_BG);
			continue;
		}
		int sel = idx == st->sel;
		ktui_draw_fill(krect(r.x, y, listw, 1), sel ? KT_ACCENT : KT_BG);
		row(idx, r.x, y, listw, sel, focus, user);
	}
	if (count > vis)
		ktui_scrollbar(krect(r.x + r.w - 1, r.y, 1, r.h), count, vis, st->off);

	ktui_hit(r, id);
	return chosen;
}

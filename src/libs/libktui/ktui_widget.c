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
	/* Press capture: the id whose rect saw the left press owns every
	 * KT_MP_DRAG until the release, however far the pointer strays — a
	 * slider or a text selection that loses its widget the moment the hand
	 * drifts a row is not a drag at all. `drag` is that id for exactly the
	 * frames a captured drag arrived on, -1 otherwise. */
	int capture;
	int drag;
} KtuiUi;

static KtuiUi ui = { .capture = -1, .drag = -1 };

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
	ui.drag = -1;

	if (ev->type == KT_EVT_MOUSE) {
		ui.mx = ev->mx;
		ui.my = ev->my;
		int is_wheel = ev->btn == KT_MB_WHEEL_UP ||
			       ev->btn == KT_MB_WHEEL_DOWN;
		if (!is_wheel && ev->press == KT_MP_DRAG && ui.capture >= 0) {
			ui.drag = ui.capture;
		} else for (int i = *prev_n - 1; i >= 0; i--) {
			if (!krect_hit(prev_hits[i].r, ev->mx, ev->my))
				continue;
			if (is_wheel) {
				ui.wheel = ev->btn == KT_MB_WHEEL_UP ? -1 : 1;
				ui.wheel_id = prev_hits[i].id;
			} else if (ev->btn == KT_MB_LEFT && ev->press == KT_MP_PRESS) {
				ui.clicked = prev_hits[i].id;
				ui.capture = prev_hits[i].id;
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
		if (!is_wheel && ev->press == KT_MP_RELEASE)
			ui.capture = -1;
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

/* Pasted text waiting for the focused input. One queue for the process: a
 * frame has at most one focused input, and the first one drawn after the push
 * takes the lot. */
static char paste_buf[4096];
static size_t paste_len;
static double paste_at;
#define PASTE_TTL 2.0		/* seconds an unclaimed paste waits */

void ktui_paste_push(const char *utf8, size_t len)
{
	/* A new paste SUPERSEDES an unclaimed one. The backend delivers a
	 * whole selection in one push, so appending never joins anything —
	 * it only accumulates pastes nobody took, until the queue is full and
	 * the one being made is the one lost. */
	paste_len = 0;
	paste_at = kb_now_s();

	/* Stripped at the door: control bytes never match a UTF-8 continuation
	 * byte, so this filter cannot split a sequence. */
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)utf8[i];
		if (c == '\n')
			c = ' ';
		else if (c < 0x20 || c == 0x7f)
			continue;
		if (paste_len + 1 >= sizeof(paste_buf))
			break;
		paste_buf[paste_len++] = (char)c;
	}
	/* A full queue may have cut a sequence in half; drop the fragment
	 * rather than hand the insert path a lead byte with no body. */
	size_t k = paste_len;
	while (k > 0 && ((unsigned char)paste_buf[k - 1] & 0xc0) == 0x80)
		k--;
	if (k > 0 && (unsigned char)paste_buf[k - 1] >= 0xc0) {
		unsigned char lead = (unsigned char)paste_buf[k - 1];
		size_t need = (lead & 0xe0) == 0xc0 ? 2
			      : (lead & 0xf0) == 0xe0 ? 3 : 4;
		if (paste_len - (k - 1) < need)
			paste_len = k - 1;
	}
	paste_buf[paste_len] = 0;
}

/*
 * Take the pending paste, for a consumer that is not a text field. A terminal
 * is the case: its "caret" is a child process on a pty, so it cannot go
 * through ktui_input and has to be handed the bytes.
 *
 * The same TTL as the input path, and the same filter: newlines arrived as
 * spaces, so a paste cannot press Enter in a shell any more than it can in a
 * field. Returns the length and clears the queue — a paste is taken once.
 */
size_t ktui_paste_take(const char **out)
{
	if (paste_len && kb_now_s() - paste_at > PASTE_TTL)
		paste_len = 0;
	if (!paste_len)
		return 0;

	size_t n = paste_len;

	if (out)
		*out = paste_buf;
	paste_len = 0;
	return n;
}

/* The caret is a BYTE index into a UTF-8 buffer; it only ever rests on a
 * sequence boundary, and these two walk between boundaries. */
static int in_prev_bound(const char *buf, int at)
{
	if (at > 0)
		at--;
	while (at > 0 && ((unsigned char)buf[at] & 0xc0) == 0x80)
		at--;
	return at;
}

static int in_next_bound(const char *buf, int at)
{
	uint32_t cp;
	return (int)(ktui_utf8_next(buf + at, &cp) - buf);
}

/* Display column of byte offset `at` — a secret field shows one bullet per
 * CODEPOINT, so its column is the sequence count, not the glyph width. */
static int in_col_at(const char *buf, int at, int secret)
{
	const char *p = buf;
	uint32_t cp;
	int col = 0;
	while (p < buf + at && *p) {
		p = ktui_utf8_next(p, &cp);
		col += secret ? 1 : ktui_wcwidth(cp);
	}
	return col;
}

/* First byte offset at or past display column `col`. */
static int in_byte_at(const char *buf, int col, int secret)
{
	const char *p = buf;
	uint32_t cp;
	int c = 0;
	while (*p && c < col) {
		p = ktui_utf8_next(p, &cp);
		c += secret ? 1 : ktui_wcwidth(cp);
	}
	return (int)(p - buf);
}

static int in_insert(char *buf, size_t cap, int *cur, int len,
		     const char *src, size_t n)
{
	/* Whole sequences only: a paste cut mid-codepoint would leave a
	 * trailing byte every later edit trips over. */
	size_t room = cap - 1 - (size_t)len;
	size_t take = 0;
	while (take < n) {
		uint32_t cp;
		size_t sl = (size_t)(ktui_utf8_next(src + take, &cp) - (src + take));
		if (take + sl > n || take + sl > room)
			break;
		take += sl;
	}
	if (!take)
		return 0;
	memmove(buf + *cur + take, buf + *cur, (size_t)(len - *cur + 1));
	memcpy(buf + *cur, src, take);
	*cur += (int)take;
	return 1;
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
	/* A caret restored from another life of this id can land mid-sequence. */
	while (*cur > 0 && ((unsigned char)buf[*cur] & 0xc0) == 0x80)
		(*cur)--;

	/* Expired rather than held: in a process with no focused input when the
	 * paste arrived — the panel, the desktop, a chooser whose list has the
	 * focus — the text would otherwise sit in the queue and be injected
	 * into whatever field is drawn next, seconds later and somewhere the
	 * user was not aiming. */
	if (paste_len && kb_now_s() - paste_at > PASTE_TTL)
		paste_len = 0;

	if (focus && paste_len) {
		if (in_insert(buf, cap, cur, len, paste_buf, paste_len))
			changed = 1;
		paste_len = 0;
		len = (int)strlen(buf);
	}

	if (focus && !ui.consumed && ui.ev.type == KT_EVT_KEY) {
		int k = ui.ev.key;
		if (k == KT_K_LEFT) {
			*cur = in_prev_bound(buf, *cur);
			ui.consumed = 1;
		} else if (k == KT_K_RIGHT) {
			if (*cur < len)
				*cur = in_next_bound(buf, *cur);
			ui.consumed = 1;
		} else if (k == KT_K_HOME) {
			*cur = 0;
			ui.consumed = 1;
		} else if (k == KT_K_END) {
			*cur = len;
			ui.consumed = 1;
		} else if (k == KT_K_BACKSPACE) {
			if (*cur > 0) {
				int p = in_prev_bound(buf, *cur);
				memmove(buf + p, buf + *cur,
					(size_t)(len - *cur + 1));
				*cur = p;
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == KT_K_DEL) {
			if (*cur < len) {
				int nx = in_next_bound(buf, *cur);
				memmove(buf + *cur, buf + nx,
					(size_t)(len - nx + 1));
				changed = 1;
			}
			ui.consumed = 1;
		} else if (k == ('u' - 'a' + 1) && (ui.ev.mods & KT_MOD_CTRL)) {
			buf[0] = 0;
			*cur = 0;
			changed = 1;
			ui.consumed = 1;
		} else if (k >= 0x20 && k != 0x7f && k < KT_K_SPECIAL) {
			char enc[4];
			int el = ktui_utf8_encode((uint32_t)k, enc);
			if ((size_t)(len + el) < cap) {
				memmove(buf + *cur + el, buf + *cur,
					(size_t)(len - *cur + 1));
				memcpy(buf + *cur, enc, (size_t)el);
				*cur += el;
				changed = 1;
			}
			ui.consumed = 1;
		}
		len = (int)strlen(buf);
	}

	int bg = KT_SURFACE;
	int fg = focus ? KT_TEXT : KT_MID;
	ktui_draw_fill(r, bg);
	ktui_draw_text(r.x, r.y, 1, focus ? ktui_glyph[KT_G_RIGHT] : " ",
		  focus ? KT_ACCENT : KT_DIM, bg, 0);

	/* Scrolling is in display COLUMNS, converted to a byte offset for the
	 * draw — byte arithmetic here is exactly the CJK-corrupts-the-row bug. */
	int field = r.w - 2;
	int ccol = in_col_at(buf, *cur, secret);
	int scol = 0;
	if (ccol > field - 1)
		scol = ccol - field + 1;
	int sbyte = in_byte_at(buf, scol, secret);
	if (sbyte)
		scol = in_col_at(buf, sbyte, secret);	/* wide-glyph straddle */

	if (!len && placeholder && !focus) {
		ktui_draw_text(r.x + 2, r.y, field, placeholder, KT_DIM, bg, 0);
	} else if (secret) {
		int glyphs = in_col_at(buf, len, 1);
		for (int i = 0; i < glyphs - scol && i < field; i++)
			ktui_draw_text(r.x + 2 + i, r.y, 1, ktui_glyph[KT_G_BULLET], fg, bg, 0);
	} else {
		ktui_draw_text(r.x + 2, r.y, field, buf + sbyte, fg, bg, 0);
	}

	if (focus) {
		int cx = r.x + 2 + (ccol - scol);
		if (cx < r.x + r.w) {
			uint32_t under = ' ';
			if (*cur < len) {
				if (secret) {
					under = 0x2022;
				} else {
					uint32_t cp;
					ktui_utf8_next(buf + *cur, &cp);
					under = cp;
				}
			}
			ktui_draw_cell(cx, r.y, under, KT_BG, KT_ACCENT, 0);
			/* A wide glyph owns two cells; without the
			 * continuation the caret highlights its left half and
			 * the right half keeps the text's colours. */
			if (ktui_wcwidth(under) == 2 && cx + 1 < r.x + r.w)
				ktui_draw_cell(cx + 1, r.y, KTUI_WIDE_CONT,
					       KT_BG, KT_ACCENT, 0);
		}
	}

	ktui_hit(r, id);
	if (ui.clicked == id) {
		int col = ui.mx - (r.x + 2) + scol;
		if (col < 0)
			col = 0;
		*cur = in_byte_at(buf, col, secret);
	}
	return changed;
}

int ktui_bar_fill(int w, double frac, double *tip)
{
	if (tip)
		*tip = 0;
	if (w <= 0 || frac <= 0)
		return 0;
	if (frac >= 1)
		return w;
	double cells = frac * w;
	int solid = (int)cells;
	double rest = cells - solid;
	/* A boundary must not sprout a tip: 0.5 of 40 is exactly 20 cells, and
	 * a sliver there reads as 51%. The epsilon absorbs the binary
	 * representation of a value the caller computed as done/total. */
	if (rest < 1e-9)
		rest = 0;
	if (solid >= w) {
		solid = w;
		rest = 0;
	}
	if (tip)
		*tip = rest;
	return solid;
}

void ktui_progress_ex(KRect r, double frac, const char *label, int style,
		      int bg)
{
	int pulse = style & KT_BAR_PULSE;
	style &= KT_BAR_STYLE_MASK;

	if (r.w < 4)
		return;
	if (frac < 0) {
		/* Indeterminate: a scanning cell rather than a filled bar, so
		 * nobody reads a stalled percentage into it. */
		ktui_draw_hline(r.x, r.y, r.w, KT_G_SHADE, KT_DIM, bg);
		int p = (int)(kb_now_s() * 12) % (r.w * 2);
		if (p >= r.w)
			p = r.w * 2 - p - 1;
		for (int i = 0; i < 3 && p + i < r.w; i++)
			ktui_draw_text(r.x + p + i, r.y, 1, ktui_glyph[KT_G_FULL],
				       KT_ACCENT, bg, 0);
	} else if (style == KT_BAR_SEGMENTED) {
		if (frac > 1)
			frac = 1;
		int fill = (int)(frac * r.w + 0.5);
		for (int i = 0; i < r.w; i++)
			ktui_draw_text(r.x + i, r.y, 1,
				       (i & 1) ? " "
					       : (i < fill ? ktui_glyph[KT_G_FULL]
							   : ktui_glyph[KT_G_SHADE]),
				       i < fill ? KT_ACCENT : KT_DIM, bg, 0);
	} else {
		double tip = 0;
		int fill = style == KT_BAR_TIP
				   ? ktui_bar_fill(r.w, frac, &tip)
				   : (frac > 1 ? r.w : (int)(frac * r.w + 0.5));
		for (int i = 0; i < r.w; i++) {
			const char *g;
			int fg;
			if (i < fill) {
				g = ktui_glyph[KT_G_FULL];
				fg = KT_ACCENT;
			} else if (i == fill && tip > 0) {
				g = ktui_ramp_h(tip);
				fg = KT_ACCENT;
			} else {
				g = ktui_glyph[KT_G_SHADE];
				fg = KT_DIM;
			}
			ktui_draw_text(r.x + i, r.y, 1, g, fg, bg, 0);
		}

		/* One cell of the filled run is lit a shade brighter and walks
		 * left to right. It says "still moving" on a bar that has not
		 * advanced a whole cell in minutes — a zig sweep would read as
		 * progress going backwards, so it wraps instead. */
		if (pulse && fill > 0) {
			int p = (int)(kb_now_s() * 9) % (fill + 4);
			if (p < fill)
				ktui_draw_text(r.x + p, r.y, 1,
					       ktui_glyph[KT_G_FULL], KT_WARN,
					       bg, 0);
		}
	}

	if (label && *label) {
		int w = ktui_utf8_width(label);
		int x = r.x + (r.w - w) / 2;
		/* Reverse video: fg/bg swap at render time, so passing the
		 * panel's bg as fg here is what makes the chip's apparent
		 * background match the panel instead of a hardcoded slot. */
		ktui_draw_text(x, r.y, w, label, bg, KT_ACCENT, KT_A_REVERSE);
	}
}

void ktui_progress(KRect r, double frac, const char *label)
{
	/* kinstall (the disc's installer) links this wrapper and only this
	 * wrapper, never ktui_progress_ex directly — it must keep drawing
	 * exactly what it always has, so the style/bg stay pinned here. */
	ktui_progress_ex(r, frac, label, KT_BAR_SOLID, KT_BG);
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
		if (count > vis && ui.mx == r.x + r.w - 1) {
			/* The scrollbar column is paint, not rows — selecting
			 * the row hidden under it is the bug. A click above
			 * the thumb pages up, below it pages down, moving the
			 * selection the way PgUp/PgDn do so the off/sel clamps
			 * below cannot drag the view straight back. */
			int th = r.h * vis / count;
			if (th < 1)
				th = 1;
			int ty = (r.h - th) * st->off / (count - vis);
			int cy = ui.my - r.y;
			if (cy < ty)
				st->sel -= vis;
			else if (cy >= ty + th)
				st->sel += vis;
		} else {
			int idx = st->off + (ui.my - r.y);
			if (idx >= 0 && idx < count) {
				st->sel = idx;
				if (ui.dblclick)
					chosen = 1;
			}
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

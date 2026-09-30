/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — the layout cursor, the splitter and the fold
 *
 * THE CURSOR IS ARITHMETIC. It hands out rects and draws a label or a
 * section rule through the same calls a page would have made itself, so a
 * page moved onto it draws the same cells it drew before.
 *
 * THE SPLITTER AND THE FOLD ARE FRAME CONTROLS: named ids, their state in
 * ktui_state(), the press through the frame's hit list. See ktui.h.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "ktui.h"

/* ────────────────────────────────────────────────────────────────────────
 * The cursor
 * ──────────────────────────────────────────────────────────────────────── */

void ktui_lay_begin(KtuiLay *l, KRect r)
{
	memset(l, 0, sizeof(*l));
	l->r = r;
	l->y = r.y;
	l->bg = KT_BG;
}

static int lay_x(const KtuiLay *l)
{
	return l->r.x + l->indent;
}

static int lay_w(const KtuiLay *l)
{
	int w = l->r.w - l->indent;

	return w > 0 ? w : 0;
}

KRect ktui_lay_row(KtuiLay *l, int h)
{
	KRect r;

	if (h < 1)
		h = 1;
	r = krect(lay_x(l), l->y, lay_w(l), h);
	l->y += h;
	return r;
}

void ktui_lay_gap(KtuiLay *l, int n)
{
	if (n > 0)
		l->y += n;
}

void ktui_lay_section(KtuiLay *l, const char *title)
{
	KRect r = ktui_lay_row(l, 1);

	ktui_section(r.x, r.y, r.w, title);
}

KRect ktui_lay_field(KtuiLay *l, const char *label, int label_w)
{
	KRect r = ktui_lay_row(l, 1);
	int cw;

	if (label_w <= 0)
		label_w = KTUI_LAY_LABEL;
	if (label)
		ktui_draw_text(r.x, r.y, label_w, label, KT_MID, l->bg, 0);
	cw = r.w - label_w - 1;
	return krect(r.x + label_w + 1, r.y, cw > 0 ? cw : 0, 1);
}

int ktui_lay_cols(KtuiLay *l, const KtuiCol *col, int n, int h, KRect *out)
{
	int x[KT_TABLE_COLS], cw[KT_TABLE_COLS];
	KRect r = ktui_lay_row(l, h);

	if (n > KT_TABLE_COLS)
		n = KT_TABLE_COLS;
	if (n <= 0)
		return 0;
	ktui_table_layout(col, n, r.w, x, cw);
	for (int i = 0; i < n; i++)
		out[i] = krect(r.x + x[i], r.y, cw[i], r.h);
	return n;
}

void ktui_lay_indent(KtuiLay *l, int n)
{
	if (n <= 0)
		n = KTUI_LAY_INDENT;
	l->indent += n;
	if (l->indent > l->r.w)
		l->indent = l->r.w > 0 ? l->r.w : 0;
}

void ktui_lay_unindent(KtuiLay *l, int n)
{
	if (n <= 0)
		n = KTUI_LAY_INDENT;
	l->indent -= n;
	if (l->indent < 0)
		l->indent = 0;
}

KRect ktui_lay_left(const KtuiLay *l)
{
	int h = l->r.y + l->r.h - l->y;

	return krect(lay_x(l), l->y, lay_w(l), h > 0 ? h : 0);
}

/* ────────────────────────────────────────────────────────────────────────
 * The splitter
 * ──────────────────────────────────────────────────────────────────────── */

/* `size` is the ANCHORED pane's — the first for `at` >= 0, the second for a
 * negative `at` — so a dragged side panel keeps its own width as the window
 * around it grows, as its default does. */
typedef struct {
	int size;
	int set;		/* moved by a hand: `size` holds, not `at`   */
} SplitState;

static int split_clamp(int pos, int len, int min)
{
	int hi;

	if (min < 0)
		min = 0;
	hi = len - 1 - min;
	/* No room for both minimums: the divider halves what there is, and
	 * a rect with no room for a divider at all puts it at 0. */
	if (hi < min)
		return len > 1 ? (len - 1) / 2 : 0;
	if (pos < min)
		return min;
	if (pos > hi)
		return hi;
	return pos;
}

/* An unmodified key: Ctrl and Alt chords stay the surface's. */
static int plain_key(const KtuiEvent *ev)
{
	return ev->type == KT_EVT_KEY &&
	       !(ev->mods & (KT_MOD_CTRL | KT_MOD_ALT));
}

/* The background the divider stands on: whatever the page put in that cell,
 * so a split inside a KT_SURFACE window is not a KT_BG seam down it. */
static int cell_bg(int x, int y)
{
	int w = 0, h = 0;
	const KtuiCell *c = ktui_draw_cells(&w, &h);

	if (!c || x < 0 || y < 0 || x >= w || y >= h)
		return KT_BG;
	return c[(size_t)y * w + x].bg;
}

int ktui_split(KRect r, const char *name, int stack, int at, int min,
	       KRect *a, KRect *b)
{
	int id = ktui_id_str(name);
	SplitState *st = ktui_state(id, sizeof(SplitState));
	const KtuiEvent *ev = ktui_event();
	int len = stack ? r.h : r.w;
	int far = at < 0;
	int anchored = st && st->set ? st->size : far ? -at : at;
	int def = far ? len - 1 + at : at;
	int pos = split_clamp(far ? len - 1 - anchored : anchored, len, min);
	int moved = 0;

	if (len < 1) {
		*a = krect(r.x, r.y, stack ? r.w : 0, stack ? 0 : r.h);
		*b = *a;
		return 0;
	}

	/*
	 * THE PRESS AND THE DRAG BEHIND IT, as the slider takes them: the
	 * capture keeps the divider in hand when the pointer runs ahead of it,
	 * which on a one-cell target is every drag.
	 */
	if (ev->type == KT_EVT_MOUSE &&
	    (ktui_clicked() == id || ktui_drag() == id)) {
		int p = (stack ? ktui_mouse_y() - r.y : ktui_mouse_x() - r.x);

		p = split_clamp(p, len, min);
		if (st && p != pos) {
			st->size = far ? len - 1 - p : p;
			st->set = 1;
			pos = p;
			moved = 1;
		}
	}

	if (ktui_focused(id) && !ktui_consumed() && plain_key(ev)) {
		int k = ev->key, step = 0, home = 0;

		if (k == (stack ? KT_K_UP : KT_K_LEFT))
			step = -1;
		else if (k == (stack ? KT_K_DOWN : KT_K_RIGHT))
			step = 1;
		else if (k == KT_K_HOME)
			home = 1;
		if (step || home) {
			ktui_consume();
			if (st) {
				int p = split_clamp(home ? def : pos + step, len,
						    min);

				st->size = far ? len - 1 - p : p;
				st->set = !home;
				moved = p != pos;
				pos = p;
			}
		}
	}

	{
		/* A press focuses what it lands on, so a divider in hand is
		 * the focused one. */
		int focus = ktui_focused(id);
		int fg = focus ? KT_ACCENT : KT_DIM;
		KRect d;

		if (stack) {
			d = krect(r.x, r.y + pos, r.w, 1);
			ktui_draw_hline(d.x, d.y, d.w, KT_G_HL, fg,
					cell_bg(d.x, d.y));
			*a = krect(r.x, r.y, r.w, pos);
			*b = krect(r.x, r.y + pos + 1, r.w, len - pos - 1);
		} else {
			d = krect(r.x + pos, r.y, 1, r.h);
			ktui_draw_vline(d.x, d.y, d.h, KT_G_VL, fg,
					cell_bg(d.x, d.y));
			*a = krect(r.x, r.y, pos, r.h);
			*b = krect(r.x + pos + 1, r.y, len - pos - 1, r.h);
		}
		ktui_hit(d, id);
		if (focus) {
			char v[16];

			snprintf(v, sizeof(v), "%d", pos);
			ktui_announce(KT_A11Y_SLIDER, name, v, 0, 0);
		}
	}
	return moved;
}

/* ────────────────────────────────────────────────────────────────────────
 * The fold
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int open;
	int set;		/* toggled by a hand: `open` holds           */
} FoldState;

int ktui_fold_begin(int x, int y, int w, const char *title, int open)
{
	int id = ktui_id_str(title);
	FoldState *st = ktui_state(id, sizeof(FoldState));
	const KtuiEvent *ev = ktui_event();
	int focus = ktui_focused(id);
	int is_open = st && st->set ? st->open : !!open;
	int want = is_open;
	KRect r = krect(x, y, w, 1);
	int fg, bg, tw;

	if (focus && !ktui_consumed() && plain_key(ev)) {
		if (ev->key == KT_K_RIGHT && !is_open) {
			want = 1;
			ktui_consume();
		} else if (ev->key == KT_K_LEFT && is_open) {
			want = 0;
			ktui_consume();
		}
	}
	if (ktui_activated(id, r))
		want = !is_open;
	if (want != is_open && st) {
		st->open = want;
		st->set = 1;
		is_open = want;
	}

	/*
	 * THE PLATE IS FOCUS AND THE MARKER IS STATE, the rule the check box
	 * keeps: the marker wears the accent open or shut, and the focused row
	 * is the quiet selection fill with the title lifted to KT_TEXT.
	 */
	ktui_sel_slots(1, focus, KT_BG, &fg, &bg);
	ktui_draw_fill(r, bg);
	ktui_draw_text(x, y, 1,
		       ktui_glyph[is_open ? KT_G_ARROW_DOWN : KT_G_ARROW_R],
		       KT_ACCENT, bg, 0);
	ktui_draw_text(x + 2, y, w - 2, title, focus ? KT_TEXT : KT_ACCENT, bg,
		       0);
	tw = ktui_utf8_width(title);
	if (w > tw + 4)
		ktui_draw_hline(x + tw + 3, y, w - tw - 3, KT_G_HL, KT_DIM, bg);
	if (focus)
		ktui_announce(KT_A11Y_BUTTON, title,
			      is_open ? "expanded" : "collapsed", 0, 0);

	if (!is_open)
		return 0;
	ktui_id_push(title);
	return 1;
}

void ktui_fold_end(void)
{
	ktui_id_pop();
}

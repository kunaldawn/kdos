/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The menu — a bar with panes, or one pane popped at a point.
 *
 * ONE WIDGET FOR BOTH, because they are the same list drawn in two places.
 * A menu bar is `bar >= 0` and a row of titles; a context menu is the same
 * pane opened at a cell with no bar above it. A second implementation of
 * "a column of labels, one of them selected" is the thing this file exists
 * to stop the tree from growing for a third time.
 *
 * `sel` INDEXES `item[]`, NEVER A DRAWN ROW. A caller's `show` callback hides
 * rows, and a selection counted in drawn rows lands on a different item the
 * moment one of them is hidden — nothing highlighted, and Enter doing
 * something else.
 *
 * `show` IS ASKED BY THE DRAW AND BY THE HIT TEST, from the same walk. Two
 * copies of a visibility rule disagree eventually, and when they do a click
 * runs the row above the one under the pointer.
 *
 * THE POPUP IS CLAMPED AT DRAW TIME AND THE CLAMPED ORIGIN IS WRITTEN BACK,
 * so the hit test measures what was drawn rather than where the caller asked.
 * A menu opened near the right edge is otherwise clickable only where it is
 * not.
 *
 * `&` MARKS THE ACCELERATOR in a label or a title, and `&&` is a literal one.
 * The letter is underlined where the tier has underline and bracketed where it
 * does not — a Linux VT has no underline at all, and drawing one there puts an
 * unowned colour on the screen.
 * ---------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "ktui.h"

enum { MENU_W_MIN = 12, MENU_W_MAX = 40 };

/* ── labels and their accelerator mark ─────────────────────────────────── */

/*
 * The accelerator letter of a label, lowercased, or 0. Read from the SAME
 * string the drawing reads, so a title cannot advertise a letter that opens
 * nothing.
 */
int ktui_menu_accel_of(const char *s)
{
	if (!s)
		return 0;
	for (; *s; s++) {
		if (*s != '&')
			continue;
		if (s[1] == '&') {
			s++;
			continue;
		}
		if (s[1] >= 'A' && s[1] <= 'Z')
			return s[1] - 'A' + 'a';
		if (s[1] >= 'a' && s[1] <= 'z')
			return s[1];
		return 0;
	}
	return 0;
}

/* Cells the label occupies once the marks are gone. */
static int label_w(const char *s)
{
	char plain[128];
	int n = 0;

	if (!s)
		return 0;
	for (; *s && n + 1 < (int)sizeof(plain); s++) {
		if (*s == '&') {
			if (s[1] == '&')
				s++;
			else
				continue;
		}
		plain[n++] = *s;
	}
	plain[n] = '\0';
	return ktui_utf8_width(plain);
}

/*
 * Draw a label, marking its accelerator. Returns the cells written.
 *
 * The mark is an ATTRIBUTE where the tier has one and a pair of brackets
 * where it does not, which is why the width is computed here rather than by
 * the caller: the two tiers are not the same width.
 */
int ktui_menu_label(int x, int y, int w, const char *s, int fg, int bg)
{
	int brackets = (ktui_caps & KT_CAP_LINUXVT) != 0;
	int start = x, end = x + w;

	if (!s)
		return 0;
	while (*s && x < end) {
		char one[8];
		int n = 0;

		if (*s == '&' && s[1] == '&') {
			s++;
		} else if (*s == '&' && s[1]) {
			s++;
			if (brackets) {
				x += ktui_draw_text(x, y, end - x, "[", fg, bg,
						    KT_A_NONE);
				one[0] = *s++;
				one[1] = '\0';
				x += ktui_draw_text(x, y, end - x, one, fg, bg,
						    KT_A_NONE);
				if (x < end)
					x += ktui_draw_text(x, y, end - x, "]",
							    fg, bg, KT_A_NONE);
			} else {
				one[0] = *s++;
				one[1] = '\0';
				x += ktui_draw_text(x, y, end - x, one, fg, bg,
						    KT_A_UNDERLINE);
			}
			continue;
		}
		/* One UTF-8 sequence at a time: the mark can fall anywhere in
		 * a label and a byte-at-a-time copy would split a codepoint
		 * that follows it. */
		one[n++] = *s++;
		while ((*s & 0xc0) == 0x80 && n + 1 < (int)sizeof(one))
			one[n++] = *s++;
		one[n] = '\0';
		x += ktui_draw_text(x, y, end - x, one, fg, bg, KT_A_NONE);
	}
	return x - start;
}

/*
 * The drawn width on THIS tier. A bracketed mark costs two cells the
 * underlined one does not, and a width that ignored them clips the last
 * letter off every accelerated label on a Linux VT.
 */
static int label_cells(const char *s)
{
	int w = label_w(s);

	if ((ktui_caps & KT_CAP_LINUXVT) && ktui_menu_accel_of(s))
		w += 2;
	return w;
}

/* ── the pane ──────────────────────────────────────────────────────────── */

static const KtuiMenuPane *pane_of(const KtuiMenu *m)
{
	if (!m || !m->pane || m->open <= 0 || m->open > m->npane)
		return NULL;
	return &m->pane[m->open - 1];
}

/* A rule — drawn, never selected. A separator that can hold the caret is a
 * menu with a row that does nothing. */
static int is_rule(const KtuiMenuItem *it)
{
	return !it->label || !it->label[0];
}

static int shown(const KtuiMenu *m, int i)
{
	const KtuiMenuPane *p = pane_of(m);

	if (!p || i < 0 || i >= p->n)
		return 0;
	return m->show ? m->show(i, m->user) : 1;
}

static int pickable(const KtuiMenu *m, int i)
{
	const KtuiMenuPane *p = pane_of(m);

	return shown(m, i) && !is_rule(&p->item[i]) && p->item[i].enabled;
}

static int rows_shown(const KtuiMenu *m)
{
	const KtuiMenuPane *p = pane_of(m);
	int n = 0;

	if (!p)
		return 0;
	for (int i = 0; i < p->n; i++)
		if (shown(m, i))
			n++;
	return n;
}

static void step(KtuiMenu *m, int dir)
{
	const KtuiMenuPane *p = pane_of(m);

	if (!p || p->n <= 0)
		return;
	for (int k = 0; k < p->n; k++) {
		m->sel = (m->sel + dir + p->n) % p->n;
		if (pickable(m, m->sel))
			return;
	}
}

/* The first row a caret may rest on, so a menu never opens with the highlight
 * on a rule or on a row the scope rules hid. */
static void sel_first(KtuiMenu *m)
{
	const KtuiMenuPane *p = pane_of(m);

	m->sel = 0;
	if (!p)
		return;
	for (int i = 0; i < p->n; i++)
		if (pickable(m, i)) {
			m->sel = i;
			return;
		}
}

void ktui_menu_open(KtuiMenu *m, int pane, int x, int y)
{
	if (!m || pane < 0 || pane >= m->npane)
		return;
	m->open = pane + 1;
	m->x = x;
	m->y = y;
	sel_first(m);
}

void ktui_menu_close(KtuiMenu *m)
{
	if (m)
		m->open = 0;
}

int ktui_menu_active(const KtuiMenu *m)
{
	return m && m->open > 0;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static int pane_width(const KtuiMenu *m)
{
	const KtuiMenuPane *p = pane_of(m);
	int w = MENU_W_MIN;

	if (!p)
		return w;
	for (int i = 0; i < p->n; i++) {
		int c;

		if (!shown(m, i) || is_rule(&p->item[i]))
			continue;
		c = label_cells(p->item[i].label) + 4;
		if (p->item[i].accel)
			c += ktui_utf8_width(p->item[i].accel) + 2;
		if (c > w)
			w = c;
	}
	return w > MENU_W_MAX ? MENU_W_MAX : w;
}

void ktui_menu_draw(KtuiMenu *m)
{
	if (!m || !m->pane || m->npane <= 0)
		return;

	/* ── the bar, when there is one ── */
	if (m->has_bar && m->bar_row >= 0 && m->bar_row < ktui_h) {
		int x = 1;

		ktui_draw_fill(krect(0, m->bar_row, ktui_w, 1), m->bar_bg);
		/* BOUNDED BY THE SPAN ARRAYS, not by npane: the hit test reads
		 * bar_x[] and bar_w[], and a bar with more panes than those
		 * hold would write past them. */
		for (int i = 0; i < m->npane && i < KTUI_MENU_PANE_MAX; i++) {
			int on = m->open == i + 1;
			int cw = label_cells(m->pane[i].title) + 2;

			if (x + cw > ktui_w)
				break;
			if (on)
				ktui_draw_fill(krect(x, m->bar_row, cw, 1),
					       KT_ACCENT);
			ktui_menu_label(x + 1, m->bar_row, cw - 2,
					m->pane[i].title,
					on ? KT_SURFACE : KT_TEXT,
					on ? KT_ACCENT : m->bar_bg);
			m->bar_x[i] = x;
			m->bar_w[i] = cw;
			x += cw;
		}
	}

	/* ── the open pane ── */
	const KtuiMenuPane *p = pane_of(m);

	if (!p) {
		/* Advertised only where there is a bar to open: a surface with
		 * three verbs and no bar must not name a key it does not
		 * answer. */
		ktui_hint_if(m->has_bar, "F10", "menu");
		return;
	}

	int rows = rows_shown(m);
	KRect r = krect(m->x, m->y, pane_width(m), rows + 2);

	if (r.x + r.w > ktui_w)
		r.x = ktui_w - r.w;
	if (r.y + r.h > ktui_h)
		r.y = ktui_h - r.h;
	if (r.x < 0)
		r.x = 0;
	if (r.y < 0)
		r.y = 0;
	/* WRITTEN BACK, so the hit test measures the box that was drawn. */
	m->x = r.x;
	m->y = r.y;
	m->w = r.w;
	m->rows = rows;

	ktui_draw_fill(r, KT_SURFACE);
	ktui_draw_box(r, NULL, KT_ACCENT, KT_SURFACE, 0);

	int y = r.y + 1;

	for (int i = 0; i < p->n; i++) {
		const KtuiMenuItem *it = &p->item[i];
		int on, fg, bg;

		if (!shown(m, i))
			continue;
		if (is_rule(it)) {
			ktui_draw_hline(r.x + 1, y, r.w - 2, KT_G_HL, KT_DIM,
					KT_SURFACE);
			y++;
			continue;
		}
		on = i == m->sel;
		bg = on ? KT_ACCENT : KT_SURFACE;
		fg = !it->enabled ? KT_DIM : on ? KT_SURFACE : KT_TEXT;
		ktui_draw_fill(krect(r.x + 1, y, r.w - 2, 1), bg);
		ktui_menu_label(r.x + 2, y, r.w - 4, it->label, fg, bg);
		if (it->accel)
			ktui_draw_text_right(r.x, y, r.x + r.w - 2, it->accel,
					     on ? KT_SURFACE : KT_MID, bg,
					     KT_A_NONE);
		y++;
	}

	/*
	 * THE MENU NAMES ITS OWN KEYS, so no surface writes a menu hint. The
	 * row follows the state for free: these appear only while a pane is
	 * down, and the F10 hint above only while one is not.
	 */
	ktui_hint("Up/Down", "move");
	ktui_hint_if(m->npane > 1 && m->has_bar, "Left/Right", "menu");
	ktui_hint("Enter", "pick");
}

/* ── the hit test ──────────────────────────────────────────────────────── */

/* Which item a cell is on, or -1. Walks the same skip the drawing walks. */
static int row_at(const KtuiMenu *m, int mx, int my)
{
	const KtuiMenuPane *p = pane_of(m);
	int want, seen = 0;

	if (!p || mx < m->x || mx >= m->x + m->w || my <= m->y ||
	    my > m->y + m->rows)
		return -1;
	want = my - m->y - 1;
	for (int i = 0; i < p->n; i++) {
		if (!shown(m, i))
			continue;
		if (seen++ == want)
			return i;
	}
	return -1;
}

static int bar_at(const KtuiMenu *m, int mx, int my)
{
	if (!m->has_bar || my != m->bar_row)
		return -1;
	for (int i = 0; i < m->npane && i < KTUI_MENU_PANE_MAX; i++)
		if (m->bar_w[i] && mx >= m->bar_x[i] &&
		    mx < m->bar_x[i] + m->bar_w[i])
			return i;
	return -1;
}

int ktui_menu_event(KtuiMenu *m, const KtuiEvent *ev, int *id)
{
	if (!m || !ev || !m->pane || m->npane <= 0)
		return KTUI_MENU_NONE;

	if (ev->type == KT_EVT_MOUSE) {
		int bi = bar_at(m, ev->mx, ev->my);

		if (ev->press == KT_MP_PRESS && bi >= 0) {
			/* A second press on the title that is already down
			 * closes it, the way every menu bar has behaved since
			 * they had two of them. */
			if (m->open == bi + 1)
				ktui_menu_close(m);
			else
				ktui_menu_open(m, bi, m->bar_x[bi],
					       m->bar_row + 1);
			return KTUI_MENU_TAKEN;
		}
		if (!m->open)
			return KTUI_MENU_NONE;

		int row = row_at(m, ev->mx, ev->my);

		if (ev->press == KT_MP_PRESS) {
			if (row < 0) {
				ktui_menu_close(m);	/* a click away */
				return KTUI_MENU_TAKEN;
			}
			if (!pickable(m, row))
				return KTUI_MENU_TAKEN;
			m->sel = row;
			if (id)
				*id = pane_of(m)->item[row].id;
			ktui_menu_close(m);
			return KTUI_MENU_PICKED;
		}
		/* A HOVER MOVES THE CARET, and it is the same caret Enter
		 * uses: two of them would let the pointer sit on one row while
		 * Enter ran another. */
		if (row >= 0 && pickable(m, row))
			m->sel = row;
		return KTUI_MENU_TAKEN;
	}

	if (ev->type != KT_EVT_KEY || !m->open)
		return KTUI_MENU_NONE;

	switch (ev->key) {
	case KT_K_ESC:
		ktui_menu_close(m);
		return KTUI_MENU_TAKEN;
	case KT_K_UP:
		step(m, -1);
		return KTUI_MENU_TAKEN;
	case KT_K_DOWN:
		step(m, 1);
		return KTUI_MENU_TAKEN;
	case KT_K_HOME:
		m->sel = -1;
		step(m, 1);
		return KTUI_MENU_TAKEN;
	case KT_K_END:
		m->sel = 0;
		step(m, -1);
		return KTUI_MENU_TAKEN;
	case KT_K_LEFT:
	case KT_K_RIGHT:
		/* ONLY ALONG A BAR. A popup has no neighbour to walk to, and
		 * swallowing an arrow there would take a key the surface
		 * underneath still answers. */
		if (!m->has_bar || m->npane <= 1)
			return KTUI_MENU_NONE;
		{
			int np = m->npane < KTUI_MENU_PANE_MAX
					 ? m->npane
					 : KTUI_MENU_PANE_MAX;
			int n = m->open - 1;

			n = (n + (ev->key == KT_K_LEFT ? -1 : 1) + np) % np;
			ktui_menu_open(m, n, m->bar_x[n], m->bar_row + 1);
		}
		return KTUI_MENU_TAKEN;
	case KT_K_ENTER:
		if (!pickable(m, m->sel))
			return KTUI_MENU_TAKEN;
		if (id)
			*id = pane_of(m)->item[m->sel].id;
		ktui_menu_close(m);
		return KTUI_MENU_PICKED;
	default:
		break;
	}

	/*
	 * A LETTER PICKS ITS OWN ROW while a pane is down — the second half of
	 * the accelerator, and the half a person reaches for. Only a row that
	 * is pickable answers: a letter that ran a disabled row would be the
	 * one path around the enable rule.
	 */
	if (ev->key >= 0x20 && ev->key < 0x7f) {
		const KtuiMenuPane *p = pane_of(m);
		int want = ev->key;

		if (want >= 'A' && want <= 'Z')
			want = want - 'A' + 'a';
		for (int i = 0; i < p->n; i++) {
			if (!pickable(m, i))
				continue;
			if (ktui_menu_accel_of(p->item[i].label) != want)
				continue;
			m->sel = i;
			if (id)
				*id = p->item[i].id;
			ktui_menu_close(m);
			return KTUI_MENU_PICKED;
		}
	}
	return KTUI_MENU_TAKEN;
}

/*
 * Alt+letter, answered whether or not a pane is down. Returns 1 if a pane
 * opened. Asked BEFORE the surface's own keys, because a bar that only
 * answered its letters while it was already open would be a bar nobody could
 * open with the keyboard.
 */
int ktui_menu_alt(KtuiMenu *m, const KtuiEvent *ev)
{
	int want;

	if (!m || !ev || ev->type != KT_EVT_KEY || !m->has_bar)
		return 0;
	if (!(ev->mods & KT_MOD_ALT) || ev->key < 0x20 || ev->key >= 0x7f)
		return 0;
	want = ev->key;
	if (want >= 'A' && want <= 'Z')
		want = want - 'A' + 'a';
	for (int i = 0; i < m->npane && i < KTUI_MENU_PANE_MAX; i++) {
		if (ktui_menu_accel_of(m->pane[i].title) != want)
			continue;
		ktui_menu_open(m, i, m->bar_x[i], m->bar_row + 1);
		return 1;
	}
	return 0;
}

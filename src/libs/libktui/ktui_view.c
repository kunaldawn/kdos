/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The four views — a page strip, a column table, a choice, a text block.
 *
 * DRAW AND KEY ARE SEPARATE CALLS, like the menu and unlike ktui_list(). The
 * surfaces these serve run their own event loop and draw from their own
 * state; an immediate-mode widget that reads the frame's focus would need
 * every one of them rewritten around ktui_frame_begin() before it drew
 * anything at all.
 *
 * A HIT TEST MEASURES WHAT THE DRAW PUT ON THE SCREEN, so every one of them
 * takes the same rect the draw took. A widget that remembered its own
 * geometry would answer for the frame before the one the click belongs to,
 * and on a resize that is the wrong row.
 *
 * NO WIDGET OWNS ITS SELECTION. The caller holds it, because the caller is
 * what persists it, dumps it and restores it — and because a page strip and
 * the body under it are one selection seen twice.
 * ---------------------------------
 */

#include <string.h>

#include "ktui.h"

/* ────────────────────────────────────────────────────────────────────────
 * Tabs — a strip of named pages, along the top or down the side
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * THE ABBREVIATION IS THE CALLER'S, and the fallback is three characters
 * rather than one. A single initial makes two pages the same control the
 * moment two names share a letter — worse than a truncation, which at least
 * reads as incomplete.
 */
static const char *tab_label(const KtuiTab *t, int room, char *store,
			     size_t cap)
{
	if (room >= ktui_utf8_width(t->name))
		return t->name;
	if (t->abbr)
		return t->abbr;

	int n = 0, cols = 0;
	while (t->name[n] && cols < 3 && n < (int)cap - 1) {
		int len = 1;
		while ((t->name[n + len] & 0xc0) == 0x80)
			len++;
		if (n + len >= (int)cap)
			break;
		n += len;
		cols++;
	}
	memcpy(store, t->name, (size_t)n);
	store[n] = '\0';
	return store;
}

int ktui_tab_span(const KtuiTab *t, int n, int vertical, int w)
{
	if (vertical)
		return w;

	int span = 0;
	for (int i = 0; i < n; i++) {
		int c = ktui_utf8_width(t[i].name) + 2;
		if (c > span)
			span = c;
	}
	return span;
}

void ktui_tabs_draw(KRect r, const KtuiTab *t, int n, int sel, int hover,
		    int vertical)
{
	int span = ktui_tab_span(t, n, vertical, r.w);

	ktui_draw_fill(r, KT_SURFACE);
	for (int i = 0; i < n; i++) {
		KRect c = vertical ? krect(r.x, r.y + i, r.w, 1)
				   : krect(r.x + i * span, r.y, span, r.h);

		if (vertical ? c.y >= r.y + r.h : c.x + c.w > r.x + r.w)
			break;
		/*
		 * A FILL, never KT_A_REVERSE over the label: the attribute
		 * inverts only the cells the glyphs cover, so a two-word name
		 * comes out as one lit block per word.
		 */
		int bg = i == sel ? KT_ACCENT : i == hover ? KT_MID : KT_SURFACE;
		int fg = i == sel ? KT_SURFACE : KT_TEXT;
		char store[8];
		const char *lab = tab_label(&t[i], c.w - 2, store,
					    sizeof(store));

		ktui_draw_fill(c, bg);
		ktui_draw_text(c.x + 1, c.y, c.w - 2, lab, fg, bg, KT_A_NONE);
	}
}

int ktui_tabs_key(int *sel, int n, int vertical, int k)
{
	int prev = *sel;
	int back = vertical ? KT_K_UP : KT_K_LEFT;
	int fwd = vertical ? KT_K_DOWN : KT_K_RIGHT;

	if (k == back && *sel > 0)
		(*sel)--;
	else if (k == fwd && *sel + 1 < n)
		(*sel)++;
	else if (k == KT_K_HOME)
		*sel = 0;
	else if (k == KT_K_END)
		*sel = n - 1;
	else
		return 0;
	return *sel != prev;
}

int ktui_tabs_hit(KRect r, const KtuiTab *t, int n, int vertical, int mx,
		  int my)
{
	if (!krect_hit(r, mx, my))
		return -1;

	int span = ktui_tab_span(t, n, vertical, r.w);
	int i = vertical ? my - r.y : (span ? (mx - r.x) / span : -1);

	return i >= 0 && i < n ? i : -1;
}

/* ────────────────────────────────────────────────────────────────────────
 * Table — columns, a header, and the scroll arithmetic every list repeats
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * ONE COLUMN MAY ASK FOR THE REMAINDER and the first that does gets it; the
 * rest keep the width they asked for. Two elastic columns would need a
 * distribution rule, and every table in this tree has exactly one field —
 * the name — that should absorb a wider window.
 */
int ktui_table_layout(const KtuiCol *col, int ncol, int w, int *x, int *cw)
{
	int fixed = 0, elastic = -1;

	for (int i = 0; i < ncol; i++) {
		if (col[i].width > 0)
			fixed += col[i].width + 1;
		else if (elastic < 0)
			elastic = i;
		else
			fixed += 1;
	}

	int rest = w - fixed;
	if (rest < 1)
		rest = 1;

	int at = 0;
	for (int i = 0; i < ncol; i++) {
		x[i] = at;
		cw[i] = i == elastic ? rest : col[i].width > 0 ? col[i].width : 0;
		at += cw[i] + 1;
	}
	return at;
}

void ktui_table_draw(KRect r, KtuiTable *st, int count, const KtuiCol *col,
		     int ncol, KtuiTableCell cell, KtuiTableSpan span,
		     void *user, int hover)
{
	int x[KT_TABLE_COLS], cw[KT_TABLE_COLS];
	int head = 0;
	int bar = 0;

	if (ncol > KT_TABLE_COLS)
		ncol = KT_TABLE_COLS;
	for (int i = 0; i < ncol; i++)
		if (col[i].title)
			head = 2;	/* the titles and the rule under them */

	int rows = r.h - head;
	if (rows < 1)
		rows = 1;
	bar = count > rows;

	ktui_table_layout(col, ncol, bar ? r.w - 1 : r.w, x, cw);

	if (head) {
		ktui_draw_fill(krect(r.x, r.y, r.w, 1), KT_BG);
		for (int i = 0; i < ncol; i++)
			if (col[i].title)
				ktui_draw_text(r.x + x[i], r.y, cw[i],
					       col[i].title, KT_MID, KT_BG,
					       KT_A_NONE);
		ktui_draw_hline(r.x, r.y + 1, r.w, KT_G_HL, KT_DIM, KT_BG);
	}

	ktui_table_clamp(st, count, rows);

	for (int i = 0; i < rows; i++) {
		int idx = st->top + i;
		int y = r.y + head + i;

		if (idx >= count) {
			ktui_draw_fill(krect(r.x, y, r.w, 1), KT_BG);
			continue;
		}
		int kind = span ? span(idx, user) : 0;
		/* A row the selection steps over never lights, whatever the
		 * caller's selection happens to be resting on. */
		int on = idx == st->sel && kind != KT_TABLE_SKIP;
		int bg = on		? KT_ACCENT
			 : idx == hover && kind != KT_TABLE_SKIP ? KT_MID
					: KT_BG;
		int fg = on ? KT_SURFACE : KT_TEXT;

		ktui_draw_fill(krect(r.x, y, bar ? r.w - 1 : r.w, 1), bg);
		if (kind) {
			cell(idx, -1, r.x, y, bar ? r.w - 1 : r.w,
			     on ? KT_SURFACE : KT_ACCENT, bg, user);
			continue;
		}
		for (int c = 0; c < ncol; c++)
			if (cw[c] > 0)
				cell(idx, c, r.x + x[c], y, cw[c], fg, bg,
				     user);
	}

	if (bar)
		ktui_scrollbar(krect(r.x + r.w - 1, r.y + head, 1, rows), count,
			       rows, st->top);
}

void ktui_table_clamp(KtuiTable *st, int count, int rows)
{
	if (st->sel >= count)
		st->sel = count ? count - 1 : 0;
	if (st->sel < 0)
		st->sel = 0;
	if (st->sel < st->top)
		st->top = st->sel;
	if (st->sel >= st->top + rows)
		st->top = st->sel - rows + 1;
	if (st->top > count - rows)
		st->top = count - rows;
	if (st->top < 0)
		st->top = 0;
}

/*
 * A SKIPPED ROW IS STEPPED OVER, NEVER LANDED ON, and the step keeps going in
 * the direction it started. Stopping on the heading instead would make one
 * press of Down do nothing visible, and the next press look like two.
 */
static int table_step(const KtuiTable *st, int count, int from, int dir,
		      KtuiTableSpan span, void *user)
{
	int i = from;

	while (i >= 0 && i < count && span && span(i, user) == KT_TABLE_SKIP)
		i += dir ? dir : 1;
	if (i < 0 || i >= count)
		return st->sel;
	return i;
}

int ktui_table_key(KtuiTable *st, int count, int rows, int k,
		   KtuiTableSpan span, void *user)
{
	int prev = st->sel;
	int dir = 1;

	if (k == KT_K_UP) {
		st->sel--;
		dir = -1;
	} else if (k == KT_K_DOWN) {
		st->sel++;
	} else if (k == KT_K_PGUP) {
		st->sel -= rows;
		dir = -1;
	} else if (k == KT_K_PGDN) {
		st->sel += rows;
	} else if (k == KT_K_HOME) {
		st->sel = 0;
	} else if (k == KT_K_END) {
		st->sel = count - 1;
		dir = -1;
	} else {
		return 0;
	}

	if (st->sel < 0)
		st->sel = 0;
	if (st->sel >= count)
		st->sel = count ? count - 1 : 0;
	st->sel = table_step(st, count, st->sel, dir, span, user);
	ktui_table_clamp(st, count, rows);
	return st->sel != prev;
}

/* The row a click lands on, refused when it is a skipped heading. */
int ktui_table_pick(KtuiTable *st, int count, int idx, KtuiTableSpan span,
		    void *user)
{
	if (idx < 0 || idx >= count)
		return 0;
	if (span && span(idx, user) == KT_TABLE_SKIP)
		return 0;
	st->sel = idx;
	return 1;
}

int ktui_table_hit(KRect r, const KtuiTable *st, int count, int ncol,
		   const KtuiCol *col, int mx, int my)
{
	int head = 0;

	for (int i = 0; i < ncol && i < KT_TABLE_COLS; i++)
		if (col[i].title)
			head = 2;
	if (!krect_hit(r, mx, my) || my < r.y + head)
		return -1;

	int idx = st->top + (my - r.y - head);
	return idx >= 0 && idx < count ? idx : -1;
}

/* ────────────────────────────────────────────────────────────────────────
 * Dropdown — a choice that shows what else there is
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * THE OPEN LIST IS DRAWN OVER WHATEVER IS UNDER IT and clamped to the screen
 * bottom, so it is drawn LAST by its surface. A dropdown drawn in reading
 * order paints the rows it is over on the frame after the one that opened it.
 */
void ktui_dropdown_draw(KRect r, const KtuiDrop *d, const char *const *opt,
			int n, int focus)
{
	int fg = focus ? KT_SURFACE : KT_TEXT;
	int bg = focus ? KT_ACCENT : KT_SURFACE;
	int sel = d->sel >= 0 && d->sel < n ? d->sel : 0;

	ktui_draw_fill(r, bg);
	ktui_draw_text(r.x + 1, r.y, r.w - 3, n ? opt[sel] : "—", fg, bg,
		       KT_A_NONE);
	ktui_draw_text(r.x + r.w - 2, r.y, 1,
		       ktui_glyph[d->open ? KT_G_UP : KT_G_DOWN], fg, bg,
		       KT_A_NONE);
}

void ktui_dropdown_draw_open(KRect r, const KtuiDrop *d, const char *const *opt,
			     int n)
{
	if (!d->open || n <= 0)
		return;

	int h = n;
	if (h > ktui_h - r.y - 1)
		h = ktui_h - r.y - 1;
	if (h < 1)
		return;

	int top = 0;
	if (d->hi >= h)
		top = d->hi - h + 1;

	KRect list = krect(r.x, r.y + 1, r.w, h);
	ktui_draw_fill(list, KT_SURFACE);
	for (int i = 0; i < h; i++) {
		int idx = top + i;
		int on = idx == d->hi;

		ktui_draw_fill(krect(list.x, list.y + i, list.w, 1),
			       on ? KT_ACCENT : KT_SURFACE);
		ktui_draw_text(list.x + 1, list.y + i, list.w - 2, opt[idx],
			       on ? KT_SURFACE : KT_TEXT,
			       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
	}
	ktui_draw_shadow(list);
}

int ktui_dropdown_key(KtuiDrop *d, int n, int k)
{
	if (!d->open) {
		if (k != KT_K_ENTER && k != ' ' && k != KT_K_DOWN)
			return 0;
		d->open = 1;
		d->hi = d->sel;
		return 0;
	}

	if (k == KT_K_UP && d->hi > 0)
		d->hi--;
	else if (k == KT_K_DOWN && d->hi + 1 < n)
		d->hi++;
	else if (k == KT_K_HOME)
		d->hi = 0;
	else if (k == KT_K_END)
		d->hi = n - 1;
	else if (k == KT_K_ESC)
		d->open = 0;
	else if (k == KT_K_ENTER || k == ' ') {
		int changed = d->hi != d->sel;
		d->sel = d->hi;
		d->open = 0;
		return changed;
	}
	return 0;
}

int ktui_dropdown_hit(KRect r, KtuiDrop *d, int n, int mx, int my)
{
	if (krect_hit(r, mx, my)) {
		d->open = !d->open;
		d->hi = d->sel;
		return 0;
	}
	if (!d->open)
		return 0;

	int i = my - r.y - 1;
	if (mx < r.x || mx >= r.x + r.w || i < 0 || i >= n) {
		/* A click anywhere else closes it WITHOUT choosing, which is
		 * the only reading of a press outside a list that is over the
		 * thing it would otherwise hit. */
		d->open = 0;
		return 0;
	}
	int changed = i != d->sel;
	d->sel = i;
	d->open = 0;
	return changed;
}

/* ────────────────────────────────────────────────────────────────────────
 * Text area — a block of lines with a caret
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * FIXED-WIDTH LINES, NOT A GAP BUFFER. Every caller here holds its text as an
 * array of bounded strings it can write to a file line by line; a rope would
 * make the widget the owner of the storage and the caller a serialiser of it.
 * `stride` is the size of one line INCLUDING its terminator.
 */
#define TA_LINE(t, s, i) ((t) + (size_t)(i) * (s))

void ktui_textarea_draw(KRect r, KtuiTextArea *ta, const char *text,
			int nlines, size_t stride, int fg, int bg)
{
	if (ta->cy < ta->top)
		ta->top = ta->cy;
	if (ta->cy >= ta->top + r.h)
		ta->top = ta->cy - r.h + 1;
	if (ta->top < 0)
		ta->top = 0;

	for (int i = 0; i < r.h; i++) {
		int idx = ta->top + i;

		ktui_draw_fill(krect(r.x, r.y + i, r.w, 1), bg);
		if (idx < nlines)
			ktui_draw_text(r.x, r.y + i, r.w,
				       TA_LINE(text, stride, idx), fg, bg,
				       KT_A_NONE);
	}
	ktui_term_caret(r.x + ta->cx, r.y + (ta->cy - ta->top));
}

int ktui_textarea_key(KtuiTextArea *ta, char *text, int *nlines, int maxlines,
		      size_t stride, int k)
{
	char *cur = TA_LINE(text, stride, ta->cy);
	int len = (int)strlen(cur);

	if (ta->cx > len)
		ta->cx = len;

	switch (k) {
	case KT_K_ENTER:
		if (*nlines >= maxlines)
			return 0;
		memmove(TA_LINE(text, stride, ta->cy + 2),
			TA_LINE(text, stride, ta->cy + 1),
			stride * (size_t)(*nlines - ta->cy - 1));
		memcpy(TA_LINE(text, stride, ta->cy + 1), cur + ta->cx,
		       (size_t)(len - ta->cx) + 1);
		cur[ta->cx] = '\0';
		(*nlines)++;
		ta->cy++;
		ta->cx = 0;
		return 1;
	case KT_K_BACKSPACE:
		if (ta->cx > 0) {
			memmove(cur + ta->cx - 1, cur + ta->cx,
				(size_t)(len - ta->cx) + 1);
			ta->cx--;
			return 1;
		}
		if (ta->cy > 0) {
			char *prev = TA_LINE(text, stride, ta->cy - 1);
			int plen = (int)strlen(prev);

			/* A join that would not fit is REFUSED rather than
			 * truncated: a line silently losing its tail is the
			 * one edit a person cannot undo by typing. */
			if (plen + len + 1 > (int)stride)
				return 0;
			memcpy(prev + plen, cur, (size_t)len + 1);
			memmove(TA_LINE(text, stride, ta->cy),
				TA_LINE(text, stride, ta->cy + 1),
				stride * (size_t)(*nlines - ta->cy - 1));
			(*nlines)--;
			ta->cy--;
			ta->cx = plen;
			return 1;
		}
		return 0;
	case KT_K_DEL:
		if (ta->cx < len) {
			memmove(cur + ta->cx, cur + ta->cx + 1,
				(size_t)(len - ta->cx));
			return 1;
		}
		return 0;
	case KT_K_UP:
		if (ta->cy > 0)
			ta->cy--;
		return 0;
	case KT_K_DOWN:
		if (ta->cy + 1 < *nlines)
			ta->cy++;
		return 0;
	case KT_K_LEFT:
		if (ta->cx > 0)
			ta->cx--;
		else if (ta->cy > 0)
			ta->cx = (int)strlen(TA_LINE(text, stride, --ta->cy));
		return 0;
	case KT_K_RIGHT:
		if (ta->cx < len)
			ta->cx++;
		else if (ta->cy + 1 < *nlines) {
			ta->cy++;
			ta->cx = 0;
		}
		return 0;
	case KT_K_HOME:
		ta->cx = 0;
		return 0;
	case KT_K_END:
		ta->cx = len;
		return 0;
	default:
		if (k >= 0x20 && k < 0x7f && len + 1 < (int)stride) {
			memmove(cur + ta->cx + 1, cur + ta->cx,
				(size_t)(len - ta->cx) + 1);
			cur[ta->cx++] = (char)k;
			return 1;
		}
		return 0;
	}
}

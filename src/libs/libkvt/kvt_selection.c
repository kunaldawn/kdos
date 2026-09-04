/*
 * libtsm - Screen Selections
 *
 * Copyright (c) 2011-2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Screen Selections
 * If a running pty-client does not support mouse-tracking extensions, a
 * terminal can manually mark selected areas if it does mouse-tracking itself.
 * This tracking is slightly different than the integrated client-tracking:
 *
 * Initial state is no-selection. At any time selection_reset() can be called to
 * clear the selection and go back to initial state.
 * If the user presses a mouse-button, the terminal can calculate the selected
 * cell and call selection_start() to notify the terminal that the user started
 * the selection. While the mouse-button is held down, the terminal should call
 * selection_target() whenever a mouse-event occurs. This will tell the screen
 * layer to draw the selection from the initial start up to the last given
 * target.
 * Please note that the selection-start cannot be modified by the terminal
 * during a selection. Instead, the screen-layer automatically moves it along
 * with any scroll-operations or inserts/deletes. This also means, the terminal
 * must _not_ cache the start-position itself as it may change under the hood.
 * This selection takes also care of scrollback-buffer selections and correctly
 * moves selection state along.
 *
 * Please note that this is not the kind of selection that some PTY applications
 * support. If the client supports the mouse-protocol, then it can also control
 * a separate screen-selection which is always inside of the actual screen. This
 * is a totally different selection.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "kvt.h"
#include "kvt_int.h"
#include "kvt_llog.h"
#include "kvt_dlist.h"

#define LLOG_SUBSYSTEM "tsm-selection"

static void selection_set(struct kvt_screen *con, struct selection_pos *sel,
			  unsigned int x, unsigned int y)
{
	struct line *line;

	sel->x = x;

	if (!con->sb.pos) {
		sel->line = con->lines[y];
		return;
	}
	if (con->sb.pos_num + y >= con->sb.count) {
		y -= con->sb.count - con->sb.pos_num;
		sel->line = con->lines[y];
		return;
	}
	line = con->sb.pos;
	while (y--)
		line = kvt_shl_dlist_next(line, &con->sb.list, list);
	sel->line = line;
}

static void word_select(struct kvt_screen *con,
			unsigned int posx,
			unsigned int posy)
{
	int start, end;
	struct line *line;

	selection_set(con, &con->sel_start, posx, posy);

	line = con->sel_start.line;

	if (!line || line->cells[posx].ch == ' ')
		return;

	for (start = posx; start >= 0; start--) {
		if (line->cells[start].ch == ' ') {
			start++;
			break;
		}
	}
	if (start < 0)
		start = 0;

	for (end = posx; end < (int)line->size; end++) {
		if (line->cells[end].ch == ' ' || line->cells[end].ch == '\n' ||
		    line->cells[end].ch == '\0') {
			end--;
			break;
		}
	}
	con->sel_start.x = start;
	con->sel_end.x = end;
	con->sel_end.line = line;
	con->sel_active = true;
}

KVT_SHL_EXPORT
void kvt_screen_selection_reset(struct kvt_screen *con)
{
	if (!con)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	con->sel_active = false;
	con->sel_start.line = NULL;
	con->sel_end.line = NULL;
}

/* calculates the line length from the beginning to the last non zero character */
static unsigned int calc_line_len(struct line *line)
{
	int i;

	for (i = line->size - 1; i >= 0; i--)
		if (line->cells[i].ch != 0)
			return i + 1;
	return 0;
}

/* TODO: kvt_ucs4_to_utf8 expects UCS4 characters, but a cell contains a
 * tsm-symbol (which can contain multiple UCS4 chars). Fix this when introducing
 * support for combining characters. */
static unsigned int copy_line(struct kvt_screen *con, struct line *line, char *buf)
{
	unsigned int i, start, end;
	char *pos = buf;
	int line_len;

	line_len = calc_line_len(line);
	start = (con->sel_start.line == line) ? con->sel_start.x : 0;
	end = (con->sel_end.line == line) ? con->sel_end.x + 1 : con->size_x;

	/* Compared in signed space: both offsets are bounded by the screen
	 * width, and calc_line_len returns a length. */
	if ((int)start > line_len)
		return 0;

	if ((int)end > line_len)
		end = (unsigned int)line_len;

	for (i = start; i < end; i++) {
		if (line->cells[i].ch)
			pos += kvt_ucs4_to_utf8(line->cells[i].ch, pos);
		else
			pos += kvt_ucs4_to_utf8(' ', pos);
	}
	pos += kvt_ucs4_to_utf8('\n', pos);
	return pos - buf;
}

/*
 * Returns true if a is before b in terminal order
 */
static bool selection_is_before(struct kvt_screen *con, struct selection_pos *a, struct selection_pos *b)
{
	int i;

	if (a->line == b->line)
		return (a->x < b->x);

	if (is_in_scrollback(a) != is_in_scrollback(b))
		return (is_in_scrollback(a));

	if (is_in_scrollback(a) && is_in_scrollback(b))
		return (a->line->sb_id < b->line->sb_id);

	/* so both are not in scroll back buffer and are not on the same line */
	for (i = 0; i < (int)con->size_y; i++) {
		if (con->lines[i] == b->line)
			return false;

		if (con->lines[i] == a->line)
			return true;
	}
	// Should not happen
	return true;
}

KVT_SHL_EXPORT
void kvt_screen_selection_start(struct kvt_screen *con,
				unsigned int posx,
				unsigned int posy)
{
	if (!con || posx >= con->size_x || posy >= con->size_y)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	con->sel_active = true;
	selection_set(con, &con->sel_begin, posx, posy);
	con->sel_start = con->sel_begin;
	con->sel_end = con->sel_begin;
}

KVT_SHL_EXPORT
void kvt_screen_selection_target(struct kvt_screen *con,
				 unsigned int posx,
				 unsigned int posy)
{
	struct selection_pos target;

	if (!con || !con->sel_active || posx >= con->size_x || posy >= con->size_y)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	selection_set(con, &target, posx, posy);
	if (selection_is_before(con, &con->sel_begin, &target)) {
		con->sel_start = con->sel_begin;
		con->sel_end = target;
	} else {
		con->sel_start = target;
		con->sel_end = con->sel_begin;
	}
}

KVT_SHL_EXPORT
void kvt_screen_selection_word(struct kvt_screen *con,
			       unsigned int posx,
			       unsigned int posy)
{
	if (!con || posx >= con->size_x || posy >= con->size_y)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	word_select(con, posx, posy);
}

/*
 * Get the index of a line in the screen
 *
 * If the line is in the scroll back buffer, return 0
 * Otherwise, return the index of the line in the screen
 */
static unsigned int get_line_index(struct kvt_screen *con, struct line *line)
{
	unsigned int i = 0;

	if (line->sb_id)
		return 0;

	for (i = 0; i < con->size_y; i++) {
		if (con->lines[i] == line)
			return i;
	}
	return 0;
}

static struct line *get_next_line(struct kvt_screen *con, struct line *line, unsigned int *index)
{
	struct line *next;

	if (line->sb_id) {
		next = kvt_shl_dlist_next(line, &con->sb.list, list);
		if (next)
			return next;
		*index = 0;
		return con->lines[0];
	} else if (*index < con->size_y - 1) {
		(*index)++;
		return con->lines[*index];
	}
	return NULL;
}

static int selection_count_lines(struct kvt_screen *con, struct selection_pos *start, struct selection_pos *end)
{
	int count = 1;
	unsigned int index = get_line_index(con, start->line);
	struct line *iter;

	iter = start->line;
	while (iter && iter != end->line) {
		count++;
		iter = get_next_line(con, iter, &index);
	}
	return count;
}

/*
 * Calculate the maximum needed space for the number of lines given
 */
static unsigned int calc_line_copy_buffer(struct kvt_screen *con, unsigned int num_lines)
{
	// 4 is the max size of a Unicode character
	return con->size_x * num_lines * 4 + 1;
}

static int copy_lines(struct kvt_screen *con, struct selection_pos *start, struct selection_pos *end, char *buf, int pos)
{
	unsigned int index = get_line_index(con, start->line);
	struct line *iter;

	iter = start->line;
	while (iter) {
		pos += copy_line(con, iter, &(buf[pos]));
		if (iter == end->line)
			break;
		iter = get_next_line(con, iter, &index);
	}
	return pos;
}

KVT_SHL_EXPORT
int kvt_screen_selection_copy(struct kvt_screen *con, char **out)
{
	struct selection_pos *start = &con->sel_start;
	struct selection_pos *end = &con->sel_end;
	int buf_size = 0;
	int pos = 0;
	int total_lines;

	if (!con || !out) {
		return -EINVAL;
	}

	if (!con->sel_active) {
		return -ENOENT;
	}

	/* invalid selection */
	if (start->line == NULL && end->line == NULL) {
		*out = strdup("");
		return 0;
	}

	if (start->line == NULL) {
		if (!kvt_shl_dlist_empty(&con->sb.list))
			start->line = kvt_shl_dlist_first(&con->sb.list, struct line, list);
		else
			start->line = con->lines[0];
		start->x = 0;
	}

	total_lines =  selection_count_lines(con, start, end);
	buf_size = calc_line_copy_buffer(con, total_lines);

	*out = calloc(buf_size, 1);
	if (!*out) {
		return -ENOMEM;
	}

	pos = copy_lines(con, start, end, *out, pos);

	/* remove last line break */
	if (pos > 0) {
		(*out)[--pos] = '\0';
	}

	return pos;
}

/*
 * ── the pointer over a terminal ──────────────────────────────────────────
 *
 * One implementation for both desktops. See kvt.h for why it is here rather
 * than in either program.
 */

int kvt_ui_mouse(struct kvt_term *t, KvtUi *ui, const KtuiEvent *ev,
		 double now, char **copied)
{
	struct kvt_screen *sc;

	if (!t || !ui || !ev || ev->type != KT_EVT_MOUSE)
		return 0;

	sc = kvt_term_screen(t);
	if (!sc)
		return 0;

	if (ev->btn == KT_MB_WHEEL_UP || ev->btn == KT_MB_WHEEL_DOWN) {
		int up = ev->btn == KT_MB_WHEEL_UP;

		/*
		 * THE WHEEL GOES TO THE CHILD FIRST. A program on the
		 * alternate screen has its own idea of scrolling and the
		 * scrollback is empty while it is up, so scrolling the
		 * scrollback there moves nothing and eats the event.
		 *
		 * Shift is the override every terminal keeps: it hands the
		 * pointer back so text can be taken out of a program that
		 * captured it.
		 */
		if (!(ev->mods & KT_MOD_SHIFT) &&
		    kvt_term_mouse(t, ev->mx, ev->my,
				   up ? KVT_MOUSE_BUTTON_WHEEL_UP
				      : KVT_MOUSE_BUTTON_WHEEL_DOWN,
				   ev->mods, KVT_MOUSE_EVENT_PRESSED))
			return 0;
		kvt_term_scroll(t, up ? -3 : 3);
		return 0;
	}

	if (!(ev->mods & KT_MOD_SHIFT) && kvt_term_mouse_mode(t)) {
		int btn = ev->btn == KT_MB_MIDDLE ? KVT_MOUSE_BUTTON_MIDDLE
			  : ev->btn == KT_MB_RIGHT ? KVT_MOUSE_BUTTON_RIGHT
						   : KVT_MOUSE_BUTTON_LEFT;
		int event = ev->press == KT_MP_PRESS ? KVT_MOUSE_EVENT_PRESSED
			    : ev->press == KT_MP_RELEASE
				      ? KVT_MOUSE_EVENT_RELEASED
				      : KVT_MOUSE_EVENT_MOVED;

		if (kvt_term_mouse(t, ev->mx, ev->my, btn, ev->mods, event))
			return 0;
	}

	if (ev->btn != KT_MB_LEFT)
		return 0;

	if (ev->press == KT_MP_PRESS) {
		/*
		 * A second click in the same cell inside the double-click
		 * window takes the word. Measured in CELLS rather than pixels:
		 * a cell surface has no pixels, and a person who moved half a
		 * character did not mean a new selection.
		 */
		if (now - ui->click_at < 0.4 && ev->mx == ui->click_x &&
		    ev->my == ui->click_y) {
			kvt_screen_selection_word(sc, (unsigned)ev->mx,
						  (unsigned)ev->my);
			ui->selecting = 0;
			ui->click_at = 0;
			return 0;
		}
		ui->click_x = ev->mx;
		ui->click_y = ev->my;
		ui->click_at = now;

		kvt_screen_selection_reset(sc);
		kvt_screen_selection_start(sc, (unsigned)ev->mx,
					   (unsigned)ev->my);
		ui->sel_x = ev->mx;
		ui->sel_y = ev->my;
		ui->selecting = 1;
		return 0;
	}

	if (ev->press == KT_MP_DRAG && ui->selecting) {
		kvt_screen_selection_target(sc, (unsigned)ev->mx,
					    (unsigned)ev->my);
		return 0;
	}

	if (ev->press == KT_MP_RELEASE && ui->selecting) {
		ui->selecting = 0;
		/*
		 * A press and a release in the same cell is a CLICK, and a
		 * click selects nothing. Without this every click leaves a
		 * one-character selection on the primary, which is what
		 * middle-click then pastes.
		 */
		if (ev->mx == ui->sel_x && ev->my == ui->sel_y) {
			kvt_screen_selection_reset(sc);
			return 0;
		}
		if (copied && kvt_screen_selection_copy(sc, copied) >= 0 &&
		    *copied)
			return 1;
	}

	return 0;
}

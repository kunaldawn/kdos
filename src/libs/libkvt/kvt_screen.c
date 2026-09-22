/*
 * libtsm - Screen Management
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
 * Screen Management
 * This provides the abstracted screen management. It does not do any
 * terminal-emulation, instead it provides a resizable table of cells. You can
 * insert, remove and modify the cells freely.
 * A screen has always a fixed, but changeable, width and height. This defines
 * the number of columns and rows. The screen doesn't care for pixels, glyphs or
 * framebuffers. The screen only contains information about each cell.
 *
 * Screens are the logical model behind a real screen of a terminal emulator.
 * Users usually allocate a screen for each terminal-emulator they run. All they
 * have to do is render the screen onto their widget on each change and forward
 * any widget-events to the screen.
 *
 * The screen object already includes scrollback-buffers, selection support and
 * more. This simplifies terminal emulators a lot, but also prevents them from
 * accessing the real screen data. However, terminal emulators should have no
 * reason to access the data directly. The screen API should provide everything
 * they need.
 *
 * AGEING:
 * Each cell, line and screen has an "age" field. This field describes when it
 * was changed the last time. After drawing a screen, the current screen age is
 * returned. This allows users to skip drawing specific cells, if their
 * framebuffer was already drawn with a newer age than a given cell.
 * However, the screen-age might overflow. This is properly detected and causes
 * drawing functions to return "0" as age. Users must reset all their
 * framebuffer ages then. Otherwise, further drawing operations might
 * incorrectly skip cells.
 * Furthermore, if a cell has age "0", it means it _has_ to be drawn. No ageing
 * information is available.
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

#define LLOG_SUBSYSTEM "tsm-screen"

static struct cell *get_cursor_cell(struct kvt_screen *con)
{
	unsigned int cur_x, cur_y;

	cur_x = con->cursor_x;
	if (cur_x >= con->size_x)
		cur_x = con->size_x - 1;

	cur_y = con->cursor_y;
	if (cur_y >= con->size_y)
		cur_y = con->size_y - 1;

	return &con->lines[cur_y]->cells[cur_x];
}

static void move_cursor(struct kvt_screen *con, unsigned int x, unsigned int y)
{
	struct cell *c;

	/* if cursor is hidden, just move it */
	if (con->flags & KVT_SCREEN_HIDE_CURSOR) {
		con->cursor_x = x;
		con->cursor_y = y;
		return;
	}

	/* If cursor is visible, we have to mark the current and the new cell
	 * as changed by resetting their age. We skip it if the cursor-position
	 * didn't actually change. */

	if (con->cursor_x == x && con->cursor_y == y)
		return;

	c = get_cursor_cell(con);
	c->age = con->age_cnt;

	con->cursor_x = x;
	con->cursor_y = y;

	c = get_cursor_cell(con);
	c->age = con->age_cnt;
}

static void screen_cell_init_generic(struct kvt_screen *con, struct cell *cell, struct kvt_screen_attr *attr)
{
	cell->ch = 0;
	cell->width = 1;
	cell->age = con->age_cnt;

	memcpy(&cell->attr, attr, sizeof(cell->attr));
}

void screen_cell_init(struct kvt_screen *con, struct cell *cell)
{
	screen_cell_init_generic(con, cell, &con->def_attr);
	/* AN ERASED CELL CARRIES NO TEXT, SO IT CARRIES NO LINK. Every blank
	 * this screen makes comes through here — an erase, a resize's fill, a
	 * scroll's new line — and a default attribute copied while a link was
	 * open would otherwise leave a clickable hole in empty space. */
	cell->attr.link = 0;
}

/*
 * A ROW OF THE VIEW, which is a scrollback line while the view is scrolled
 * back and a screen line below that.
 *
 * THE WALK IS REMEMBERED. Reaching row y means stepping y links from the
 * scroll position, and the callers ask per CELL — a link under the pointer is
 * looked up for every cell of every frame — so an O(rows) walk per lookup is
 * O(rows x cells) a frame. Consecutive rows are the overwhelmingly common
 * pattern, so the last answer is kept and a request for the row after it is
 * one step. The scroll position and the scrollback's link/unlink generation
 * are part of the key: a scroll or any change to the history moves every row.
 */
struct line *screen_line_at(struct kvt_screen *con, unsigned int y)
{
	struct line *line;

	if (!con || y >= con->size_y)
		return NULL;
	if (!con->sb.pos)
		return con->lines[y];
	if (con->sb.pos_num + y >= con->sb.count)
		return con->lines[y - (con->sb.count - con->sb.pos_num)];

	if (con->at_line && con->at_pos == con->sb.pos &&
	    con->at_gen == con->sb.gen && con->at_y <= y) {
		line = con->at_line;
		for (unsigned int k = con->at_y; k < y; k++)
			line = kvt_shl_dlist_next(line, &con->sb.list, list);
	} else {
		line = con->sb.pos;
		for (unsigned int k = 0; k < y; k++)
			line = kvt_shl_dlist_next(line, &con->sb.list, list);
	}
	con->at_line = line;
	con->at_y = y;
	con->at_pos = con->sb.pos;
	con->at_gen = con->sb.gen;
	return line;
}

/*
 * ── OSC 133, the prompt marks ────────────────────────────────────────────
 *
 * A shell says where its prompt starts and what the last command exited with.
 * The mark is what makes "jump to the previous prompt" possible at all: the
 * lines are the terminal's and the shell cannot reach them.
 */
KVT_SHL_EXPORT
void kvt_screen_mark_prompt(struct kvt_screen *con)
{
	struct line *line;

	if (!con || con->cursor_y >= con->size_y)
		return;
	line = con->lines[con->cursor_y];
	if (!line)
		return;
	line->mark = 1;
	/* A NEW PROMPT HAS NO STATUS YET. The same line reached a second time
	 * — a shell redrawing its prompt after a resize — must not keep the
	 * previous command's exit code, or the mark says the next command
	 * failed before it has run. */
	line->status = -1;
	line->age = con->age_cnt;
}

struct line *screen_mark_last(struct kvt_screen *con)
{
	struct line *line;

	if (!con)
		return NULL;
	for (int y = (int)con->cursor_y; y >= 0; y--)
		if (con->lines[y] && con->lines[y]->mark)
			return con->lines[y];
	if (kvt_shl_dlist_empty(&con->sb.list))
		return NULL;
	for (line = kvt_shl_dlist_last(&con->sb.list, struct line, list);
	     line && &line->list != &con->sb.list;
	     line = kvt_shl_dlist_prev(line, &con->sb.list, list))
		if (line->mark)
			return line;
	return NULL;
}

/*
 * The exit status belongs to the PROMPT the command was typed at, which is
 * where a person looks for it — so it is walked back to rather than written
 * where the cursor happens to be when the shell reports it.
 */
KVT_SHL_EXPORT
void kvt_screen_mark_status(struct kvt_screen *con, int status)
{
	struct line *line = screen_mark_last(con);

	if (!line)
		return;
	line->status = status;
	line->age = con->age_cnt;
}

/* The mark on a VISIBLE row: 1 for a prompt, and `status` set to what the
 * command run there exited with, or -1 while it has not finished. */
KVT_SHL_EXPORT
int kvt_screen_mark_at(struct kvt_screen *con, unsigned int y, int *status)
{
	struct line *line = screen_line_at(con, y);

	if (status)
		*status = -1;
	if (!line || !line->mark)
		return 0;
	if (status)
		*status = line->status;
	return 1;
}

/*
 * THE VIEW MOVES TO THE NEXT MARK, one line at a time through the same
 * scrolling the wheel uses.
 *
 * Stepping rather than computing an offset: the scrollback is a list whose
 * position the screen owns, and a second implementation of "where am I" is a
 * second thing to get wrong when a line is added under the view. It stops when
 * a step changes nothing, which is what the end of the history is.
 */
KVT_SHL_EXPORT
int kvt_screen_scroll_to_mark(struct kvt_screen *con, int dir)
{
	unsigned int steps;

	if (!con || !dir)
		return 0;
	steps = con->sb.count + con->size_y;
	while (steps--) {
		struct line *was = screen_line_at(con, 0);

		if (dir < 0)
			kvt_screen_sb_up(con, 1);
		else
			kvt_screen_sb_down(con, 1);

		struct line *now = screen_line_at(con, 0);

		if (now == was)
			return 0;
		if (now && now->mark)
			return 1;
	}
	return 0;
}

/*
 * THE LINK A CELL CARRIES, as an id into the vte's table. 0 is none.
 *
 * The lookup goes through the same row-to-line map the selection uses, so a
 * pointer over a scrolled-back line names the link that text actually has
 * rather than the one at the same coordinate on the live screen.
 */
KVT_SHL_EXPORT
unsigned int kvt_screen_link_at(struct kvt_screen *con, unsigned int x,
				unsigned int y)
{
	struct line *line = screen_line_at(con, y);

	if (!con || !line || x >= con->size_x || x >= line->size)
		return 0;
	return line->cells[x].attr.link;
}

static int line_new(struct kvt_screen *con, struct line **out,
		    unsigned int width)
{
	struct line *line;
	unsigned int i;

	if (!width)
		return -EINVAL;

	line = malloc(sizeof(*line));
	if (!line)
		return -ENOMEM;
	line->list.next = NULL;
	line->list.prev = NULL;
	line->sb_id = 0;
	line->size = width;
	line->age = con->age_cnt;
	line->mark = 0;
	line->status = -1;

	line->cells = malloc(sizeof(struct cell) * width);
	if (!line->cells) {
		free(line);
		return -ENOMEM;
	}

	for (i = 0; i < width; ++i)
		screen_cell_init(con, &line->cells[i]);

	*out = line;
	return 0;
}

static void line_free(struct line *line)
{
	free(line->cells);
	free(line);
}

static int line_resize(struct kvt_screen *con, struct line *line,
		       unsigned int width)
{
	struct cell *tmp;

	if (!line || !width)
		return -EINVAL;

	if (line->size < width) {
		tmp = realloc(line->cells, width * sizeof(struct cell));
		if (!tmp)
			return -ENOMEM;

		line->cells = tmp;

		while (line->size < width) {
			screen_cell_init(con, &line->cells[line->size]);
			++line->size;
		}
	}

	return 0;
}

/*
 * The same two, for a line of the MAIN screen: the cells are initialised from
 * the main screen's saved defaults rather than whatever the alternate screen
 * put in force. Off the alternate screen the two are the same array and this
 * costs a copy of one attribute struct.
 */
static int new_main_line(struct kvt_screen *con, struct line **out,
			 unsigned int width)
{
	struct kvt_screen_attr save;
	int ret;

	memcpy(&save, &con->def_attr, sizeof(save));
	memcpy(&con->def_attr, &con->def_attr_main, sizeof(con->def_attr));
	ret = line_new(con, out, width);
	memcpy(&con->def_attr, &save, sizeof(con->def_attr));
	return ret;
}

static int resize_main_line(struct kvt_screen *con, struct line *line,
			    unsigned int width)
{
	struct kvt_screen_attr save;
	int ret;

	memcpy(&save, &con->def_attr, sizeof(save));
	memcpy(&con->def_attr, &con->def_attr_main, sizeof(con->def_attr));
	ret = line_resize(con, line, width);
	memcpy(&con->def_attr, &save, sizeof(con->def_attr));
	return ret;
}

static void clear_selection_on_line(struct kvt_screen *con, struct line *line)
{
	if (!con->sel_active)
		return;
	if (con->sel_begin.line == line)
		con->sel_begin.line = NULL;
	if (con->sel_start.line == line)
		con->sel_start.line = NULL;
	if (con->sel_end.line == line)
		con->sel_end.line = NULL;
}

/*
 * TURN A LINE INTO A BLANK SCREEN ROW.
 *
 * A BLANK ROW CARRIES NOTHING OF THE ROW IT REPLACES. The storage is reused —
 * from the scrollback's evicted line, from the row being scrolled off, or
 * fresh — but the OSC 133 prompt mark, the exit status, the scrollback id and
 * any selection anchored on the line all describe text that is no longer
 * there. Left behind, a recycled row reports itself as a prompt line, the next
 * exit status lands on it instead of the real prompt, and a selection follows
 * the blank down the screen.
 *
 * THE WHOLE LINE IS BLANKED, not the width of the screen. A resize only ever
 * widens a line, so a line can still be as wide as the terminal used to be —
 * and the text past the right edge is readable by a selection copy, which
 * bounds itself by the line rather than by the screen.
 */
static void line_blank(struct kvt_screen *con, struct line *line)
{
	unsigned int j;

	for (j = 0; j < line->size; ++j)
		screen_cell_init(con, &line->cells[j]);
	clear_selection_on_line(con, line);
	line->sb_id = 0;
	line->mark = 0;
	line->status = -1;
	line->age = con->age_cnt;
}

/* This links the given line into the scrollback-buffer */
/*
 * MAKE ROOM IN THE SCROLLBACK, and hand back the line that was evicted.
 *
 * Separate from the link below because the caller wants the evicted line as
 * the blank row that replaces the one being scrolled off: the two have the
 * same storage, so in the steady state — a full scrollback, which is where a
 * program printing a long file spends all its time — a scrolled row touches
 * the allocator not at all, where freeing one line and allocating an
 * identical one is two mallocs and two frees per printed row.
 *
 * Nothing is linked here, so a caller that cannot find a replacement line has
 * changed nothing and can still put the row back on the screen.
 */
static struct line *sb_evict(struct kvt_screen *con)
{
	struct line *tmp;

	if (con->sb.max == 0 || con->sb.count < con->sb.max)
		return NULL;

	tmp = kvt_shl_dlist_first(&con->sb.list, struct line, list);
	kvt_shl_dlist_unlink(&tmp->list);
	++con->sb.gen;
	--con->sb.count;

	/* Only consider sb.max > 1, so there is always another line in sb. */
	if (con->sb.pos == tmp) {
		con->sb.pos = kvt_shl_dlist_first(&con->sb.list, struct line,
						  list);
		con->sb.pos_num = 0;
	} else {
		con->sb.pos_num--;
	}
	clear_selection_on_line(con, tmp);
	return tmp;
}

static void link_to_scrollback(struct kvt_screen *con, struct line *line)
{
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (con->sb.max == 0) {
		clear_selection_on_line(con, line);
		line_free(line);
		return;
	}

	/* Room was made by sb_evict() where there was a caller to use the
	 * evicted line; make it here for one that had none. */
	if (con->sb.count >= con->sb.max) {
		struct line *tmp = sb_evict(con);

		if (tmp)
			line_free(tmp);
	}

	line->sb_id = ++con->sb.last_id;
	kvt_shl_dlist_link_tail(&con->sb.list, &line->list);
	++con->sb.gen;
	++con->sb.count;
	if (con->sb.pos == NULL)
		con->sb.pos_num = con->sb.count;
}

/* Remove num lines from scroll back to current buffer */
static void remove_from_sb(struct kvt_screen *con, unsigned int num)
{
	struct line *tmp;

	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (!con->sb.max || !con->sb.count || kvt_shl_dlist_empty(&con->sb.list))
		return;

	if (num > con->sb.count)
		num = con->sb.count;

	while (num--) {
		tmp = kvt_shl_dlist_last(&con->sb.list, struct line, list);

		if (tmp->size < con->size_x)
			if (line_resize(con, tmp, con->size_x) < 0)
				goto end_sbpos;
		kvt_shl_dlist_unlink(&tmp->list);
		++con->sb.gen;
		--con->sb.count;

		if (con->sb.pos == tmp) {
			con->sb.pos_num = con->sb.count;
			con->sb.pos = NULL;
		}
		clear_selection_on_line(con, con->lines[num]);
		line_free(con->lines[num]);
		tmp->sb_id = 0;
		con->lines[num] = tmp;
		con->lines[num]->age = con->age_cnt;
	}
end_sbpos:
	if (!con->sb.pos)
		con->sb.pos_num = con->sb.count;
}

static void screen_scroll_up(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, max, pos;
	int ret;

	if (!num)
		return;

	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	max = con->margin_bottom + 1 - con->margin_top;
	if (num > max)
		num = max;

	/* We cache lines on the stack to speed up the scrolling. However, if
	 * num is too big we might get overflows here so use recursion if num
	 * exceeds a hard-coded limit.
	 * 128 seems to be a sane limit that should never be reached but should
	 * also be small enough so we do not get stack overflows. */
	if (num > 128) {
		screen_scroll_up(con, 128);
		return screen_scroll_up(con, num - 128);
	}
	struct line *cache[num];

	for (i = 0; i < num; ++i) {
		pos = con->margin_top + i;
		/*
		 * A ROW GOES TO THE SCROLLBACK ONLY IF THERE IS A HISTORY TO
		 * TAKE IT. The alternate screen keeps none, and a terminal
		 * configured without one discards the row — in both cases the
		 * row's own storage is exactly what the blank that replaces it
		 * needs, where handing it to the allocator and asking for an
		 * identical line back is two mallocs and two frees for every
		 * row a program prints.
		 */
		if ((con->flags & KVT_SCREEN_ALTERNATE) || !con->sb.max) {
			ret = -EAGAIN;
		} else {
			/*
			 * THE LINE THE SCROLLBACK IS ABOUT TO DROP IS THE ONE
			 * THAT REPLACES THIS ROW. Taken BEFORE anything is
			 * linked, so a failure here leaves the screen exactly
			 * as it was.
			 */
			struct line *reuse = sb_evict(con);

			if (reuse && reuse->size >= con->size_x) {
				cache[i] = reuse;
				cache[i]->size = con->size_x;
				ret = 0;
			} else {
				if (reuse)
					line_free(reuse);
				ret = line_new(con, &cache[i], con->size_x);
			}
		}

		if (!ret) {
			line_blank(con, cache[i]);
			link_to_scrollback(con, con->lines[pos]);
		} else {
			/* No line to put in its place, so the row stays on
			 * the screen and is blanked where it is — also the
			 * only answer that cannot lose a row to a failed
			 * allocation. */
			cache[i] = con->lines[pos];
			line_blank(con, cache[i]);
		}
	}

	if (num < max) {
		memmove(&con->lines[con->margin_top],
			&con->lines[con->margin_top + num],
			(max - num) * sizeof(struct line*));
	}

	memcpy(&con->lines[con->margin_top + (max - num)],
	       cache, num * sizeof(struct line*));
}

static void screen_scroll_down(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, j, max;

	if (!num)
		return;

	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	max = con->margin_bottom + 1 - con->margin_top;
	if (num > max)
		num = max;

	/* see screen_scroll_up() for an explanation */
	if (num > 128) {
		screen_scroll_down(con, 128);
		return screen_scroll_down(con, num - 128);
	}
	struct line *cache[num];

	for (i = 0; i < num; ++i) {
		cache[i] = con->lines[con->margin_bottom - i];
		for (j = 0; j < con->size_x; ++j)
			screen_cell_init(con, &cache[i]->cells[j]);
	}

	if (num < max) {
		memmove(&con->lines[con->margin_top + num],
			&con->lines[con->margin_top],
			(max - num) * sizeof(struct line*));
	}

	memcpy(&con->lines[con->margin_top],
	       cache, num * sizeof(struct line*));
}

static void screen_write(struct kvt_screen *con, unsigned int x,
			  unsigned int y, kvt_symbol_t ch, unsigned int len,
			  const struct kvt_screen_attr *attr)
{
	struct line *line;
	unsigned int i;

	if (!len)
		return;

	if (x >= con->size_x || y >= con->size_y) {
		llog_warning(con, "writing beyond buffer boundary");
		return;
	}

	line = con->lines[y];

	if ((con->flags & KVT_SCREEN_INSERT_MODE) &&
	    (int)x < ((int)con->size_x - (int)len)) {
		line->age = con->age_cnt;
		memmove(&line->cells[x + len], &line->cells[x],
			sizeof(struct cell) * (con->size_x - len - x));
	}

	/*
	 * A WIDE GLYPH WHOSE HALVES ARE SPLIT IS REPAIRED FROM BOTH SIDES.
	 *
	 * Landing on the SECOND half of a double-width character leaves the
	 * first still claiming to be two cells wide, and the renderer places a
	 * continuation marker over the very cell just written — the new
	 * character is in the screen and nothing draws it. Landing on the
	 * FIRST half leaves an orphaned continuation after it, which draws as
	 * a blank the cursor can sit inside.
	 *
	 * Both halves become spaces, which is what every terminal shows when
	 * half a wide character is overwritten.
	 */
	if (x > 0 && line->cells[x - 1].width > 1) {
		line->cells[x - 1].age = con->age_cnt;
		line->cells[x - 1].ch = ' ';
		line->cells[x - 1].width = 1;
	}
	if (x + len < con->size_x && line->cells[x + len].width == 0 &&
	    line->cells[x + len].ch == 0) {
		line->cells[x + len].age = con->age_cnt;
		line->cells[x + len].ch = ' ';
		line->cells[x + len].width = 1;
	}

	line->cells[x].age = con->age_cnt;
	line->cells[x].ch = ch;
	line->cells[x].width = len;
	memcpy(&line->cells[x].attr, attr, sizeof(*attr));

	for (i = 1; i < len && i + x < con->size_x; ++i) {
		line->cells[x + i].age = con->age_cnt;
		line->cells[x + i].width = 0;
		line->cells[x + i].ch = 0;
	}
}

static void screen_erase_region(struct kvt_screen *con,
				 unsigned int x_from,
				 unsigned int y_from,
				 unsigned int x_to,
				 unsigned int y_to,
				 bool protect)
{
	unsigned int to;
	struct line *line;

	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (y_to >= con->size_y)
		y_to = con->size_y - 1;
	if (x_to >= con->size_x)
		x_to = con->size_x - 1;

	for ( ; y_from <= y_to; ++y_from) {
		line = con->lines[y_from];
		if (!line) {
			x_from = 0;
			continue;
		}

		if (y_from == y_to)
			to = x_to;
		else
			to = con->size_x - 1;
		for ( ; x_from <= to; ++x_from) {
			if (protect && line->cells[x_from].attr.protect)
				continue;

			screen_cell_init(con, &line->cells[x_from]);
		}
		x_from = 0;
	}
}

/* The horizontal axis has no relative-origin mode, so this is the identity —
 * it exists so the two axes read alike at every call site. */
static inline unsigned int to_abs_x(struct kvt_screen *con, unsigned int x)
{
	(void)con;
	return x;
}

static inline unsigned int to_abs_y(struct kvt_screen *con, unsigned int y)
{
	if (!(con->flags & KVT_SCREEN_REL_ORIGIN))
		return y;

	return con->margin_top + y;
}

static void reset_scrollback_position(struct kvt_screen *con)
{
	con->sb.pos = NULL;
	con->sb.pos_num = con->sb.count;
}

KVT_SHL_EXPORT
int kvt_screen_new(struct kvt_screen **out, kvt_log_t log, void *log_data)
{
	struct kvt_screen *con;
	int ret;
	unsigned int i;

	if (!out)
		return -EINVAL;

	con = malloc(sizeof(*con));
	if (!con)
		return -ENOMEM;

	memset(con, 0, sizeof(*con));
	con->ref = 1;
	con->llog = log;
	con->llog_data = log_data;
	con->age_cnt = 1;
	con->age = con->age_cnt;
	con->def_attr.fr = 255;
	con->def_attr.fg = 255;
	con->def_attr.fb = 255;
	kvt_shl_dlist_init(&con->sb.list);

	ret = kvt_symbol_table_new(&con->sym_table);
	if (ret)
		goto err_free;

	ret = kvt_screen_resize(con, 80, 24);
	if (ret)
		goto err_free;

	llog_debug(con, "new screen");
	*out = con;

	return 0;

err_free:
	for (i = 0; i < con->line_num; ++i) {
		line_free(con->main_lines[i]);
		line_free(con->alt_lines[i]);
	}
	free(con->main_lines);
	free(con->alt_lines);
	free(con->tab_ruler);
	kvt_symbol_table_unref(con->sym_table);
	free(con);
	return ret;
}

KVT_SHL_EXPORT
void kvt_screen_ref(struct kvt_screen *con)
{
	if (!con)
		return;

	++con->ref;
}

KVT_SHL_EXPORT
void kvt_screen_unref(struct kvt_screen *con)
{
	unsigned int i;

	if (!con || !con->ref || --con->ref)
		return;

	llog_debug(con, "destroying screen");
	kvt_screen_clear_sb(con);

	for (i = 0; i < con->line_num; ++i) {
		line_free(con->main_lines[i]);
		line_free(con->alt_lines[i]);
	}
	free(con->main_lines);
	free(con->alt_lines);
	free(con->tab_ruler);
	kvt_symbol_table_unref(con->sym_table);
	free(con);
}

KVT_SHL_EXPORT
unsigned int kvt_screen_get_width(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->size_x;
}

KVT_SHL_EXPORT
unsigned int kvt_screen_get_height(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->size_y;
}

KVT_SHL_EXPORT
int kvt_screen_resize(struct kvt_screen *con, unsigned int x,
		      unsigned int y)
{
	struct line **cache;
	unsigned int i, width, diff;
	int ret;
	bool *tab_ruler;

	if (!con || !x || !y)
		return -EINVAL;

	if (con->size_x == x && con->size_y == y)
		return 0;

	/* First make sure the line buffer is big enough for our new screen.
	 * That is, allocate all new lines and make sure each line has enough
	 * cells to hold the new screen or the current screen. If we fail, we
	 * can safely return -ENOMEM and the buffer is still valid. We must
	 * allocate the new lines to at least the same size as the current
	 * lines. Otherwise, if this function fails in later turns, we will have
	 * invalid lines in the buffer. */
	if (y > con->line_num) {
		/* resize main buffer */
		cache = realloc(con->main_lines, sizeof(struct line*) * y);
		if (!cache)
			return -ENOMEM;

		if (con->lines == con->main_lines)
			con->lines = cache;
		con->main_lines = cache;

		/* resize alt buffer */
		cache = realloc(con->alt_lines, sizeof(struct line*) * y);
		if (!cache)
			return -ENOMEM;

		if (con->lines == con->alt_lines)
			con->lines = cache;
		con->alt_lines = cache;

		/* allocate new lines */
		if (x > con->size_x)
			width = x;
		else
			width = con->size_x;

		/*
		 * A NEW MAIN-SCREEN CELL TAKES THE MAIN SCREEN'S DEFAULTS.
		 * While the alternate screen is up the default attributes in
		 * force are the alternate's; a full-screen program that sets
		 * its own background and is then resized would otherwise leave
		 * the main screen's new rows in that background, which is what
		 * the shell scrolls back into when the program exits.
		 */
		while (con->line_num < y) {
			ret = new_main_line(con,
					    &con->main_lines[con->line_num],
					    width);
			if (ret)
				return ret;

			ret = line_new(con, &con->alt_lines[con->line_num],
				       width);
			if (ret) {
				line_free(con->main_lines[con->line_num]);
				return ret;
			}

			++con->line_num;
		}
	}

	/* Resize all lines in the buffer if we increase screen width. This
	 * will guarantee that all lines are big enough so we can resize the
	 * buffer without reallocating them later. */
	if (x > con->size_x) {
		tab_ruler = realloc(con->tab_ruler, sizeof(bool) * x);
		if (!tab_ruler)
			return -ENOMEM;
		con->tab_ruler = tab_ruler;

		for (i = 0; i < con->line_num; ++i) {
			ret = resize_main_line(con, con->main_lines[i], x);
			if (ret)
				return ret;
			ret = line_resize(con, con->alt_lines[i], x);
			if (ret)
				return ret;
		}
	}

	screen_inc_age(con);

	/* xterm destroys margins on resize, so do we */
	con->margin_top = 0;
	con->margin_bottom = con->size_y - 1;

	/* reset tabs */
	for (i = 0; i < x; ++i) {
		if (i % 8 == 0)
			con->tab_ruler[i] = true;
		else
			con->tab_ruler[i] = false;
	}

	/* We need to adjust x-size first as screen_scroll_up() and friends may
	 * have to reallocate lines. The y-size is adjusted after them to avoid
	 * missing lines when shrinking y-size.
	 * We need to carefully look for the functions that we call here as they
	 * have stronger invariants as when called normally. */

	con->size_x = x;
	if (con->cursor_x >= con->size_x)
		move_cursor(con, con->size_x - 1, con->cursor_y);

	/* scroll buffer if screen height shrinks */
	if (y < con->size_y) {
		diff = con->size_y - y;
		if (kvt_shl_dlist_empty(&con->sb.list) || (con->flags & KVT_SCREEN_ALTERNATE)) {
			/* If there is nothing in the scrollback buffer,
			 * Only scroll up if the cursor would go off-screen */
			if (con->cursor_y >= y) {
				diff = con->cursor_y - y + 1;
				kvt_screen_scroll_up(con, diff);
				move_cursor(con, con->cursor_x, y - 1);
			}
		} else {
			kvt_screen_scroll_up(con, diff);
			if (con->cursor_y > diff)
				move_cursor(con, con->cursor_x, con->cursor_y - diff);
			else
			 	move_cursor(con, con->cursor_x, 0);
		}
	} else if (y > con->size_y) {
		diff = y - con->size_y;
		if (diff > con->sb.count)
			diff = con->sb.count;
		/*
		 * When increasing the terminal number of rows, we can move some
		 * lines from the scrollback buffer to the main buffer.
		 */
		if (diff && !(con->flags & KVT_SCREEN_ALTERNATE)) {
			con->size_y = y;
			con->margin_bottom = con->size_y - 1;
			kvt_screen_scroll_down(con, diff);
			remove_from_sb(con, diff);
			move_cursor(con, con->cursor_x, con->cursor_y + diff);
		}
	}

	con->size_y = y;
	con->margin_bottom = con->size_y - 1;
	if (con->cursor_y >= con->size_y)
		move_cursor(con, con->cursor_x, con->size_y - 1);

	return 0;
}

KVT_SHL_EXPORT
int kvt_screen_set_margins(struct kvt_screen *con,
			       unsigned int top, unsigned int bottom)
{
	unsigned int upper, lower;

	if (!con)
		return -EINVAL;

	if (!top)
		top = 1;

	if (bottom <= top) {
		upper = 0;
		lower = con->size_y - 1;
	} else if (bottom > con->size_y) {
		upper = 0;
		lower = con->size_y - 1;
	} else {
		upper = top - 1;
		lower = bottom - 1;
	}

	con->margin_top = upper;
	con->margin_bottom = lower;
	return 0;
}

/*
 * SET THE SCROLLBACK LIMIT. Lowering it discards the oldest lines
 * immediately and drags the scroll position down with them.
 *
 * EVERY LINE THAT LEAVES THE HEAD MOVES sb.pos_num, which is the index of
 * sb.pos within the scrollback and is what screen_line_at() subtracts from
 * sb.count to reach the screen. Dropping a line without it leaves pos_num
 * above count and that subtraction wraps, so a row lookup indexes the screen
 * from far outside it. The fixup mirrors sb_evict()'s exactly, because two
 * spellings of one rule drift.
 */
KVT_SHL_EXPORT
void kvt_screen_set_max_sb(struct kvt_screen *con,
			       unsigned int max)
{
	struct line *line;

	if (!con)
		return;

	// Don't allow only one line in the scrollback buffer, this simplifies
	// the code, and is not a useful usecase.
	if (max == 1)
		max = 2;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	while (con->sb.count > max) {
		line = kvt_shl_dlist_first(&con->sb.list, struct line, list);
		kvt_shl_dlist_unlink(&line->list);
		++con->sb.gen;
		--con->sb.count;

		if (con->sb.pos == line) {
			con->sb.pos = kvt_shl_dlist_first(&con->sb.list,
							  struct line, list);
			con->sb.pos_num = 0;
		} else if (con->sb.pos) {
			--con->sb.pos_num;
		}

		clear_selection_on_line(con, line);
		line_free(line);
	}

	/* An emptied list has no first line to hold the position, and
	 * kvt_shl_dlist_first() on one returns the head read as a line. */
	if (kvt_shl_dlist_empty(&con->sb.list)) {
		con->sb.pos = NULL;
		con->sb.pos_num = con->sb.count;
	}
	con->sb.max = max;
}

/* clear scrollback buffer */
KVT_SHL_EXPORT
void kvt_screen_clear_sb(struct kvt_screen *con)
{
	struct kvt_shl_dlist *iter, *safe;
	struct line *tmp;

	if (!con)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (con->sel_active) {
		if (con->sel_begin.line && is_in_scrollback(&con->sel_begin))
			con->sel_begin.line = NULL;
		if (con->sel_start.line && is_in_scrollback(&con->sel_start))
			con->sel_start.line = NULL;
		if (con->sel_end.line && is_in_scrollback(&con->sel_end))
			con->sel_end.line = NULL;
	}
	kvt_shl_dlist_for_each_safe(iter, safe, &con->sb.list) {
		tmp = kvt_shl_dlist_entry(iter, struct line, list);
		kvt_shl_dlist_unlink(&tmp->list);
		++con->sb.gen;
		line_free(tmp);
	}
	con->sb.count = 0;
	con->sb.pos = NULL;
	con->sb.pos_num = 0;
}

KVT_SHL_EXPORT
void kvt_screen_sb_up(struct kvt_screen *con, unsigned int num)
{
	struct line *prev;

	if (!con || !num)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (kvt_shl_dlist_empty(&con->sb.list))
		return;

	while (num--) {
		if (con->sb.pos) {
			if (con->sb.pos_num == 0)
				return;

			prev = kvt_shl_dlist_prev(con->sb.pos, &con->sb.list, list);
			if (!prev) {
				llog_error(con, "prev is NULL, con->sb.pos_num: %d con->sb.count: %d",
					   con->sb.pos_num, con->sb.count);
				return;
			}
			--con->sb.pos_num;
			con->sb.pos = prev;
		} else {
			con->sb.pos = kvt_shl_dlist_last(&con->sb.list, struct line, list);
			con->sb.pos_num = con->sb.count - 1;
		}
	}
}

KVT_SHL_EXPORT
void kvt_screen_sb_down(struct kvt_screen *con, unsigned int num)
{
	if (!con || !num)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	while (num-- && con->sb.pos && con->sb.pos_num < con->sb.count) {
			con->sb.pos = kvt_shl_dlist_next(con->sb.pos, &con->sb.list, list);
			++con->sb.pos_num;
	}
	if (con->sb.pos_num == con->sb.count)
		con->sb.pos = NULL;
}

KVT_SHL_EXPORT
void kvt_screen_sb_reset(struct kvt_screen *con)
{
	if (!con || !con->sb.pos)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	con->sb.pos = NULL;
	con->sb.pos_num = con->sb.count;
}

KVT_SHL_EXPORT
void kvt_screen_set_def_attr(struct kvt_screen *con,
				 const struct kvt_screen_attr *attr)
{
	if (!con || !attr)
		return;
	memcpy(&con->def_attr, attr, sizeof(*attr));
	if (!(con->flags & KVT_SCREEN_ALTERNATE))
		memcpy(&con->def_attr_main, attr, sizeof(*attr));
}

KVT_SHL_EXPORT
void kvt_screen_reset(struct kvt_screen *con)
{
	unsigned int i;

	if (!con)
		return;

	screen_inc_age(con);
	con->age = con->age_cnt;

	/*
	 * THE ALTERNATE SCREEN IS NOT A MODE THIS RESET OWNS. DECSTR comes
	 * through here, and a program that issues one while on the alternate
	 * screen would otherwise have the display switched back to the main
	 * buffer while the vte still believes it is on the alternate one —
	 * the later DECRST 1049 then restores a cursor into a screen that was
	 * never left. Only DECRST 47/1047/1049 and kvt_vte_reset_modes leave
	 * it.
	 */
	con->flags &= KVT_SCREEN_ALTERNATE;
	con->margin_top = 0;
	con->margin_bottom = con->size_y - 1;
	if (!(con->flags & KVT_SCREEN_ALTERNATE))
		con->lines = con->main_lines;

	for (i = 0; i < con->size_x; ++i) {
		if (i % 8 == 0)
			con->tab_ruler[i] = true;
		else
			con->tab_ruler[i] = false;
	}
	kvt_screen_selection_reset(con);
	reset_scrollback_position(con);
}

KVT_SHL_EXPORT
void kvt_screen_set_flags(struct kvt_screen *con, unsigned int flags)
{
	unsigned int old;
	struct cell *c;

	if (!con || !flags)
		return;

	screen_inc_age(con);

	old = con->flags;
	con->flags |= flags;

	if (!(old & KVT_SCREEN_ALTERNATE) && (flags & KVT_SCREEN_ALTERNATE)) {
		con->age = con->age_cnt;
		con->lines = con->alt_lines;
		kvt_screen_selection_reset(con);
		reset_scrollback_position(con);

		/* save attributes of main screen when we switch to alt screen */
		memcpy(&con->def_attr_main, &con->def_attr, sizeof(con->def_attr));
	}

	if (!(old & KVT_SCREEN_HIDE_CURSOR) &&
	    (flags & KVT_SCREEN_HIDE_CURSOR)) {
		c = get_cursor_cell(con);
		c->age = con->age_cnt;
	}

	if (!(old & KVT_SCREEN_INVERSE) && (flags & KVT_SCREEN_INVERSE))
		con->age = con->age_cnt;
}

KVT_SHL_EXPORT
void kvt_screen_reset_flags(struct kvt_screen *con, unsigned int flags)
{
	unsigned int old;
	struct cell *c;

	if (!con || !flags)
		return;

	screen_inc_age(con);

	old = con->flags;
	con->flags &= ~flags;

	if ((old & KVT_SCREEN_ALTERNATE) && (flags & KVT_SCREEN_ALTERNATE)) {
		con->age = con->age_cnt;
		con->lines = con->main_lines;
		kvt_screen_selection_reset(con);
		reset_scrollback_position(con);
	}

	if ((old & KVT_SCREEN_HIDE_CURSOR) &&
	    (flags & KVT_SCREEN_HIDE_CURSOR)) {
		c = get_cursor_cell(con);
		c->age = con->age_cnt;
	}

	if ((old & KVT_SCREEN_INVERSE) && (flags & KVT_SCREEN_INVERSE))
		con->age = con->age_cnt;
}

KVT_SHL_EXPORT
unsigned int kvt_screen_get_flags(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->flags;
}

KVT_SHL_EXPORT
unsigned int kvt_screen_get_cursor_x(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->cursor_x;
}

KVT_SHL_EXPORT
unsigned int kvt_screen_get_cursor_y(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->cursor_y;
}

KVT_SHL_EXPORT
void kvt_screen_set_tabstop(struct kvt_screen *con)
{
	if (!con || con->cursor_x >= con->size_x)
		return;

	con->tab_ruler[con->cursor_x] = true;
}

KVT_SHL_EXPORT
void kvt_screen_reset_tabstop(struct kvt_screen *con)
{
	if (!con || con->cursor_x >= con->size_x)
		return;

	con->tab_ruler[con->cursor_x] = false;
}

KVT_SHL_EXPORT
void kvt_screen_reset_all_tabstops(struct kvt_screen *con)
{
	unsigned int i;

	if (!con)
		return;

	for (i = 0; i < con->size_x; ++i)
		con->tab_ruler[i] = false;
}

KVT_SHL_EXPORT
void kvt_screen_write(struct kvt_screen *con, kvt_symbol_t ch,
			  const struct kvt_screen_attr *attr)
{
	unsigned int last, len;

	if (!con)
		return;

	len = kvt_symbol_get_width(con->sym_table, ch);
	if (!len)
		return;

	screen_inc_age(con);

	if (con->cursor_y <= con->margin_bottom ||
	    con->cursor_y >= con->size_y)
		last = con->margin_bottom;
	else
		last = con->size_y - 1;

	if (con->cursor_x >= con->size_x) {
		if (con->flags & KVT_SCREEN_AUTO_WRAP)
			move_cursor(con, 0, con->cursor_y + 1);
		else
			move_cursor(con, con->size_x - 1, con->cursor_y);
	}

	if (con->cursor_y > last) {
		move_cursor(con, con->cursor_x, last);
		screen_scroll_up(con, 1);
	}

	screen_write(con, con->cursor_x, con->cursor_y, ch, len, attr);
	move_cursor(con, con->cursor_x + len, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_newline(struct kvt_screen *con)
{
	if (!con)
		return;

	screen_inc_age(con);

	kvt_screen_move_down(con, 1, true);
	kvt_screen_move_line_home(con);
}

KVT_SHL_EXPORT
void kvt_screen_scroll_up(struct kvt_screen *con, unsigned int num)
{
	if (!con || !num)
		return;

	screen_inc_age(con);

	screen_scroll_up(con, num);
}

KVT_SHL_EXPORT
void kvt_screen_scroll_down(struct kvt_screen *con, unsigned int num)
{
	if (!con || !num)
		return;

	screen_inc_age(con);

	screen_scroll_down(con, num);
}

KVT_SHL_EXPORT
void kvt_screen_move_to(struct kvt_screen *con, unsigned int x,
			    unsigned int y)
{
	unsigned int last;

	if (!con)
		return;

	screen_inc_age(con);

	if (con->flags & KVT_SCREEN_REL_ORIGIN)
		last = con->margin_bottom;
	else
		last = con->size_y - 1;

	x = to_abs_x(con, x);
	if (x >= con->size_x)
		x = con->size_x - 1;

	y = to_abs_y(con, y);
	if (y > last)
		y = last;

	move_cursor(con, x, y);
}

KVT_SHL_EXPORT
void kvt_screen_move_up(struct kvt_screen *con, unsigned int num,
			    bool scroll)
{
	unsigned int diff, size;

	if (!con || !num)
		return;

	screen_inc_age(con);

	if (con->cursor_y >= con->margin_top)
		size = con->margin_top;
	else
		size = 0;

	diff = con->cursor_y - size;
	if (num > diff) {
		num -= diff;
		if (scroll)
			screen_scroll_down(con, num);
		move_cursor(con, con->cursor_x, size);
	} else {
		move_cursor(con, con->cursor_x, con->cursor_y - num);
	}
}

KVT_SHL_EXPORT
void kvt_screen_move_down(struct kvt_screen *con, unsigned int num,
			      bool scroll)
{
	unsigned int diff, size;

	if (!con || !num)
		return;

	screen_inc_age(con);

	if (con->cursor_y <= con->margin_bottom)
		size = con->margin_bottom + 1;
	else
		size = con->size_y;

	diff = size - con->cursor_y - 1;
	if (num > diff) {
		num -= diff;
		if (scroll)
			screen_scroll_up(con, num);
		move_cursor(con, con->cursor_x, size - 1);
	} else {
		move_cursor(con, con->cursor_x, con->cursor_y + num);
	}
}

KVT_SHL_EXPORT
void kvt_screen_move_left(struct kvt_screen *con, unsigned int num)
{
	unsigned int x;

	if (!con || !num)
		return;

	screen_inc_age(con);

	if (num > con->size_x)
		num = con->size_x;

	x = con->cursor_x;
	if (x >= con->size_x)
		x = con->size_x - 1;

	if (num > x)
		move_cursor(con, 0, con->cursor_y);
	else
		move_cursor(con, x - num, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_move_right(struct kvt_screen *con, unsigned int num)
{
	if (!con || !num)
		return;

	screen_inc_age(con);

	if (num > con->size_x)
		num = con->size_x;

	if (num + con->cursor_x >= con->size_x)
		move_cursor(con, con->size_x - 1, con->cursor_y);
	else
		move_cursor(con, con->cursor_x + num, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_move_line_home(struct kvt_screen *con)
{
	if (!con)
		return;

	screen_inc_age(con);

	move_cursor(con, 0, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_tab_right(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, j, x;

	if (!con || !num)
		return;

	screen_inc_age(con);

	x = con->cursor_x;
	for (i = 0; i < num; ++i) {
		for (j = x + 1; j < con->size_x; ++j) {
			if (con->tab_ruler[j])
				break;
		}

		x = j;
		if (x + 1 >= con->size_x)
			break;
	}

	/* tabs never cause pending new-lines */
	if (x >= con->size_x)
		x = con->size_x - 1;

	move_cursor(con, x, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_tab_left(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, x;
	int j;

	if (!con || !num)
		return;

	screen_inc_age(con);

	x = con->cursor_x;

	/* cursor_x may exceed size_x (e.g. CHT then a wide glyph at the last
	 * column); clamp before indexing tab_ruler[0..size_x-1]. */
	if (x > con->size_x)
		x = con->size_x;
	for (i = 0; i < num; ++i) {
		for (j = x - 1; j > 0; --j) {
			if (con->tab_ruler[j])
				break;
		}

		if (j <= 0) {
			x = 0;
			break;
		}
		x = j;
	}

	move_cursor(con, x, con->cursor_y);
}

KVT_SHL_EXPORT
void kvt_screen_insert_lines(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, j, max;

	if (!con || !num)
		return;

	if (con->cursor_y < con->margin_top ||
	    con->cursor_y > con->margin_bottom)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	max = con->margin_bottom - con->cursor_y + 1;
	if (num > max)
		num = max;

	struct line *cache[num];

	for (i = 0; i < num; ++i) {
		cache[i] = con->lines[con->margin_bottom - i];
		for (j = 0; j < con->size_x; ++j)
			screen_cell_init(con, &cache[i]->cells[j]);
	}

	if (num < max) {
		memmove(&con->lines[con->cursor_y + num],
			&con->lines[con->cursor_y],
			(max - num) * sizeof(struct line*));

		memcpy(&con->lines[con->cursor_y],
		       cache, num * sizeof(struct line*));
	}

	con->cursor_x = 0;
}

KVT_SHL_EXPORT
void kvt_screen_delete_lines(struct kvt_screen *con, unsigned int num)
{
	unsigned int i, j, max;

	if (!con || !num)
		return;

	if (con->cursor_y < con->margin_top ||
	    con->cursor_y > con->margin_bottom)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	max = con->margin_bottom - con->cursor_y + 1;
	if (num > max)
		num = max;

	struct line *cache[num];

	for (i = 0; i < num; ++i) {
		cache[i] = con->lines[con->cursor_y + i];
		for (j = 0; j < con->size_x; ++j)
			screen_cell_init(con, &cache[i]->cells[j]);
	}

	if (num < max) {
		memmove(&con->lines[con->cursor_y],
			&con->lines[con->cursor_y + num],
			(max - num) * sizeof(struct line*));

		memcpy(&con->lines[con->cursor_y + (max - num)],
		       cache, num * sizeof(struct line*));
	}

	con->cursor_x = 0;
}

KVT_SHL_EXPORT
void kvt_screen_insert_chars(struct kvt_screen *con, unsigned int num)
{
	struct cell *cells;
	unsigned int max, mv, i;

	if (!con || !num || !con->size_y || !con->size_x)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (con->cursor_x >= con->size_x)
		con->cursor_x = con->size_x - 1;
	if (con->cursor_y >= con->size_y)
		con->cursor_y = con->size_y - 1;

	max = con->size_x - con->cursor_x;
	if (num > max)
		num = max;
	mv = max - num;

	cells = con->lines[con->cursor_y]->cells;
	if (mv)
		memmove(&cells[con->cursor_x + num],
			&cells[con->cursor_x],
			mv * sizeof(*cells));

	for (i = 0; i < num; ++i)
		screen_cell_init(con, &cells[con->cursor_x + i]);
}

void kvt_screen_repeat_char(struct kvt_screen *con, unsigned int num)
{
	struct cell *cells;
	unsigned int max, i;

	if (!con || !num || !con->size_y || !con->size_x)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (con->cursor_x >= con->size_x)
		con->cursor_x = con->size_x - 1;
	if (con->cursor_y >= con->size_y)
		con->cursor_y = con->size_y - 1;

	if (!con->cursor_x)
		return;

	max = con->size_x - con->cursor_x;
	if (num > max)
		num = max;

	cells = con->lines[con->cursor_y]->cells;
	for (i = 0; i < num; i++)
		cells[con->cursor_x + i] = cells[con->cursor_x - 1];
	con->cursor_x += num;
}

KVT_SHL_EXPORT
void kvt_screen_delete_chars(struct kvt_screen *con, unsigned int num)
{
	struct cell *cells;
	unsigned int max, mv, i;

	if (!con || !num || !con->size_y || !con->size_x)
		return;

	screen_inc_age(con);
	/* TODO: more sophisticated ageing */
	con->age = con->age_cnt;

	if (con->cursor_x >= con->size_x)
		con->cursor_x = con->size_x - 1;
	if (con->cursor_y >= con->size_y)
		con->cursor_y = con->size_y - 1;

	max = con->size_x - con->cursor_x;
	if (num > max)
		num = max;
	mv = max - num;

	cells = con->lines[con->cursor_y]->cells;
	if (mv)
		memmove(&cells[con->cursor_x],
			&cells[con->cursor_x + num],
			mv * sizeof(*cells));

	for (i = 0; i < num; ++i)
		screen_cell_init(con, &cells[con->cursor_x + mv + i]);
}

KVT_SHL_EXPORT
void kvt_screen_erase_chars(struct kvt_screen *con, unsigned int num)
{
	unsigned int x;

	if (!con || !num)
		return;

	screen_inc_age(con);

	if (con->cursor_x >= con->size_x)
		x = con->size_x - 1;
	else
		x = con->cursor_x;

	screen_erase_region(con, x, con->cursor_y, x + num - 1, con->cursor_y,
			     false);
}

KVT_SHL_EXPORT
void kvt_screen_erase_cursor_to_end(struct kvt_screen *con,
				        bool protect)
{
	unsigned int x;

	if (!con)
		return;

	screen_inc_age(con);

	if (con->cursor_x >= con->size_x)
		x = con->size_x - 1;
	else
		x = con->cursor_x;

	screen_erase_region(con, x, con->cursor_y, con->size_x - 1,
			     con->cursor_y, protect);
}

KVT_SHL_EXPORT
void kvt_screen_erase_home_to_cursor(struct kvt_screen *con,
					 bool protect)
{
	if (!con)
		return;

	screen_inc_age(con);

	screen_erase_region(con, 0, con->cursor_y, con->cursor_x,
			     con->cursor_y, protect);
}

KVT_SHL_EXPORT
void kvt_screen_erase_current_line(struct kvt_screen *con,
				       bool protect)
{
	if (!con)
		return;

	screen_inc_age(con);

	screen_erase_region(con, 0, con->cursor_y, con->size_x - 1,
			     con->cursor_y, protect);
}

KVT_SHL_EXPORT
void kvt_screen_erase_screen_to_cursor(struct kvt_screen *con,
					   bool protect)
{
	if (!con)
		return;

	screen_inc_age(con);

	screen_erase_region(con, 0, 0, con->cursor_x, con->cursor_y, protect);
}

KVT_SHL_EXPORT
void kvt_screen_erase_cursor_to_screen(struct kvt_screen *con,
					   bool protect)
{
	unsigned int x;

	if (!con)
		return;

	screen_inc_age(con);

	if (con->cursor_x >= con->size_x)
		x = con->size_x - 1;
	else
		x = con->cursor_x;

	screen_erase_region(con, x, con->cursor_y, con->size_x - 1,
			     con->size_y - 1, protect);
}

KVT_SHL_EXPORT
void kvt_screen_erase_screen(struct kvt_screen *con, bool protect)
{
	if (!con)
		return;

	screen_inc_age(con);

	screen_erase_region(con, 0, 0, con->size_x - 1, con->size_y - 1,
			     protect);
}

KVT_SHL_EXPORT
enum kvt_screen_cursor_style kvt_screen_get_cursor_style(struct kvt_screen *con)
{
	if (!con)
		return 0;

	return con->cstyle;
}

KVT_SHL_EXPORT
void kvt_screen_set_cursor_style(struct kvt_screen *con, enum kvt_screen_cursor_style type)
{
	if (!con)
		return;

	con->cstyle = type;
}

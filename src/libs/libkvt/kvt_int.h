/*
 * TSM - Main internal header
 *
 * Copyright (c) 2018 Aetf <aetf@unlimitedcodeworks.xyz>
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

#ifndef KVT_KVT_INT_H
#define KVT_KVT_INT_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "kvt.h"
#include "kvt_dlist.h"
#include "kvt_llog.h"

#define KVT_SHL_EXPORT __attribute__((visibility("default")))

/* max combined-symbol length */
#define KVT_UCS4_MAXLEN 10

/* symbols */

struct kvt_symbol_table;

extern const kvt_symbol_t kvt_symbol_default;

int kvt_symbol_table_new(struct kvt_symbol_table **out);
void kvt_symbol_table_ref(struct kvt_symbol_table *tbl);
void kvt_symbol_table_unref(struct kvt_symbol_table *tbl);

kvt_symbol_t kvt_symbol_make(uint32_t ucs4);
kvt_symbol_t kvt_symbol_append(struct kvt_symbol_table *tbl,
			       kvt_symbol_t sym, uint32_t ucs4);
const uint32_t *kvt_symbol_get(struct kvt_symbol_table *tbl,
			       kvt_symbol_t *sym, size_t *size);
unsigned int kvt_symbol_get_width(struct kvt_symbol_table *tbl,
				  kvt_symbol_t sym);

/* utf8 state machine */

struct kvt_utf8_mach;

enum kvt_utf8_mach_state {
	KVT_UTF8_START,
	KVT_UTF8_ACCEPT,
	KVT_UTF8_REJECT,
	KVT_UTF8_EXPECT1,
	KVT_UTF8_EXPECT2,
	KVT_UTF8_EXPECT3,
};

int kvt_utf8_mach_new(struct kvt_utf8_mach **out);
void kvt_utf8_mach_free(struct kvt_utf8_mach *mach);

int kvt_utf8_mach_feed(struct kvt_utf8_mach *mach, char c);
uint32_t kvt_utf8_mach_get(struct kvt_utf8_mach *mach);
void kvt_utf8_mach_reset(struct kvt_utf8_mach *mach);

/* TSM screen */

struct cell {
	kvt_symbol_t ch;		/* stored character */
	unsigned int width;		/* character width */
	struct kvt_screen_attr attr;	/* cell attributes */
	kvt_age_t age;			/* age of the single cell */
};

struct line {
	struct kvt_shl_dlist list;		/* list node, next/prev are NULL if not in sb */
	unsigned int size;		/* real width */
	struct cell *cells;		/* actuall cells */
	uint64_t sb_id;			/* sb ID, 0 if not in sb */
	kvt_age_t age;			/* age of the whole line */
};

struct selection_pos {
	unsigned int x;			/* x offset from the start of the line */
	struct line *line;		/* line the selection is on */
};

struct kvt_scrollback {
	/* scroll-back buffer */
	unsigned int count;		/* number of lines in sb */
	struct kvt_shl_dlist list;	/* list of lines in sb */
	unsigned int max;		/* max-limit of lines in sb */
	struct line *pos;		/* current position in sb or NULL */
	unsigned int pos_num;	/* current numeric position in sb */
	uint64_t last_id;		/* last id given to sb-line */
};

struct kvt_screen {
	size_t ref;
	llog_submit_t llog;
	void *llog_data;
	unsigned int opts;
	unsigned int flags;
	struct kvt_symbol_table *sym_table;

	/* default attributes for new cells */
	struct kvt_screen_attr def_attr;

	/* save default attributes of main screen here when we switch to alt screen
	 * on resize of the alt screen we need to init the new cells of the main
	 * screen with these attributes and not the ones of the alt screen */
	struct kvt_screen_attr def_attr_main;

	/* ageing */
	kvt_age_t age_cnt;			/* current age counter */
	unsigned int age_reset : 1;		/* age-overflow flag */

	/* current buffer */
	unsigned int size_x;			/* width of screen */
	unsigned int size_y;			/* height of screen */
	unsigned int margin_top;		/* top-margin index */
	unsigned int margin_bottom;		/* bottom-margin index */
	unsigned int line_num;			/* real number of allocated lines */
	struct line **lines;			/* active lines; copy of main/alt */
	struct line **main_lines;		/* real main lines */
	struct line **alt_lines;		/* real alternative lines */
	kvt_age_t age;				/* whole screen age */

	struct kvt_scrollback sb;

	/* cursor: positions are always in-bound, but cursor_x might be
	 * bigger than size_x if new-line is pending */
	unsigned int cursor_x;			/* current cursor x-pos */
	unsigned int cursor_y;			/* current cursor y-pos */

	enum kvt_screen_cursor_style cstyle;	/* cursor shape */

	/* tab ruler */
	bool *tab_ruler;			/* tab-flag for all cells of one row */

	/* selection */
	bool sel_active;
	struct selection_pos sel_begin;		/* First cell selected */
	struct selection_pos sel_start;		/* First cell to copy in terminal order */
	struct selection_pos sel_end;		/* Last cell to copy */

	/* draw2 interface */
	struct kvt_screen_cell *cells;
	unsigned int cells_count;
};

void screen_cell_init(struct kvt_screen *con, struct cell *cell);

void kvt_screen_set_opts(struct kvt_screen *scr, unsigned int opts);
void kvt_screen_reset_opts(struct kvt_screen *scr, unsigned int opts);
unsigned int kvt_screen_get_opts(struct kvt_screen *scr);
void kvt_screen_repeat_char(struct kvt_screen *con, unsigned int num);

static inline void screen_inc_age(struct kvt_screen *con)
{
	if (!++con->age_cnt) {
		con->age_reset = 1;
		++con->age_cnt;
	}
}

static inline bool is_in_scrollback(struct selection_pos *sel) {
	return (sel->line && sel->line->sb_id);
}

/* available character sets */

typedef kvt_symbol_t kvt_vte_charset[96];

extern kvt_vte_charset kvt_vte_unicode_lower;
extern kvt_vte_charset kvt_vte_unicode_upper;
extern kvt_vte_charset kvt_vte_dec_supplemental_graphics;
extern kvt_vte_charset kvt_vte_dec_special_graphics;

#endif /* KVT_KVT_INT_H */

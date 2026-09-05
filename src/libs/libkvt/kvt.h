/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkvt — the terminal, as a state machine
 *
 * A HARD FORK OF libtsm 4.7.1 (github.com/kmscon/libtsm), which is kmscon's own
 * VT100-VT520 state machine and is MIT. Upstream describes it as "a simple
 * plain state machine without any external dependencies" needing only "an
 * ISO-C compatible C library" — which is this library's constraint in its own
 * words, and why it was taken rather than written.
 *
 * PINNED, NOT TRACKED, in the sense kdos-comp is: this does not rebase onto
 * upstream, because the changes below are not ones upstream would take.
 *
 * WHAT THE FORK CHANGED
 *
 *   tsm_ -> kvt_, shl_ -> kvt_shl_. Every exported symbol in a libk* library
 *   carries its library's prefix, and unprefixed generic names have already
 *   made two of our own programs unlinkable together.
 *
 *   THE HASH TABLE IS OURS. Upstream keeps its symbol table in an LGPL table
 *   borrowed from CCAN. KDOS is MIT and every fork it takes is MIT; a library
 *   that put relinking obligations on every binary linking this one is not one
 *   this tree can carry. kvt_htable.c is written, and it is the only file here
 *   that is not upstream's.
 *
 *   CHARACTER WIDTH IS libktui's. Upstream vendors its own wcwidth; two
 *   answers to how wide a character is puts a glyph and its cell out of step.
 *
 *   Warnings. The tree builds under -Wall -Wextra and the self-test adds
 *   -Werror, so a handful of unused parameters and signed comparisons were
 *   made explicit. No behaviour moved.
 *
 * THE CELL STAYS UPSTREAM'S, AND THAT IS DELIBERATE.
 *
 * The plan said this fork would replace the internal cell with KtuiCell,
 * citing kcell.h's refusal of "a second answer to what a cell is". Having read
 * the source: that refusal is about two LIBRARIES OF THE TOOLKIT disagreeing,
 * and this is a terminal emulator's private screen buffer that nothing outside
 * this library ever sees. The toolkit still has exactly one cell.
 *
 * Replacing it would also have cost three things the terminal needs and
 * KtuiCell cannot hold: 24-bit colour per cell, the per-cell age that drives
 * damage, and the symbol table that makes combining characters possible.
 * kvt_grid.c converts at the render boundary instead — once per frame, over
 * the runs that changed, in one function.
 * ---------------------------------
 */

/*
 * TSM - Main Header
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

#ifndef KVT_KVT_H
#define KVT_KVT_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @mainpage
 *
 * TSM is a Terminal-emulator State Machine. It implements all common DEC-VT100
 * to DEC-VT520 control codes and features. A state-machine is used to parse TTY
 * input and saved in a virtual screen. TSM does not provide any rendering,
 * glyph/font handling or anything more advanced. TSM is just a simple
 * state-machine for control-codes handling.
 * The main use-case for TSM are terminal-emulators. TSM has no dependencies
 * other than an ISO-C99 compiler and C-library. Any terminal emulator for any
 * window-environment or rendering-pipline can make use of TSM. However, TSM can
 * also be used for control-code validation, TTY-screen-capturing or other
 * advanced users of terminal escape-sequences.
 */

/**
 * @defgroup misc Miscellaneous Definitions
 * Miscellaneous definitions
 *
 * This section contains several miscellaneous definitions of small helpers and
 * constants. These are shared between other parts of the API and have common
 * semantics/syntax.
 *
 * @{
 */

/**
 * Logging Callback
 *
 * @param data: User-provided data
 * @param file: Source code file where the log message originated or NULL
 * @param line: Line number in source code or 0
 * @param func: C function name or NULL
 * @param subs: Subsystem where the message came from or NULL
 * @param sev: Kernel-style severity between 0=FATAL and 7=DEBUG
 * @param format: Printf-formatted message
 * @param args: Arguments for printf-style @p format
 *
 * This is the type of a logging callback function. You can always pass NULL
 * instead of such a function to disable logging.
 */
typedef void (*kvt_log_t) (void *data,
			   const char *file,
			   int line,
			   const char *func,
			   const char *subs,
			   unsigned int sev,
			   const char *format,
			   va_list args);

/** @} */

/**
 * @defgroup symbols Unicode Helpers
 * Unicode helpers
 *
 * Unicode uses 32bit types to uniquely represent symbols. However, combining
 * characters allow modifications of such symbols but require additional space.
 * To avoid passing around allocated strings, TSM provides a symbol-table which
 * can store combining-characters with their base-symbol to create a new symbol.
 * This way, only the symbol-identifiers have to be passed around (which are
 * simple integers). No string allocation is needed by the API user.
 *
 * The symbol table is currently not exported. Once the API is fixed, we will
 * provide it to outside users.
 *
 * Additionally, this contains some general UTF8/UCS4 helpers.
 *
 * @{
 */

/* UCS4 helpers */

#define KVT_UCS4_MAX_BITS 31
#define KVT_UCS4_MAX ((1UL << KVT_UCS4_MAX_BITS) - 1UL)
#define KVT_UCS4_INVALID (KVT_UCS4_MAX + 1)
#define KVT_UCS4_REPLACEMENT (0xfffdUL)

/* ucs4 to utf8 converter */

unsigned int kvt_ucs4_get_width(uint32_t ucs4);
size_t kvt_ucs4_to_utf8(uint32_t ucs4, char *out);
char *kvt_ucs4_to_utf8_alloc(const uint32_t *ucs4, size_t len, size_t *len_out);

/* symbols */

typedef uint32_t kvt_symbol_t;

/** @} */

/**
 * @defgroup screen Terminal Screens
 * Virtual terminal-screen implementation
 *
 * A TSM screen respresents the real screen of a terminal/application. It does
 * not render anything, but only provides a table of cells. Each cell contains
 * the stored symbol, attributes and more. Applications iterate a screen to
 * render each cell on their framebuffer.
 *
 * Screens provide all features that are expected from terminals. They include
 * scroll-back buffers, alternate screens, cursor positions and selection
 * support. Thus, it needs event-input from applications to drive these
 * features. Most of them are optional, though.
 *
 * @{
 */

struct kvt_screen;
typedef uint_fast32_t kvt_age_t;

#define KVT_SCREEN_INSERT_MODE	0x01
#define KVT_SCREEN_AUTO_WRAP	0x02
#define KVT_SCREEN_REL_ORIGIN	0x04
#define KVT_SCREEN_INVERSE	0x08
#define KVT_SCREEN_HIDE_CURSOR	0x10
#define KVT_SCREEN_FIXED_POS	0x20
#define KVT_SCREEN_ALTERNATE	0x40

struct kvt_screen_attr {
	int8_t fccode;			/* foreground color code or <0 for rgb */
	int8_t bccode;			/* background color code or <0 for rgb */
	uint8_t fr;			/* foreground red */
	uint8_t fg;			/* foreground green */
	uint8_t fb;			/* foreground blue */
	uint8_t br;			/* background red */
	uint8_t bg;			/* background green */
	uint8_t bb;			/* background blue */
	unsigned int bold : 1;		/* bold character */
	unsigned int italic : 1;	/* italics character */
	unsigned int underline : 1;	/* underlined character */
	unsigned int inverse : 1;	/* inverse colors */
	unsigned int protect : 1;	/* cannot be erased */
	unsigned int blink : 1;		/* blinking character */
	unsigned int dim:1;		/* dim color */
};

/* Attributes that alter the glyph shape */
typedef union{
	struct {
		uint8_t bold      : 1;
		uint8_t italic    : 1;
		uint8_t underline : 1;
		uint8_t blink     : 1;
		uint8_t reserved  : 4;
	};
	uint8_t u8;
} kvt_screen_attr2_t;

struct kvt_screen_color {
	uint8_t r; /* red */
	uint8_t g; /* green */
	uint8_t b; /* blue */
};

struct kvt_screen_cell {
	uint32_t ch;                  /* character */
	struct kvt_screen_color fg;   /* foreground color */
	struct kvt_screen_color bg;   /* background color */
	kvt_screen_attr2_t attr2;     /* glyph attributes */
};

enum kvt_screen_cursor_style {
	KVT_SCREEN_CURSOR_DEFAULT		= 0,
	KVT_SCREEN_CURSOR_BLOCK_BLINK		= 1,
	KVT_SCREEN_CURSOR_BLOCK_STEADY		= 2,
	KVT_SCREEN_CURSOR_UNDERLINE_BLINK	= 3,
	KVT_SCREEN_CURSOR_UNDERLINE_STEADY	= 4,
	KVT_SCREEN_CURSOR_VBAR_BLINK		= 5,
	KVT_SCREEN_CURSOR_VBAR_STEADY		= 6,
};

typedef int (*kvt_screen_draw_cb) (struct kvt_screen *con,
				   uint64_t id,
				   const uint32_t *ch,
				   size_t len,
				   unsigned int width,
				   unsigned int posx,
				   unsigned int posy,
				   const struct kvt_screen_attr *attr,
				   kvt_age_t age,
				   void *data);

int kvt_screen_new(struct kvt_screen **out, kvt_log_t log, void *log_data);
void kvt_screen_ref(struct kvt_screen *con);
void kvt_screen_unref(struct kvt_screen *con);

unsigned int kvt_screen_get_width(struct kvt_screen *con);
unsigned int kvt_screen_get_height(struct kvt_screen *con);
int kvt_screen_resize(struct kvt_screen *con, unsigned int x,
		      unsigned int y);
int kvt_screen_set_margins(struct kvt_screen *con,
			   unsigned int top, unsigned int bottom);
void kvt_screen_set_max_sb(struct kvt_screen *con, unsigned int max);
void kvt_screen_clear_sb(struct kvt_screen *con);

void kvt_screen_sb_up(struct kvt_screen *con, unsigned int num);
void kvt_screen_sb_down(struct kvt_screen *con, unsigned int num);
void kvt_screen_sb_page_up(struct kvt_screen *con, unsigned int num);
void kvt_screen_sb_page_down(struct kvt_screen *con, unsigned int num);
void kvt_screen_sb_reset(struct kvt_screen *con);
unsigned int kvt_screen_sb_get_line_count(struct kvt_screen *con);
unsigned int kvt_screen_sb_get_line_pos(struct kvt_screen *con);

void kvt_screen_set_def_attr(struct kvt_screen *con,
			     const struct kvt_screen_attr *attr);
void kvt_screen_reset(struct kvt_screen *con);
void kvt_screen_set_flags(struct kvt_screen *con, unsigned int flags);
void kvt_screen_reset_flags(struct kvt_screen *con, unsigned int flags);
unsigned int kvt_screen_get_flags(struct kvt_screen *con);

unsigned int kvt_screen_get_cursor_x(struct kvt_screen *con);
unsigned int kvt_screen_get_cursor_y(struct kvt_screen *con);

void kvt_screen_set_tabstop(struct kvt_screen *con);
void kvt_screen_reset_tabstop(struct kvt_screen *con);
void kvt_screen_reset_all_tabstops(struct kvt_screen *con);

void kvt_screen_write(struct kvt_screen *con, kvt_symbol_t ch,
		      const struct kvt_screen_attr *attr);
void kvt_screen_newline(struct kvt_screen *con);
void kvt_screen_scroll_up(struct kvt_screen *con, unsigned int num);
void kvt_screen_scroll_down(struct kvt_screen *con, unsigned int num);
void kvt_screen_move_to(struct kvt_screen *con, unsigned int x,
			unsigned int y);
void kvt_screen_move_up(struct kvt_screen *con, unsigned int num,
			bool scroll);
void kvt_screen_move_down(struct kvt_screen *con, unsigned int num,
			  bool scroll);
void kvt_screen_move_left(struct kvt_screen *con, unsigned int num);
void kvt_screen_move_right(struct kvt_screen *con, unsigned int num);
void kvt_screen_move_line_end(struct kvt_screen *con);
void kvt_screen_move_line_home(struct kvt_screen *con);
void kvt_screen_tab_right(struct kvt_screen *con, unsigned int num);
void kvt_screen_tab_left(struct kvt_screen *con, unsigned int num);
void kvt_screen_insert_lines(struct kvt_screen *con, unsigned int num);
void kvt_screen_delete_lines(struct kvt_screen *con, unsigned int num);
void kvt_screen_insert_chars(struct kvt_screen *con, unsigned int num);
void kvt_screen_delete_chars(struct kvt_screen *con, unsigned int num);
void kvt_screen_erase_cursor(struct kvt_screen *con);
void kvt_screen_erase_chars(struct kvt_screen *con, unsigned int num);
void kvt_screen_erase_cursor_to_end(struct kvt_screen *con,
				    bool protect);
void kvt_screen_erase_home_to_cursor(struct kvt_screen *con,
				     bool protect);
void kvt_screen_erase_current_line(struct kvt_screen *con,
				   bool protect);
void kvt_screen_erase_screen_to_cursor(struct kvt_screen *con,
				       bool protect);
void kvt_screen_erase_cursor_to_screen(struct kvt_screen *con,
				       bool protect);
void kvt_screen_erase_screen(struct kvt_screen *con, bool protect);

void kvt_screen_selection_reset(struct kvt_screen *con);
void kvt_screen_selection_start(struct kvt_screen *con,
				unsigned int posx,
				unsigned int posy);
void kvt_screen_selection_target(struct kvt_screen *con,
				 unsigned int posx,
				 unsigned int posy);
void kvt_screen_selection_word(struct kvt_screen *con,
			       unsigned int posx,
			       unsigned int posy);
int kvt_screen_selection_copy(struct kvt_screen *con, char **out);

/* ── the KDOS render boundary (kvt_grid.c) ──────────────────────────────
 *
 * The screen as KtuiCells. Colours are reduced to the theme's eight slots by
 * nearest distance — one rule for the ANSI sixteen, the 256 and truecolor
 * alike — so `kdos theme` moves a terminal's colours with everything else.
 * A combining sequence collapses to its base codepoint, because a cell holds
 * one. */
#include "ktui.h"

kvt_age_t kvt_grid_render(struct kvt_screen *con, KtuiCell *cells,
			  int w, int h);

/* ── a screen, a state machine and a child, as one thing (kvt_term.c) ────
 *
 * What a terminal window holds: a descriptor to poll, a grid to draw, and a
 * child whose exit status OUTLIVES it, so a window can say how its program
 * finished rather than vanishing with the message.
 *
 * The argument vector arrives built and is executed directly. No shell. */
struct kvt_term;

struct kvt_term *kvt_term_open(const char *const argv[], int cols, int rows);
void kvt_term_close(struct kvt_term *t);

int kvt_term_fd(struct kvt_term *t);
int kvt_term_pump(struct kvt_term *t);		/* read the child, reap it */
void kvt_term_write(struct kvt_term *t, const char *u8, size_t len);
void kvt_term_scrollback(struct kvt_term *t, unsigned int lines);
void kvt_term_resize(struct kvt_term *t, int cols, int rows);
kvt_age_t kvt_term_render(struct kvt_term *t, KtuiCell *cells, int w, int h);

int kvt_term_alive(struct kvt_term *t);
int kvt_term_status(struct kvt_term *t);	/* kept after the child dies */
int kvt_term_signal(struct kvt_term *t, int sig);

void kvt_term_scroll(struct kvt_term *t, int lines);	/* -up, +down */
struct kvt_screen *kvt_term_screen(struct kvt_term *t);
int kvt_term_copy_selection(struct kvt_term *t);

/* Write a registered picture into the screen at the cursor, as sprite cells
 * naming the tiles held under `key`. In the SCREEN, so it scrolls with the
 * output, clears with it and reaches the scrollback; a tile the table has
 * since dropped becomes a blank. */
int kvt_term_place(struct kvt_term *t, uint64_t key, int cw, int ch);

/* Pasted text, bracketed when the child asked for it. */
void kvt_term_paste(struct kvt_term *t, const char *text);
/* True when this payload would execute if pasted. See
 * kvt_vte_paste_needs_confirm(). */
int kvt_term_paste_needs_confirm(struct kvt_term *t, const char *text);

/*
 * One libktui key event, as the bytes this terminal is in the mode to send.
 * The escape an arrow produces depends on application cursor mode and on the
 * modifiers, and both live in the state machine — which is why a caller must
 * not build the sequence itself. `key` is a codepoint or a KT_K_*, `mods` is
 * KT_MOD_*. Zero when the key means nothing to a terminal.
 */
int kvt_term_key(struct kvt_term *t, int key, int mods);

/* The mouse, reported to the child. Zero when the child has not asked for
 * mouse reports, which is what leaves the pointer to the terminal's own
 * selection. `btn` and `event` are the state machine's own KVT_MOUSE_*. */
int kvt_term_mouse(struct kvt_term *t, int cell_x, int cell_y, int btn,
		   int mods, int event);
int kvt_term_mouse_mode(struct kvt_term *t);

kvt_age_t kvt_screen_draw(struct kvt_screen *con, kvt_screen_draw_cb draw_cb,
			  void *data);

const struct kvt_screen_cell *kvt_screen_draw2(struct kvt_screen *con);

enum kvt_screen_cursor_style kvt_screen_get_cursor_style(struct kvt_screen *con);
void kvt_screen_set_cursor_style(struct kvt_screen *con, enum kvt_screen_cursor_style type);

/** @} */

/**
 * @defgroup vte State Machine
 * Virtual terminal emulation with state machine
 *
 * A TSM VTE object provides the terminal state machine. It takes input from the
 * application (which usually comes from a TTY/PTY from a client), parses it,
 * modifies the attach screen or returns data which has to be written back to
 * the client.
 *
 * Furthermore, VTE objects accept keyboard or mouse input from the application
 * which is interpreted compliant to DEV-VTs.
 *
 * @{
 */

/* virtual terminal emulator */

struct kvt_vte;

/* terminal flags */
#define KVT_VTE_FLAG_CURSOR_KEY_MODE			0x00000001 /* DEC cursor key mode */
#define KVT_VTE_FLAG_KEYPAD_APPLICATION_MODE		0x00000002 /* DEC keypad application mode; TODO: toggle on numlock? */
#define KVT_VTE_FLAG_LINE_FEED_NEW_LINE_MODE		0x00000004 /* DEC line-feed/new-line mode */
#define KVT_VTE_FLAG_8BIT_MODE				0x00000008 /* Disable UTF-8 mode and enable 8bit compatible mode */
#define KVT_VTE_FLAG_7BIT_MODE				0x00000010 /* Disable 8bit mode and use 7bit compatible mode */
#define KVT_VTE_FLAG_USE_C1				0x00000020 /* Explicitly use 8bit C1 codes; TODO: implement */
#define KVT_VTE_FLAG_KEYBOARD_ACTION_MODE		0x00000040 /* Disable keyboard; TODO: implement? */
#define KVT_VTE_FLAG_INSERT_REPLACE_MODE		0x00000080 /* Enable insert mode */
#define KVT_VTE_FLAG_SEND_RECEIVE_MODE			0x00000100 /* Disable local echo */
#define KVT_VTE_FLAG_TEXT_CURSOR_MODE			0x00000200 /* Show cursor */
#define KVT_VTE_FLAG_INVERSE_SCREEN_MODE		0x00000400 /* Inverse colors */
#define KVT_VTE_FLAG_ORIGIN_MODE			0x00000800 /* Relative origin for cursor */
#define KVT_VTE_FLAG_AUTO_WRAP_MODE			0x00001000 /* Auto line wrap mode */
#define KVT_VTE_FLAG_AUTO_REPEAT_MODE			0x00002000 /* Auto repeat key press; TODO: implement */
#define KVT_VTE_FLAG_NATIONAL_CHARSET_MODE		0x00004000 /* Send keys from nation charsets; TODO: implement */
#define KVT_VTE_FLAG_BACKGROUND_COLOR_ERASE_MODE	0x00008000 /* Set background color on erase (bce) */
#define KVT_VTE_FLAG_PREPEND_ESCAPE			0x00010000 /* Prepend escape character to next output */
#define KVT_VTE_FLAG_TITE_INHIBIT_MODE			0x00020000 /* Prevent switching to alternate screen buffer */

/* keep in sync with kvt_shl_xkb_mods */
enum kvt_vte_modifier {
	KVT_SHIFT_MASK		= (1 << 0),
	KVT_LOCK_MASK		= (1 << 1),
	KVT_CONTROL_MASK	= (1 << 2),
	KVT_ALT_MASK		= (1 << 3),
	KVT_LOGO_MASK		= (1 << 4),
};

/* keep in sync with KVT_INPUT_INVALID */
#define KVT_VTE_INVALID 0xffffffff

enum kvt_vte_color {
	KVT_COLOR_BLACK,
	KVT_COLOR_RED,
	KVT_COLOR_GREEN,
	KVT_COLOR_YELLOW,
	KVT_COLOR_BLUE,
	KVT_COLOR_MAGENTA,
	KVT_COLOR_CYAN,
	KVT_COLOR_LIGHT_GREY,
	KVT_COLOR_DARK_GREY,
	KVT_COLOR_LIGHT_RED,
	KVT_COLOR_LIGHT_GREEN,
	KVT_COLOR_LIGHT_YELLOW,
	KVT_COLOR_LIGHT_BLUE,
	KVT_COLOR_LIGHT_MAGENTA,
	KVT_COLOR_LIGHT_CYAN,
	KVT_COLOR_WHITE,

	KVT_COLOR_FOREGROUND,
	KVT_COLOR_BACKGROUND,

	KVT_COLOR_NUM
};

/**
 * Mouse Tracking
 *
 * Reference:
 *
 * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking
 *
 * The application running in the terminal can request a mouse tracking mode
 * and it can configure the event type (send position on click only or on click
 * and on mouse movement). The terminal will then send the position
 * of the mouse cursor as requested by the application.
 *
 * Since libtsm doesn't know anything about the UI or the mouse this can only
 * work if the terminal emulator built on top of libtsm cooperates.
 *
 * To implement mouse tracking a terminal emulator must first set a mouse
 * callback with kvt_vte_set_mouse_cb. This callback will be called whenever the
 * mouse mode changes in a way that is relevant to the terminal. It tells the
 * terminal if it needs to pass mouse events on click, on move, or not at all.
 *
 * To pass the mouse events the terminal needs to call kvt_vte_handle_mouse with
 * all parameters filled appropriately. The function will then take care of
 * sending the mouse events to the application in the correct encoding and
 * it will also discard events that are duplicate or unnecessary.
 */

/* control sequence codes sent be the application */
#define KVT_VTE_MOUSE_MODE_X10      9 /* legacy mode (only cell mode, only on mouse click and x and y can be 223 max) */
#define KVT_VTE_MOUSE_MODE_VT200 1000 /* normal tracking mode (sends mouse position both on button press and release) */
#define KVT_VTE_MOUSE_EVENT_BTN  1002 /* sends position on mouse click only */
#define KVT_VTE_MOUSE_EVENT_ANY  1003 /* sends position on mouse click and mouse move */
#define KVT_VTE_MOUSE_MODE_SGR   1006 /* modern mode that allows unlimited x and y coordinates */
#define KVT_VTE_MOUSE_MODE_PIXEL 1016 /* sends pixel coordinates instead of cell coordinates */
#define KVT_VTE_BRACKETED_PASTE  2004 /* enclose paste data with escape characters */

enum kvt_mouse_track_mode {
	KVT_MOUSE_TRACK_DISABLE = 0, /* don't track mouse events */
	KVT_MOUSE_TRACK_BTN = KVT_VTE_MOUSE_EVENT_BTN, /* call kvt_vte_handle_mouse only for mouse clicks */
	KVT_MOUSE_TRACK_ANY = KVT_VTE_MOUSE_EVENT_ANY  /* call kvt_vte_handle_mouse for mouse clicks and mouse movement */
};

/* mouse buttons to be passed to kvt_vte_handle_mouse */
#define KVT_MOUSE_BUTTON_LEFT       0
#define KVT_MOUSE_BUTTON_MIDDLE     1
#define KVT_MOUSE_BUTTON_RIGHT      2
#define KVT_MOUSE_BUTTON_WHEEL_UP   4
#define KVT_MOUSE_BUTTON_WHEEL_DOWN 5

/* modifier keys to be passed to kvt_vte_handle_mouse (can be combined with OR) */
#define KVT_MOUSE_MODIFIER_SHIFT  4
#define KVT_MOUSE_MODIFIER_META   8
#define KVT_MOUSE_MODIFIER_CTRL  16

/* events to be passed to kvt_vte_handle_mouse */
#define KVT_MOUSE_EVENT_PRESSED  1
#define KVT_MOUSE_EVENT_RELEASED 2
#define KVT_MOUSE_EVENT_MOVED    4

typedef void (*kvt_vte_write_cb) (struct kvt_vte *vte,
				  const char *u8,
				  size_t len,
				  void *data);

typedef void (*kvt_vte_osc_cb) (struct kvt_vte *vte,
				  const char *u8,
				  size_t len,
				  void *data);

/*
 * A PICTURE ARRIVED, and this library did not decode it.
 *
 * Three protocols carry an image into a terminal and they differ only in how
 * they are delimited — sixel is a DCS, iTerm2's is an OSC, kitty's is an APC —
 * so one callback serves all three and `kind` says which. `params` is the
 * introducer's text, already NUL-terminated: the sixel introducer parameters,
 * the OSC 1337 key=value list, or the kitty control block. `payload` is the
 * bytes between the introducer and the terminator, and it is borrowed for the
 * length of the call.
 *
 * libkvt decodes NOTHING. It is linked by kdos-con, which links no pixel code
 * at all, and it has to stay that way — so the decoder is a function pointer
 * the consumer sets, backed by libkimg where a consumer wants pictures.
 *
 * A payload past the cap set by kvt_vte_set_img_cb is DROPPED ENTIRELY rather
 * than truncated: half an image is not a smaller image, and a decoder handed
 * one would be a decoder handed a deliberately malformed file.
 */
enum kvt_img_kind {
	KVT_IMG_SIXEL = 0,	/* DCS <params> q … ST                      */
	KVT_IMG_OSC1337,	/* OSC 1337 ; File=<args> : <base64> ST     */
	KVT_IMG_KITTY,		/* APC G <control> ; <payload> ST           */
};

typedef void (*kvt_vte_img_cb) (struct kvt_vte *vte,
				enum kvt_img_kind kind,
				const char *params,
				const uint8_t *payload,
				size_t len,
				void *data);

typedef void (*kvt_vte_mouse_cb) (struct kvt_vte *vte,
				  enum kvt_mouse_track_mode track_mode,
				  bool track_pixels,
				  void *data);

typedef void (*kvt_vte_bell_cb) (struct kvt_vte *vte,
				 void *data);

/*
 * SYNCHRONIZED OUTPUT (DECSET 2026) WENT ON OR OFF.
 *
 * The renderer skips a frame while it is on, so a program that draws its
 * screen in one bracket is never seen half-drawn — which on a slow link is the
 * difference between a frame and a tear. **The watchdog is the renderer's**: a
 * child that sets the mode and dies would otherwise freeze its window forever,
 * and the terminal cannot tell that from a program taking its time.
 */
typedef void (*kvt_vte_sync_cb) (struct kvt_vte *vte, bool on, void *data);

/*
 * A PROGRAM INSIDE THE TERMINAL SAYS IT FINISHED — OSC 9, OSC 777 and OSC 99,
 * the three spellings that exist because none of them won.
 *
 * The callback is the TERMINAL'S rather than this library's: raising a toast
 * means a session bus, and `libkvt` links nothing and must not start. A
 * terminal that sets no handler drops them, which is right for one with no
 * desktop behind it.
 */
typedef void (*kvt_vte_notify_cb) (struct kvt_vte *vte, const char *summary,
				   const char *body, void *data);

enum kvt_vte_led {
	KVT_VTE_LED_SCROLL_LOCK = (1 << 0),
	KVT_VTE_LED_NUM_LOCK    = (1 << 1),
	KVT_VTE_LED_CAPS_LOCK   = (1 << 2),
};

typedef void (*kvt_vte_led_cb) (struct kvt_vte *vte,
				unsigned int leds,
				void *data);

int kvt_vte_new(struct kvt_vte **out, struct kvt_screen *con,
		kvt_vte_write_cb write_cb, void *data,
		kvt_log_t log, void *log_data);
void kvt_vte_ref(struct kvt_vte *vte);
void kvt_vte_unref(struct kvt_vte *vte);

void kvt_vte_set_osc_cb(struct kvt_vte *vte, kvt_vte_osc_cb osc_cb, void *osc_data);

/*
 * A CHILD PUT SOMETHING ON THE CLIPBOARD, through OSC 52. `text` is the
 * decoded bytes and is borrowed. `primary` says which selection.
 *
 * THE READ DIRECTION IS NOT HERE. `52;c;?` asks a terminal to hand the
 * clipboard back to the program running inside it, which is the one direction
 * that has to be opted into: a program that can read the clipboard can read
 * whatever was last copied anywhere on the desktop. It is refused in the state
 * machine and there is no callback to enable it by mistake.
 */
typedef void (*kvt_vte_clip_cb)(struct kvt_vte *vte, const char *text,
				size_t len, int primary, void *data);

void kvt_vte_set_clip_cb(struct kvt_vte *vte, kvt_vte_clip_cb cb, void *data);
void kvt_term_clip_cb(struct kvt_term *t, kvt_vte_clip_cb cb, void *user);
/*
 * BEL, for a consumer that can show one. The state machine calls it and
 * decides nothing: what a bell looks like is a window's question, and a
 * terminal with no callback set swallows it exactly as it did before.
 */
void kvt_term_bell_cb(struct kvt_term *t, kvt_vte_bell_cb cb, void *user);
void kvt_term_sync_cb(struct kvt_term *t, kvt_vte_sync_cb cb, void *user);
void kvt_term_notify_cb(struct kvt_term *t, kvt_vte_notify_cb cb, void *user);
/* The focus moved. Sends CSI I / CSI O only while the child asked for them. */
void kvt_term_focus(struct kvt_term *t, int in);
/* True while the child has synchronized output on. */
int kvt_term_sync_output(struct kvt_term *t);
/* True while this frame should be held back — synchronized output is on and
 * the child has not held it past the watchdog. */
int kvt_term_sync_hold(struct kvt_term *t);
/* The same, for a terminal that owns its state machine (kvt_term.c). */
void kvt_term_osc_cb(struct kvt_term *t, kvt_vte_osc_cb cb, void *user);

/*
 * Set the image callback and the cap on one payload. A NULL callback turns the
 * three protocols OFF — and off is the default, so a consumer that wants no
 * pictures parses exactly what it parsed before: sixel keeps going to the DCS
 * sink, and an APC keeps being ignored until its terminator.
 */
void kvt_vte_set_img_cb(struct kvt_vte *vte, kvt_vte_img_cb img_cb,
			size_t max_bytes, void *img_data);
/* The same, for a terminal that owns its state machine (kvt_term.c). */
void kvt_term_img_cb(struct kvt_term *t, kvt_vte_img_cb cb, size_t max_bytes,
		     void *user);

/*
 * WHAT `CSI ? 2 ; 1 ; S` (XTSMGRAPHICS) REPORTS AS THE SIXEL GEOMETRY, in
 * pixels. The library cannot derive it: the ceiling is the consumer's own
 * `image_cells` multiplied by its cell size, and libkvt knows neither.
 *
 * A TERMINAL THAT DOES NOT CALL THIS ANSWERS THE QUERY WITH A FAILURE, which
 * is the honest reply. A program told a geometry the terminal will not honour
 * sends a picture that comes back clipped, and nothing in that failure names
 * the terminal that lied about its size.
 */
void kvt_vte_set_img_geom(struct kvt_vte *vte, int max_w_px, int max_h_px);
void kvt_term_img_geom(struct kvt_term *t, int max_w_px, int max_h_px);
void kvt_vte_set_mouse_cb(struct kvt_vte *vte, kvt_vte_mouse_cb mouse_cb, void *mouse_data);
void kvt_vte_set_bell_cb(struct kvt_vte *vte, kvt_vte_bell_cb bell_cb, void *bell_data);
void kvt_vte_set_sync_cb(struct kvt_vte *vte, kvt_vte_sync_cb cb, void *data);
void kvt_vte_set_notify_cb(struct kvt_vte *vte, kvt_vte_notify_cb cb, void *data);
/* CSI I / CSI O to the child, and only while it asked with DECSET 1004. */
void kvt_vte_focus(struct kvt_vte *vte, bool in);
/* True while the child has synchronized output on. The renderer skips a frame
 * while it is, under a watchdog of its own. */
bool kvt_vte_sync_output(struct kvt_vte *vte);
void kvt_vte_set_led_cb(struct kvt_vte *vte, kvt_vte_led_cb led_cb, void *led_data);

/**
 * @brief Set color palette to one of the predefined palette on the vte object.
 *
 * Current supported palette names are
 *
 * - solarized
 * - solarized-black
 * - solarized-white
 * - soft-black
 * - base16-dark
 * - base16-light
 *
 * In addition, when palette name is "custom", the custom palette set in
 * kvt_vte_set_custom_palette() is used.
 *
 * @sa kvt_vte_set_custom_palette to set custom palette.
 *
 * @param vte The vte object to set on.
 * @param palette_name Name of the color palette. Pass NULL to reset to default.
 *
 * @retval 0 on success.
 * @retval -EINVAL if vte is NULL.
 * @retval -ENOMEM if malloc fails.
 */
int kvt_vte_set_palette(struct kvt_vte *vte, const char *palette_name);

/**
 * @brief Set a custom palette on the vte object.
 *
 * An example:
 *
 * @code
 * static uint8_t color_palette[KVT_COLOR_NUM][3] = {
 * 	[KVT_COLOR_BLACK]         = { 0x00, 0x00, 0x00 },
 * 	[KVT_COLOR_RED]           = { 0xab, 0x46, 0x42 },
 * 	[KVT_COLOR_GREEN]         = { 0xa1, 0xb5, 0x6c },
 * 	[KVT_COLOR_YELLOW]        = { 0xf7, 0xca, 0x88 },
 * 	[KVT_COLOR_BLUE]          = { 0x7c, 0xaf, 0xc2 },
 * 	[KVT_COLOR_MAGENTA]       = { 0xba, 0x8b, 0xaf },
 * 	[KVT_COLOR_CYAN]          = { 0x86, 0xc1, 0xb9 },
 * 	[KVT_COLOR_LIGHT_GREY]    = { 0xaa, 0xaa, 0xaa },
 * 	[KVT_COLOR_DARK_GREY]     = { 0x55, 0x55, 0x55 },
 * 	[KVT_COLOR_LIGHT_RED]     = { 0xab, 0x46, 0x42 },
 * 	[KVT_COLOR_LIGHT_GREEN]   = { 0xa1, 0xb5, 0x6c },
 *	[KVT_COLOR_LIGHT_YELLOW]  = { 0xf7, 0xca, 0x88 },
 * 	[KVT_COLOR_LIGHT_BLUE]    = { 0x7c, 0xaf, 0xc2 },
 * 	[KVT_COLOR_LIGHT_MAGENTA] = { 0xba, 0x8b, 0xaf },
 * 	[KVT_COLOR_LIGHT_CYAN]    = { 0x86, 0xc1, 0xb9 },
 * 	[KVT_COLOR_WHITE]         = { 0xff, 0xff, 0xff },
 *
 * 	[KVT_COLOR_FOREGROUND]    = { 0x18, 0x18, 0x18 },
 * 	[KVT_COLOR_BACKGROUND]    = { 0xd8, 0xd8, 0xd8 },
 * };
 * @endcode
 *
 * The palette array is copied into the vte object.
 *
 * @param vte The vte object to set on
 * @param palette The palette array, which should have shape `uint8_t palette[KVT_COLOR_NUM][3]`. Pass NULL to clear.
 *
 * @retval 0 on success.
 * @retval -EINVAL if vte is NULL.
 * @retval -ENOMEM if malloc fails.
 */
int kvt_vte_set_custom_palette(struct kvt_vte *vte, uint8_t (*palette)[3]);

void kvt_vte_get_def_attr(struct kvt_vte *vte, struct kvt_screen_attr *out);
unsigned int kvt_vte_get_flags(struct kvt_vte *vte);

unsigned int kvt_vte_get_mouse_mode(struct kvt_vte *vte);
unsigned int kvt_vte_get_mouse_event(struct kvt_vte *vte);

/*
 * What the child is told this terminal is called, in `TERM_PROGRAM_VERSION`.
 * A consumer that passes -DKDOS_TERM_VERSION gets its own package version;
 * anything else gets "0", which is a version a probe can compare and not a
 * variable it finds missing.
 */
#ifdef KDOS_TERM_VERSION
#define KVT_TERM_VERSION KDOS_TERM_VERSION
#elif defined(KDOS_CON_VERSION)
#define KVT_TERM_VERSION KDOS_CON_VERSION
#else
#define KVT_TERM_VERSION "0"
#endif

void kvt_vte_reset(struct kvt_vte *vte);
void kvt_vte_hard_reset(struct kvt_vte *vte);
void kvt_vte_input(struct kvt_vte *vte, const char *u8, size_t len);

/**
 * @brief Set backspace key to send either backspace or delete.
 *
 * Some terminals send ASCII backspace (010, 8, 0x08), some send ASCII delete
 * (0177, 127, 0x7f).
 *
 * The default for vte is to send ASCII backspace.
 *
 * @param vte The vte object to set on
 * @param enable Send ASCII delete if \c true, send ASCII backspace if \c false.
 */
void kvt_vte_set_backspace_sends_delete(struct kvt_vte *vte, bool enable);
bool kvt_vte_handle_keyboard(struct kvt_vte *vte, uint32_t keysym,
			     uint32_t ascii, unsigned int mods,
			     uint32_t unicode);
bool kvt_vte_handle_mouse(struct kvt_vte *vte, unsigned int cell_x,
        unsigned int cell_y, unsigned int pixel_x, unsigned int pixel_y,
        unsigned int button, unsigned int event, unsigned char flags);

void kvt_vte_paste(struct kvt_vte *vte, const char *data);
/* True when an unbracketed payload carries a control byte other than tab —
 * a newline in one EXECUTES at a shell. The caller confirms first. */
int kvt_vte_paste_needs_confirm(struct kvt_vte *vte, const char *data);
/** @} */

#ifdef __cplusplus
}
#endif

/*
 * THE POINTER OVER A TERMINAL, and the selection it makes.
 *
 * IT LIVES HERE BECAUSE TWO PROGRAMS NEED IT. `kdos-term` is a window on the
 * graphical desktop and `kdos-con` runs terminals of its own inside the
 * session, and both must decide the same things: when the wheel belongs to the
 * child rather than to the scrollback, when a drag is a selection rather than
 * a mouse report, and that a press and a release in one cell is a click and
 * selects nothing. Written twice, the two would drift, and the difference
 * would be a terminal that behaves differently depending on which desktop it
 * is on.
 *
 * `KvtUi` is the caller's — one per terminal, zeroed once. Nothing in libkvt
 * keeps it, so a program with many terminals keeps many.
 */
typedef struct {
	double click_at;
	int click_x, click_y;
	int sel_x, sel_y;
	int selecting;
} KvtUi;

/*
 * Returns 1 when a selection was completed, and then `*copied` is a
 * malloc'd string the caller owns and must free. 0 otherwise, `*copied`
 * untouched. `now` is a monotonic seconds value — passed in rather than read
 * here so a test can drive a double click without waiting for one.
 */
int kvt_ui_mouse(struct kvt_term *t, KvtUi *ui, const KtuiEvent *ev,
		 double now, char **copied);

#endif /* KVT_KVT_H */

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * libkchrome — the KDOS window furniture, drawn as cells.
 *
 * The header band, the group heading, the button bar, the shared answer to
 * what a wheel does over a list, and the pixel tile.
 *
 * A library rather than one program's source so that a second program does not
 * need a second answer to what a KDOS window looks like: two implementations
 * of a button bar are two button bars, and the one nobody is looking at is the
 * one that drifts.
 *
 * Links libktui, libkicon, libkcolor and libkbase. It draws, and it holds the
 * hit map of what it drew; it owns no application state, which is why the
 * favourites store stayed in kdos-shell rather than coming along.
 */

#ifndef KCHROME_H
#define KCHROME_H

#include <stddef.h>
#include <stdint.h>

#include "kbase.h"
#include "kcolor.h"
#include "kicon.h"
#include "ktui.h"

struct KCellCanvas;

/* ────────────────────────────────────────────────────────────────────────
 * Tiles (kch_tile.c) — a block of cells a caller paints as PIXELS
 *
 * The escape hatch from "a control is one row of text tall", without a second
 * renderer: libkcell rasterises a canvas of N x M cells and libktui's sprite
 * table carries it, so layout, damage, palette and the text fallback are
 * unchanged. See kch_tile.c for the two-slot alternation that keeps the row diff
 * precise, and kcell_canvas.c for why this shape rather than another.
 *
 * EVERY CALLER MUST DRAW WITHOUT IT. `kch_tile_begin`/`kch_tile_slot` answer
 * NULL/-1 on a terminal, under `icons = no`, with no font, and when the table
 * is full — and the caller then draws the glyph layout it always had.
 * ──────────────────────────────────────────────────────────────────────── */

struct KCellCanvas;

/* Stable ids, one per tile the shell owns. */
enum { SH_TILE_START = 0, SH_TILE_METERS };

/* The canvas to draw into, or NULL when the tile already shows exactly this
 * content. `content` is the caller's hash of everything it is about to draw. */
struct KCellCanvas *kch_tile_begin(int id, int cw, int ch, uint64_t content);
int kch_tile_commit(int id);
int kch_tile_slot(int id);
/* Drop every tile — what a live `kdos theme <accent>` needs, because each one
 * was rasterised in the palette that is being replaced. */
void kch_tile_reset(void);
void kch_tile_enable(int on);

/* ────────────────────────────────────────────────────────────────────────
 * Window chrome (kch_chrome.c)
 *
 * The header band, group headings and button bar the device apps share.
 * See chrome.c for what these are for and why they are not four copies.
 * ──────────────────────────────────────────────────────────────────────── */

#define SH_MAX_BTN 6

struct kch_button {
	const char *label;
	int enabled;		/* drawn dim and never the focus when 0 */
};

/* Draws into rows 1..3 of a boxed window and returns the first BODY row. */
int kch_header(int w, const char *icon_name, const char *title,
		     const char *subtitle, int icons_on);
void kch_group(int x, int y, int w, const char *label);
/*
 * Right-aligned on `row`, recording the span each button actually got. Never
 * half a button: a bar too wide for the window drops them from the RIGHT — the
 * callers order them most-useful-first — until what is left fits.
 *
 * Returns the leftmost column the bar took, which is where the caller's status
 * text on that row has to stop.
 */
int kch_buttons(int w, int row, const struct kch_button *b, int n,
		      int focus);
/* Which button the last frame drew at this cell, or -1. */
int kch_button_at(int mx, int my);
/*
 * WHERE THE POINTER IS, so a button can light under it.
 *
 * A button bar that looks identical whether or not the pointer is on it is a
 * row of words with brackets round them: the only way to find out that
 * `[ Connect ]` is a control was to click it. Fed from the same motion branch
 * that already moves the list selection; (-1, -1) — libkwl's leave — clears it.
 */
void kch_hover(int mx, int my);

/* ────────────────────────────────────────────────────────────────────────
 * Scrolling lists (kch_chrome.c)
 *
 * ONE ANSWER TO "WHAT DOES THE WHEEL DO", because it was answered fifteen
 * times. The rule is what a mature list toolkit does and it has two halves:
 *
 *   - The list FITS: the wheel is a cursor step, the same as ↑ and ↓.
 *   - The list SCROLLS: the wheel moves the VIEWPORT and the cursor stays on
 *     the row it was on, travelling with the content.
 *
 * The other half of it is in libkwl (pt_motion): hover re-selects only when
 * the pointer has actually MOVED to another cell. Without that, a wheel notch
 * — which on every absolute pointing device arrives with the position repeated
 * — put the highlight straight back on the row under a stationary pointer, so
 * the selection appeared to snap back the instant it moved. Reported as "the
 * highlight jumps"; it was two correct behaviours fighting.
 * ──────────────────────────────────────────────────────────────────────── */

/* Rows per notch when the wheel is scrolling a viewport. */
#define SH_WHEEL_ROWS 3

/*
 * Returns 1 when the viewport was scrolled — the caller must NOT move its
 * cursor — and 0 when the list fits, which means the wheel is a cursor step.
 */
int kch_list_wheel(int up, int *top, int n, int body);
/*
 * Clamp `top` into the list, pulling `sel` into view only when `follow` says
 * the SELECTION is what moved. A draw that pulls unconditionally undoes the
 * scroll on the very next frame, which is the whole reason this is a flag.
 */
void kch_list_clamp(int *top, int sel, int n, int body, int follow);
/*
 * ONE COLUMN THAT SAYS THERE IS MORE.
 *
 * A list that scrolls and gives no sign of it is a list people believe they
 * have seen all of — and the wheel rule above made that worse rather than
 * better, because the page can now move without the cursor. The track is the
 * shade glyph and the thumb is the full block, both in the vt tier, so it
 * draws on tty1 and in a golden frame exactly as it does under fcft. Nothing
 * at all when everything fits: a full-height thumb is a decoration.
 */
/*
 * `id` is the caller's own small number, one per bar it can have on screen at
 * once. It is what the press and the drag name, so the hit map is the thing
 * that was DRAWN rather than a geometry computed a second time somewhere else.
 * Call it every frame, including the frames where the list fits: a bar that
 * has stopped being drawn must stop being grabbable.
 */
#define KCH_SCROLLBARS 4
void kch_scrollbar(int id, int x, int y, int rows, int n, int top, int bg);
/*
 * AND THE BAR IS A CONTROL. `press` answers the new `top` when the pointer is
 * on bar `id` and -1 when it is not, and arms the drag; `drag` keeps answering
 * for whichever bar was grabbed until `release`. A bar that only reports is a
 * bar people try to drag once and then stop trusting.
 *
 * The caller assigns the answer itself — the library holds no pointer into
 * anybody's state — and clears its own `sel_follow` when it does, or the next
 * draw pulls the selection back into view and undoes the scroll.
 */
int  kch_scrollbar_press(int id, int mx, int my);
int  kch_scrollbar_drag(int my);
void kch_scrollbar_release(void);
/* WHICH bar the live drag belongs to, for a surface with more than one; -1
 * when nothing is grabbed. */
int  kch_scrollbar_grabbed(void);

#endif /* KCHROME_H */

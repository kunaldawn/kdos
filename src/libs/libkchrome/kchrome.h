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

/*
 * pixman_image_t without dragging <pixman.h> onto every consumer's include
 * path. It is a UNION, not a struct — declaring the wrong tag is a hard error
 * — and C11 allows a typedef to be repeated with the same type, so this and
 * pixman's own definition coexist in either order.
 */
union pixman_image;
typedef union pixman_image pixman_image_t;


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
 * The tone ladder (kch_tone.c) — what a raised surface is made of
 *
 * The eight slots say what a cell is. They cannot say what a BUTTON is: the
 * palette's dark end is compressed to 1.00-1.05:1, so a panel painted in its
 * own background is the same colour as the desktop behind it, and the only way
 * to make anything visible was full accent at 14:1.
 *
 * These are derived — every one a kcol_mix() of two scheme colours, so a live
 * accent switch retints them — and they are the missing middle. One table for
 * the taskbar and the Start menu both, so a plate means the same thing on each.
 *
 * A tone is a colour AND the alpha it is laid on at; ask for both.
 * ──────────────────────────────────────────────────────────────────────── */
typedef enum {
	KCH_T_BODY_TOP = 0,	/* the bar's own fill, top of the gradient  */
	KCH_T_BODY_BOT,		/* ...and the bottom                        */
	KCH_T_EDGE,		/* the 1px line that says where the bar is  */
	KCH_T_LIP,		/* the highlight just inside the edge       */
	KCH_T_REST,		/* a button at rest                         */
	KCH_T_HOVER,		/* under the pointer                        */
	KCH_T_ACTIVE,		/* the focused window — SOLVED, see the .c  */
	KCH_T_N
} KchTone;

uint32_t kch_tone(KchTone t);
uint8_t kch_tone_alpha(KchTone t);
/* The body alpha a POPUP wears — higher than the bar's, because a menu is
 * read and a taskbar is glanced at. See kch_tone.c. */
uint8_t kch_popup_alpha(void);
/* Drop the cache. Not normally needed — kch_tone() notices a theme swap by
 * itself — but it is what a retint calls beside kch_tile_reset(). */
void kch_tone_reset(void);

/* ────────────────────────────────────────────────────────────────────────
 * The pixel display list (kch_px.c) — recorded while drawing, replayed by
 * the backdrop under the cell grid.
 *
 * PIXELS FOR PAINT, CELLS FOR LAYOUT: the degradation passes, the hit maps
 * and every --dump stay in cells. Coordinates are real pixels at scale 1 and
 * are multiplied on replay, so a HiDPI output needs nothing from the caller.
 * ──────────────────────────────────────────────────────────────────────── */

/* Small on purpose. A radius is the one thing a cell grid cannot express, so
 * it is the one thing that gives pixel chrome away — three pixels softens a
 * plate, eight puts it beside window frames this distro draws deliberately
 * square and makes the two look like different toolkits. */
#define KCH_PLATE_RADIUS 3

void kch_px_reset(void);
void kch_px_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a);
void kch_px_round(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a);
void kch_px_grad(int x, int y, int w, int h, int r, uint32_t top,
		 uint32_t bot, uint8_t a);
/* CELL coordinates — every caller has them, and one conversion means a plate
 * and the glyphs on it cannot disagree about where the button is. */
void kch_px_plate(int cx, int cy, int cw, int ch, KchTone tone, int inset);
/*
 * A SELECTED ROW, in the language a task button is drawn in — the plate, plus
 * an accent bar down its leading edge.
 *
 * Shared because "what does a selection look like" is one question and this
 * desktop was answering it in three places: the Start menu, the cascading
 * menu and the window menu each drew their own plate-and-bar, and the two
 * nobody was looking at were the two that would drift. The cells are left on
 * the page's own slot by the caller, so the plate shows through under the
 * label.
 */
void kch_px_row(int cx, int cy, int cw, KchTone tone);
void kch_px_vrule(int cx, int y0, int rows);
void kch_px_replay(pixman_image_t *dst, int scale);
/*
 * The body, the edge and the lip — what every KDOS surface sits on.
 *
 * `edge` says which side gets the bright line: KCH_EDGE_TOP or _BOTTOM for a
 * bar, whose only boundary with the desktop is the one it is not anchored to,
 * and KCH_EDGE_NONE for anything that draws its own `╔══ Title ══╗`. A framed
 * surface with a pixel edge as well has TWO top borders a few pixels apart,
 * which reads as a rendering fault rather than as a highlight.
 */
enum { KCH_EDGE_NONE = -1, KCH_EDGE_TOP = 0, KCH_EDGE_BOTTOM = 1 };
void kch_px_body(pixman_image_t *dst, int w, int h, int scale, uint8_t alpha,
		 int edge);
/*
 * Wear that body, in one call: install the backdrop and hand it the slot the
 * surface fills itself with. Everything a panel opens goes through this, so a
 * popup cannot come up opaque grey beside a translucent bar.
 *
 * `body_slot` is whichever of libktui's eight the surface treats as its
 * background — the two halves of this desktop disagree, and asking rather than
 * assuming is what lets both keep their own.
 *
 * NO PIXEL EDGE. A popup draws its own `╔══ Title ══╗`, and that box IS where
 * the surface starts; a bright hairline a few pixels above it is a second top
 * border. The bar gets one because it has no box to draw.
 */
void kch_px_popup(int body_slot);
/*
 * The same hand-off with NO body of its own — the surface is nothing but the
 * plates it recorded, and everything else is see-through.
 *
 * For a surface that is not a window: a stack of toasts is a column of
 * separate cards with desktop between them, so a body painted across the whole
 * surface would fill the gaps in as well and turn the stack into one slab.
 */
void kch_px_bare(int body_slot);
/* The slot the surface behind this chrome is drawn in — see kch_px.c. */
int kch_body_slot(void);
/* One of libktui's eight slots as the packed value the painters take. */
uint32_t kch_slot_rgb(int slot);

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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — the KDOS terminal toolkit
 *
 * Links nothing but libc. No terminfo, no ncurses: a KDOS program can use
 * this in phase 1, before any library exists to link against.
 *
 * It carries two things nothing off the shelf does. The palette is installed
 * into the Linux VT with PIO_CMAP and exactly restored, so a tty and a
 * truecolor terminal render the same picture. And the mouse works on tty1
 * with no gpm, because the Linux console has no mouse reporting at all and
 * the input layer reads /dev/input/event* itself.
 * ---------------------------------
 */

#ifndef KTUI_H
#define KTUI_H

#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

/* ────────────────────────────────────────────────────────────────────────
 * Geometry
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	int x, y, w, h;
} KRect;

static inline KRect krect(int x, int y, int w, int h)
{
	KRect r = { x, y, w, h };
	return r;
}

static inline int krect_hit(KRect r, int x, int y)
{
	return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* ────────────────────────────────────────────────────────────────────────
 * Colour
 *
 * Eight slots and no more. That is not minimalism for its own sake: the
 * console font is 512 glyphs, so the VT steals the foreground intensity bit
 * for the 9th glyph bit and colours 8-15 become unreachable as foreground.
 * Designing to eight means the TTY and a truecolor foot window render the
 * SAME picture, one with exact hex and one with the palette we install.
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_BG = 0,	/* backdrop                        */
	KT_ERR,		/* urgent                          */
	KT_ACCENT,	/* primary                         */
	KT_WARN,	/* secondary                       */
	KT_DIM,		/* inactive borders, disabled text */
	KT_MID,		/* secondary text, bar fill        */
	KT_SURFACE,	/* panel background                */
	KT_TEXT,	/* body text                       */
	KT_NCOLOR
};

typedef struct {
	uint8_t r, g, b;
} KRgb;

typedef struct {
	const char *name;
	const char *label;
	KRgb slot[KT_NCOLOR];
} KtuiTheme;

extern const KtuiTheme ktui_themes[];
extern int ktui_ntheme;
extern const KtuiTheme *ktui_theme;

int ktui_theme_set(const char *name);

/*
 * NIGHT LIGHT — a warm transform over the eight slots, not a scheme of its own.
 *
 * Eight accents times a warm copy is sixteen palettes to keep in step, and
 * the cast belongs to the screen rather than to the theme: the scheme stays
 * the one the user chose and `ktui_theme` points at a warmed copy of it while
 * this is on. Blue loses the most and red nothing, which is what a colour
 * temperature is and why a warmed accent still reads as itself.
 *
 * Returns non-zero when the palette actually changed, so a caller can skip a
 * repaint it does not owe. THE CALLER READS THE TOGGLE: this library holds no
 * opinion about where a desktop keeps its state, and a surface that loads its
 * scheme already has the state directory in hand. A surface that reads the
 * scheme and not the toggle draws the cold palette while the toggle is on.
 */
int ktui_theme_night(int on);

/*
 * THE NEAREST SLOT TO AN ARBITRARY COLOUR, by squared distance.
 *
 * The one rule for reducing a colour that came from outside the palette — a
 * terminal's SGR, a picture's average — to something this desktop can draw. A
 * table mapping "red means the error slot" would be a second set of colour
 * decisions beside the palette, and it would stop following the accent:
 * `kdos theme amber` has to move every colour with it.
 */
int ktui_theme_nearest(uint32_t rgb);

/*
 * THE NAME OF A SLOT, for a colour shown to a person rather than drawn. The
 * names are the ones the enum uses, lowercased, so what a person is handed is
 * what they would write in a configuration file. NULL for a value that is not
 * a slot.
 *
 * NO SURFACE IN THIS TREE CALLS IT — the library's own tests are its only
 * caller. It is the answer a colour picker owes a person beside the hex, and
 * a header entry with neither a caller nor this line reads as load-bearing to
 * whoever changes the palette next.
 */
const char *ktui_slot_name(int slot);

/* ────────────────────────────────────────────────────────────────────────
 * Terminal
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_CAP_TRUECOLOR = 1 << 0,	/* 24-bit SGR                        */
	KT_CAP_256 = 1 << 1,		/* indexed 256                       */
	KT_CAP_LINUXVT = 1 << 2,	/* real VT: PIO_CMAP palette, no     */
					/* bold, no xterm mouse -> evdev     */
	KT_CAP_UTF8 = 1 << 3,
	KT_CAP_MOUSE = 1 << 4,
	/* DECSET 2026 is understood, so a frame may be bracketed and is shown
	 * whole or not at all. Probed with DECRQM and never assumed: a
	 * terminal that does not know the mode ignores the brackets, but one
	 * that knows it and is left INSIDE a block shows nothing further, so
	 * the bit also says who is owed the closing sequence. */
	KT_CAP_SYNC = 1 << 5
};

extern int ktui_caps;
extern int ktui_w, ktui_h;
extern volatile sig_atomic_t ktui_resized;

int ktui_term_init(int want_mouse);
void ktui_term_shutdown(void);
void ktui_term_suspend(void);	/* drop back to the cooked terminal        */
void ktui_term_resume(void);	/* and take it over again                  */
void ktui_term_size_refresh(void);
void ktui_term_write(const char *s, size_t n);
void ktui_term_flush(void);
/* Bound one flush to `ms` milliseconds, dropping whatever will not go out
 * in that time; -1 (the default) waits as long as the terminal takes. A
 * caller that must keep servicing something else while it draws sets this,
 * and asks ktui_term_flush_dropped() whether the last frame survived. */
void ktui_term_set_write_timeout(int ms);
int ktui_term_flush_dropped(void);

/*
 * THE HOST TERMINAL WENT AWAY — an `ssh` drop, a closed window, a pty whose
 * far end is gone. Sticky: a terminal that has hung up does not come back.
 *
 * A CONSUMER THAT DRAWS FOR EVER HAS TO ASK. Nothing else says so: a write to
 * a hung-up descriptor fails and a read returns end of file, and a loop that
 * checked neither spins at its poll timeout painting frames nobody receives —
 * and never reaches the exit that would have put the terminal back.
 */
int ktui_term_hungup(void);
/* Record the same thing from the reading side: the input layer sees a
 * terminal go as an end of file rather than as a failed write. */
void ktui_term_mark_hungup(void);
void ktui_term_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ktui_term_repalette(void);	/* after a live accent switch              */
/* OSC 52 clipboard write, base64 encoded by hand (this library links nothing
 * but libc). Returns whether anything was actually emitted: the Linux VT
 * (KT_CAP_LINUXVT) has no OSC 52 handler at all — not "some terminals don't",
 * every VT — so this is a deliberate no-op there, and the caller must not
 * tell the user something was copied when it was not. */
int ktui_clip_copy(const char *text);

/* ────────────────────────────────────────────────────────────────────────
 * Cell buffer
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_A_NONE = 0,
	KT_A_BOLD = 1 << 0,	/* suppressed on a VT: the bit is the font page */
	KT_A_REVERSE = 1 << 1,
	KT_A_UNDERLINE = 1 << 2,
	/* The three a terminal's SGR carries and this desktop draws. They are
	 * bits in the byte a cell already had, so no consumer reading the low
	 * byte alone is widened for them; a real VT is where they are dropped,
	 * because there an attribute bit selects a FONT PAGE rather than a
	 * style. */
	KT_A_ITALIC = 1 << 3,
	KT_A_STRIKE = 1 << 4,
	KT_A_OVERLINE = 1 << 5,

	/*
	 * THIS CELL IS AN EMBEDDED GUEST'S PIXELS, and the guest's own
	 * compositor has already drawn a cursor into them.
	 *
	 * NOT A STYLE — a fact about where the cell came from, and the flush is
	 * its only reader: it is what says the pointer must not be drawn here,
	 * because one drawn over a composited cursor is a SECOND pointer a cell
	 * from the first and the one a person aims with is the guest's.
	 *
	 * IN THE LOW BYTE, WHICH EVERY CONSUMER READS UNCONDITIONALLY. Above
	 * the eighth bit a bit is meaningful only to a consumer that also reads
	 * the literal colours, and one that ignores them would draw two
	 * pointers.
	 *
	 * Set on a guest's CONTENT cells alone. The chrome around the window is
	 * drawn in cells and carries none, so the pointer comes back the moment
	 * it reaches the border — which is where a window is grabbed, moved and
	 * resized — and a block whose picture has not arrived is a shade mark
	 * with no bit, so a window that has not drawn keeps its pointer.
	 */
	KT_A_GUEST = 1 << 6,

	/*
	 * ABOVE THE EIGHTH BIT NOTHING IS PART OF THE PORTABLE ATTRIBUTE.
	 *
	 * The low byte is what a consumer with slots alone honours;
	 * `fgc`/`bgc`/`ulc` beside it are the literals, and these bits say
	 * which of them mean anything. Putting them in the low byte would hand
	 * a display that draws in slots a cell claiming a colour it never
	 * reads, and it would draw the black it was never given.
	 *
	 * ONE BIT PER COLOUR, not one for the pair. A program that sets a
	 * foreground and leaves the background alone is the common case, and a
	 * single bit would have to freeze the theme's background into the cell
	 * as a literal — after which that cell stops following `kdos theme`
	 * and a retint leaves a rectangle of the old scheme behind.
	 */
	KT_A_FGRGB = 1 << 8,		/* fgc is the glyph's colour         */
	KT_A_BGRGB = 1 << 9,		/* bgc is the colour behind it       */
	KT_A_ULCOLOR = 1 << 10		/* ulc is the underline's own colour */
};

/*
 * THE UNDERLINE'S SHAPE, in three bits above those.
 *
 * `KT_A_UNDERLINE` says there is one and is what every consumer already
 * honours; the style refines it, so a display that draws in slots alone draws
 * a straight line rather than nothing. SGR `4:0`-`4:5` in order, and 0
 * means the plain line the attribute alone asks for.
 */
enum {
	KT_UL_PLAIN = 0,
	KT_UL_SINGLE,
	KT_UL_DOUBLE,
	KT_UL_CURLY,
	KT_UL_DOTTED,
	KT_UL_DASHED
};

#define KT_UL_SHIFT 11
#define KT_A_ULSTYLE (7u << KT_UL_SHIFT)
#define KT_UL_STYLE(a) (((unsigned)(a) >> KT_UL_SHIFT) & 7u)
#define KT_UL_SET(n) (((unsigned)(n) & 7u) << KT_UL_SHIFT)

typedef struct {
	uint32_t ch;
	uint8_t fg, bg;
	uint16_t attr;
	/*
	 * The literal a terminal asked for, kept BESIDE the slot the same
	 * colour reduced to rather than instead of it. Every consumer that has
	 * only slots — a dump, a golden, a tty with sixteen colours — reads
	 * `fg`/`bg` and is unaffected by whatever is here. Meaningful only with KT_A_FGRGB, KT_A_BGRGB and
	 * KT_A_ULCOLOR.
	 */
	uint32_t fgc, bgc, ulc;
} KtuiCell;

/* A double-width codepoint occupies TWO cells: the glyph in cell i and this
 * marker in cell i+1 (same colours). Backends skip the marker — the glyph
 * already covered it — and a painter may extend a wide glyph's clip into the
 * next cell only when it holds this marker. 0x1 is a control code no text
 * path ever writes, so it cannot collide with a real character. */
#define KTUI_WIDE_CONT 0x1u

/* ────────────────────────────────────────────────────────────────────────
 * Sprites — a picture occupying whole cells (ktui_sprite.c)
 *
 * The desktop is a character grid and stays one; a sprite is an ENHANCEMENT
 * layer over it, never a replacement. Every consumer must draw correctly when
 * ktui_sprite_put() answers -1, which is what a tty, a missing icon theme, a
 * full table and `icons = off` all look like.
 *
 * A sprite cell's codepoint carries everything a painter needs:
 *
 *     0x02 | slot(16) | sy(4) | sx(4)
 *
 * — so no separate damage list exists, and none is needed: the row diff sees a
 * different slot as a different cell. 0x02000000 is above Unicode's last
 * codepoint (0x10FFFF), so it cannot collide with text.
 * ──────────────────────────────────────────────────────────────────────── */

#define KTUI_SPRITE_BASE  0x02000000u
/*
 * FOUR THOUSAND, and the number comes from a full screen. A picture is tiled
 * into 16x16-cell sprites, so a 240x67 grid covered edge to edge is
 * ceil(240/16) * ceil(67/16) = 75 of them — and a terminal showing several
 * pictures, plus every icon the panel and the desktop hold, is the case that
 * has to fit. The slot encoding already carries sixteen bits, so this is the
 * table's size and nothing else.
 */
#define KTUI_MAX_SPRITES  4096
#define KTUI_IS_SPRITE(ch) (((ch) & 0xff000000u) == KTUI_SPRITE_BASE)
#define KTUI_SPRITE_SLOT(ch) (((ch) >> 8) & 0xffffu)
#define KTUI_SPRITE_SX(ch) ((ch) & 0xfu)
#define KTUI_SPRITE_SY(ch) (((ch) >> 4) & 0xfu)

typedef struct {
	uint64_t key;		/* content identity, the CALLER's hash      */
	const void *pix;	/* pixman_image_t *, owned by the caller    */
	uint32_t fallback;	/* what a text backend puts there instead   */
	int w, h;		/* size in cells, 1..16                     */
	/*
	 * BUMPED ON EVERY PUT, AND IT IS WHAT A CACHING BACKEND COMPARES.
	 * An animation re-registers the same key so the cells go on naming the
	 * same slot and only the pixels change — and the new picture is very
	 * often the SAME POINTER, because the evictor freed the old one and
	 * the allocator handed the memory straight back. A backend that keyed
	 * its "already uploaded" cache on the pointer would then never take a
	 * frame after the first, and the animation would stop on its first
	 * picture.
	 */
	unsigned long gen;
} KtuiSprite;

/* Register (or refresh) the picture for `key`. `pix` must already be scaled to
 * cw*cell_w x ch*cell_h at the backend's scale — this library does no pixel
 * work at all. Returns a slot, or -1 when the caller must draw its glyph. */
int ktui_sprite_put(uint64_t key, const void *pix, int cw, int ch,
		    uint32_t fallback);
int ktui_sprite_find(uint64_t key);
const KtuiSprite *ktui_sprite_get(int slot);
/* How many slots the table has ever handed out — a high-water mark and not a
 * live count, since a dropped slot is reused rather than renumbered. No
 * backend in this tree calls it; it is what a backend sizing a cache of its
 * own would ask. */
int ktui_sprite_slots(void);
/* Call BEFORE freeing the picture. */
void ktui_sprite_drop(uint64_t key);

/*
 * EVICTION IS OPT-IN, and it is opt-in because of what a sprite is: the table
 * holds a borrowed pointer and this library does no pixel work, so it cannot
 * free a picture and must not drop one somebody is still drawing. Both
 * problems are solved by the owner saying how:
 *
 *   - `fn` is called with the key and the picture when a slot is taken back,
 *     so the owner frees it at the moment the table stops naming it. It is
 *     also called for a picture the table REFUSED mid-way through a tiled
 *     put, which carries the same message: nothing here will ever name it.
 *   - Only a sprite NOT referenced by the current cell buffer is evictable.
 *     The table can check that because the cell buffer is this library's.
 *   - Least recently used first, where "used" means put or found.
 *
 * With no evictor registered the table fills and `ktui_sprite_put` answers -1,
 * which every consumer already handles by drawing its glyph. That is the right
 * behaviour for icons, which are owned for the life of the session.
 */
typedef void (*KtuiSpriteFree)(uint64_t key, const void *pix, void *user);

void ktui_sprite_evictor(KtuiSpriteFree fn, void *user);

/*
 * A byte budget on top of the slot count, for pictures rather than icons: a
 * full-screen photograph is megabytes and a hundred of them is a leak with a
 * cap. `cell_px` is how many pixels one cell is at the current scale — the
 * table does no pixel work, so it cannot know that and has to be told. Zero
 * bytes, or an unset cell size, means the slot count is the only limit.
 */
void ktui_sprite_budget(size_t max_bytes, int cell_w_px, int cell_h_px);
size_t ktui_sprite_bytes(void);

/*
 * The buffer a backend last diffed against, and its OWN size — which is not
 * ktui_w by ktui_h between a backend resize and the consumer's
 * ktui_draw_resize(). For the sprite table's eviction check and nothing else.
 *
 * IT IS NOT WHAT IS ON THE SCREEN, and a backend need not maintain it at all:
 * one that keeps previous frames of its own ignores it. Read
 * `ktui_draw_cells()` for the composed frame.
 */
const KtuiCell *ktui_cells(int *w, int *h);
/* The frame being composed: what ktui_draw_cell writes and what the next flush
 * sends. This is what is on the screen. */
const KtuiCell *ktui_draw_cells(int *w, int *h);

/*
 * A picture larger than one slot is a GRID of slots sharing a key prefix, so
 * it evicts and re-registers as a unit rather than leaving three quarters of a
 * photograph on the screen. The stride is XORed rather than added, because two
 * pictures with adjacent keys — a file path and a frame number is exactly that
 * — would otherwise collide on their tiles.
 */
#define KTUI_TILE_STRIDE 0x9e3779b97f4a7c15ULL

/* Called once per tile with the sub-rectangle it covers, in CELLS. Returns the
 * picture for that tile, already scaled, or NULL to abandon the whole thing. */
typedef const void *(*KtuiSpriteTile)(void *user, int cell_x, int cell_y,
				      int cw, int ch);

/*
 * Tiles registered, or -1. All or nothing: a partial picture draws a hole.
 *
 * `fallback` is what EVERY tile shows where pixels cannot be drawn — a tty, a
 * view with no pixel library, a dump. A picture is worth a mark there: a
 * photograph that renders as nothing at all is indistinguishable from output
 * that never arrived.
 */
int ktui_sprite_put_tiled(uint64_t key, int cw, int ch, uint32_t fallback,
			  KtuiSpriteTile tile, void *user);
void ktui_sprite_drop_tiled(uint64_t key, int cw, int ch);
int ktui_sprite_tile_at(uint64_t key, int cw, int cell_x, int cell_y,
			int *sx, int *sy);
void ktui_sprite_clear(void);
void ktui_draw_sprite(KRect r, int slot, int fg, int bg);
/* A text backend's substitute for a sprite cell. */
uint32_t ktui_sprite_text_cell(uint32_t ch);

/* Glyphs, resolved once against what the console font actually carries. */
enum {
	KT_G_HL, KT_G_VL, KT_G_TL, KT_G_TR, KT_G_BL, KT_G_BR,	/* single box */
	KT_G_TEE_L, KT_G_TEE_R, KT_G_TEE_T, KT_G_TEE_B, KT_G_CROSS,
	KT_G_DHL, KT_G_DVL, KT_G_DTL, KT_G_DTR, KT_G_DBL, KT_G_DBR, /* double */
	KT_G_FULL, KT_G_SHADE, KT_G_SHADE_MED,
	KT_G_DOT, KT_G_BULLET, KT_G_SQUARE,
	KT_G_UP, KT_G_DOWN, KT_G_LEFT, KT_G_RIGHT, KT_G_ELLIPSIS, KT_G_DEG,
	/*
	 * CONTROL FURNITURE, and separate from KT_G_UP/DOWN/LEFT/RIGHT on
	 * purpose. Those four are arrows in running text and in a hint row,
	 * where they mean DIRECTION; a solid triangle there reads as a control
	 * somebody can press. These five are the parts a control is built from
	 * — a scrollbar's end caps, the marker on a selected row, the shadow
	 * under a button — and they never appear in a sentence.
	 */
	KT_G_ARROW_UP, KT_G_ARROW_DOWN, KT_G_ARROW_L, KT_G_ARROW_R,
	KT_G_N
};

extern const char *ktui_glyph[KT_G_N];

/* ────────────────────────────────────────────────────────────────────────
 * Backends
 *
 * The cell buffer, the widgets, the glyph tiers and the layout do not care
 * where the cells end up. This vtable is that seam, and it exists so the
 * Wayland backend can live in a DIFFERENT ARCHIVE: libktui links nothing but
 * musl, and it has to keep doing so, because kinstall links it in phase 1
 * before any library exists to link against. libkwl is where wayland-client,
 * pixman, fcft and xkbcommon go. If libktui ever gains a real `-l`, kinstall
 * moves to phase 4 with it — which is not a trade anyone wants.
 *
 * `flush` receives both buffers and decides for itself what changed. It is not
 * handed a damage list because the tty backend's diff is fused into its
 * emission — it walks cells and writes escapes in the same pass, tracking
 * cursor position and SGR state as it goes — and splitting that in two to fit
 * a tidier signature would move pixels that must not move.
 * ──────────────────────────────────────────────────────────────────────── */

/* Tagged and forward-declared because the backend vtable above needs the name
 * before the input layer below defines the fields. */
typedef struct KtuiEvent KtuiEvent;
typedef struct KtuiRaw KtuiRaw;

typedef struct {
	const char *name;
	/* `prev` is the last-presented buffer, updated by the backend as it
	 * presents. `force_full` means ignore it and repaint everything. */
	void (*flush)(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full);
	/*
	 * THESE CELLS OWE A REPAINT THOUGH THEIR BYTES DID NOT CHANGE, for the
	 * one thing a cell names rather than carries: a sprite cell holds a
	 * SLOT, so a new picture in the same slot writes an identical cell.
	 *
	 * A BACKEND THAT KEEPS ITS OWN PREVIOUS FRAME MUST IMPLEMENT THIS, AND
	 * SPOIL EVERY COPY IT KEEPS. `prev` above is libktui's, and a backend
	 * is free to diff against copies of its own instead — libkwl keeps one
	 * per buffer plus the cells the compositor is showing. Spoiling
	 * libktui's changes nothing such a backend reads; spoiling some but
	 * not all of its own repaints pixels into a buffer nothing is told to
	 * re-read, or damages rows the paint left alone. Either way the screen
	 * holds the first frame of every animation for ever, with every other
	 * part of the path reporting success. NULL means the backend diffs
	 * against `prev`, and libktui spoils that instead.
	 */
	void (*dirty)(int x, int y, int w, int h);
	int (*poll_event)(KtuiEvent *ev, int timeout_ms);
	void (*size)(int *w, int *h);
	int (*caps)(void);
	/*
	 * WHERE THIS SURFACE'S CARET IS, in its own cells, or a negative x for
	 * none. For a backend drawing on somebody else's screen: it has no
	 * terminal cursor of its own to place, and only it knows where this
	 * surface sits on that screen. NULL is a backend that places its own
	 * cursor, and ktui_term_caret() then writes the escape — which is what
	 * every backend in this tree does, so filling this in turns a branch
	 * on rather than replacing one.
	 */
	void (*caret)(int x, int y);
	/*
	 * WHERE THE POINTER IS, in this surface's cells, for a backend that
	 * draws one ITSELF. NULL is a backend with no pixels of its own, and
	 * ktui_draw_flush() then reverses the cell under it.
	 *
	 * RETURNS 1 TO CLAIM THE POINTER, and the flush leaves the cells
	 * exactly as the session composed them; 0 declines this frame and the
	 * reversed cell is drawn as if there were no hook at all. A backend
	 * that cannot draw yet — no font, no screen lit — declines rather than
	 * claiming a pointer nobody can see.
	 *
	 * A NEGATIVE x IS NO POINTER, and the call still has to be made: it is
	 * the only thing that tells a backend to take the last one off the
	 * screen. The flush sends it for a hidden pointer and for one over an
	 * embedded guest's own cells alike, because over a guest nothing of
	 * this desktop's is drawn at all — the guest's compositor puts a
	 * cursor in those pixels and a second one a cell away is the one
	 * nobody is aiming with.
	 *
	 * CELLS AND NOT PIXELS, because a cell is the whole of what the session
	 * decided: it reports a cooked motion only when the cell changes, so a
	 * backend given pixels here would be given the same pixel until it did.
	 * A BACKEND THAT OWNS THE DEVICE MAY DRAW FINER THAN THIS, and one that
	 * does reads its own position rather than these coordinates — what it
	 * takes from here is that there is a pointer and which cell the session
	 * believes it is on, which is what says whose it is to draw.
	 *
	 * `shape` IS A KT_PTR_* AND A BACKEND MAY IGNORE IT. It says what a
	 * press would do where the pointer is — a resize, a drag, a text
	 * caret — and a backend that draws one picture answers the same way
	 * whatever it is given. Nothing about the CELLS depends on it, which
	 * is what lets a backend that draws a reversed cell and one that draws
	 * an arrow render the same frame identically.
	 */
	int (*pointer)(int x, int y, int shape);
	/*
	 * WHETHER THE LAST FLUSH ACTUALLY REACHED THE SCREEN, or NULL for a
	 * backend that always presents what it is given.
	 *
	 * The Wayland backend does not: it stashes a frame while the
	 * compositor holds both buffers. A skipped frame leaves `prev`
	 * describing a picture nobody saw, so a full repaint handed to that
	 * flush would be forgotten — the flag is cleared before the backend
	 * runs, by design, so that a backend may ask for one from inside it.
	 * Answering no here is how the repaint survives to the next frame.
	 */
	int (*presented)(void);
	/*
	 * THE SAME PHYSICAL INPUT, UNRESOLVED, or NULL for a backend with no
	 * device of its own.
	 *
	 * poll_event above answers a CHARACTER and a CELL, which is everything
	 * a cell surface wants and nothing a client of its own pixels can use:
	 * such a client holds a key down, repeats from its own keymap, reads a
	 * modifier that produces no character, and aims at a scrollbar two
	 * pixels wide. So the switch and the pixel travel too, in a queue of
	 * their own.
	 *
	 * A QUEUE OF ITS OWN BECAUSE MOTION COALESCES AND A KEY MUST NOT. A
	 * thousand-hertz mouse in the cooked queue evicts the click that came
	 * before it, which is the oldest entry there; here consecutive motions
	 * merge into one and nothing else merges at all.
	 *
	 * THE COOKED EVENT FOR ONE PHYSICAL INPUT IS SENT BEFORE ITS RAW
	 * PARTNER, which is what lets a session decide whether a chord ate a
	 * key before the guest is handed it. The order is carried by
	 * `KtuiRaw.after` and not by the order the two queues were filled in:
	 * a caller drains them TOGETHER against that number, rather than one
	 * queue and then the other. Returns 1 and fills `ev`, or 0 when the
	 * queue is empty.
	 */
	int (*poll_raw)(KtuiRaw *ev);
	/*
	 * THE LAYOUT THIS BACKEND'S KEYBOARD IS RUNNING, in xkb's text format,
	 * NUL-terminated, owned by the backend and valid until the next call.
	 * NULL for a backend with no keyboard of its own.
	 *
	 * A KEYCODE MEANS NOTHING WITHOUT IT: a guest handed codes alone reads
	 * US positions, and a French keyboard types the wrong letters. `*gen`
	 * is set to a counter the backend bumps whenever the text changes, so
	 * a caller forwards the layout again without comparing tens of
	 * kilobytes.
	 */
	const char *(*keymap)(unsigned *gen);
} KtuiBackend;

/* NULL selects the built-in tty backend. A backend must outlive the library's
 * use of it; libkwl hands over a pointer to a static. */
void ktui_backend_set(const KtuiBackend *b);
const KtuiBackend *ktui_backend(void);

int ktui_draw_init(void);
/* Render with no terminal at all: allocate the cell buffer at a fixed size and
 * write the result as plain text instead of escapes. This is what lets a
 * screen be looked at, and diffed, without a two-hour build behind it — every
 * geometry defect these widgets have shipped (text over a box border, a heat
 * strip past its rect, a column out from under its own header) was invisible
 * to the compiler and to a test suite that cannot draw. */
int ktui_offscreen_init(int w, int h);
/* Whether this process is drawing offscreen. Asked by the few places that
 * would otherwise write to a terminal that is not there — the caret is one,
 * and the escape it wrote landed inside a committed reference frame. */
int ktui_offscreen(void);
void ktui_draw_dump(void);
void ktui_draw_resize(void);
void ktui_draw_clear(void);
void ktui_draw_flush(void);
void ktui_draw_invalidate(void);	/* force a full repaint next flush */
/*
 * REPAINT THESE CELLS NEXT FLUSH EVEN IF THEY DID NOT CHANGE, by spoiling the
 * previous-frame copy for them. For the one thing a cell names rather than
 * carries: a sprite cell holds a SLOT, so a new picture in the same slot
 * writes identical cells and the flush's diff would find nothing to send.
 * Costs the rectangle; ktui_draw_invalidate() costs the screen.
 */
void ktui_draw_dirty(int x, int y, int w, int h);

void ktui_draw_cell(int x, int y, uint32_t ch, int fg, int bg, int attr);
/*
 * A CELL COPIED WHOLE, which is the only way a literal colour reaches a frame.
 *
 * Terminal content and nothing else uses it: chrome is slots, always, because
 * a chrome colour that stopped following `kdos theme` would be a second
 * palette nobody can change. It clips like any other draw.
 */
void ktui_draw_put(int x, int y, const KtuiCell *c);
/* XOR the reverse attribute over a rectangle of the frame being composed —
 * a selection, which leaves the content and changes only how it reads. Not
 * expressible through ktui_cells(), which hands out the flushed frame. */
void ktui_draw_reverse(KRect r);
void ktui_draw_fill(KRect r, int bg);
int ktui_draw_text(int x, int y, int maxw, const char *s, int fg, int bg,
		   int attr);
int ktui_draw_textf(int x, int y, int maxw, int fg, int bg, int attr,
		    const char *fmt, ...) __attribute__((format(printf, 7, 8)));
/* Draws `s` so that it ENDS at x + w - 1. Every duration, size and count
 * column in the KDOS TUIs was hand-padded with a %8s-style guess, which drifts
 * out of line with its own header the moment a value overflows the field. */
int ktui_draw_text_right(int x, int y, int w, const char *s, int fg, int bg,
			 int attr);
void ktui_draw_hline(int x, int y, int w, int g, int fg, int bg);
void ktui_draw_vline(int x, int y, int h, int g, int fg, int bg);
void ktui_draw_box(KRect r, const char *title, int fg, int bg, int dbl);
/*
 * A ONE-CELL DROP SHADOW hanging a column right of `r` and a row below it.
 *
 * IT DARKENS AND DOES NOT ERASE: every cell keeps its glyph and both halves
 * are mixed towards KT_BG, so the window underneath stays legible and an
 * embedded application's picture is left alone entirely. The background slot
 * goes to KT_BG beside the literal, which is the whole of the shadow on a
 * display that draws in slots alone.
 */
void ktui_draw_shadow(KRect r);
/*
 * SEE THROUGH A RECTANGLE OF THE COMPOSED FRAME.
 *
 * A grid holds one colour per cell and nothing behind it, so translucency is
 * two calls around the drawing: ktui_draw_bg_take() copies the background
 * colours of `r` into `out`, which the caller sizes r.w * r.h, and
 * ktui_draw_blend() mixes what has since been drawn there back towards them.
 * `alpha` is the weight of the new content, 0..255; 255 does nothing.
 *
 * THE BLEND IS WRITTEN AS THE CELL'S LITERAL and the slot is left alone, so a
 * display that draws in slots alone shows an opaque rectangle. Backgrounds
 * only — the ink keeps the colour it was drawn in — and a sprite cell is
 * skipped, because a picture's pixels are not a background.
 *
 * UNDER KT_A_REVERSE THE FOREGROUND IS THE BACKGROUND, and both calls follow
 * the swap the painter makes: reading or writing `bg` through a reversed cell
 * would leave it opaque and make its ink translucent instead.
 *
 * NO SURFACE IN THIS TREE CALLS THE PAIR. A surface that wants its whole body
 * seen through sets `KDispConfig.opacity` instead, which dims the one slot
 * that body is drawn in and leaves the ink alone; this pair is the route for a
 * RECTANGLE of cells rather than a whole surface.
 */
void ktui_draw_bg_take(KRect r, uint32_t *out);
void ktui_draw_blend(KRect r, const uint32_t *under, int alpha);
void ktui_draw_cursor(int x, int y);	/* where the pointer is, in cells  */
/*
 * THE POINTER'S SHAPES.
 *
 * THIS LIST IS WHAT THE DESKTOP MEANS AND NOT WHAT A PROTOCOL CARRIES. It is
 * not `wp_cursor_shape_device_v1`'s enumeration and not X11's: a number that
 * happened to equal an upstream one would be a coupling neither end could
 * see, so whatever forwards these maps them in a switch — the rule the raw
 * event codes already keep.
 *
 * SEVEN, BECAUSE SEVEN IS WHAT THIS DESKTOP CAN MEAN. Every one of them
 * answers a question a person asks with their hand: can I resize this, and in
 * which direction; can I drag it; is this text I can select. There is no
 * hand, no crosshair, no help pointer and NO BUSY POINTER — a shape nothing
 * sets is a picture nobody maintains, and nothing in this session tracks a
 * window as not-answering in a way a pointer could report. A busy pointer
 * arrives with the state that justifies it or not at all.
 */
enum {
	KT_PTR_ARROW = 0,	/* the default, and every unhandled case    */
	KT_PTR_IBEAM,		/* text: a caret goes where you press       */
	KT_PTR_SIZE_NS,		/* a top or bottom edge                     */
	KT_PTR_SIZE_WE,		/* a left or right edge                     */
	KT_PTR_SIZE_NWSE,	/* the top-left / bottom-right corners      */
	KT_PTR_SIZE_NESW,	/* the top-right / bottom-left corners      */
	KT_PTR_MOVE,		/* a handle: pressing moves the whole thing */
	KT_PTR_N
};

/*
 * WHAT A PRESS WHERE THE POINTER IS WOULD DO, as a KT_PTR_*.
 *
 * SEPARATE FROM THE POSITION because they are decided in different places and
 * at different rates: ktui_draw_cursor() is called every frame by whatever
 * owns the pointer, and the shape changes only when the thing under it does.
 *
 * IT IS A HINT AND NOT A GUARANTEE. Only a backend that fills
 * `KtuiBackend.pointer` draws a shape, and none in this tree fills it: a
 * terminal, a dump, `tty1` and the Wayland surface all reverse the cell under
 * the pointer whatever this says, because a character grid has one pointer and
 * that is it. So NOTHING may depend on the shape being visible — a control
 * that says what it does only through the pointer is a control that says
 * nothing at all here.
 */
void ktui_draw_cursor_shape(int shape);
int ktui_cursor_shape(void);
void ktui_draw_hide_cursor(void);
void ktui_draw_clip(KRect r);		/* confine drawing to a pane       */
void ktui_draw_clip_none(void);

/* How tall the last pass ASKED to be, clipping ignored — the scroll range is
 * computed from it. Reset to the top of the pane before drawing, read after.
 * Was a bare global that callers assigned to; it is a measurement, so it gets
 * a measurement's interface. */
void ktui_extent_reset(int y);
int ktui_extent(void);

int ktui_utf8_width(const char *s);	/* display cells, ignores overlong */
const char *ktui_utf8_next(const char *s, uint32_t *cp);
int ktui_utf8_encode(uint32_t cp, char *out);	/* out needs 4 bytes; ret len */
/* Display cells a codepoint occupies: 0 combining, 2 East-Asian wide and
 * fullwidth, 1 everything else. A compact range table, not the libc's — musl's
 * wcwidth answers for the locale, this answers for the cell grid, and the two
 * must not drift apart per-consumer. */
int ktui_wcwidth(uint32_t cp);

/* ────────────────────────────────────────────────────────────────────────
 * Ramps and charts
 *
 * Three tiers, not two. The console font KDOS ships is 512 glyphs of xos4-2
 * plus six double-line box characters: it has FULL BLOCK and the two shades
 * and it does NOT have eighth blocks, half blocks or braille. kinstall runs
 * on that VT and shares these widgets with kdosbuild, which runs on the host
 * in a modern terminal — so a chart is drawn at eight levels there and three
 * levels on a tty rather than being drawn in glyphs that come out blank.
 * ──────────────────────────────────────────────────────────────────────── */

void ktui_ramp_init(void);	/* called by ktui_draw_init                */
int ktui_ramp_levels(void);
const char *ktui_ramp_v(double f);	/* bottom-aligned — sparklines     */
const char *ktui_ramp_h(double f);	/* left-aligned   — bar tips       */

/* One ramp cell per column, newest sample at the RIGHT so a short window does
 * not slide the history sideways as it fills. vmax 0 autoscales over the
 * window that is actually drawn. */
void ktui_sparkline(KRect r, const double *v, int n, double vmax, int bg);
/* The value the top of the ramp represents for that same window — an
 * autoscaled chart with no stated peak tells you the SHAPE of the last two
 * minutes and nothing about its magnitude, which is half the reading. Pass
 * the same `cols` the sparkline was drawn with. */
double ktui_sparkline_peak(const double *v, int n, int cols);
/* A short bounded meter — memory, disk. Fill in `fg`, track in KT_DIM. */
void ktui_gauge(int x, int y, int w, double frac, int fg, int bg);
/* One cell per sample, averaged down when there are more samples than cells.
 * Used for the per-step duration strip: darkest cell is the slowest step.
 * Trailing `bg` matches ktui_gauge/ktui_sparkline — draw on the caller's
 * actual background, not a hardcoded one, or a heat strip on a highlighted
 * row punches a hole through the highlight. */
void ktui_heat(KRect r, const double *v, int n, double vmax, int bg);

/* ────────────────────────────────────────────────────────────────────────
 * Events
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_EVT_NONE = 0,
	KT_EVT_KEY,
	KT_EVT_MOUSE,
	KT_EVT_RESIZE,
	KT_EVT_TICK,
	KT_EVT_TOUCH,
	KT_EVT_DROP
};

/* Touch phases. CANCEL is not UP: the compositor or the driver has taken the
 * sequence away, and a gesture in progress is abandoned rather than completed. */
enum { KT_TOUCH_DOWN = 0, KT_TOUCH_MOVE, KT_TOUCH_UP, KT_TOUCH_CANCEL };

enum {
	KT_K_ESC = 27,
	KT_K_ENTER = 13,
	KT_K_TAB = 9,
	KT_K_BACKSPACE = 127,
	KT_K_SPECIAL = 0x110000,
	KT_K_UP, KT_K_DOWN, KT_K_LEFT, KT_K_RIGHT,
	KT_K_HOME, KT_K_END, KT_K_PGUP, KT_K_PGDN, KT_K_INS, KT_K_DEL,
	KT_K_BTAB,
	KT_K_F1, KT_K_F2, KT_K_F3, KT_K_F4, KT_K_F5, KT_K_F6,
	KT_K_F7, KT_K_F8, KT_K_F9, KT_K_F10, KT_K_F11, KT_K_F12,
	/*
	 * THE KEYS A KEYBOARD HAS AND A TERMINAL DOES NOT. A media key
	 * produces no character, so a backend reading a terminal never sees one
	 * and never will: these reach a surface through libkwl and nowhere
	 * else.
	 *
	 * APPENDED, NEVER INSERTED. The enum is positional from
	 * KT_K_SPECIAL and the NUMBER travels, so a key added in the middle
	 * renumbers every key after it and a client built before the change
	 * reads Home where
	 * the session sent End.
	 */
	KT_K_VOLUP, KT_K_VOLDOWN, KT_K_MUTE,
	KT_K_PLAY, KT_K_STOP, KT_K_NEXT, KT_K_PREV,
	/* Print is one of them: it produces no character either, so a terminal
	 * reports nothing for it and the capture chords on it reach a KMS view
	 * and nowhere else. */
	KT_K_PRINT
};

/* KT_MOD_SUPER is the desktop's own modifier — the one every window-management
 * chord is on, so that none of them can collide with what a program inside a
 * window wants. A backend that cannot report it leaves it clear, and those
 * chords simply do not fire.
 *
 * A CTRL CHORD IS THE LETTER PLUS KT_MOD_CTRL, never the control code. Every
 * backend delivers it that way: the terminal decoder unfolds the byte the tty
 * sends, and the Wayland backend reads the unmodified keysym. A chord table
 * that tested for 0x16 would fire on one backend and not the others. */
enum {
	KT_MOD_SHIFT = 1,
	KT_MOD_ALT = 2,
	KT_MOD_CTRL = 4,
	KT_MOD_SUPER = 8
};

enum {
	KT_MB_LEFT = 0, KT_MB_MIDDLE, KT_MB_RIGHT,
	KT_MB_WHEEL_UP, KT_MB_WHEEL_DOWN,
	KT_MB_MOVE
};

enum { KT_MP_RELEASE = 0, KT_MP_PRESS = 1, KT_MP_DRAG = 2 };

struct KtuiEvent {
	int type;
	int key;		/* codepoint or KT_K_*                     */
	int mods;
	int mx, my;
	int btn;
	int press;
	/* Touch only. `ms` is the BACKEND'S timestamp, not a clock read here:
	 * both libinput and wl_touch carry one, and taking theirs is what lets
	 * the recogniser be a pure function of its input and the test suite
	 * drive it without sleeping. */
	int slot;
	int phase;
	unsigned ms;
	/* What the recogniser made of it, KT_GEST_*, or KT_GEST_NONE. Filled by
	 * whichever backend fed ktui_gesture_feed, so a surface that wants the
	 * gesture reads it here instead of running a second recogniser. */
	int gesture;
};

/* ────────────────────────────────────────────────────────────────────────
 * Raw input
 *
 * THE SAME PHYSICAL INPUT AS ABOVE, BEFORE IT WAS RESOLVED. A key as a switch
 * with the evdev code the device sent, a pointer in pixels with the distance
 * the device actually moved, and a scroll with a second axis and a real value.
 *
 * NOTHING DRAWN IN CELLS READS ANY OF IT, and no widget in this toolkit does.
 * It is for a consumer that is not cells: one that holds keys down, resolves
 * the layout itself and aims at controls smaller than the grid pointing at
 * them. A backend fills it beside the KtuiEvent for the same event, and it is
 * drained through KtuiBackend.poll_raw — which nothing in this tree does. A
 * backend that fills the queue is paying for a consumer that has to arrive
 * with its own reader.
 *
 * THE FIELDS ARE FLAT AND NAMED PER TYPE, the shape KtuiEvent already keeps. A
 * union would save a dozen words per queue slot and cost every reader a switch
 * before it may look at anything.
 * ──────────────────────────────────────────────────────────────────────── */

enum { KT_RAW_NONE = 0, KT_RAW_KEY, KT_RAW_PTR, KT_RAW_AXIS };

/*
 * Which way a scroll went, and what made it. A wheel is already quantised and
 * a finger is not, and a toolkit that is told which behaves like every other
 * desktop — so the distinction travels rather than being flattened to a tick.
 *
 * THESE NUMBERS ARE THIS TOOLKIT'S OWN. They are not wl_pointer's and not
 * libinput's: a value that happened to equal an upstream enum would be a
 * coupling neither end could see, so whatever forwards them maps them in a
 * switch.
 */
enum { KT_RAW_VERT = 0, KT_RAW_HORIZ };
enum {
	KT_RAW_SRC_WHEEL = 0,
	KT_RAW_SRC_FINGER,
	KT_RAW_SRC_CONTINUOUS,
	KT_RAW_SRC_WHEEL_TILT
};
/* The device reverses the direction itself — natural scrolling. It is the
 * guest's to know, because the guest draws the scrollbar. */
#define KT_RAW_INVERTED 0x1

struct KtuiRaw {
	int type;		/* KT_RAW_*                                */
	/*
	 * KT_RAW_KEY: the evdev keycode, NOT +8 — xkb's offset is added by
	 * whoever compiles a keymap, and adding it twice is a keyboard one row
	 * out. KT_RAW_PTR: the evdev button, BTN_LEFT and up, and 0 for a
	 * motion that pressed nothing.
	 *
	 * THE FULL CODE AND NOT A NARROWED ONE. A mouse with side buttons
	 * drives Back and Forward in a browser, and KT_MB_* has three names in
	 * it.
	 */
	int code;
	int state;		/* 1 pressed, 0 released                   */
	/*
	 * KT_RAW_KEY: the xkb modifier state as this backend holds it, which
	 * is a RESYNC and not a per-key event. A guest drives its own xkb
	 * state from the key stream, and xkb's rule is that a state driven by
	 * keys must not also be set by mask; these are what no key can
	 * establish — a lock, or a layout group set before the guest existed.
	 */
	unsigned depressed, latched, locked, group;
	/*
	 * KT_RAW_PTR: where the pointer is in THIS BACKEND'S pixels, and the
	 * cell those pixels were measured in.
	 *
	 * THE CELL TRAVELS WITH THE POSITION so that whoever derives a cell
	 * from it derives the one this backend would, with no rounding of its
	 * own and no disagreement across a font step it has not been told
	 * about yet. Never zero while `type` is KT_RAW_PTR: it is a divisor.
	 */
	int x, y;
	int cell_w, cell_h;
	/*
	 * HOW FAR THE DEVICE MOVED, in 1/256 pixel, accelerated and — where
	 * the backend's own protocol carries a distance — as the device
	 * reported it. A position cannot say this: a guest that grabbed the
	 * pointer reads deltas and nothing else, and deltas reconstructed from
	 * two positions are the pointer's speed after acceleration, clamping
	 * and rounding to the desktop's own grid.
	 *
	 * A BACKEND WHOSE PROTOCOL CARRIES ONLY A POSITION puts that
	 * reconstruction in both pairs, because it is the only distance it
	 * has. A guest reading the unaccelerated pair on such a backend is
	 * reading the accelerated one, which is coarse and never absent.
	 */
	int dx, dy;
	int dx_un, dy_un;
	/*
	 * KT_RAW_AXIS: the distance scrolled in 1/256 pixel, and the
	 * high-resolution detent count where the device measured one — 120 to
	 * a detent, 0 where nothing counted them.
	 *
	 * BOTH NUMBERS, because a client reads one or the other and never
	 * both. A value of zero on both is the end of a gesture, which is what
	 * lets a client stop its kinetic scrolling.
	 */
	int value, value120;
	int axis;		/* KT_RAW_VERT or KT_RAW_HORIZ             */
	int source;		/* KT_RAW_SRC_*                            */
	int flags;		/* KT_RAW_INVERTED                         */
	/*
	 * KT_RAW_PTR and KT_RAW_AXIS: KT_MOD_*, for whoever ROUTES this —
	 * Super+drag is a chord on a pointer. A guest reads its own modifiers
	 * from the key stream instead, because a mask and a key stream that
	 * both drive one xkb state disagree about which keys are down.
	 */
	int mods;
	/* The BACKEND'S own clock, milliseconds, the rule KtuiEvent.ms
	 * keeps. */
	unsigned ms;
	/*
	 * HOW MANY COOKED EVENTS MUST BE TAKEN BEFORE THIS ONE IS SENT,
	 * counting from the first this backend produced. It is the whole of
	 * the ordering between two queues that cannot be interleaved by
	 * looking at them: the cooked partner of a raw event is the LAST event
	 * this number counts, so a caller sends the cooked queue up to `after`
	 * and only then sends this. A backend that queues the switch before
	 * the character it resolves to raises the number when that character
	 * is queued, so the field is the answer and the fill order is not.
	 *
	 * DRAINING ONE QUEUE TO EMPTY AND THEN THE OTHER LOSES IT. Two keys
	 * inside one poll arrive as two cooked messages and then two raw ones,
	 * and a session that swallowed the first key's chord has no way left
	 * to say which raw press it swallowed.
	 *
	 * A MERGED MOTION TAKES THE LATER NUMBER, because it is also the later
	 * event. A backend that queues no cooked event for an input — a raw
	 * motion inside one cell — leaves this at the count it already had,
	 * which sends it as soon as everything before it has gone.
	 */
	unsigned long long after;
};

/* ────────────────────────────────────────────────────────────────────────
 * Gestures
 *
 * ONE recogniser, fed by wl_touch under the Wayland backend. Putting the
 * disambiguation in a backend would mean writing it twice and having it
 * disagree twice, so a backend added later feeds this one rather than
 * carrying a recogniser of its own.
 *
 * It emits a gesture AND synthesises the ordinary mouse events every existing
 * widget already handles, so the toolkit inherits touch without being
 * rewritten. A widget that wants the gesture reads it; a widget that does not
 * sees a mouse.
 *
 * KT_GEST_, not KT_G_: the glyph tiers above own that prefix, and a collision
 * there is a compile error in every consumer at once.
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_GEST_NONE = 0,
	KT_GEST_TAP,
	KT_GEST_LONG,
	KT_GEST_DRAG,
	KT_GEST_SCROLL,
	KT_GEST_PINCH,
	KT_GEST_SWIPE_EDGE
};

/* Milliseconds. A press shorter than TAP that never left its cell is a tap; one
 * held past LONG without leaving it is a long press, reported ONCE. */
enum { KT_TAP_MS = 250, KT_LONG_MS = 500 };

typedef struct {
	int type;		/* KT_GEST_*                               */
	int x, y;		/* cell the gesture is at                  */
	int dx, dy;		/* cells moved since the last report       */
	int fingers;
	int edge;		/* KT_K_LEFT/RIGHT/UP/DOWN for an edge swipe */
} KtuiGesture;

/*
 * Feed one touch event. Returns 1 when `g` holds a gesture.
 *
 * `mouse` is filled with the synthesised pointer event when one is due and
 * `*have_mouse` set; a caller that only wants gestures may pass NULL for both.
 *
 * MOVEMENT IS MEASURED IN CELLS, so a drag begins when the finger leaves the
 * cell it started in. That is coarse on purpose: everything above this line is
 * a grid, and a threshold in pixels would be a number this library cannot see.
 */
int ktui_gesture_feed(const KtuiEvent *ev, KtuiGesture *g,
		      KtuiEvent *mouse, int *have_mouse);

/* Abandon anything in progress. A backend calls this when it loses the seat. */
void ktui_gesture_reset(void);

/*
 * Long press has no event of its own to arrive on: the finger is still down and
 * nothing is moving. A caller that wants it polls with the current timestamp,
 * from its own idle tick.
 */
int ktui_gesture_tick(unsigned ms, KtuiGesture *g);

int ktui_input_init(int want_mouse);
void ktui_input_shutdown(void);
void ktui_input_suspend(void);
void ktui_input_resume(void);
int ktui_input_next(KtuiEvent *ev, int timeout_ms);
int ktui_input_mouse_visible(int *x, int *y);

/* ────────────────────────────────────────────────────────────────────────
 * Immediate-mode UI
 *
 * Controls register a hit rect as they draw; the click that arrives on the
 * NEXT frame is matched against that list. Keeps every control a single
 * call with no retained tree to keep in sync with a resize.
 *
 * The frame state is private and reached only through the accessors below.
 * An application able to set `.consumed` by hand is one that could not be
 * moved to a new version of this file.
 * ──────────────────────────────────────────────────────────────────────── */

/* Hit ids for mouse-only chrome — a sidebar, a tab bar, a title button.
 * Far above anything ktui_id() hands out, so clicking one is recognised
 * without it joining the Tab ring or dragging the page scroll after it. */
#define KTUI_ID_CHROME 10000

void ktui_frame_begin(KtuiEvent *ev);
void ktui_frame_end(void);
int ktui_id(void);		/* claim the next focus id                 */
int ktui_id_base(void);		/* the next id; restarts outside a frame   */
void ktui_hit(KRect r, int id);
void ktui_hit_chrome(KRect r, int id);	/* id is caller-local, 0..N        */
int ktui_chrome_clicked(int id);
int ktui_focused(int id);
int ktui_activated(int id, KRect r);	/* Enter on focus, or a click      */

/* ────────────────────────────────────────────────────────────────────────
 * What a widget is, said out loud
 *
 * A WIDGET ALREADY KNOWS WHICH ITEM HAS FOCUS. It computes that every frame
 * from the same id the hit test uses, so a reader that worked it out again
 * from a grid of cells would be guessing at what the surface has in hand — and
 * it guesses wrong first on the controls that matter most: which cell of a
 * table, which tab of a strip, which item of how many in a menu.
 *
 * SO THE WIDGET SAYS IT, AND IT SAYS IT HERE. A record composed by a surface
 * would be composed once per surface; one set in this library is set once and
 * every surface gets it.
 *
 * EVERY WIDGET FILLS IT AND NOTHING DRAWN READS IT BACK. There is no screen
 * reader on this image and no route to one, so the queue is kept for what it
 * is: the material such a reader needs, stated by the only thing that knows
 * it — the widget, in the frame it drew.
 *
 * THE QUEUE IS PER FRAME AND FIXED. Nothing on the draw path allocates — a
 * widget that allocated to say its own name would drop frames on the link this
 * desktop is sold on — and it is cleared at the start of every frame, so a
 * widget that says nothing announces nothing. SILENCE IS THE FAILURE MODE,
 * NEVER A STALE NAME: a reader told the wrong control is worse off than one
 * told nothing, and last frame's record is the wrong control by default.
 * A frame with more to say than the queue holds drops the rest.
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_A11Y_NONE = 0,
	KT_A11Y_BUTTON,
	KT_A11Y_CHECK,
	KT_A11Y_RADIO,
	KT_A11Y_INPUT,
	KT_A11Y_LIST,
	KT_A11Y_TABLE,
	KT_A11Y_TAB,
	KT_A11Y_CHOICE,
	KT_A11Y_TEXT,
	KT_A11Y_SLIDER
};

/*
 * `index` and `count` are one-based and are 0 when the control is not one of a
 * set — so "3 of 9" is a fact the widget states rather than a count a reader
 * has to make from what it can see.
 */
typedef struct {
	int role;
	char label[64];
	char value[64];
	int index, count;
} KtuiA11y;

#define KTUI_A11Y_MAX 16

void ktui_announce(int role, const char *label, const char *value, int index,
		   int count);
int ktui_announce_count(void);
const KtuiA11y *ktui_announce_at(int i);
int ktui_key(int k);		/* consume a key press this frame          */
void ktui_focus_next(int dir);
void ktui_focus_set(int id);

/* Frame state, read-only: every field is reached through a call. */
const KtuiEvent *ktui_event(void);
int ktui_consumed(void);
void ktui_consume(void);
int ktui_focus_get(void);
int ktui_clicked(void);
int ktui_drag(void);	/* id holding the press capture, -1 if none */		/* id clicked this frame, -1 if none       */
int ktui_mouse_x(void);
int ktui_mouse_y(void);
/* Wheel that no control claimed, if the pointer is inside r. Takes it. */
int ktui_wheel_take(KRect r);
/* Where the focused control landed; 0 if it was not drawn this frame. */
int ktui_focus_rect(KRect *out);

typedef struct {
	int sel;
	int off;
} KtuiList;

/* Row painter gets an already-cleared line; `sel` marks the current row and
 * `focus` whether the list itself owns the keyboard. */
typedef void (*KtuiListRow)(int idx, int x, int y, int w, int sel, int focus,
			    void *user);

int ktui_list(KRect r, KtuiList *st, int count, KtuiListRow row, void *user,
	      int id);

/* ── HOW A SELECTED ROW IS DRAWN, EVERYWHERE ─────────────────────────
 *
 *     Files                FM  ■        a row
 *   ► System Monitor       MO  ■        the caret, pane not focused
 *   ►▓Git▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓GI▓▓■▓        the caret, pane focused
 *
 * ONE FUNCTION, AND NOTHING WORKS OUT A SELECTION COLOUR ANYWHERE ELSE.
 * Twenty-five surfaces each wrote `bg = on ? KT_ACCENT : KT_SURFACE`, which
 * fills the whole row with the accent and puts the background colour on the
 * label — a lit plate that follows the pointer across the screen. It is also
 * against the rule this desktop already had: `KT_DIM` is what owns SELECTION
 * BACKGROUNDS, and the accent is an accent.
 *
 * THREE STATES, NOT TWO, AND THEY SAY THREE DIFFERENT THINGS:
 *
 *   - a row                    KT_TEXT on the page, no fill
 *   - the caret, pane cold     a marker in KT_MID, still no fill
 *   - the caret, pane focused  the row filled KT_DIM, marker in KT_ACCENT
 *
 * Measured against the palette, `KT_TEXT` on `KT_DIM` is 8.30:1 in the worst
 * scheme and 10.22:1 in the best, so the label clears the 7:1 floor in every
 * accent; the marker clears 4.5:1 in every accent. The fill is quiet because
 * it is a fill, and the accent is spent on one cell that says where the caret
 * is.
 *
 * THE MUTED COLOUR IS NOT LEGIBLE ON THE FILL — 2.18:1 to 3.43:1 measured
 * against the live palette, below any reading floor. A row with a secondary
 * column in it (a tag, a two-letter code, a units suffix) must lift that
 * column when the row is selected; leaving it muted makes the right-hand half
 * of the row disappear exactly when somebody is looking at it. ktui_sel_dim()
 * is that question asked in one place.
 *
 * `page` is the background the surface is drawn on — KT_BG or KT_SURFACE, and
 * half this desktop's surfaces use the second. It is a parameter rather than
 * an assumption because a control that guessed would paint an opaque band
 * across a translucent window.
 */
void ktui_sel_slots(int selected, int pane_focused, int page, int *fg,
		    int *bg);

/* The colour a SECONDARY column takes on a row in that state — muted on a
 * page, KT_TEXT on the fill, because muted on the fill cannot be read. */
int ktui_sel_dim(int selected, int pane_focused);

/*
 * Fill the row and draw the caret marker, then return the slots its text takes.
 * `r` is the whole row including the marker column; text starts at r.x + 2.
 *
 * ANYTHING THAT OVERDRAWS PART OF A ROW OWNS ALL OF IT, so this clears the
 * full width before it marks: a marker drawn onto a row somebody else filled
 * leaves the old fill either side of it.
 */
void ktui_sel_row(KRect r, int selected, int pane_focused, int page_bg,
		  int *fg, int *bg);

int ktui_button(KRect r, const char *label, int enabled, int primary);
int ktui_check(int x, int y, int w, const char *label, int *val);
int ktui_radio(int x, int y, int w, const char *label, int *val, int on);
/* ── A VALUE ON A TRACK ──────────────────────────────────────────────
 *
 *     ◀ ███████░░░░░░  55 ▶
 *
 * DRAW / KEY / HIT, AND ONE FRAME CONTROL ON TOP OF THEM — the shape
 * ktui_dropdown_* has, and for its reason: a surface that runs an event loop
 * of its own must be able to use the same control as one written inside
 * ktui_frame_begin(), or it writes the control a second time.
 *
 * Below KT_SLIDER_MIN_W the end caps are dropped, and below ten cells the
 * number goes too: a slider with no track cannot be pointed at, and one with
 * no number can still be read off the fill.
 *
 * `ktui_slider_hit` takes `press` because the caps answer a press and never a
 * drag: a drag that crossed one would step the value on top of the position it
 * is already setting, and the thumb would fight the pointer.
 */
#define KT_SLIDER_MIN_W 14

/* `bg` is the row's own background, as ktui_progress_ex takes one: a slider
 * sits in a rectangle somebody else filled, and ink picked without asking is
 * the colour that rectangle is already painted in. */
void ktui_slider_draw(KRect r, int val, int min, int max, int focus, int bg);
int ktui_slider_key(int *val, int min, int max, int step, int k);
int ktui_slider_hit(KRect r, int *val, int min, int max, int step, int mx,
		    int my, int press);
/* Focus, keys, click, drag and wheel in one call. 1 when the value moved. */
int ktui_slider(KRect r, int *val, int min, int max, int step,
		const char *label);

/* ── A LIST OF ROWS, POINTED AT ───────────────────────────────────────
 *
 * FOR A SURFACE THAT RUNS ITS OWN EVENT LOOP AND DRAWS ITS OWN ROWS — a glyph
 * grid, a colour swatch, a file size. What such a surface is missing is not a
 * widget to draw but an answer to "which row is that, and what did they mean
 * by it", and eleven of them answered it by dropping every pointer event.
 *
 * ONE RULE, AND IT IS `kdos-pick`'S: a press MOVES the caret, a press on the
 * row the caret is already on PICKS, the wheel walks, and the right button is
 * Back. One hand learns one thing.
 *
 * `r` is the rectangle the ROWS are drawn in — not the window — and `top` is
 * the index its first line carries.
 */
enum {
	KTUI_ROWS_NONE = 0,	/* not this list's, or nothing changed      */
	KTUI_ROWS_MOVED,	/* the caret moved; redraw                  */
	KTUI_ROWS_PICKED,	/* act on *sel                              */
	KTUI_ROWS_CLOSE		/* the right button: go back               */
};

int ktui_rows_hit(KRect r, int top, int count, int mx, int my);
int ktui_rows_event(KRect r, int *sel, int *top, int count,
		    const KtuiEvent *ev);
/* Scroll `top` so `sel` is on screen, which a keyboard caret needs too. */
void ktui_rows_follow(KRect r, int sel, int *top, int count);

int ktui_input(KRect r, char *buf, size_t cap, int secret,
	       const char *placeholder);
/* Queue pasted text; the focused ktui_input inserts it at the caret on its
 * next pass. Control characters are stripped and newlines become spaces, so a
 * multi-line paste cannot fake an Enter. libkwl calls this when an async
 * clipboard receive completes; the tty backend has no paste channel and
 * simply never calls it. */
/*
 * WHERE THE CARET IS, said once by every surface that has one.
 *
 * On a terminal it places that terminal's own cursor, or hides it with a
 * negative x — a screen this library paints itself draws its caret as a cell
 * like everything else. It is what lets a `--tty` view show a person their
 * caret in the cursor their own terminal draws, and what a screen reader
 * following a terminal reads to know where the focus is.
 *
 * A BACKEND WITH A `caret` ENTRY TAKES IT INSTEAD, and no escape is written:
 * a surface drawing through a display server is not on a terminal, and the
 * position it knows is in its own cells, which only the server can place.
 */
void ktui_term_caret(int x, int y);

void ktui_paste_push(const char *utf8, size_t len);

/* Take the pending paste instead, for a consumer with no text field to insert
 * into — a terminal, whose caret is a child on a pty. Returns the length and
 * clears the queue; the text stays valid until the next push. */
size_t ktui_paste_take(const char **out);

/* A drop that landed on this surface. KT_EVT_DROP carries WHERE in mx/my and
 * the payload is taken separately, because a drop is a position and a payload
 * and an event has room for one of them. The text is held until taken and
 * replaced by the next drop; taking it twice returns NULL the second time, so
 * two surfaces in one process cannot both act on one drop.
 *
 * text/uri-list arrives as it came: CRLF-separated URIs, comment lines and all.
 * Unpicking that is the caller's, because what a URI means differs per
 * surface. */
void ktui_drop_push(const char *utf8, size_t len);
const char *ktui_drop_take(size_t *len);
/* Bar styles. SOLID is the original: whole cells only. TIP adds one
 * fractional cell from the horizontal ramp, so a 40-column bar carries 320
 * positions on a rich terminal instead of 40 — a solid bar quantises to 2.5%
 * and visibly lies during a long step. SEGMENTED draws one gapped segment per
 * unit, for a small discrete count like "12 of 14 steps". */
enum { KT_BAR_SOLID, KT_BAR_TIP, KT_BAR_SEGMENTED, KT_BAR_STYLE_MASK = 0xf };
/* OR into the style: a highlight sweeps the filled region so a bar that is
 * making slow progress still reads as ALIVE. Time-based, never frame-based —
 * the build screen redraws at whatever rate its child is producing output,
 * and a per-frame step would make the sweep race or crawl accordingly.
 * Deliberately a flag rather than a style: ktui_progress() must keep drawing
 * exactly what it always has for kinstall, so animation is opt-in. */
#define KT_BAR_PULSE (1 << 4)

void ktui_progress(KRect r, double frac, const char *label);
void ktui_progress_ex(KRect r, double frac, const char *label, int style,
		      int bg);
/* Whole cells filled; *tip receives the leftover fraction of the next cell,
 * 0 when it lands on a boundary. Exposed because it is the one piece of bar
 * arithmetic worth asserting. */
int ktui_bar_fill(int w, double frac, double *tip);
void ktui_scrollbar(KRect r, int total, int shown, int off);

/* ────────────────────────────────────────────────────────────────────────
 * Text helpers — the things every page in every KDOS TUI redraws
 * ──────────────────────────────────────────────────────────────────────── */

void ktui_section(int x, int y, int w, const char *title);
void ktui_kv(int x, int y, int w, const char *k, const char *v, int fg);
void ktui_note(int x, int y, int w, const char *s);
int ktui_para(int x, int y, int w, const char *s, int fg);	/* wraps  */
void ktui_pw_meter(int x, int y, int w, const char *p);
int ktui_pw_score(const char *p);

/* Terminal too small for the application to draw at all. */
void ktui_toosmall(const char *title, int min_w, int min_h);

/* ────────────────────────────────────────────────────────────────────────
 * The four views — a page strip, a column table, a choice, a text block
 *
 * DRAW AND KEY ARE SEPARATE CALLS, unlike ktui_list() and like the menu.
 * The surfaces these serve run their own event loop and hold their own
 * selection; an immediate-mode widget reading the frame's focus would need
 * every one of them rebuilt around ktui_frame_begin() first.
 *
 * A HIT TEST TAKES THE RECT THE DRAW TOOK, so it measures what is on the
 * screen rather than what the widget remembered from an earlier size.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	const char *name;
	const char *abbr;	/* drawn where `name` will not fit; NULL     */
				/* takes the first three characters          */
} KtuiTab;

/* The cells one tab occupies: the widest name plus its padding across a row,
 * or the whole width down a column. Exposed because a caller sizing the strip
 * and the caller drawing it must agree. */
int ktui_tab_span(const KtuiTab *t, int n, int vertical, int w);

void ktui_tabs_draw(KRect r, const KtuiTab *t, int n, int sel, int hover,
		    int vertical);
/* 1 when `*sel` moved. */
int ktui_tabs_key(int *sel, int n, int vertical, int k);
/* The tab under the pointer, or -1. */
int ktui_tabs_hit(KRect r, const KtuiTab *t, int n, int vertical, int mx,
		  int my);

#define KT_TABLE_COLS 8

typedef struct {
	const char *title;	/* NULL in every column: no header row       */
	int width;		/* cells; <= 0 asks for the remainder        */
} KtuiCol;

/* Paint one cell of one row. The table has filled the row and chosen the
 * colours; `col` is -1 for a span row, where `w` is the whole table. */
typedef void (*KtuiTableCell)(int idx, int col, int x, int y, int w, int fg,
			      int bg, void *user);
/* What kind of row this is: 0 a record, KT_TABLE_HEAD a heading drawn across
 * the table and still selectable, KT_TABLE_SKIP a heading the selection steps
 * over. Both readings are in the tree — a network device heading is the row
 * Enter rescans from, and a device-section caption is furniture — so the
 * callback says which rather than the widget deciding for both. */
enum { KT_TABLE_HEAD = 1, KT_TABLE_SKIP = 2 };
typedef int (*KtuiTableSpan)(int idx, void *user);

typedef struct {
	int sel;
	int top;
} KtuiTable;

/* Column origins and widths for a table `w` cells wide; returns the cells
 * used. The FIRST column asking for the remainder gets it and the rest keep
 * what they asked for — two elastic columns would need a distribution rule,
 * and every table here has exactly one field that should absorb a wider
 * window. */
int ktui_table_layout(const KtuiCol *col, int ncol, int w, int *x, int *cw);
/* `hover` is the row under the pointer or -1; it is an ARGUMENT rather than a
 * field of KtuiTable because a zeroed struct would then light row 0 on a
 * surface that never tracks the pointer at all. */
void ktui_table_draw(KRect r, KtuiTable *st, int count, const KtuiCol *col,
		     int ncol, KtuiTableCell cell, KtuiTableSpan span,
		     void *user, int hover);
void ktui_table_clamp(KtuiTable *st, int count, int rows);
int ktui_table_key(KtuiTable *st, int count, int rows, int k,
		   KtuiTableSpan span, void *user);
/* Move the selection to a clicked row; 0 when that row refuses it. */
int ktui_table_pick(KtuiTable *st, int count, int idx, KtuiTableSpan span,
		    void *user);
/* The row under the pointer, or -1; a click on the header is not a row. */
int ktui_table_hit(KRect r, const KtuiTable *st, int count, int ncol,
		   const KtuiCol *col, int mx, int my);

/*
 * ONE POINTER RULE FOR EVERY TABLE, the same one the lists keep: the wheel
 * scrolls, a press MOVES the caret, a press on the row it is already on PICKS,
 * and the right button is Back. `rows` is the visible row count the key path
 * is given, and `span`/`user` are the caller's row classifier.
 *
 * Ten surfaces drew a table and answered a press but not the WHEEL, which is
 * the first thing a hand reaches for on a list longer than its window.
 */
enum {
	KTUI_TABLE_NONE = 0,
	KTUI_TABLE_MOVED,
	KTUI_TABLE_PICKED,
	KTUI_TABLE_CLOSE
};

int ktui_table_event(KRect r, KtuiTable *st, int count, int rows, int ncol,
		     const KtuiCol *col, const KtuiEvent *ev,
		     KtuiTableSpan span, void *user);

typedef struct {
	int sel;
	int open;
	int hi;			/* the highlighted row while open            */
} KtuiDrop;

void ktui_dropdown_draw(KRect r, const KtuiDrop *d, const char *const *opt,
			int n, int focus);
/* The open list is a SECOND call because it is drawn over whatever is under
 * it: a surface draws every closed control, then this, last. */
void ktui_dropdown_draw_open(KRect r, const KtuiDrop *d,
			     const char *const *opt, int n);
/* 1 when the choice changed. */
int ktui_dropdown_key(KtuiDrop *d, int n, int k);
int ktui_dropdown_hit(KRect r, KtuiDrop *d, int n, int mx, int my);

typedef struct {
	int cy, cx;		/* the caret, in lines and columns           */
	int top;		/* the first line drawn                      */
} KtuiTextArea;

/* FIXED-WIDTH LINES, NOT A ROPE: `text` is `maxlines` strings of `stride`
 * bytes each, terminator included, which is what a caller writes to a file
 * line by line. A rope would make the widget the owner of the storage and the
 * caller a serialiser of it. */
void ktui_textarea_draw(KRect r, KtuiTextArea *ta, const char *text,
			int nlines, size_t stride, int fg, int bg);
/* 1 when the text changed. */
int ktui_textarea_key(KtuiTextArea *ta, char *text, int *nlines, int maxlines,
		      size_t stride, int k);

/* ────────────────────────────────────────────────────────────────────────
 * The contract every surface answers
 *
 * A hint row that names the keys that do something RIGHT NOW, and the keys
 * themselves. A surface holds one KtuiKeys, calls ktui_keys() first in the
 * dispatch it already has and ktui_hint_row() last in the draw it already has.
 * ktui_keys() returns PASS for everything it does not own, so a surface that
 * has not adopted it behaves exactly as it did.
 *
 * THE ROW IS PUSHED DURING THE DRAW by whatever holds the focus, which is why
 * it is a toolkit function and not a string a surface writes: a fixed string
 * cannot follow the focus, and a row naming keys the focused control does not
 * answer is worse than no row at all.
 * ──────────────────────────────────────────────────────────────────────── */

/* ────────────────────────────────────────────────────────────────────────
 * The menu — a bar with panes, or one pane popped at a point
 *
 * One widget for both, because they are the same list drawn in two places.
 * `F10` opens a bar, `Alt+letter` opens a pane by its mark, `Shift+F10` pops
 * the context pane where the surface says its focus is, and `Esc` closes what
 * is down before it touches the Esc ladder.
 * ──────────────────────────────────────────────────────────────────────── */

enum { KTUI_MENU_PANE_MAX = 8 };
enum { KTUI_MENU_NONE = 0, KTUI_MENU_TAKEN, KTUI_MENU_PICKED };

typedef struct {
	/* `&` marks the accelerator and `&&` is a literal one. NULL or an
	 * empty label is a RULE: drawn, never selected — a separator that can
	 * hold the caret is a menu with a row that does nothing. */
	const char *label;
	int id;			/* handed back when picked; a rule has none */
	const char *accel;	/* "Ctrl+N", drawn right-aligned, or NULL   */
	int enabled;
} KtuiMenuItem;

/* Is item i on the menu right now? NULL means every one is. Asked by the draw
 * AND by the hit test from the same walk: two copies of a visibility rule
 * disagree eventually, and a click then runs the row above the one under the
 * pointer. */
typedef int (*KtuiMenuShow)(int i, void *user);

typedef struct {
	const char *title;	/* "&File" — only a bar draws it            */
	const KtuiMenuItem *item;
	int n;
} KtuiMenuPane;

typedef struct {
	/* Declared by the surface, once. */
	const KtuiMenuPane *pane;
	int npane;
	KtuiMenuShow show;
	void *user;
	/* A BAR IS OPT-IN, not a row number that defaults to zero: a menu
	 * declared for a popup alone would otherwise draw a bar across the top
	 * of a surface that never asked for one. */
	int has_bar;
	int bar_row;
	int bar_bg;
	/* Owned here. `sel` indexes item[], NEVER a drawn row — a selection
	 * counted in drawn rows moves to a different item the moment `show`
	 * hides one. */
	int open;		/* 0 closed, else 1 + the pane that is down */
	int sel;
	int x, y, w, rows;	/* the popup AS DRAWN, clamped on screen    */
	int bar_x[KTUI_MENU_PANE_MAX], bar_w[KTUI_MENU_PANE_MAX];
} KtuiMenu;

void ktui_menu_open(KtuiMenu *m, int pane, int x, int y);
void ktui_menu_close(KtuiMenu *m);
int ktui_menu_active(const KtuiMenu *m);
/* Draws the bar (where there is one) and the open pane, and pushes its own
 * hints — so no surface writes them. */
void ktui_menu_draw(KtuiMenu *m);
/* One event, keys and pointer alike. PICKED writes the item's id through
 * `id`. A surface that calls ktui_keys() need not call this: ktui_keys()
 * routes into it. */
int ktui_menu_event(KtuiMenu *m, const KtuiEvent *ev, int *id);
int ktui_menu_alt(KtuiMenu *m, const KtuiEvent *ev);
/* The accelerator letter of a label, lowercased, or 0 — read from the same
 * string the drawing reads, so a title cannot advertise a letter that opens
 * nothing. */
int ktui_menu_accel_of(const char *s);
/* A label with its accelerator marked: underlined where the tier has
 * underline, bracketed where it does not. Returns the cells written. */
int ktui_menu_label(int x, int y, int w, const char *s, int fg, int bg);

/* Is this Esc layer up RIGHT NOW? Asked at the instant the key arrives and
 * never cached: a dialog that dismissed itself from a click would otherwise
 * leave a raised bit that swallows the next Esc. */
typedef int (*KtuiLayerUp)(void *user);
typedef void (*KtuiLayerClose)(void *user);	/* take down exactly one   */

enum { KTUI_LAYER_MAX = 6 };

typedef struct {
	const char *verb;	/* what Esc reads as here: "Back", "Cancel" */
	KtuiLayerUp up;
	KtuiLayerClose close;
	void *user;
} KtuiLayer;

typedef struct {
	/* Declared by the surface. `doc` NULL means F1 is neither advertised
	 * nor answered — a key that opens an index saying "no such document"
	 * teaches that help is broken. */
	const char *doc;
	void (*help)(const char *doc, void *user);
	void *user;
	KtuiLayer layer[KTUI_LAYER_MAX];
	int nlayer;
	/* The surface's menu, or NULL. Routed into FIRST, so a pane that is
	 * down owns the arrows and Esc before the ladder sees them. */
	KtuiMenu *menu;
	/* Where Shift+F10 pops the context pane. ONLY THE SURFACE KNOWS where
	 * its focus is drawn; a menu that opened at the origin would name a
	 * row nobody is looking at. Returns 0 to refuse — nothing is focused. */
	int (*ctx_at)(int *x, int *y, void *user);
	int ctx_pane;
	/* The item KTUI_KEY_MENU is reporting. Read only after that return. */
	int menu_id;
} KtuiKeys;

enum { KTUI_KEY_PASS = 0, KTUI_KEY_TAKEN, KTUI_KEY_CLOSE, KTUI_KEY_MENU };

/* Registered ONCE at surface start, INNERMOST LAST: the walk runs from the
 * end, so registration order is the order Esc unwinds. */
void ktui_keys_layer(KtuiKeys *k, const char *verb, KtuiLayerUp up,
		     KtuiLayerClose close, void *user);

/* Called FIRST in the surface's dispatch, above its own switch. Classifies;
 * it neither polls nor draws. Takes any event, not only a key: a surface with
 * a menu would otherwise need a second call site in its pointer path, and the
 * two would drift. KTUI_KEY_MENU means an item was picked and `k->menu_id`
 * names it. */
int ktui_keys(KtuiKeys *k, const KtuiEvent *ev);

/* Pushed during the draw. Both strings are COPIED, so no lifetime rule
 * reaches the caller. */
void ktui_hint(const char *key, const char *verb);
void ktui_hint_if(int on, const char *key, const char *verb);

/* Draws the pushed hints into `r` and CLEARS THE POOL as its first act — a
 * pool emptied at flush time would carry one surface's hints into the next
 * dump in the same process. Returns 1 if a row was drawn, 0 on a window
 * shorter than eight rows or too narrow for one whole hint. */
int ktui_hint_row(const KtuiKeys *k, KRect r, int bg);

/* The verb of the topmost OPEN layer, or "Close". Read when building the row,
 * so it cannot say Close on a screen where Esc goes back. */
const char *ktui_esc_verb(const KtuiKeys *k);

/* Drop out of the TUI, run a program on the real terminal, come back. */
int ktui_run_console(char *const argv[]);

/* ────────────────────────────────────────────────────────────────────────
 * Modals
 *
 * A modal owns the focus ring completely and hands it back on close, so
 * dismissing a dialog does not silently move the caret on the page beneath.
 * ──────────────────────────────────────────────────────────────────────── */

void ktui_modal_alert(const char *title, const char *msg);
void ktui_modal_confirm(const char *title, const char *msg, const char *yes,
			const char *no, void (*on_yes)(void));
int ktui_modal_active(void);
void ktui_modal_draw(void);
int ktui_modal_event(KtuiEvent *ev);

#endif /* KTUI_H */

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
 * THE NEAREST SLOT TO AN ARBITRARY COLOUR, by squared distance.
 *
 * The one rule for reducing a colour that came from outside the palette — a
 * terminal's SGR, a picture's average — to something this desktop can draw. A
 * table mapping "red means the error slot" would be a second set of colour
 * decisions beside the palette, and it would stop following the accent:
 * `kdos theme amber` has to move every colour with it.
 */
int ktui_theme_nearest(uint32_t rgb);

/* ────────────────────────────────────────────────────────────────────────
 * Terminal
 * ──────────────────────────────────────────────────────────────────────── */

enum {
	KT_CAP_TRUECOLOR = 1 << 0,	/* 24-bit SGR                        */
	KT_CAP_256 = 1 << 1,		/* indexed 256                       */
	KT_CAP_LINUXVT = 1 << 2,	/* real VT: PIO_CMAP palette, no     */
					/* bold, no xterm mouse -> evdev     */
	KT_CAP_UTF8 = 1 << 3,
	KT_CAP_MOUSE = 1 << 4
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
	KT_A_UNDERLINE = 1 << 2
};

typedef struct {
	uint32_t ch;
	uint8_t fg, bg, attr;
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
} KtuiSprite;

/* Register (or refresh) the picture for `key`. `pix` must already be scaled to
 * cw*cell_w x ch*cell_h at the backend's scale — this library does no pixel
 * work at all. Returns a slot, or -1 when the caller must draw its glyph. */
int ktui_sprite_put(uint64_t key, const void *pix, int cw, int ch,
		    uint32_t fallback);
int ktui_sprite_find(uint64_t key);
const KtuiSprite *ktui_sprite_get(int slot);
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
 * `kdos-con`'s ignores it, because a session with several views has one
 * previous frame per view rather than one between them. Read
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
	KT_G_FULL, KT_G_SHADE, KT_G_DOT, KT_G_BULLET, KT_G_SQUARE,
	KT_G_UP, KT_G_DOWN, KT_G_LEFT, KT_G_RIGHT, KT_G_ELLIPSIS, KT_G_DEG,
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

typedef struct {
	const char *name;
	/* `prev` is the last-presented buffer, updated by the backend as it
	 * presents. `force_full` means ignore it and repaint everything. */
	void (*flush)(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int force_full);
	int (*poll_event)(KtuiEvent *ev, int timeout_ms);
	void (*size)(int *w, int *h);
	int (*caps)(void);
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
void ktui_draw_dump(void);
void ktui_draw_resize(void);
void ktui_draw_clear(void);
void ktui_draw_flush(void);
void ktui_draw_invalidate(void);	/* force a full repaint next flush */

void ktui_draw_cell(int x, int y, uint32_t ch, int fg, int bg, int attr);
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
void ktui_draw_shadow(KRect r);
void ktui_draw_cursor(int x, int y);	/* pointer overlay, evdev backend  */
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
	KT_K_F7, KT_K_F8, KT_K_F9, KT_K_F10, KT_K_F11, KT_K_F12
};

/* KT_MOD_SUPER is the desktop's own modifier — the one every window-management
 * chord is on, so that none of them can collide with what a program inside a
 * window wants. A backend that cannot report it leaves it clear, and those
 * chords simply do not fire. */
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
	/*
	 * WHERE IN THE CELL, as an offset from its CENTRE in 1/256ths of a
	 * cell width and height, -128..127. Zero is the centre — which is what
	 * a backend with no pixel geometry leaves behind, and is the right
	 * answer for one, because a cell's corner is a pixel that belongs to
	 * its neighbour.
	 *
	 * Nothing drawn in cells reads these. They exist for the one thing on
	 * this desktop that is not cells: a pixel guest embedded in a window,
	 * whose buttons are smaller than the grid pointing at them.
	 */
	int subx, suby;
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
 * Gestures
 *
 * ONE recogniser, fed by every backend that has touch: libinput under the KMS
 * backend and wl_touch under the Wayland one. Putting the disambiguation in a
 * backend would mean writing it twice and having it disagree twice.
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
 * The frame state used to be a public `Ui` struct that applications wrote to
 * field by field. It is private now — an application that sets `.consumed`
 * by hand is one that cannot be moved to a new version of this file.
 * ──────────────────────────────────────────────────────────────────────── */

/* Hit ids for mouse-only chrome — a sidebar, a tab bar, a title button.
 * Far above anything ktui_id() hands out, so clicking one is recognised
 * without it joining the Tab ring or dragging the page scroll after it. */
#define KTUI_ID_CHROME 10000

void ktui_frame_begin(KtuiEvent *ev);
void ktui_frame_end(void);
int ktui_id(void);		/* claim the next focus id                 */
void ktui_hit(KRect r, int id);
void ktui_hit_chrome(KRect r, int id);	/* id is caller-local, 0..N        */
int ktui_chrome_clicked(int id);
int ktui_focused(int id);
int ktui_activated(int id, KRect r);	/* Enter on focus, or a click      */
int ktui_key(int k);		/* consume a key press this frame          */
void ktui_focus_next(int dir);
void ktui_focus_set(int id);

/* Frame state, read-only where it used to be a struct field. */
const KtuiEvent *ktui_event(void);
int ktui_consumed(void);
void ktui_consume(void);
int ktui_focus_get(void);
int ktui_clicked(void);		/* id clicked this frame, -1 if none       */
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

int ktui_button(KRect r, const char *label, int enabled, int primary);
int ktui_check(int x, int y, int w, const char *label, int *val);
int ktui_radio(int x, int y, int w, const char *label, int *val, int on);
int ktui_input(KRect r, char *buf, size_t cap, int secret,
	       const char *placeholder);
/* Queue pasted text; the focused ktui_input inserts it at the caret on its
 * next pass. Control characters are stripped and newlines become spaces, so a
 * multi-line paste cannot fake an Enter. libkwl calls this when an async
 * clipboard receive completes; the tty backend has no paste channel and
 * simply never calls it. */
/*
 * PUT THE TERMINAL'S OWN CURSOR AT A CELL, or hide it with a negative x.
 *
 * Only a terminal backend has one to place; a screen this library paints
 * itself draws its caret as a cell like everything else. It is what lets a
 * `--tty` view show a person their caret in the cursor their own terminal
 * draws — and what a screen reader following a terminal reads to know where
 * the focus is.
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

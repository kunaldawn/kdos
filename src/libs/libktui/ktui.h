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
	KT_EVT_TICK
};

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

enum { KT_MOD_SHIFT = 1, KT_MOD_ALT = 2, KT_MOD_CTRL = 4 };

enum {
	KT_MB_LEFT = 0, KT_MB_MIDDLE, KT_MB_RIGHT,
	KT_MB_WHEEL_UP, KT_MB_WHEEL_DOWN,
	KT_MB_MOVE
};

enum { KT_MP_RELEASE = 0, KT_MP_PRESS = 1, KT_MP_DRAG = 2 };

typedef struct {
	int type;
	int key;		/* codepoint or KT_K_*                     */
	int mods;
	int mx, my;
	int btn;
	int press;
} KtuiEvent;

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

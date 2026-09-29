/*
 * The shell's front ends, drawn OFFSCREEN with no compositor.
 *
 * `kdos-menu --dump`, `kdos-pick --dump`, `kdos-launcher --dump` and
 * `kdos-cal --dump` render one frame into libktui's cell buffer and print it as
 * text — the same seam `kdos-shell --dump` and `kdosbuild --preview` already
 * have, and it exists for the same reason: every geometry defect this toolkit
 * has shipped (text over a box border, a button on top of a hint, a column
 * drifted out from under its header) was invisible to the compiler and to a
 * test suite with no terminal.
 *
 * That path touches no fcft and no wlroots — libkwl is stubbed out here, so
 * the front ends link against libktui alone and the layouts can be looked at
 * on a host that has neither. It does now need libwayland-client, because
 * shell.c and menu.c reach for wlr-foreign-toplevel and ext-workspace on the
 * INTERACTIVE path and the generated glue is compiled in whether the dump
 * calls it or not.
 *
 * Two things here exist for testing/goldens/:
 *
 *   - Every front end is declared WEAK. New surfaces land one agent at a time
 *     and a harness that named a file which is not on the tree yet would fail
 *     to link; instead the name is simply not dispatchable and selftest.sh
 *     says so out loud.
 *   - KDOS_DUMP_SIZE=WxH overrides the geometry a surface asked for, through a
 *     linker --wrap on ktui_offscreen_init. A surface picks its own dump size
 *     (cal 42 columns, pick 64x22) and a golden wants a common one; forcing it
 *     also checks the thing a fixed size never can — that a draw pass does not
 *     assume the buffer is exactly the size it hoped for.
 *
 * Not shipped. testing/selftest.sh compiles it and nothing else does.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"	/* KCellCanvas, for the canvas stubs below */
#include "kwl.h"
#include "shell.h"

/* libkwl, stubbed. A dump never reaches any of it; the symbols exist only
 * because the same translation units carry the interactive path. */
/*
 * The stub stands in for libkdisp, not for libkwl: the front ends reach a
 * display through the interface now, so that is what has to be absent here.
 * Returning -1 is what it always did — no compositor — and the front ends take
 * their non-Wayland path exactly as before.
 */
/*
 * EXCEPT UNDER A KEY DRIVE. `$KDOS_DUMP_KEYS` is a script of input events and
 * with it set the "display" is a backend that reads the script: the surface
 * takes its LIVE path — its own loop, ktui_keys() first, its handlers, its
 * resize step — and the frame it last presented is printed, as text, when it
 * shuts down. A golden of that frame is what shows a loop still does what it
 * did, which a --dump never runs. See drive_* below.
 */
static int drive_on;
static int drive_done;

static int drive_start(const KDispConfig *cfg);

int kdisp_init(const KDispConfig *cfg, const KDispImpl *const *impls, int n)
{
	(void)impls;
	(void)n;
	return drive_start(cfg);
}

const KDispImpl *kdisp_current(void) { return NULL; }

/* The front ends name this list; nothing in it is reachable from here. */
const KDispImpl *const kdos_disp[] = { NULL };
const int kdos_disp_n = 0;
static void drive_finish(void);
void kdisp_shutdown(void) { drive_finish(); }
int kdisp_should_close(void) { return drive_on ? drive_done : 1; }
int kdisp_cell_w(void) { return 16; }
int kdisp_cell_h(void) { return 32; }
void kdisp_input_cells(const KRect *r, int n) { (void)r; (void)n; }
void *kwl_display(void) { return NULL; }
void *kwl_seat(void) { return NULL; }
void kdisp_overlay_hide(void) {}
int kdisp_overlay_show(int c, int r) { (void)c; (void)r; return 0; }
int kdisp_overlay_resize(int c, int r) { (void)c; (void)r; return -1; }
void kdisp_cursor_set(enum kdisp_cursor c) { (void)c; }
int kdisp_fd(void) { return -1; }
void kdisp_pump(void) {}
int kdisp_scale(void) { return 1; }
/* One kind of pixel: the number it was given. */
int kdisp_px_logical(int px) { return px; }
/* Nothing is drawing a frame round an offscreen grid, so the surface draws its
 * own — which is what makes a golden the picture tty1 shows. */
int kdisp_decorated(void) { return 0; }
int kdisp_px_h(void) { return 0; }
int kdisp_popup_offset(void) { return 0; }
void kdisp_report_error(void) {}
void kdisp_set_backdrop(KDispBackdropFn fn) { (void)fn; }
/* No output, so no scale ever changes and nothing is ever called back. */
void kdisp_on_scale(KDispScaleFn fn) { (void)fn; }
int kdisp_copy(const char *t, size_t n, int p)
{
	(void)t; (void)n; (void)p;
	return -1;
}
/*
 * libkcell, stubbed for the same reason libkwl is: kdos-devices previews a
 * camera frame through kcell_ascii_image(), which would drag fcft and pixman
 * into a harness whose whole point is that a dump touches neither. -1 is the
 * library's own "there is no font", which every caller already handles — so
 * the surface draws exactly what it draws on a machine with no font, which is
 * the layout the goldens are of.
 */
int kcell_ascii_image(const uint32_t *argb, int w, int h, int stride_px,
		      int cell_w, int cell_h, uint32_t *out_cp,
		      uint32_t *out_tint, int *out_cols, int *out_rows)
{
	(void)argb; (void)w; (void)h; (void)stride_px;
	(void)cell_w; (void)cell_h; (void)out_cp; (void)out_tint;
	(void)out_cols; (void)out_rows;
	return -1;
}

/*
 * THE FIVE kdos-desk AND kdos-notifyd REACH FOR, and every one of them is a
 * verb rather than a reading: a dump presses nothing, drags nothing and asks
 * the session for nothing, so each does what it does on a machine with no
 * compositor — nothing, and says so in its return value.
 *
 * A MISSING ONE COSTS EVERY FRONT-END GOLDEN AND NOT ITS OWN. There is no
 * compiler check that this stub set is complete; what there is, is a link that
 * fails and a harness that then skips the whole family. See the note at the
 * top of this file.
 */
void kdisp_session_action(const char *verb) { (void)verb; }
int kdisp_drag_start(const char *mime, const char *data, size_t len)
{ (void)mime; (void)data; (void)len; return -1; }
/* 0 is "no window has the keyboard", which is the truth with no server. */
int kdisp_focused(void) { return 0; }
void kicon_forget_paths(void) {}
/* The plate under a toast. kch_px_* is the pixel layer and this harness has
 * none — see the kch_tile_* set above, which is stubbed for the same reason. */
void kch_px_bare(int body_slot) { (void)body_slot; }

int kdisp_lock_engaged(void) { return 0; }
int kdisp_lock_finished(void) { return 1; }
void kdisp_unlock(void) {}

/*
 * libkicon, stubbed to "there are no icons".
 *
 * That is not a convenience — it is the POINT. Every surface here must draw
 * correctly on a machine with no artwork, because a tty has none and
 * `icons = off` is a supported setting, so a golden frame is the CHARACTER
 * grid and a layout that only lines up once the pictures load is a layout that
 * is broken. Stubbing also keeps pixman and libpng out of this harness, which
 * is what lets it run on a host that has neither.
 */
int kicon_init(int cw, int ch, int s) { (void)cw; (void)ch; (void)s; return -1; }
void kicon_finish(void) {}
void kicon_recell(int cw, int ch, int s) { (void)cw; (void)ch; (void)s; }
void kicon_set_enabled(int on) { (void)on; }
int kicon_enabled(void) { return 0; }
int kicon_slot(const char *n, int cw, int ch)
{
	(void)n; (void)cw; (void)ch;
	return -1;
}
int kicon_slot_for_path(const char *p, int d, int cw, int ch)
{
	(void)p; (void)d; (void)cw; (void)ch;
	return -1;
}
/* A tray item's own `icon-data`, which is a PNG on the bus rather than a
 * theme name — and this harness decodes nothing, for the reason above. */
int kicon_slot_png(const void *png, size_t len, int cw, int ch)
{
	(void)png; (void)len; (void)cw; (void)ch;
	return -1;
}
const char *kicon_app_icon(const char *id) { (void)id; return NULL; }
void kicon_retint(void) {}
int kicon_cached(void) { return 0; }

/*
 * The PIXEL LAYER, stubbed to nothing — and that is the point of this harness
 * rather than a gap in it.
 *
 * A golden frame is the CHARACTER grid. Every surface has to draw correctly
 * with the plates, the gradients and the hairlines absent, exactly as it has
 * to draw correctly with kicon_slot() answering -1: a layout that only lines
 * up once the pixels arrive is a layout that is broken, and on tty1 they never
 * arrive at all. Recording an op here would prove nothing about the grid and
 * would drag pixman onto a link line that deliberately has neither pixman nor
 * fcft on it.
 *
 * kch_tone.c IS linked for real — it needs nothing but libkcolor, and a stub
 * of the tone table would be a second answer to what a plate's colour is.
 */
void kch_px_reset(void) {}
void kch_px_rect(int x, int y, int w, int h, uint32_t c, uint8_t a)
{ (void)x; (void)y; (void)w; (void)h; (void)c; (void)a; }
void kch_px_round(int x, int y, int w, int h, int r, uint32_t c, uint8_t a)
{ (void)x; (void)y; (void)w; (void)h; (void)r; (void)c; (void)a; }
void kch_px_grad(int x, int y, int w, int h, int r, uint32_t t, uint32_t b,
		 uint8_t a)
{ (void)x; (void)y; (void)w; (void)h; (void)r; (void)t; (void)b; (void)a; }
void kch_px_plate(int cx, int cy, int cw, int ch, KchTone t, int inset)
{ (void)cx; (void)cy; (void)cw; (void)ch; (void)t; (void)inset; }
void kch_px_row(int cx, int cy, int cw, KchTone t)
{ (void)cx; (void)cy; (void)cw; (void)t; }
void kch_px_row_anim(int key, int item, int cx, int cy, int cw, KchTone t)
{ (void)key; (void)item; (void)cx; (void)cy; (void)cw; (void)t; }
void kch_px_vrule(int cx, int y0, int rows)
{ (void)cx; (void)y0; (void)rows; }
/* Display text is an op like the plates, so it is refused here and
 * kch_display_text() — kch_chrome.c, linked for real — draws its cell form:
 * the string on the rectangle's first row, which is what the golden holds. */
int kch_px_text(int cx, int cy, int cw, int ch, const char *s, int fg, int bg,
		int align)
{
	(void)cx; (void)cy; (void)cw; (void)ch; (void)s; (void)fg; (void)bg;
	(void)align;
	return 0;
}
int kch_display_live(void) { return 0; }
int kch_display_cols(const char *s, int rows) { (void)s; (void)rows; return 0; }
void kch_px_flat(int body_slot) { (void)body_slot; }
/* A list's glide is pixels between two whole-row frames; a dump has neither. */
void kch_list_view(int x, int y, int w, int h, int top)
{ (void)x; (void)y; (void)w; (void)h; (void)top; }
void kch_px_replay(pixman_image_t *dst, int scale) { (void)dst; (void)scale; }
void kch_px_body(pixman_image_t *dst, int w, int h, int s, uint8_t a, int e)
{ (void)dst; (void)w; (void)h; (void)s; (void)a; (void)e; }

/* libkcell's own painters, for the same reason. */
void kcell_set_slot_alpha(int slot, uint8_t a) { (void)slot; (void)a; }
/* The popup body is the same stub as the rest of the pixel layer: a
 * golden is the CHARACTER grid, and a layout that only lines up once
 * the paint arrives is a layout that is broken. */
void kch_px_popup(int body_slot) { (void)body_slot; }
/* The taskbar's own body goes in the same way and is dormant here for the
 * same reason. */
void kch_px_custom(const KchPxBackdrop *bd) { (void)bd; }
/* THE PLATE IS DORMANT IN A DUMP, and every surface that asks is told so: a
 * plate is pixels a backdrop replays and there is no surface here to install
 * one on. A row highlighted by the plate falls back to the cell highlight,
 * which is what the same surface draws on a tty. */
int kch_px_live(void) { return 0; }
/* No surface, so nothing is waiting to be told its pixels moved. */
void kwl_pixels_dirty(void) {}
/* No backdrop in a dump, so the page is the slot it always was. */
int kch_body_slot(void) { return KT_BG; }

/* ── the taskbar's own dependencies ─────────────────────────────────────
 *
 * `kdos-shell` is the most geometry-dense surface in this tree and was the
 * only one with no committed golden, because panel.c reaches for four things
 * the harness does not link: the tray and MPRIS over sd-bus, the recording
 * indicator over PipeWire, the mixer over ALSA, and the Unity badge over
 * sd-bus again. None of them is geometry. Stubbed to the answer each gives on
 * a machine that has none of it — no tray items, nothing recording, no mixer —
 * which is also the picture a dump should draw: the bar's own chrome, with
 * every readout at its "not available" state.
 *
 * A tile is refused for the same reason libkicon is: a golden is the CHARACTER
 * grid, and a layout that only lines up once the pictures rasterise is a
 * layout that is broken.
 */
int kdisp_edge_bottom(void) { return 0; }
void kdisp_layer_autohide(bool hidden) { (void)hidden; }

/*
 * THE WINDOW LIST IS EMPTY HERE, and answering `supported` with 0 is the point:
 * a dump renders one frame with no compositor, so a surface that draws window
 * rows must draw the state it has on a machine with no window manager. A stub
 * that invented two windows would make the goldens assert a fiction.
 */
int kdisp_win_supported(void) { return 0; }
int kdisp_win_count(void) { return 0; }
int kdisp_win_at(int i, KDispWin *out) { (void)i; (void)out; return 0; }
void kdisp_win_activate(unsigned id) { (void)id; }
void kdisp_win_close(unsigned id) { (void)id; }
void kdisp_win_minimise(unsigned id, int on) { (void)id; (void)on; }
void kdisp_win_maximise(unsigned id, int on) { (void)id; (void)on; }
void kdisp_win_fullscreen(unsigned id, int on) { (void)id; (void)on; }

/*
 * AND THE SCREEN'S FONT LIST IS EMPTY, for the reason the window list is: a
 * dump has no display, and a display is the only thing that knows what faces
 * it can render. `$KDOS_FONT_LIST` is what puts rows in front of a golden —
 * one name per line, read here rather than gathered, because a real
 * enumeration is the HOST'S fonts and would differ on every machine.
 */
int kdisp_font_count(void)
{
	const char *f = getenv("KDOS_FONT_LIST");
	FILE *fp = f && *f ? fopen(f, "r") : NULL;
	char line[256];
	int n = 0;

	if (!fp)
		return 0;
	while (n < 64 && fgets(line, sizeof(line), fp))
		if (line[0] != '\n')
			n++;
	fclose(fp);
	return n;
}

int kdisp_font_at(int i, char *out, int cap)
{
	const char *f = getenv("KDOS_FONT_LIST");
	FILE *fp = f && *f ? fopen(f, "r") : NULL;
	char line[256];
	int n = 0;

	if (!fp || i < 0 || !out || cap <= 0)
		return 0;
	while (fgets(line, sizeof(line), fp)) {
		if (line[0] == '\n')
			continue;
		line[strcspn(line, "\r\n")] = '\0';
		if (n++ == i) {
			snprintf(out, (size_t)cap, "%s", line);
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

int kdisp_font_current(void) { return kdisp_font_count() > 0 ? 0 : -1; }
void kdisp_font_set(int index, int keep) { (void)index; (void)keep; }
void kdisp_font_ask(void) { }

int kicon_slot_pad(const char *n, int cw, int ch, int pad)
{ (void)n; (void)cw; (void)ch; (void)pad; return -1; }
pixman_image_t *kicon_pixmap(const char *n, int w, int h)
{ (void)n; (void)w; (void)h; return NULL; }
void kicon_pixmap_free(pixman_image_t *img) { (void)img; }

struct KCellCanvas *kch_tile_begin(int id, int cw, int ch, uint64_t content)
{ (void)id; (void)cw; (void)ch; (void)content; return NULL; }
int kch_tile_commit(int id) { (void)id; return -1; }
int kch_tile_slot(int id) { (void)id; return -1; }
int kch_tile_draw(int id, KRect r, int fg, int bg)
{ (void)id; (void)r; (void)fg; (void)bg; return -1; }
void kch_tile_reset(void) {}
void kch_tile_drop(int id) { (void)id; }
void kch_tile_enable(int on) { (void)on; }

/* The chart lives in libkchrome beside the tile and is stubbed with it: its
 * raster is only ever handed a canvas kch_tile_begin() gave out, and that is
 * NULL here, so a dump draws every chart's cells. */
uint64_t kch_plot_hash(const KchPlot *p) { (void)p; return 0; }
void kch_plot_draw(struct KCellCanvas *cv, int x, int y, int w, int h,
		   const KchPlot *p)
{ (void)cv; (void)x; (void)y; (void)w; (void)h; (void)p; }
int kch_plot(int id, KRect r, const KchPlot *p, int fg, int bg)
{ (void)id; (void)r; (void)p; (void)fg; (void)bg; return -1; }

int sh_tray_init(struct sh_state *sh) { (void)sh; return -1; }
void sh_tray_dispatch(struct sh_state *sh) { (void)sh; }
void sh_tray_free(struct sh_state *sh) { (void)sh; }
int sh_tray_count(const struct sh_state *sh) { (void)sh; return 0; }
const struct sh_tray_item *sh_tray_get(const struct sh_state *sh, int i)
{ (void)sh; (void)i; return NULL; }
void sh_tray_activate(struct sh_state *sh, int i, int b, int x, int y)
{ (void)sh; (void)i; (void)b; (void)x; (void)y; }
void sh_tray_scroll(struct sh_state *sh, int i, int d)
{ (void)sh; (void)i; (void)d; }
void sh_tray_notify(struct sh_state *sh, const char *s, const char *b)
{ (void)sh; (void)s; (void)b; }
void *sh_tray_bus(const struct sh_state *sh) { (void)sh; return NULL; }

struct sh_mpris *sh_mpris_init(void *bus) { (void)bus; return NULL; }
void sh_mpris_dispatch(struct sh_mpris *p) { (void)p; }
void sh_mpris_free(struct sh_mpris *p) { (void)p; }
int sh_mpris_have(const struct sh_mpris *p) { (void)p; return 0; }
int sh_mpris_playing(const struct sh_mpris *p) { (void)p; return 0; }
const char *sh_mpris_title(const struct sh_mpris *p) { (void)p; return ""; }
const char *sh_mpris_artist(const struct sh_mpris *p) { (void)p; return ""; }
void sh_mpris_action(struct sh_mpris *p, const char *m) { (void)p; (void)m; }

void sh_unity_init(void *bus) { (void)bus; }
int sh_unity_get(const char *id, long *c, int *p, int *u)
{ (void)id; (void)c; (void)p; (void)u; return 0; }

int sh_priv_init(struct sh_state *sh) { (void)sh; return -1; }
void sh_priv_dispatch(struct sh_state *sh) { (void)sh; }
void sh_priv_settle(struct sh_state *sh, int ms) { (void)sh; (void)ms; }
void sh_priv_free(struct sh_state *sh) { (void)sh; }
int sh_priv_count(const struct sh_state *sh, int kind)
{ (void)sh; (void)kind; return 0; }
const char *sh_priv_name(const struct sh_state *sh, int kind)
{ (void)sh; (void)kind; return ""; }
/* The tooltip's "which box is recording" answer. 0 is "no box", which is a
 * valid answer and the one a dump must give: the real one walks conmon over a
 * live /proc. Missing it was an `undefined reference` from panel.c that took
 * EVERY front end's golden down with it, reported as "the new front ends do
 * not link". */
int sh_priv_box(const struct sh_state *sh, int kind, char *out, size_t n)
{ (void)sh; (void)kind; if (out && n) out[0] = '\0'; return 0; }

/*
 * THE MIXER AND THE MICROPHONE SWITCH ARE NOT STUBBED HERE, and the reason is
 * that `osd.c` defines them and `osd.c` is linked: it is a front end AND the
 * file panel.c gets `sh_volume_get` and `sh_mic_muted` from, which is cal.c's
 * and shell.c's shape. A stub for either beside the real one is a multiple
 * definition, on exactly the hosts where the harness otherwise works — and
 * that link failure skips EVERY front-end golden rather than one.
 *
 * What a dump gets from the real code is "no mixer" and "not muted", which are
 * the answers a machine with no sound server gives and the layout the goldens
 * are of.
 */

/*
 * The pixel canvas, stubbed with the rest of the paint layer. `kch_tile_begin`
 * above already answers NULL, so nothing here is ever reached with a real
 * canvas — these exist because panel.c's tile code is compiled in whether the
 * dump calls it or not, and because a measurement that answered a plausible
 * number would let a layout depend on a font this harness has not loaded.
 */
void kcell_canvas_font(const char *name) { (void)name; }
pixman_image_t *kcell_canvas_image(KCellCanvas *c) { (void)c; return NULL; }
int kcell_canvas_w(const KCellCanvas *c) { (void)c; return 0; }
int kcell_canvas_h(const KCellCanvas *c) { (void)c; return 0; }
void kcell_canvas_fill(KCellCanvas *c, int x, int y, int w, int h, int slot,
		       int alpha)
{ (void)c; (void)x; (void)y; (void)w; (void)h; (void)slot; (void)alpha; }
int kcell_canvas_text(KCellCanvas *c, int x, int base, int px,
		      const char *utf8, int slot)
{ (void)c; (void)x; (void)base; (void)px; (void)utf8; (void)slot; return 0; }
int kcell_canvas_text_width(int px, const char *utf8)
{ (void)px; (void)utf8; return 0; }
int kcell_canvas_text_ascent(int px) { (void)px; return 0; }
int kcell_canvas_text_height(int px) { (void)px; return 0; }

/* ── the size override ──────────────────────────────────────────────────── */

int __real_ktui_offscreen_init(int w, int h);

int __wrap_ktui_offscreen_init(int w, int h)
{
	const char *want = getenv("KDOS_DUMP_SIZE");
	int ww, hh;

	if (want && sscanf(want, "%dx%d", &ww, &hh) == 2 && ww > 0 && hh > 0) {
		w = ww;
		h = hh;
	}
	return __real_ktui_offscreen_init(w, h);
}

/* ── the key drive ──────────────────────────────────────────────────────
 *
 * `KDOS_DUMP_KEYS` holds whitespace-separated steps, read in order:
 *
 *   up down left right home end pgup pgdn ins del tab btab enter esc bs
 *   space f1..f12        a named key
 *   ctrl+K alt+K shift+K a named key or one character, with that modifier
 *   x                    one character (any single byte that is not a name)
 *   text:WORD            each character of WORD, one key event each
 *   click:X,Y            a left press and its release at cell X,Y
 *   rclick:X,Y           the same with the right button
 *   wheelup:X,Y wheeldown:X,Y   one detent
 *   tick                 a poll that times out
 *   resize:WxH           the surface is resized, and the poll times out
 *
 * The grid is `$KDOS_DUMP_SIZE` when set, else the size the surface asked
 * for, and it is drawn in the ascii tier like every other dump. After the
 * last step the next poll times out and the display closes; the frame is
 * printed at kdisp_shutdown(), followed by one line saying how many of the
 * script's events were read (a click is two, text:WORD one per character,
 * a tick and a resize one each) — a surface that closed itself early stops
 * short of the total.
 * A step the drive cannot parse is an error before the surface draws.
 */
enum { DRIVE_MAX = 256 };
static KtuiEvent drive_ev[DRIVE_MAX];
static int drive_resize_w[DRIVE_MAX], drive_resize_h[DRIVE_MAX];
static int drive_n, drive_at, drive_w = 80, drive_h = 24;

static const struct {
	const char *name;
	int key;
} drive_keys[] = {
	{ "up", KT_K_UP }, { "down", KT_K_DOWN }, { "left", KT_K_LEFT },
	{ "right", KT_K_RIGHT }, { "home", KT_K_HOME }, { "end", KT_K_END },
	{ "pgup", KT_K_PGUP }, { "pgdn", KT_K_PGDN }, { "ins", KT_K_INS },
	{ "del", KT_K_DEL }, { "tab", KT_K_TAB }, { "btab", KT_K_BTAB },
	{ "enter", KT_K_ENTER }, { "esc", KT_K_ESC }, { "bs", KT_K_BACKSPACE },
	{ "space", ' ' }, { "f1", KT_K_F1 }, { "f2", KT_K_F2 },
	{ "f3", KT_K_F3 }, { "f4", KT_K_F4 }, { "f5", KT_K_F5 },
	{ "f6", KT_K_F6 }, { "f7", KT_K_F7 }, { "f8", KT_K_F8 },
	{ "f9", KT_K_F9 }, { "f10", KT_K_F10 }, { "f11", KT_K_F11 },
	{ "f12", KT_K_F12 },
};

static int drive_add(KtuiEvent ev)
{
	if (drive_n >= DRIVE_MAX)
		return -1;
	drive_ev[drive_n++] = ev;
	return 0;
}

static int drive_key(const char *t, int mods)
{
	for (size_t i = 0; i < sizeof(drive_keys) / sizeof(*drive_keys); i++)
		if (!strcmp(t, drive_keys[i].name))
			return drive_add((KtuiEvent){ .type = KT_EVT_KEY,
						      .key = drive_keys[i].key,
						      .mods = mods });
	if (t[0] && !t[1])
		return drive_add((KtuiEvent){ .type = KT_EVT_KEY,
					      .key = (unsigned char)t[0],
					      .mods = mods });
	return -1;
}

static int drive_step(const char *t)
{
	int x, y, w, h;

	if (!strncmp(t, "ctrl+", 5))
		return drive_key(t + 5, KT_MOD_CTRL);
	if (!strncmp(t, "alt+", 4))
		return drive_key(t + 4, KT_MOD_ALT);
	if (!strncmp(t, "shift+", 6))
		return drive_key(t + 6, KT_MOD_SHIFT);
	if (!strncmp(t, "text:", 5)) {
		for (const char *c = t + 5; *c; c++)
			if (drive_add((KtuiEvent){ .type = KT_EVT_KEY,
						   .key = (unsigned char)*c }))
				return -1;
		return 0;
	}
	if (!strcmp(t, "tick"))
		return drive_add((KtuiEvent){ .type = KT_EVT_NONE });
	if (sscanf(t, "resize:%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
		drive_resize_w[drive_n] = w;
		drive_resize_h[drive_n] = h;
		return drive_add((KtuiEvent){ .type = KT_EVT_RESIZE });
	}
	if (sscanf(t, "click:%d,%d", &x, &y) == 2 ||
	    sscanf(t, "rclick:%d,%d", &x, &y) == 2) {
		int b = t[0] == 'r' ? KT_MB_RIGHT : KT_MB_LEFT;

		return drive_add((KtuiEvent){ .type = KT_EVT_MOUSE, .mx = x,
					      .my = y, .btn = b,
					      .press = KT_MP_PRESS }) ||
		       drive_add((KtuiEvent){ .type = KT_EVT_MOUSE, .mx = x,
					      .my = y, .btn = b,
					      .press = KT_MP_RELEASE });
	}
	if (sscanf(t, "wheelup:%d,%d", &x, &y) == 2 ||
	    sscanf(t, "wheeldown:%d,%d", &x, &y) == 2)
		return drive_add((KtuiEvent){ .type = KT_EVT_MOUSE, .mx = x,
					      .my = y,
					      .btn = t[5] == 'u' ? KT_MB_WHEEL_UP
								 : KT_MB_WHEEL_DOWN,
					      .press = KT_MP_PRESS });
	return drive_key(t, 0);
}

static void drive_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
			int ff)
{
	(void)ff;
	memcpy(prev, cur, sizeof(*cur) * (size_t)w * (size_t)h);
}

/* A step that is not an event — a tick, a resize — is a poll that times out,
 * which is how every backend reports both. */
static int drive_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)timeout_ms;
	if (drive_at >= drive_n) {
		drive_done = 1;
		return 0;
	}
	*ev = drive_ev[drive_at];
	if (ev->type == KT_EVT_RESIZE) {
		drive_w = drive_resize_w[drive_at];
		drive_h = drive_resize_h[drive_at];
		ktui_resized = 1;
	}
	drive_at++;
	return ev->type == KT_EVT_KEY || ev->type == KT_EVT_MOUSE;
}

static void drive_size(int *w, int *h)
{
	*w = drive_w;
	*h = drive_h;
}

static int drive_caps(void) { return 0; }

/* No terminal to move a caret on: without this, libktui would write the
 * escape into the frame on stdout. */
static void drive_caret(int x, int y) { (void)x; (void)y; }

static const KtuiBackend drive_backend = {
	.name = "key-drive",
	.flush = drive_flush,
	.poll_event = drive_poll,
	.size = drive_size,
	.caps = drive_caps,
	.caret = drive_caret,
};

static int drive_start(const KDispConfig *cfg)
{
	const char *keys = getenv("KDOS_DUMP_KEYS");
	const char *want = getenv("KDOS_DUMP_SIZE");
	char buf[4096], *sp = NULL;
	int w, h;

	if (!keys)
		return -1;
	snprintf(buf, sizeof(buf), "%s", keys);
	for (char *t = strtok_r(buf, " \t\n", &sp); t;
	     t = strtok_r(NULL, " \t\n", &sp))
		if (drive_step(t)) {
			fprintf(stderr, "dumpcheck: KDOS_DUMP_KEYS: bad step "
					"'%s'\n", t);
			exit(2);
		}
	if (want && sscanf(want, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
		drive_w = w;
		drive_h = h;
	} else if (cfg && cfg->cols > 0 && cfg->rows > 0) {
		drive_w = cfg->cols;
		drive_h = cfg->rows;
	}
	ktui_backend_set(&drive_backend);
	drive_on = 1;
	return 0;
}

static void drive_finish(void)
{
	if (!drive_on)
		return;
	ktui_draw_dump();
	printf("-- %d of %d events read\n", drive_at, drive_n);
	drive_on = 0;
}

/* ── the runner's own surface ─────────────────────────────────────────────
 *
 * sh_run() with nothing of any real surface's in the way: two immediate-mode
 * buttons, a Close button, and a line saying what happened. It exists for the
 * FRAME opt-in, which no shipped front end on the runner takes yet and which
 * changes what a key does:
 *
 *   --frame     every event is dispatched and drawn inside a ktui frame
 *   --eat-tab   event() answers Tab TAKEN, as a surface with panes would
 *
 * Framed, Tab walks the ring from One to Two and Enter presses the focused
 * button; a Tab event() took stays where it was; a press lands on the button
 * drawn under it; Close ends the loop through sh_run_close(). Unframed, the
 * same keys reach event() and the buttons never act — which is why framing
 * is the surface's choice and not the runner's default.
 */
static KtuiKeys rn_keys;
static const char *rn_pressed = "none";
static int rn_tabs, rn_eat;

static void rn_draw(void)
{
	ktui_draw_fill(krect(0, 0, ktui_w, ktui_h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, ktui_w, ktui_h), "Runner", KT_ACCENT,
		      KT_SURFACE, 1);
	ktui_draw_textf(2, 1, ktui_w - 4, KT_TEXT, KT_SURFACE, KT_A_NONE,
			"pressed: %s  tabs taken: %d", rn_pressed, rn_tabs);
	if (ktui_button(krect(2, 3, 9, 1), "One", 1, 1))
		rn_pressed = "One";
	if (ktui_button(krect(13, 3, 9, 1), "Two", 1, 0))
		rn_pressed = "Two";
	if (ktui_button(krect(24, 3, 9, 1), "Close", 1, 0))
		sh_run_close();
	ktui_hint("Esc", ktui_esc_verb(&rn_keys));
	ktui_hint_row(&rn_keys, krect(2, ktui_h - 2, ktui_w - 4, 1),
		      KT_SURFACE);
}

static int rn_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_KEY && ev->key == KT_K_TAB && rn_eat) {
		rn_tabs++;
		return SH_EV_TAKEN;
	}
	return SH_EV_PASS;
}

static int rn_arg(int argc, char **argv, int *i)
{
	(void)argc;
	if (!strcmp(argv[*i], "--frame"))
		;	/* read before sh_run(), into the descriptor */
	else if (!strcmp(argv[*i], "--eat-tab"))
		rn_eat = 1;
	else
		return 0;
	return 1;
}

static int runner_main(int argc, char **argv)
{
	ShSurface s = {
		.cfg = {
			.role = KDISP_ROLE_OVERLAY,
			.cols = 40,
			.rows = 8,
			.app_id = "kdos-runner",
			.keyboard = 1,
		},
		.usage = "[--frame] [--eat-tab] [--dump]",
		.keys = &rn_keys,
		.arg = rn_arg,
		.draw = rn_draw,
		.event = rn_event,
	};

	/* The flag is read by the runner's own argument pass, so the
	 * descriptor is built after a pass of its own over the same words. */
	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], "--frame"))
			s.frame = 1;
	return sh_run(&s, argc, argv);
}

/* ── the front ends ─────────────────────────────────────────────────────── */

/* Weak so that a surface which has not landed yet is a missing NAME rather
 * than a failed link. */
#define FRONT_END(sym) __attribute__((weak)) int sym(int argc, char **argv)

FRONT_END(cal_main);
FRONT_END(display_main);
FRONT_END(peek_main);
FRONT_END(find_main);
FRONT_END(pix_main);
FRONT_END(menu_main);
FRONT_END(pick_main);
FRONT_END(keys_main);
FRONT_END(teams_main);
FRONT_END(saver_main);
FRONT_END(slit_main);
FRONT_END(doc_main);
FRONT_END(settings_main);
FRONT_END(openwith_main);
FRONT_END(audio_main);
FRONT_END(start_main);
FRONT_END(net_main);
FRONT_END(bt_main);
FRONT_END(devices_main);
FRONT_END(notify_main);
FRONT_END(status_main);
FRONT_END(tip_main);
FRONT_END(panel_main);
FRONT_END(trash_main);
FRONT_END(rec_main);
FRONT_END(chars_main);
FRONT_END(contacts_main);
FRONT_END(disks_main);
FRONT_END(print_main);
FRONT_END(timezone_main);
FRONT_END(users_main);
FRONT_END(update_main);
FRONT_END(store_main);
FRONT_END(firewall_main);
FRONT_END(backup_main);
FRONT_END(burn_main);
FRONT_END(verify_main);
FRONT_END(theme_main);
FRONT_END(palette_main);
/* The five that had no dump at all. Each is a surface somebody looks at every
 * day and none had a reference frame, which is the one class of surface where
 * a geometry regression ships unseen. */
FRONT_END(run_main);
FRONT_END(connect_main);
FRONT_END(traymenu_main);
FRONT_END(prompt_main);
FRONT_END(osd_main);
FRONT_END(notifyd_main);
FRONT_END(desk_main);
/* Linked for the key drive and not for a golden: the card is a report of the
 * machine it runs on and the pad is the person's own scratch file. */
FRONT_END(about_main);
FRONT_END(note_main);

static const struct {
	const char *name;
	int (*fn)(int, char **);
} fronts[] = {
	{ "cal",	cal_main },
	{ "trash",	trash_main },
	{ "peek",	peek_main },
	{ "find",	find_main },
	{ "pix",	pix_main },
	{ "menu",	menu_main },
	{ "launcher",	palette_main },
	{ "pick",	pick_main },
	{ "keys",	keys_main },
	{ "teams",	teams_main },
	{ "display",	display_main },
	{ "saver",	saver_main },
	{ "slit",	slit_main },
	{ "doc",	doc_main },
	{ "settings",	settings_main },
	{ "openwith",	openwith_main },
	{ "audio",	audio_main },
	{ "start",	start_main },
	{ "net",	net_main },
	{ "bt",		bt_main },
	{ "devices",	devices_main },
	{ "notify",	notify_main },
	{ "status",	status_main },
	{ "tip",	tip_main },
	{ "rec",	rec_main },
	{ "chars",	chars_main },
	{ "contacts",	contacts_main },
	{ "disks",	disks_main },
	{ "print",	print_main },
	{ "time",	timezone_main },
	{ "users",	users_main },
	{ "update",	update_main },
	{ "store",	store_main },
	{ "firewall",	firewall_main },
	{ "backup",	backup_main },
	{ "burn",	burn_main },
	{ "verify",	verify_main },
	{ "theme",	theme_main },
	{ "palette",	palette_main },
	{ "shell",	panel_main },
	{ "run",	run_main },
	{ "connect",	connect_main },
	{ "traymenu",	traymenu_main },
	{ "prompt",	prompt_main },
	{ "osd",	osd_main },
	/* The DAEMON, which is the toast stack — `notify` above is the
	 * centre that reads its history. Two surfaces, two frames. */
	{ "notifyd",	notifyd_main },
	{ "desk",	desk_main },
	{ "about",	about_main },
	{ "note",	note_main },
	{ "runner",	runner_main },
};

int main(int argc, char **argv)
{
	size_t n = sizeof(fronts) / sizeof(*fronts);

	/* `dumpcheck --have <name>` answers whether a surface is linked in, so
	 * selftest.sh can skip a golden loudly instead of guessing. */
	if (argc == 3 && !strcmp(argv[1], "--have")) {
		for (size_t i = 0; i < n; i++)
			if (!strcmp(fronts[i].name, argv[2]))
				return fronts[i].fn ? 0 : 1;
		return 1;
	}
	if (argc < 2) {
		fprintf(stderr, "usage: dumpcheck <surface> [args…]\n"
				"       dumpcheck --have <surface>\n");
		return 2;
	}
	for (size_t i = 0; i < n; i++) {
		if (strcmp(fronts[i].name, argv[1]))
			continue;
		if (!fronts[i].fn) {
			fprintf(stderr, "dumpcheck: '%s' is not linked in\n",
				argv[1]);
			return 3;
		}
		return fronts[i].fn(argc - 1, argv + 1);
	}
	fprintf(stderr, "dumpcheck: no front end named '%s'\n", argv[1]);
	return 2;
}

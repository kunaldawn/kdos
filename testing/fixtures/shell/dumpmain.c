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
int kdisp_init(const KDispConfig *cfg, const KDispImpl *const *impls, int n)
{
	(void)cfg;
	(void)impls;
	(void)n;
	return -1;
}

const KDispImpl *kdisp_current(void) { return NULL; }

/* The front ends name this list; nothing in it is reachable from here. */
const KDispImpl *const kdos_disp[] = { NULL };
const int kdos_disp_n = 0;
void kdisp_shutdown(void) {}
int kdisp_should_close(void) { return 1; }
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
/* Nothing is drawing a frame round an offscreen grid, so the surface draws its
 * own — which is what makes a golden the picture tty1 shows. */
int kdisp_decorated(void) { return 0; }
int kdisp_px_h(void) { return 0; }
int kdisp_popup_offset(void) { return 0; }
void kdisp_report_error(void) {}
void kdisp_set_backdrop(KDispBackdropFn fn) { (void)fn; }
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
/*
 * The microphone switch, stubbed. It is ALSA's, through privacy.c, which
 * needs libpipewire — and kdos-devices draws the lamp beside its camera list.
 * A dump records nothing and mutes nothing, so "not muted" is the honest
 * answer and the one that keeps the layout the goldens are of.
 */
int sh_mic_muted(void) { return 0; }
void sh_mic_toggle(void) {}

int kcell_ascii_image(const uint32_t *argb, int w, int h, int stride_px,
		      int cell_w, int cell_h, uint32_t *out_cp,
		      uint32_t *out_tint, int *out_cols, int *out_rows)
{
	(void)argb; (void)w; (void)h; (void)stride_px;
	(void)cell_w; (void)cell_h; (void)out_cp; (void)out_tint;
	(void)out_cols; (void)out_rows;
	return -1;
}

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
void kch_px_vrule(int cx, int y0, int rows)
{ (void)cx; (void)y0; (void)rows; }
void kch_px_replay(pixman_image_t *dst, int scale) { (void)dst; (void)scale; }
void kch_px_body(pixman_image_t *dst, int w, int h, int s, uint8_t a, int e)
{ (void)dst; (void)w; (void)h; (void)s; (void)a; (void)e; }

/* libkcell's own painters, for the same reason. */
void kcell_set_slot_alpha(int slot, uint8_t a) { (void)slot; (void)a; }
/* The popup body is the same stub as the rest of the pixel layer: a
 * golden is the CHARACTER grid, and a layout that only lines up once
 * the paint arrives is a layout that is broken. */
void kch_px_popup(int body_slot) { (void)body_slot; }
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

int kicon_slot_pad(const char *n, int cw, int ch, int pad)
{ (void)n; (void)cw; (void)ch; (void)pad; return -1; }
pixman_image_t *kicon_pixmap(const char *n, int w, int h)
{ (void)n; (void)w; (void)h; return NULL; }
void kicon_pixmap_free(pixman_image_t *img) { (void)img; }

struct KCellCanvas *kch_tile_begin(int id, int cw, int ch, uint64_t content)
{ (void)id; (void)cw; (void)ch; (void)content; return NULL; }
int kch_tile_commit(int id) { (void)id; return -1; }
int kch_tile_slot(int id) { (void)id; return -1; }
void kch_tile_reset(void) {}
void kch_tile_enable(int on) { (void)on; }

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

/* -1 is "no mixer", which is what every readout here has to survive. */
int sh_volume_get(int *muted) { if (muted) *muted = 0; return -1; }
void sh_volume_set(int pct) { (void)pct; }
void sh_volume_toggle(void) {}
void sh_alsa_quiet(void) {}

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

/* ── the front ends ─────────────────────────────────────────────────────── */

/* Weak so that a surface which has not landed yet is a missing NAME rather
 * than a failed link. */
#define FRONT_END(sym) __attribute__((weak)) int sym(int argc, char **argv)

FRONT_END(cal_main);
FRONT_END(peek_main);
FRONT_END(menu_main);
FRONT_END(launcher_main);
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

static const struct {
	const char *name;
	int (*fn)(int, char **);
} fronts[] = {
	{ "cal",	cal_main },
	{ "trash",	trash_main },
	{ "peek",	peek_main },
	{ "menu",	menu_main },
	{ "launcher",	launcher_main },
	{ "pick",	pick_main },
	{ "keys",	keys_main },
	{ "teams",	teams_main },
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
	{ "shell",	panel_main },
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

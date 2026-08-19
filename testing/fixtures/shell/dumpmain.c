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

#include "kwl.h"
#include "shell.h"

/* libkwl, stubbed. A dump never reaches any of it; the symbols exist only
 * because the same translation units carry the interactive path. */
int kwl_init(const KwlConfig *cfg) { (void)cfg; return -1; }
void kwl_shutdown(void) {}
int kwl_should_close(void) { return 1; }
int kwl_cell_w(void) { return 16; }
int kwl_cell_h(void) { return 32; }
void kwl_input_cells(const KRect *r, int n) { (void)r; (void)n; }
void *kwl_display(void) { return NULL; }
void *kwl_seat(void) { return NULL; }
void kwl_overlay_hide(void) {}
int kwl_overlay_show(int c, int r) { (void)c; (void)r; return 0; }
void kwl_overlay_resize(int c, int r) { (void)c; (void)r; }
void kwl_cursor_set(enum kwl_cursor c) { (void)c; }
int kwl_fd(void) { return -1; }
void kwl_pump(void) {}
int kwl_scale(void) { return 1; }
int kwl_copy(const char *t, size_t n, int p)
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

int kwl_lock_engaged(void) { return 0; }
int kwl_lock_finished(void) { return 1; }
void kwl_unlock(void) {}

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

static const struct {
	const char *name;
	int (*fn)(int, char **);
} fronts[] = {
	{ "cal",	cal_main },
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

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-saver — attract mode, between dim and lock
 *
 *   ░ ▒     █        ▒
 *   ▒ █  ░  ▒     ░  █   ░
 *   █    ▒  ░  ▒  ▒  ░   ▒
 *
 * The gap the idle policy left. kdos-comp dims, then locks, then powers the
 * outputs off; between the dim and the lock there was a dimmed desktop sitting
 * there being read over somebody's shoulder. This covers it with something that
 * is unmistakably not the desktop.
 *
 * NOTHING STARTS IT YET, and saying so is the point: kdos-idle.c spawns
 * kdos-lock and no saver, and TEMPLATES[] in kdos-child.c does not carry one —
 * a feature with no line there does not run, whatever a comment says. So today
 * this is a program you run by hand or from a keybind; wiring it to the dim
 * stage (spawn on dim, SIGTERM on activity, one per output) is a change to the
 * compositor, not to this file.
 *
 * IT NEVER WATCHES INPUT, and that is the whole of its safety story. A
 * screensaver that decides for itself when to go away is a screensaver that can
 * decide wrong — and one that took the keyboard would be a lock screen with no
 * password. This surface asks for NO keyboard interactivity and claims NO
 * pointer region at all (kwl_input_cells(NULL, 0)), so every keystroke and every
 * click goes to whatever is underneath exactly as if this were not here.
 * Activity detection stays where it already is, in the compositor's idle policy,
 * which kills this process on the first event.
 *
 * ONE PROCESS PER OUTPUT, named with `--output`, the same convention the panel
 * and the desktop already follow: libktui has a single cell buffer, so a second
 * screen cannot be a second surface of the same process.
 *
 * The frame rate is capped at 15. A screensaver exists for a machine nobody is
 * looking at, and burning a battery to animate for nobody is the one thing it
 * must not do.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>

#include "kwl.h"
#include "shell.h"

#define SV_FPS_MAX	15
#define SV_FPS_DEF	10
#define SV_MAX_COLS	512
#define SV_MAX_ROWS	256
#define SV_LOGO_PATH	"/usr/share/kdos/logo.txt"
#define SV_LOGO_LINES	48
#define SV_LOGO_BYTES	512

enum { SV_MODE_RAIN = 0, SV_MODE_LOGO, SV_MODE_OFF };

/* ── the ramp ────────────────────────────────────────────────────────────
 *
 * Three levels and no more. The console font is 512 glyphs of xos4-2: it has
 * FULL BLOCK, ░ and ▒ and it does NOT have ▓, half blocks or braille — so a
 * gradient with a fourth step in it comes out blank on the one screen this is
 * most likely to be running on. libktui's own ramp is the wrong shape here:
 * its rich tier is eighth blocks, which are partial cells rather than
 * densities, and rain drawn out of them reads as a bar chart.
 */
static const char *sv_ramp[3];

static void sv_ramp_init(void)
{
	if (ktui_caps & KT_CAP_UTF8) {
		sv_ramp[0] = "\xe2\x96\x91";	/* ░ U+2591 LIGHT SHADE  */
		sv_ramp[1] = "\xe2\x96\x92";	/* ▒ U+2592 MEDIUM SHADE */
		sv_ramp[2] = "\xe2\x96\x88";	/* █ U+2588 FULL BLOCK   */
	} else {
		sv_ramp[0] = ".";
		sv_ramp[1] = ":";
		sv_ramp[2] = "#";
	}
}

static uint32_t sv_cp(const char *g)
{
	uint32_t cp = 0;
	ktui_utf8_next(g, &cp);
	return cp;
}

/* ── randomness ──────────────────────────────────────────────────────────
 *
 * Ours, not libc's: `--dump` has to produce the same frame every time it is
 * run, and rand() is shared state that any library on the link line may have
 * pulled a number out of first.
 */
static uint32_t sv_seed = 1;

static uint32_t sv_rand(void)
{
	sv_seed ^= sv_seed << 13;
	sv_seed ^= sv_seed >> 17;
	sv_seed ^= sv_seed << 5;
	return sv_seed;
}

static int sv_range(int lo, int hi)	/* inclusive */
{
	if (hi <= lo)
		return lo;
	return lo + (int)(sv_rand() % (uint32_t)(hi - lo + 1));
}

/* ── phosphor rain ───────────────────────────────────────────────────────
 *
 * One drop per column, positioned in sixteenths of a row so a drop can fall
 * slower than one row per frame without the position being a float. The tip is
 * the SECONDARY colour and the trail the accent, which is what makes the head
 * of a column read as the live end of it.
 */
struct sv_drop {
	int pos;		/* head position, rows * 16 */
	int vel;		/* rows * 16 per frame      */
	int len;		/* trail length in rows     */
};

static struct sv_drop sv_drops[SV_MAX_COLS];
static int sv_ndrops;

static void sv_drop_spawn(struct sv_drop *d, int rows, int seeded)
{
	d->len = sv_range(4, rows > 8 ? rows / 2 : 4);
	d->vel = sv_range(3, 14);
	/*
	 * A fresh drop starts above the screen by a random gap, so the columns
	 * do not all restart on the same frame and beat together. `seeded`
	 * scatters the FIRST generation over the whole height instead — without
	 * it the first second of the saver is an empty screen.
	 */
	if (seeded)
		d->pos = sv_range(-rows, rows) * 16;
	else
		d->pos = -sv_range(1, rows > 4 ? rows : 4) * 16;
}

static void sv_rain_init(int cols, int rows)
{
	sv_ndrops = cols > SV_MAX_COLS ? SV_MAX_COLS : cols;
	for (int i = 0; i < sv_ndrops; i++)
		sv_drop_spawn(&sv_drops[i], rows, 1);
}

static void sv_rain_step(int rows)
{
	for (int i = 0; i < sv_ndrops; i++) {
		struct sv_drop *d = &sv_drops[i];
		d->pos += d->vel;
		if (d->pos / 16 - d->len >= rows)
			sv_drop_spawn(d, rows, 0);
	}
}

static void sv_rain_draw(int cols, int rows)
{
	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);

	for (int c = 0; c < cols && c < sv_ndrops; c++) {
		const struct sv_drop *d = &sv_drops[c];
		int head = d->pos / 16;

		for (int i = 0; i < d->len; i++) {
			int y = head - i;
			int fg, lvl;

			if (y < 0 || y >= rows)
				continue;
			if (i == 0) {
				fg = KT_WARN;	/* the live end */
				lvl = 2;
			} else if (i * 3 < d->len) {
				fg = KT_ACCENT;
				lvl = 2;
			} else if (i * 3 < d->len * 2) {
				fg = KT_ACCENT;
				lvl = 1;
			} else {
				fg = KT_DIM;
				lvl = 0;
			}
			ktui_draw_cell(c, y, sv_cp(sv_ramp[lvl]), fg, KT_BG,
				       KT_A_NONE);
		}
	}
}

/* ── the drifting mascot ─────────────────────────────────────────────────
 *
 * /usr/share/kdos/logo.txt is the one the login banner draws, generated from
 * the same quantised crop of kdos.png the boot splash uses — so the saver
 * cannot drift away from the mascot the rest of the system shows.
 */
static char sv_logo[SV_LOGO_LINES][SV_LOGO_BYTES];
static int sv_logo_n, sv_logo_w;

/*
 * logo.txt is COLOURED — kdos-banner reads it and replays it a raster line at a
 * time, so genlogo.py writes SGR sequences into it. Painting those into cells
 * puts `[1;32m` on the screen as text and makes every width measurement wrong
 * by the length of the escapes. The desktop's palette is libktui's eight slots
 * anyway, so the colour is dropped rather than translated: this is the same
 * strip_ansi kdos-banner does in pure bash, and for the same reason.
 */
static void sv_strip_ansi(char *s)
{
	char *w = s;

	for (const char *r = s; *r; ) {
		if (*r != '\033') {
			*w++ = *r++;
			continue;
		}
		r++;
		if (*r == '[') {
			r++;
			/* CSI runs until a final byte in 0x40..0x7e. */
			while (*r && (unsigned char)*r < 0x40)
				r++;
			if (*r)
				r++;
		} else if (*r) {
			r++;	/* a two-byte escape; nothing here emits one */
		}
	}
	*w = '\0';
}

static int sv_logo_load(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[SV_LOGO_BYTES];

	sv_logo_n = 0;
	sv_logo_w = 0;
	if (!f)
		return -1;
	while (sv_logo_n < SV_LOGO_LINES && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = '\0';
		sv_strip_ansi(line);
		snprintf(sv_logo[sv_logo_n], SV_LOGO_BYTES, "%s", line);
		int w = ktui_utf8_width(sv_logo[sv_logo_n]);
		if (w > sv_logo_w)
			sv_logo_w = w;
		sv_logo_n++;
	}
	fclose(f);
	/* Trailing blank lines would bounce off the bottom edge early, with a
	 * gap under the penguin nobody can see the reason for. */
	while (sv_logo_n > 0 && !sv_logo[sv_logo_n - 1][0])
		sv_logo_n--;
	return sv_logo_n > 0 ? 0 : -1;
}

/* Position and velocity in sixteenths again: a whole cell per frame at 10 fps
 * crosses a 1080p screen in four seconds, which is a bouncing ball rather than
 * a drift. */
static int sv_lx, sv_ly, sv_lvx, sv_lvy, sv_lcolor;

static const int SV_LOGO_COLORS[] = { KT_ACCENT, KT_WARN, KT_MID, KT_TEXT };

static void sv_logo_init(int cols, int rows)
{
	int mx = cols - sv_logo_w, my = rows - sv_logo_n;

	sv_lx = mx > 0 ? sv_range(0, mx) * 16 : 0;
	sv_ly = my > 0 ? sv_range(0, my) * 16 : 0;
	sv_lvx = mx > 0 ? 5 : 0;
	sv_lvy = my > 0 ? 3 : 0;
	sv_lcolor = 0;
}

static void sv_logo_step(int cols, int rows)
{
	int mx = (cols - sv_logo_w) * 16, my = (rows - sv_logo_n) * 16;
	int bounced = 0;

	/* A logo wider or taller than the screen has nowhere to go: pinning it
	 * is right, and reversing a velocity against a negative bound would
	 * make it shudder in place. */
	if (mx <= 0) {
		sv_lx = 0;
		sv_lvx = 0;
	}
	if (my <= 0) {
		sv_ly = 0;
		sv_lvy = 0;
	}

	sv_lx += sv_lvx;
	sv_ly += sv_lvy;
	if (mx > 0 && (sv_lx < 0 || sv_lx > mx)) {
		sv_lx = sv_lx < 0 ? 0 : mx;
		sv_lvx = -sv_lvx;
		bounced = 1;
	}
	if (my > 0 && (sv_ly < 0 || sv_ly > my)) {
		sv_ly = sv_ly < 0 ? 0 : my;
		sv_lvy = -sv_lvy;
		bounced = 1;
	}
	if (bounced)
		sv_lcolor = (sv_lcolor + 1) %
			    (int)(sizeof(SV_LOGO_COLORS) / sizeof(SV_LOGO_COLORS[0]));
}

static void sv_logo_draw(int cols, int rows)
{
	int fg = SV_LOGO_COLORS[sv_lcolor];
	int x = sv_lx / 16, y = sv_ly / 16;

	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);
	for (int i = 0; i < sv_logo_n; i++) {
		int ly = y + i;
		if (ly < 0 || ly >= rows)
			continue;
		ktui_draw_text(x, ly, cols - x, sv_logo[i], fg, KT_BG,
			       KT_A_NONE);
	}
}

/* ── how big the output actually is ──────────────────────────────────────
 *
 * layer-shell sizes an unanchored overlay in pixels and libkwl has no output
 * geometry to hand out, so the surface is measured before it is created: a
 * registry of our own on libkwl's connection, one wl_output listener, one
 * roundtrip. wl_output version 4 is bound for the `name` event — a registry id
 * cannot be written on a command line, and `--output eDP-1` is the whole
 * interface a supervisor has.
 *
 * Doing it under KWL_ROLE_NONE (connect, bind, no surface) is what keeps this
 * from being visible: the alternative is creating a small overlay and resizing
 * it, which flashes a box in the middle of the screen on every idle timeout.
 */
#define SV_MAX_OUT 16

struct sv_output {
	char name[64];
	int w, h;
};

static struct sv_output sv_outs[SV_MAX_OUT];
static int sv_nouts;

static void sv_out_geometry(void *d, struct wl_output *o, int32_t x, int32_t y,
			    int32_t pw, int32_t ph, int32_t sub,
			    const char *make, const char *model, int32_t tr)
{ (void)d; (void)o; (void)x; (void)y; (void)pw; (void)ph; (void)sub;
  (void)make; (void)model; (void)tr; }

static void sv_out_mode(void *d, struct wl_output *o, uint32_t flags, int32_t w,
			int32_t h, int32_t refresh)
{
	int i = (int)(intptr_t)d;
	(void)o;
	(void)refresh;
	/* CURRENT only: a monitor advertises every mode it can do, and the
	 * last one in the list is not the one it is running. */
	if (!(flags & WL_OUTPUT_MODE_CURRENT) || i < 0 || i >= SV_MAX_OUT)
		return;
	sv_outs[i].w = w;
	sv_outs[i].h = h;
}

static void sv_out_done(void *d, struct wl_output *o) { (void)d; (void)o; }
static void sv_out_scale(void *d, struct wl_output *o, int32_t f)
{ (void)d; (void)o; (void)f; }

static void sv_out_name(void *d, struct wl_output *o, const char *name)
{
	int i = (int)(intptr_t)d;
	(void)o;
	if (i < 0 || i >= SV_MAX_OUT || !name)
		return;
	snprintf(sv_outs[i].name, sizeof(sv_outs[0].name), "%s", name);
}

static void sv_out_description(void *d, struct wl_output *o, const char *s)
{ (void)d; (void)o; (void)s; }

static const struct wl_output_listener sv_output_listener = {
	.geometry = sv_out_geometry,
	.mode = sv_out_mode,
	.done = sv_out_done,
	.scale = sv_out_scale,
	.name = sv_out_name,
	.description = sv_out_description,
};

static void sv_reg_global(void *data, struct wl_registry *reg, uint32_t id,
			  const char *iface, uint32_t ver)
{
	(void)data;
	if (strcmp(iface, "wl_output") || sv_nouts >= SV_MAX_OUT)
		return;
	struct wl_output *o = wl_registry_bind(reg, id, &wl_output_interface,
					       ver < 4 ? ver : 4);
	if (!o)
		return;
	wl_output_add_listener(o, &sv_output_listener,
			       (void *)(intptr_t)sv_nouts);
	sv_nouts++;
}

static void sv_reg_remove(void *data, struct wl_registry *reg, uint32_t id)
{ (void)data; (void)reg; (void)id; }

static const struct wl_registry_listener sv_registry_listener = {
	.global = sv_reg_global,
	.global_remove = sv_reg_remove,
};

/*
 * Measure the named output (or the first one) in CELLS. Returns 0 on success;
 * a failure is not fatal, it just leaves the caller with its fallback size.
 */
static int sv_measure(const char *want, const char *font, int *cols, int *rows)
{
	/* The SAME font the surface will be created with: cells are what this
	 * converts pixels into, and measuring with libkwl's default while
	 * drawing at `--font Terminus:pixelsize=16` covers a quarter of the
	 * screen and leaves the desktop showing around it. */
	KwlConfig probe = { .role = KWL_ROLE_NONE, .font = font };
	struct wl_display *dpy;
	struct wl_registry *reg;
	int cw, ch, pick = -1, rc = -1;

	if (kwl_init(&probe) != 0)
		return -1;
	dpy = kwl_display();
	cw = kwl_cell_w();
	ch = kwl_cell_h();
	if (!dpy || cw < 1 || ch < 1) {
		kwl_shutdown();
		return -1;
	}

	sv_nouts = 0;
	memset(sv_outs, 0, sizeof(sv_outs));
	reg = wl_display_get_registry(dpy);
	if (reg) {
		wl_registry_add_listener(reg, &sv_registry_listener, NULL);
		wl_display_roundtrip(dpy);	/* the globals   */
		wl_display_roundtrip(dpy);	/* and their events */
	}

	/*
	 * The named output, then ANY output. An unknown name falls back to the
	 * compositor's own choice in libkwl (a screen unplugged between the
	 * supervisor deciding and this process starting is a race, not a
	 * mistake), so measuring it as 80x24 would leave a small box in the
	 * middle of a screen that is otherwise showing the desktop.
	 */
	for (int pass = 0; pass < 2 && pick < 0; pass++)
		for (int i = 0; i < sv_nouts; i++) {
			if (sv_outs[i].w < 1 || sv_outs[i].h < 1)
				continue;
			if (pass == 0 && want && *want &&
			    strcmp(sv_outs[i].name, want))
				continue;
			pick = i;
			break;
		}
	if (pick >= 0) {
		/*
		 * Rounded UP, not down. A screen 1080 pixels tall with a 32
		 * pixel cell is 33.75 rows: 33 rows leaves a strip of the
		 * desktop showing along the bottom edge of a saver whose whole
		 * job is to cover it. The compositor crops the overflow.
		 */
		*cols = (sv_outs[pick].w + cw - 1) / cw;
		*rows = (sv_outs[pick].h + ch - 1) / ch;
		if (*cols > SV_MAX_COLS)
			*cols = SV_MAX_COLS;
		if (*rows > SV_MAX_ROWS)
			*rows = SV_MAX_ROWS;
		rc = 0;
	}

	if (reg)
		wl_registry_destroy(reg);
	kwl_shutdown();
	return rc;
}

/* ── main ──────────────────────────────────────────────────────────────── */

static int sv_usage(void)
{
	fprintf(stderr,
		"usage: kdos-saver [--mode rain|logo|off] [--fps N] "
		"[--output NAME]\n"
		"                  [--font NAME] [--dump]\n");
	return 2;
}

int saver_main(int argc, char **argv)
{
	const char *font = NULL, *output = NULL;
	int mode = SV_MODE_RAIN, fps = SV_FPS_DEF, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--output") && i + 1 < argc) {
			output = argv[++i];
		} else if (!strcmp(argv[i], "--fps") && i + 1 < argc) {
			fps = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
			const char *m = argv[++i];
			if (!strcmp(m, "rain"))
				mode = SV_MODE_RAIN;
			else if (!strcmp(m, "logo"))
				mode = SV_MODE_LOGO;
			else if (!strcmp(m, "off"))
				mode = SV_MODE_OFF;
			else
				return sv_usage();
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			return sv_usage();
		}
	}

	/* `off` is an honest off: nothing is drawn and nothing is connected to,
	 * so the idle policy can start this unconditionally and have it cost a
	 * process that exits. */
	if (mode == SV_MODE_OFF)
		return 0;
	if (fps < 1)
		fps = 1;
	if (fps > SV_FPS_MAX)
		fps = SV_FPS_MAX;

	sh_theme_from_cache();

	if (dump) {
		/*
		 * A fixed seed and frame zero. The animation is a function of
		 * the clock, so the only frame that can be compared against a
		 * committed golden one is the first, drawn from a known seed.
		 */
		int cols = 80, rows = 24;
		sv_seed = 20260814u;
		ktui_offscreen_init(cols, rows);
		sv_ramp_init();
		if (mode == SV_MODE_LOGO && sv_logo_load(SV_LOGO_PATH) == 0) {
			sv_logo_init(cols, rows);
			sv_logo_draw(cols, rows);
		} else {
			sv_rain_init(cols, rows);
			sv_rain_draw(cols, rows);
		}
		ktui_draw_dump();
		return 0;
	}

	sv_seed = (uint32_t)(time(NULL) ^ (long)getpid());
	if (!sv_seed)
		sv_seed = 1;

	/* Measured first, created at the right size: see sv_measure(). A
	 * machine whose outputs cannot be read still gets a saver, just a
	 * conservatively sized one. */
	int cols = 80, rows = 24;
	sv_measure(output, font, &cols, &rows);

	KwlConfig cfg = {
		.role = KWL_ROLE_OVERLAY,
		.cols = cols,
		.rows = rows,
		.app_id = "kdos-saver",
		.font = font,
		.output = output,
		/*
		 * No keyboard and no dismiss-on-unfocus. This surface must not
		 * be able to take a keystroke: it is not a lock screen, and one
		 * that swallowed input would be a lock screen with no password.
		 */
	};
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-saver: no compositor or no layer-shell\n");
		return 1;
	}
	/* And no pointer region at all — a click goes through to the desktop,
	 * where the compositor's idle policy sees it and kills this. */
	kwl_input_cells(NULL, 0);
	ktui_draw_init();
	sv_ramp_init();

	if (mode == SV_MODE_LOGO && sv_logo_load(SV_LOGO_PATH) != 0) {
		/* No mascot on this machine is not a reason to show nothing:
		 * the rain needs no data file. */
		fprintf(stderr, "kdos-saver: no %s; falling back to rain\n",
			SV_LOGO_PATH);
		mode = SV_MODE_RAIN;
	}

	int cw = ktui_w, ch = ktui_h;
	if (mode == SV_MODE_LOGO)
		sv_logo_init(cw, ch);
	else
		sv_rain_init(cw, ch);

	const int frame_ms = 1000 / fps;

	while (!kwl_should_close()) {
		if (mode == SV_MODE_LOGO)
			sv_logo_draw(cw, ch);
		else
			sv_rain_draw(cw, ch);
		/*
		 * No ktui_draw_invalidate() anywhere in this loop. The cell
		 * diff is what keeps a full-screen animation cheap: at 10 fps
		 * the rain touches a few hundred cells a frame out of tens of
		 * thousands, and forcing a full repaint would upload the whole
		 * buffer sixty times a minute for nothing.
		 */
		ktui_draw_flush();

		KtuiEvent ev;
		ktui_backend()->poll_event(&ev, frame_ms);
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
			cw = ktui_w;
			ch = ktui_h;
			if (mode == SV_MODE_LOGO)
				sv_logo_init(cw, ch);
			else
				sv_rain_init(cw, ch);
		}
		if (mode == SV_MODE_LOGO)
			sv_logo_step(cw, ch);
		else
			sv_rain_step(ch);
	}

	kwl_shutdown();
	return 0;
}

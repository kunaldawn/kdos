/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-saver — attract mode, between idle and lock
 *
 *   ░ ▒     █        ▒
 *   ▒ █  ░  ▒     ░  █   ░
 *   █    ▒  ░  ▒  ▒  ░   ▒
 *
 * The step between a desktop being left alone and it being locked. What is
 * there otherwise is a desktop sitting on screen being read over somebody's
 * shoulder; this covers it with something that is unmistakably not the desktop.
 *
 * STARTED BY THE DISPLAY'S IDLE POLICY, not by itself. kdos-con spawns it at
 * `idle_saver` and asks it to close on the first keystroke. On the graphical
 * desktop kdos-idle.c spawns kdos-lock and no saver, and TEMPLATES[] in
 * kdos-child.c does not carry one — a feature with no line there does not run,
 * whatever a comment says — so there it is a program you run by hand.
 *
 * IT NEVER WATCHES INPUT, and that is the whole of its safety story. A
 * screensaver that decides for itself when to go away is a screensaver that can
 * decide wrong — and one that took the keyboard would be a lock screen with no
 * password. This surface asks for NO keyboard interactivity and claims NO
 * pointer region at all (kdisp_input_cells(NULL, 0)), so every keystroke and every
 * click goes to whatever is underneath exactly as if this were not here.
 * Activity detection stays with the display, which takes the surface away on
 * the first event.
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

#include "shell.h"

#define SV_FPS_MAX	15
#define SV_FPS_DEF	10
#define SV_MAX_COLS	512
#define SV_MAX_ROWS	256
#define SV_LOGO_PATH	"/usr/share/kdos/logo.txt"
/* The artwork's own limits are shell.h's — see sh_logo_load(). */
#define SV_LOGO_LINES	SH_LOGO_LINES
#define SV_LOGO_BYTES	SH_LOGO_BYTES

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

static int sv_logo_load(const char *path)
{
	return sh_logo_load(path, sv_logo, SV_LOGO_LINES, &sv_logo_n,
			    &sv_logo_w);
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

	/*
	 * NO SIZE IS ASKED FOR. KDISP_ROLE_SAVER is the whole screen and the
	 * display says how big that is — a client that measured it would have
	 * to round pixels into cells, and a row rounded down is a strip of
	 * desktop along the bottom edge of a surface whose whole job is to
	 * cover it. The first configure arrives before anything is drawn.
	 */
	KDispConfig cfg = {
		.role = KDISP_ROLE_SAVER,
		.app_id = "kdos-saver",
		.font = font,
		.output = output,
		/*
		 * No keyboard and no dismiss-on-unfocus. This surface must not
		 * be able to take a keystroke: it is not a lock screen, and one
		 * that swallowed input would be a lock screen with no password.
		 */
	};
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-saver: no display\n");
		return 1;
	}
	/* And no pointer region at all — a click goes through to the desktop,
	 * where the display's idle policy sees it and takes this away. */
	kdisp_input_cells(NULL, 0);
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

	while (!kdisp_should_close()) {
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

	kdisp_shutdown();
	return 0;
}

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

#include "kbase.h"
#include "kcon.h"
#include "shell.h"

#define SV_FPS_MAX	15
/* How far a `--dump` runs the effect before it draws — four seconds at the
 * default rate, which is long enough for every effect here to have something
 * on the screen and short enough to stay one picture. */
#define SV_DUMP_FRAMES	60
#define SV_FPS_DEF	10
#define SV_MAX_COLS	512
#define SV_MAX_ROWS	256
#define SV_ART_PATH	"/usr/share/kdos/screensaver.txt"
/* The artwork's own limits are shell.h's — see sh_logo_load(). */
#define SV_ART_LINES	SH_LOGO_LINES
#define SV_ART_BYTES	SH_LOGO_BYTES

/*
 * `off` and `random` are answered before any effect runs and are not effects:
 * one draws nothing and the other picks one of the rest. Everything else is a
 * row of sv_effects[] at the bottom of this file.
 */
enum { SV_MODE_OFF = -2, SV_MODE_RANDOM = -3, SV_MODE_UNSET = -1 };

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

static void sv_rain_step(int cols, int rows)
{
	(void)cols;
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

/* ── the drifting art ────────────────────────────────────────────────────
 *
 * EVERY EFFECT THAT IS NOT WEATHER IS A TRANSFORM OVER ONE LOADED GRID, and
 * the grid is a file rather than a table in this source: art belongs to
 * whoever is looking at it, and an effect that carried its own picture would
 * be an effect nobody could change without a compiler.
 *
 * `~/.config/kdos/screensaver.txt` wins over `/usr/share/kdos/screensaver.txt`
 * — the same rule every other overridable file here keeps. Not `logo.txt`:
 * that one is the login banner's, generated from the same quantised crop of
 * kdos.png the boot splash uses, and a person who wanted their own screensaver
 * would otherwise be changing the picture the machine boots with.
 *
 * SGR is stripped by the loader, because a surface paints slots.
 */
static char sv_art[SV_ART_LINES][SV_ART_BYTES];
static int sv_art_n, sv_art_w;

static const char *sv_art_path(char *buf, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(buf, n, "%s/kdos/screensaver.txt", cfg);
	else if (home && *home)
		snprintf(buf, n, "%s/.config/kdos/screensaver.txt", home);
	else
		return SV_ART_PATH;
	return kb_path_exists(buf) ? buf : SV_ART_PATH;
}

static int sv_art_load(void)
{
	char buf[512];

	return sh_logo_load(sv_art_path(buf, sizeof(buf)), sv_art,
			    SV_ART_LINES, &sv_art_n, &sv_art_w);
}

/* Position and velocity in sixteenths again: a whole cell per frame at 10 fps
 * crosses a 1080p screen in four seconds, which is a bouncing ball rather than
 * a drift. */
static int sv_lx, sv_ly, sv_lvx, sv_lvy, sv_lcolor;

static const int SV_ART_COLORS[] = { KT_ACCENT, KT_WARN, KT_MID, KT_TEXT };

static void sv_art_init(int cols, int rows)
{
	int mx = cols - sv_art_w, my = rows - sv_art_n;

	sv_lx = mx > 0 ? sv_range(0, mx) * 16 : 0;
	sv_ly = my > 0 ? sv_range(0, my) * 16 : 0;
	sv_lvx = mx > 0 ? 5 : 0;
	sv_lvy = my > 0 ? 3 : 0;
	sv_lcolor = 0;
}

static void sv_art_step(int cols, int rows)
{
	int mx = (cols - sv_art_w) * 16, my = (rows - sv_art_n) * 16;
	int bounced = 0;

	/* Art wider or taller than the screen has nowhere to go: pinning it
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
			    (int)(sizeof(SV_ART_COLORS) / sizeof(SV_ART_COLORS[0]));
}

static void sv_art_draw(int cols, int rows)
{
	int fg = SV_ART_COLORS[sv_lcolor];
	int x = sv_lx / 16, y = sv_ly / 16;

	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);
	for (int i = 0; i < sv_art_n; i++) {
		int ly = y + i;
		if (ly < 0 || ly >= rows)
			continue;
		ktui_draw_text(x, ly, cols - x, sv_art[i], fg, KT_BG,
			       KT_A_NONE);
	}
}

/* ── the effects of the era ──────────────────────────────────────────────
 *
 * Each is a TRANSFORM AT THE EXISTING CAP and nothing more: no interpreter, no
 * plug-in path, no file of its own beyond the one grid `art` already reads. A
 * screensaver that could run what a file told it to would be a program running
 * code on a machine nobody is watching.
 *
 * THE VOCABULARY IS WHAT THE CONSOLE FONT HAS, which is a limit and not a
 * style. `ter-kdos32n` is 512 glyphs — the single box set, six double-line
 * corners, ░ ▒ █ — and it carries no ▓, no half block and no braille. An
 * effect drawn out of what it lacks is BLANK on tty1 and correct in a terminal
 * and over ssh, which is the worst way for this to fail: it looks right on the
 * machine it was written on. So the density effects take sv_ramp, which is
 * already tiered; `pipes` has a tier of its own for the same reason; and
 * `matrix` and `starfield` are ASCII on purpose, so the question cannot arise
 * for them at all.
 *
 * AND EVERY COLOUR IS A SLOT, so `kdos theme` moves the saver with the rest of
 * the desktop. A literal here would be the one animation a retint cannot
 * reach.
 */

/* The frame counter, which only `matrix` reads. */
static unsigned sv_frame;

/* ── matrix ──────────────────────────────────────────────────────────────
 *
 * THE MOTION IS THE RAIN'S. A column of falling glyphs and a column of falling
 * shades are the same drop with a different alphabet, so this shares
 * sv_rain_init and sv_rain_step rather than carrying a second copy of the
 * spawn, the velocity and the trail — two copies of that would drift apart the
 * first time either is tuned.
 *
 * THE ALPHABET IS ASCII, and that is the whole of why this is not katakana:
 * the console font is Terminus' xos4-2, which has Latin, Greek and Cyrillic
 * and no kana at all, so the picture everybody has in mind is a screen of
 * blanks on tty1 and correct everywhere else.
 */
static const char SV_MATRIX_GLYPHS[] =
	"0123456789ABCDEFGHJKLMNPQRSTUVWXYZ<>[]{}=+*/\\|:;.?!$#%&@";

/*
 * THE GLYPH BELONGS TO THE CELL AND THE BRIGHTNESS TO THE DROP. A column whose
 * characters travelled with its head would read as text scrolling past; what
 * the effect is, is a fixed field of characters with a lit head falling
 * through it. So the glyph is a hash of the position, and the frame counter's
 * high bits are in the hash to make it mutate every sixteen frames rather than
 * every one — a field that changed each frame is noise, and one that never
 * changed is wallpaper.
 */
static uint32_t sv_matrix_glyph(int x, int y)
{
	unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^
		     (sv_frame >> 4) * 83492791u;

	h ^= h >> 13;
	h *= 2654435761u;
	h ^= h >> 15;
	return (uint32_t)(unsigned char)
		SV_MATRIX_GLYPHS[h % (sizeof(SV_MATRIX_GLYPHS) - 1)];
}

static void sv_matrix_draw(int cols, int rows)
{
	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);

	for (int c = 0; c < cols && c < sv_ndrops; c++) {
		const struct sv_drop *d = &sv_drops[c];
		int head = d->pos / 16;

		for (int i = 0; i < d->len; i++) {
			int y = head - i, fg;

			if (y < 0 || y >= rows)
				continue;
			/* The head is TEXT and not the accent: the live end of
			 * a column has to stand off the trail behind it, and
			 * the trail is already the accent. */
			if (i == 0)
				fg = KT_TEXT;
			else if (i * 3 < d->len)
				fg = KT_ACCENT;
			else if (i * 3 < d->len * 2)
				fg = KT_MID;
			else
				fg = KT_DIM;
			ktui_draw_cell(c, y, sv_matrix_glyph(c, y), fg, KT_BG,
				       KT_A_NONE);
		}
	}
}

/* ── pipes ───────────────────────────────────────────────────────────────
 *
 * THE TRAIL IS THE EFFECT, so it is kept rather than redrawn: what is on the
 * screen is every cell every pipe has passed through, and a draw that
 * recomputed it each frame would have to remember the whole path anyway. The
 * grid IS that memory, one byte a cell — the piece in the low nibble with zero
 * meaning empty, the colour slot in the high one.
 *
 * MALLOC'D AND NOT STATIC, unlike the drops. A static grid at the maxima this
 * file already declares is a quarter of a megabyte of BSS in a binary that is
 * forty surfaces, thirty-nine of which are not this one; the drops are six
 * kilobytes and can afford to be static.
 */
enum { SV_UP = 0, SV_RIGHT, SV_DOWN, SV_LEFT };
static const int SV_DX[4] = { 0, 1, 0, -1 };
static const int SV_DY[4] = { -1, 0, 1, 0 };

/* │ ─ ┌ ┐ └ ┘, and the ASCII a font without them gets. A corner is a plus
 * there: three ASCII characters cannot say which way a pipe turned, and a
 * wrong corner is worse than a plain one. */
enum { SV_P_V = 0, SV_P_H, SV_P_TL, SV_P_TR, SV_P_BL, SV_P_BR, SV_P_N };
static const char *sv_pipe_g[SV_P_N];

static void sv_pipe_glyphs(void)
{
	if (ktui_caps & KT_CAP_UTF8) {
		sv_pipe_g[SV_P_V]  = "\xe2\x94\x82";	/* │ */
		sv_pipe_g[SV_P_H]  = "\xe2\x94\x80";	/* ─ */
		sv_pipe_g[SV_P_TL] = "\xe2\x94\x8c";	/* ┌ */
		sv_pipe_g[SV_P_TR] = "\xe2\x94\x90";	/* ┐ */
		sv_pipe_g[SV_P_BL] = "\xe2\x94\x94";	/* └ */
		sv_pipe_g[SV_P_BR] = "\xe2\x94\x98";	/* ┘ */
	} else {
		sv_pipe_g[SV_P_V]  = "|";
		sv_pipe_g[SV_P_H]  = "-";
		sv_pipe_g[SV_P_TL] = "+";
		sv_pipe_g[SV_P_TR] = "+";
		sv_pipe_g[SV_P_BL] = "+";
		sv_pipe_g[SV_P_BR] = "+";
	}
}

/*
 * WHICH PIECE A CELL IS, from the direction the pipe ENTERED it moving and the
 * direction it LEAVES. Entering moving up means it came from below, so the
 * piece connects downwards; the rest follows. A reversal is never generated,
 * so those four entries are -1 and are never read.
 */
static const signed char SV_PIPE_PIECE[4][4] = {
	[SV_UP]    = { [SV_UP] = SV_P_V,  [SV_RIGHT] = SV_P_TL,
		       [SV_DOWN] = -1,	  [SV_LEFT] = SV_P_TR },
	[SV_RIGHT] = { [SV_UP] = SV_P_BR, [SV_RIGHT] = SV_P_H,
		       [SV_DOWN] = SV_P_TR, [SV_LEFT] = -1 },
	[SV_DOWN]  = { [SV_UP] = -1,	  [SV_RIGHT] = SV_P_BL,
		       [SV_DOWN] = SV_P_V, [SV_LEFT] = SV_P_BR },
	[SV_LEFT]  = { [SV_UP] = SV_P_BL, [SV_RIGHT] = -1,
		       [SV_DOWN] = SV_P_TL, [SV_LEFT] = SV_P_H },
};

#define SV_PIPES 5
static const int SV_PIPE_COLORS[] = { KT_ACCENT, KT_WARN, KT_MID, KT_TEXT };

struct sv_pipe {
	int x, y, dir, color;
};

static struct sv_pipe sv_pipes[SV_PIPES];
static unsigned char *sv_pipe_grid;
static int sv_pipe_w, sv_pipe_h, sv_pipe_used;

static void sv_pipe_place(struct sv_pipe *p, int cols, int rows)
{
	p->x = sv_range(0, cols - 1);
	p->y = sv_range(0, rows - 1);
	p->dir = sv_range(0, 3);
	p->color = sv_range(0, (int)(sizeof(SV_PIPE_COLORS) /
				    sizeof(SV_PIPE_COLORS[0])) - 1);
}

static void sv_pipes_init(int cols, int rows)
{
	free(sv_pipe_grid);
	sv_pipe_w = cols > 0 ? cols : 1;
	sv_pipe_h = rows > 0 ? rows : 1;
	sv_pipe_grid = calloc((size_t)sv_pipe_w * (size_t)sv_pipe_h, 1);
	sv_pipe_used = 0;
	sv_pipe_glyphs();
	for (int i = 0; i < SV_PIPES; i++)
		sv_pipe_place(&sv_pipes[i], sv_pipe_w, sv_pipe_h);
}

static void sv_pipes_step(int cols, int rows)
{
	(void)cols;
	(void)rows;
	if (!sv_pipe_grid)
		return;

	/*
	 * THE SCREEN IS CLEARED WHEN IT FILLS, which is what the effect is
	 * FOR: pipes that never reset end as a solid rectangle, and a
	 * screensaver whose steady state is a filled screen is a bright
	 * rectangle left on for the night.
	 */
	if (sv_pipe_used * 2 > sv_pipe_w * sv_pipe_h) {
		memset(sv_pipe_grid, 0,
		       (size_t)sv_pipe_w * (size_t)sv_pipe_h);
		sv_pipe_used = 0;
		for (int i = 0; i < SV_PIPES; i++)
			sv_pipe_place(&sv_pipes[i], sv_pipe_w, sv_pipe_h);
		return;
	}

	for (int i = 0; i < SV_PIPES; i++) {
		struct sv_pipe *p = &sv_pipes[i];
		int in = p->dir, out = p->dir, piece;

		/* One turn in six, and never a reversal: a pipe that doubled
		 * back would draw over the cell it just left and the piece
		 * there would be a corner joining nothing. */
		if (sv_range(0, 5) == 0)
			out = sv_range(0, 1) ? (p->dir + 1) & 3
					     : (p->dir + 3) & 3;

		piece = SV_PIPE_PIECE[in][out];
		if (piece >= 0 && p->x >= 0 && p->x < sv_pipe_w &&
		    p->y >= 0 && p->y < sv_pipe_h) {
			unsigned char *cell =
				&sv_pipe_grid[p->y * sv_pipe_w + p->x];

			if (!*cell)
				sv_pipe_used++;
			*cell = (unsigned char)((piece + 1) |
						(p->color << 4));
		}

		p->dir = out;
		p->x += SV_DX[out];
		p->y += SV_DY[out];
		if (p->x < 0 || p->x >= sv_pipe_w ||
		    p->y < 0 || p->y >= sv_pipe_h)
			sv_pipe_place(p, sv_pipe_w, sv_pipe_h);
	}
}

static void sv_pipes_draw(int cols, int rows)
{
	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);
	if (!sv_pipe_grid)
		return;
	for (int y = 0; y < rows && y < sv_pipe_h; y++)
		for (int x = 0; x < cols && x < sv_pipe_w; x++) {
			unsigned char c = sv_pipe_grid[y * sv_pipe_w + x];

			if (!(c & 0x0f))
				continue;
			ktui_draw_cell(x, y,
				       sv_cp(sv_pipe_g[(c & 0x0f) - 1]),
				       SV_PIPE_COLORS[(c >> 4) & 3], KT_BG,
				       KT_A_NONE);
		}
}

/* ── starfield ───────────────────────────────────────────────────────────
 *
 * Stars at a distance, flying past. Position is fixed-point around the centre
 * and the depth divides it, which is the whole of the perspective: a star at
 * z = 1000 sits two cells from the middle and the same star at z = 50 is at
 * the edge, so a constant step in z is an accelerating step on the screen.
 *
 * THE THREE MARKS ARE ASCII on purpose. Depth here is carried by the colour
 * ladder and by the mark getting heavier, and three ASCII characters say that
 * on every screen this can run on — so unlike the density effects there is no
 * font question to answer.
 *
 * A CELL IS TWICE AS TALL AS IT IS WIDE, so the vertical spread is halved.
 * Without that the field is an ellipse standing on end, which reads as a
 * tunnel rather than as stars.
 */
#define SV_STARS 240
#define SV_STAR_NEAR 50
#define SV_STAR_FAR 1000
#define SV_STAR_SPREAD 2000

static const char SV_STAR_MARK[3] = { '.', '+', '*' };
static const int SV_STAR_COLOR[3] = { KT_DIM, KT_MID, KT_TEXT };

struct sv_star {
	int x, y, z;
};

static struct sv_star sv_stars[SV_STARS];

static void sv_star_spawn(struct sv_star *s, int fresh)
{
	s->x = sv_range(-SV_STAR_SPREAD, SV_STAR_SPREAD);
	s->y = sv_range(-SV_STAR_SPREAD, SV_STAR_SPREAD);
	/* A fresh field is scattered through the whole depth; a replacement
	 * comes in at the back, or every star would arrive at once and the
	 * field would pulse. */
	s->z = fresh ? sv_range(SV_STAR_NEAR, SV_STAR_FAR) : SV_STAR_FAR;
}

static void sv_stars_init(int cols, int rows)
{
	(void)cols;
	(void)rows;
	for (int i = 0; i < SV_STARS; i++)
		sv_star_spawn(&sv_stars[i], 1);
}

static void sv_stars_step(int cols, int rows)
{
	(void)cols;
	(void)rows;
	for (int i = 0; i < SV_STARS; i++) {
		sv_stars[i].z -= 14;
		if (sv_stars[i].z < SV_STAR_NEAR)
			sv_star_spawn(&sv_stars[i], 0);
	}
}

static void sv_stars_draw(int cols, int rows)
{
	int cx = cols / 2, cy = rows / 2;

	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);
	for (int i = 0; i < SV_STARS; i++) {
		const struct sv_star *s = &sv_stars[i];
		int px, py, lvl;

		if (s->z < 1)
			continue;
		px = cx + s->x / s->z;
		py = cy + (s->y / s->z) / 2;
		if (px < 0 || px >= cols || py < 0 || py >= rows)
			continue;
		lvl = s->z > 600 ? 0 : (s->z > 300 ? 1 : 2);
		ktui_draw_cell(px, py, (uint32_t)SV_STAR_MARK[lvl],
			       SV_STAR_COLOR[lvl], KT_BG, KT_A_NONE);
	}
}

/* ── fire ────────────────────────────────────────────────────────────────
 *
 * The demo effect: a hot bottom row, and every cell above it the average of
 * the three below plus a decay. What that produces is heat rising and cooling,
 * with the decay deciding how tall the flame stands.
 *
 * FOUR SLOTS AND THREE DENSITIES, which is the ladder this palette can spell:
 * dim, urgent, secondary, text — cold through hot — crossed with ░ ▒ █. A
 * fourth density does not exist on the console font, so the ladder is climbed
 * with colour where it cannot be climbed with glyphs.
 *
 * The heat plane is malloc'd for the same reason the pipe grid is.
 */
static unsigned char *sv_heat;
static int sv_heat_w, sv_heat_h;

static void sv_fire_init(int cols, int rows)
{
	free(sv_heat);
	sv_heat_w = cols > 0 ? cols : 1;
	sv_heat_h = rows > 0 ? rows : 1;
	sv_heat = calloc((size_t)sv_heat_w * (size_t)sv_heat_h, 1);
	if (!sv_heat)
		return;
	for (int x = 0; x < sv_heat_w; x++)
		sv_heat[(sv_heat_h - 1) * sv_heat_w + x] = 255;
}

static void sv_fire_step(int cols, int rows)
{
	(void)cols;
	(void)rows;
	if (!sv_heat)
		return;

	for (int y = 0; y < sv_heat_h - 1; y++)
		for (int x = 0; x < sv_heat_w; x++) {
			int l = x > 0 ? x - 1 : 0;
			int r = x + 1 < sv_heat_w ? x + 1 : sv_heat_w - 1;
			int b2 = y + 2 < sv_heat_h ? y + 2 : sv_heat_h - 1;
			int sum = sv_heat[(y + 1) * sv_heat_w + l] +
				  sv_heat[(y + 1) * sv_heat_w + x] +
				  sv_heat[(y + 1) * sv_heat_w + r] +
				  sv_heat[b2 * sv_heat_w + x];
			/*
			 * NINE FORTIETHS AND NOT A QUARTER. A plain average
			 * of the cells below loses nothing on the way up —
			 * four cells at full heat average to full heat — so
			 * the flame reaches the top of the screen and the
			 * effect is a lit rectangle. The tenth taken off each
			 * row is what gives it a height: it fades out over
			 * about twenty rows, which is a flame on a screen
			 * this tall.
			 */
			int v = sum * 9 / 40;
			int decay = sv_range(0, 3);

			sv_heat[y * sv_heat_w + x] =
				(unsigned char)(v > decay ? v - decay : 0);
		}

	/* The source row, re-lit each frame with cold spots in it: a solid
	 * source burns as a flat wall, and the flicker is where the shape
	 * comes from. */
	for (int x = 0; x < sv_heat_w; x++)
		sv_heat[(sv_heat_h - 1) * sv_heat_w + x] =
			(unsigned char)(sv_range(0, 9) ? 255 : 40);
}

static void sv_fire_draw(int cols, int rows)
{
	ktui_draw_fill(krect(0, 0, cols, rows), KT_BG);
	if (!sv_heat)
		return;
	for (int y = 0; y < rows && y < sv_heat_h; y++)
		for (int x = 0; x < cols && x < sv_heat_w; x++) {
			int h = sv_heat[y * sv_heat_w + x];
			int lvl, fg;

			if (h < 32)
				continue;
			if (h < 96) {
				lvl = 0;
				fg = KT_DIM;
			} else if (h < 160) {
				lvl = 1;
				fg = KT_ERR;
			} else if (h < 216) {
				lvl = 2;
				fg = KT_WARN;
			} else {
				lvl = 2;
				fg = KT_TEXT;
			}
			ktui_draw_cell(x, y, sv_cp(sv_ramp[lvl]), fg, KT_BG,
				       KT_A_NONE);
		}
}

/* ── clock ───────────────────────────────────────────────────────────────
 *
 * THE ART, WITH THE TIME AS THE GRID. The drift, the bounce, the colour change
 * on each edge and the pinning when the picture is bigger than the screen are
 * already written and are what `art` is; a clock that reimplemented them would
 * be a second copy to keep in step. So this replaces the LOADER and reuses the
 * transform.
 *
 * DRAWN OUT OF sv_ramp[2] AND NOT A LITERAL BLOCK. The grid goes through
 * ktui_draw_text, which turns anything above ASCII into `?` on a terminal with
 * no UTF-8 — so a hard-coded █ here would be a screen of question marks on
 * exactly the terminals the tier exists for.
 *
 * THE WALL CLOCK IS sh_wall()'s, so a dump can be goldened: the frame is
 * otherwise a different picture every minute, which is a golden that fails an
 * hour after it is written.
 */
static const char *const SV_DIGIT[11][5] = {
	{ "###", "# #", "# #", "# #", "###" },	/* 0 */
	{ " # ", "## ", " # ", " # ", "###" },	/* 1 */
	{ "###", "  #", "###", "#  ", "###" },	/* 2 */
	{ "###", "  #", "###", "  #", "###" },	/* 3 */
	{ "# #", "# #", "###", "  #", "  #" },	/* 4 */
	{ "###", "#  ", "###", "  #", "###" },	/* 5 */
	{ "###", "#  ", "###", "# #", "###" },	/* 6 */
	{ "###", "  #", "  #", "  #", "  #" },	/* 7 */
	{ "###", "# #", "###", "# #", "###" },	/* 8 */
	{ "###", "# #", "###", "  #", "###" },	/* 9 */
	{ "   ", " # ", "   ", " # ", "   " },	/* : */
};

#define SV_CLOCK_GLYPHS 5	/* HH:MM */
#define SV_CLOCK_SRC_W (SV_CLOCK_GLYPHS * 3 + (SV_CLOCK_GLYPHS - 1))
#define SV_CLOCK_SRC_H 5

static int sv_clock_min = -1;

/*
 * Render the time into sv_art at the largest scale the screen holds. The
 * vertical scale is half the horizontal because a cell is about twice as tall
 * as it is wide, and digits scaled equally in both come out as columns.
 */
static void sv_clock_render(int cols, int rows)
{
	time_t now = sh_wall();
	struct tm tm;
	int digits[SV_CLOCK_GLYPHS];
	int hs, vs;

	if (!localtime_r(&now, &tm))
		return;
	sv_clock_min = tm.tm_hour * 60 + tm.tm_min;
	digits[0] = tm.tm_hour / 10;
	digits[1] = tm.tm_hour % 10;
	digits[2] = 10;			/* the colon */
	digits[3] = tm.tm_min / 10;
	digits[4] = tm.tm_min % 10;

	hs = (cols - 4) / SV_CLOCK_SRC_W;
	vs = (rows - 2) / SV_CLOCK_SRC_H;
	if (vs > hs / 2)
		vs = hs / 2;
	if (hs < 1)
		hs = 1;
	if (vs < 1)
		vs = 1;
	if (SV_CLOCK_SRC_H * vs > SV_ART_LINES)
		vs = SV_ART_LINES / SV_CLOCK_SRC_H;
	if (vs < 1)
		vs = 1;

	sv_art_n = 0;
	sv_art_w = 0;
	for (int sy = 0; sy < SV_CLOCK_SRC_H; sy++) {
		char line[SV_ART_BYTES];
		size_t used = 0;

		line[0] = '\0';
		for (int g = 0; g < SV_CLOCK_GLYPHS; g++) {
			const char *row = SV_DIGIT[digits[g]][sy];

			for (int sx = 0; sx < 3; sx++)
				for (int k = 0; k < hs; k++) {
					const char *cell = row[sx] == '#'
						? sv_ramp[2] : " ";
					size_t n = strlen(cell);

					if (used + n + 1 >= sizeof(line))
						break;
					memcpy(line + used, cell, n);
					used += n;
				}
			/* The gap between glyphs, at the same scale, so the
			 * digits do not run into one another as the scale
			 * rises. */
			for (int k = 0; g + 1 < SV_CLOCK_GLYPHS && k < hs; k++)
				if (used + 1 < sizeof(line))
					line[used++] = ' ';
		}
		line[used] = '\0';

		for (int k = 0; k < vs && sv_art_n < SV_ART_LINES; k++) {
			snprintf(sv_art[sv_art_n], SV_ART_BYTES, "%s", line);
			sv_art_n++;
		}
		int w = ktui_utf8_width(line);

		if (w > sv_art_w)
			sv_art_w = w;
	}
}

static int sv_clock_load(void)
{
	/* Nothing to open: unlike `art`, the grid is generated. The hook
	 * exists so the table has one shape. */
	sv_clock_min = -1;
	return 0;
}

static void sv_clock_init(int cols, int rows)
{
	sv_ramp_init();
	sv_clock_render(cols, rows);
	sv_art_init(cols, rows);
}

static void sv_clock_step(int cols, int rows)
{
	time_t now = sh_wall();
	struct tm tm;

	/* REGENERATED ON THE MINUTE AND NOT EVERY FRAME. Rendering the grid
	 * fifteen times a second would be fifteen times the work for a picture
	 * that changes once a minute — and the bounce would restart from the
	 * new grid's size each time. */
	if (localtime_r(&now, &tm) &&
	    tm.tm_hour * 60 + tm.tm_min != sv_clock_min)
		sv_clock_render(cols, rows);
	sv_art_step(cols, rows);
}

/* ── the table ───────────────────────────────────────────────────────────
 *
 * ONE ROW PER MODE, because eight modes reached through an if/else chain is
 * four chains in the loop below that have to agree with one another — the
 * init, the draw, the resize and the step. A row is the only place a mode is
 * named.
 */
struct sv_effect {
	const char *name;
	int (*load)(void);		/* NULL when nothing has to be read */
	void (*init)(int cols, int rows);
	void (*step)(int cols, int rows);
	void (*draw)(int cols, int rows);
};

static const struct sv_effect sv_effects[] = {
	{ "rain",	NULL,		sv_rain_init,	sv_rain_step,
	  sv_rain_draw },
	{ "art",	sv_art_load,	sv_art_init,	sv_art_step,
	  sv_art_draw },
	/*
	 * `bounce` IS `art`, AND IS A NAME RATHER THAN A SIXTH EFFECT. The art
	 * bouncing off the edges of the screen is exactly what `art` does, and
	 * a second implementation of it would be a copy that goes stale the
	 * first time either is tuned. The name is here because it is what the
	 * effect is called.
	 */
	{ "bounce",	sv_art_load,	sv_art_init,	sv_art_step,
	  sv_art_draw },
	{ "matrix",	NULL,		sv_rain_init,	sv_rain_step,
	  sv_matrix_draw },
	{ "pipes",	NULL,		sv_pipes_init,	sv_pipes_step,
	  sv_pipes_draw },
	{ "starfield",	NULL,		sv_stars_init,	sv_stars_step,
	  sv_stars_draw },
	{ "fire",	NULL,		sv_fire_init,	sv_fire_step,
	  sv_fire_draw },
	{ "clock",	sv_clock_load,	sv_clock_init,	sv_clock_step,
	  sv_art_draw },
};
#define SV_NEFFECT ((int)(sizeof(sv_effects) / sizeof(sv_effects[0])))

/* The name a person wrote, as an index. `off` and `random` are not effects and
 * are answered by their own sentinels. */
static int sv_mode_of(const char *name)
{
	if (!strcmp(name, "off"))
		return SV_MODE_OFF;
	if (!strcmp(name, "random"))
		return SV_MODE_RANDOM;
	for (int i = 0; i < SV_NEFFECT; i++)
		if (!strcmp(sv_effects[i].name, name))
			return i;
	return SV_MODE_UNSET;
}


/* ── main ──────────────────────────────────────────────────────────────── */

static int sv_usage(void)
{
	fprintf(stderr, "usage: kdos-saver [--mode NAME] [--fps N] "
			"[--output NAME]\n"
			"                  [--font NAME] [--dump]\n"
			"modes: off, random");
	for (int i = 0; i < SV_NEFFECT; i++)
		fprintf(stderr, ", %s", sv_effects[i].name);
	fputc('\n', stderr);
	return 2;
}

int saver_main(int argc, char **argv)
{
	const char *font = NULL, *output = NULL;
	int mode = SV_MODE_UNSET, fps = SV_FPS_DEF, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--output") && i + 1 < argc) {
			output = argv[++i];
		} else if (!strcmp(argv[i], "--fps") && i + 1 < argc) {
			fps = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
			mode = sv_mode_of(argv[++i]);
			if (mode == SV_MODE_UNSET)
				return sv_usage();
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			return sv_usage();
		}
	}

	/*
	 * THE FLAG BEATS THE FILE, and that is load-bearing rather than a
	 * convention: `kcon_conf` reads /etc/kdos/con.conf unconditionally,
	 * before any XDG path and with nothing able to shadow it, so a machine
	 * with KDOS installed would otherwise decide what a `--mode art` dump
	 * draws. A golden that depended on the developer's own /etc is a
	 * golden that passes on one machine.
	 *
	 * An unknown name in the file falls back to `art` rather than
	 * refusing: a screensaver that would not start because a configuration
	 * key was misspelled is a black screen with no explanation on it.
	 */
	if (mode == SV_MODE_UNSET) {
		mode = sv_mode_of(kcon_conf_str("saver_mode", "art"));
		if (mode == SV_MODE_UNSET)
			mode = sv_mode_of("art");
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
		/*
		 * EIGHTY BY TWENTY-FOUR AND NOTHING ELSE. The harness resizes
		 * the cell buffer from $KDOS_DUMP_SIZE, but every effect here
		 * is placed against this size — a bounce computes its bounds
		 * from it and the clock its scale — so a golden at another
		 * size is a picture positioned for a screen it is not on, and
		 * it fails by looking plausible.
		 */
		int cols = 80, rows = 24;
		const struct sv_effect *e;

		sv_seed = 20260814u;
		sv_frame = 0;
		ktui_offscreen_init(cols, rows);
		sv_ramp_init();
		if (mode == SV_MODE_RANDOM)
			mode = (int)(sv_rand() % (uint32_t)SV_NEFFECT);
		e = &sv_effects[mode];
		if (e->load && e->load() != 0)
			e = &sv_effects[0];	/* no art is not no picture */
		e->init(cols, rows);
		/*
		 * A SETTLED FRAME AND NOT THE FIRST ONE. Half these effects
		 * have nothing on the screen at frame zero — a pipe has drawn
		 * no cell yet and a fire is one hot row — so a golden of the
		 * first frame would be a golden of an empty rectangle, which
		 * passes whatever the effect later does. Stepping a fixed
		 * count from a fixed seed is still one picture, and it is the
		 * picture a person would see.
		 */
		for (int f = 0; f < SV_DUMP_FRAMES; f++) {
			e->step(cols, rows);
			sv_frame++;
		}
		e->draw(cols, rows);
		ktui_draw_dump();
		return 0;
	}

	sv_seed = (uint32_t)(time(NULL) ^ (long)getpid());
	if (!sv_seed)
		sv_seed = 1;

	/*
	 * `random` IS RESOLVED HERE AND NOT AT PARSE TIME. sv_seed is a static
	 * initialised to 1 and is only seeded from the clock and the pid on
	 * the line above, so a pick made while the arguments were being read
	 * would draw the same "random" effect on every boot of every machine.
	 */
	if (mode == SV_MODE_RANDOM)
		mode = (int)(sv_rand() % (uint32_t)SV_NEFFECT);

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

	const struct sv_effect *e = &sv_effects[mode];

	if (e->load && e->load() != 0) {
		/* No art on this machine is not a reason to show nothing: the
		 * rain needs no data file. */
		fprintf(stderr, "kdos-saver: no %s; falling back to rain\n",
			SV_ART_PATH);
		e = &sv_effects[0];
	}

	int cw = ktui_w, ch = ktui_h;

	e->init(cw, ch);

	const int frame_ms = 1000 / fps;

	while (!kdisp_should_close()) {
		e->draw(cw, ch);
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
			e->init(cw, ch);
		}
		e->step(cw, ch);
		sv_frame++;
	}

	kdisp_shutdown();
	return 0;
}

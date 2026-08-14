/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * kdos-splash — the KDOS boot animation.
 *
 * Draws straight to /dev/fb0. It can do that because of a happy accident of
 * this system's console setup: the kernel is built with
 * CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER, so fbcon does not claim the
 * framebuffer until something actually prints to tty0 — and nothing does,
 * because `console=tty0 console=ttyS0` makes the LAST one /dev/console, so
 * every boot message goes to the serial port. The screen is therefore ours,
 * uncontested, from the moment devtmpfs is mounted until agetty starts.
 *
 * The animation is the window-open shader from the compositor, applied to the
 * whole machine: a flash, then the picture unfolding vertically from a
 * hairline as the deflection settles, scanline banding burning off as the tube
 * warms up. Boot then reports itself as POST lines. Quitting runs it backwards
 * — collapse to a line, pinch to a dot, decay.
 *
 * Every failure path here exits 0 without drawing. A boot animation that can
 * stop a boot is a bug, not a feature. Boot messages still go to the serial
 * console either way, so a machine that dies behind the splash is still
 * debuggable from there.
 *
 *   kdos-splash run                 draw, and read commands until told to quit
 *   kdos-splash step "PROBING..."   begin a stage (prints NAME ....... )
 *   kdos-splash ok | fail           close the current stage
 *   kdos-splash msg  "text"         a plain line
 *   kdos-splash total N             add N to the expected step count (the
 *                                   progress bar; senders add their share as
 *                                   soon as they know it — until the first
 *                                   total arrives the bar sweeps)
 *   kdos-splash quit                power-off animation, then exit
 *
 * Commands reach the running instance through a FIFO in /dev, which is the
 * one filesystem that survives switch_root intact (devtmpfs is moved, not
 * remounted), so a single splash process spans the initramfs and the real
 * root without a seam.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <linux/fb.h>

#include "penguin.h"

#define FIFO_PATH "/dev/.kdos-splash"

/* PHOSPHOR. Keep in sync with the desktop palette in accent.kdl. */
#define C_PHOS    0x39ff14
#define C_PHOSDIM 0x1f8f0c
#define C_DIM     0x12401f
#define C_AMBER   0xffb000
#define C_ALARM   0xff3131
#define C_TEXT    0xb8ffc8
#define C_DEEP    0x000a03
#define C_HOT     0xd8ffd8

/* How long the tube takes to warm up. Boot is ~10s; this is the part of it
 * anyone actually watches, so it gets a full second. */
#define INTRO_S 1.0

#define MAX_LINES 64
#define LINE_COLS 40   /* stage name is dotted out to this column */

enum { ST_PENDING = 0, ST_OK, ST_FAIL, ST_PLAIN };

struct line {
	char text[72];
	int  state;
};

/*
 * When an init script fails, `FAIL` on its own tells the user nothing — the
 * reason scrolled past on a console that fbcon has not taken over yet, so on
 * a framebuffer boot it is simply gone. rcS feeds the tail of the script's
 * output back as detail lines and they are painted under the stage list.
 *
 * Fedora's greenboot does health-check-and-rollback for servers, headlessly.
 * Nobody does it on the framebuffer in the boot idiom, which is the whole
 * point of having written our own splash.
 *
 * Retry and rollback are NOT here: retry needs an input path the splash does
 * not have (it never reads a key), and rollback needs the A/B slots that do
 * not exist yet. Showing the reason is the part that is buildable now, and it
 * is most of the value.
 */
#define MAX_DETAIL 10
#define DETAIL_COLS 68

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;
static unsigned char *fbmem;
static size_t fbsize;
static int fbw, fbh, fbbypp;
static int fbfd = -1;
static int ttyfd = -1;

static uint32_t *canvas;      /* what the screen would show, unmodulated */

static unsigned char *glyphs; /* PSF bitmap data */
static int gw, gh, gstride, gcount;

static struct line lines[MAX_LINES];
static int nlines;
static char detail[MAX_DETAIL][DETAIL_COLS + 1];
static int ndetail;
static int failed;
static int total_steps, done_steps;
/*
 * The total is additive and each boot phase reports its own share late, so
 * "all currently-known steps done" happens repeatedly at phase boundaries.
 * Until quit arrives the boot is by definition not finished — hold the bar
 * at 99% / one segment short so the user never watches a "100%" boot wait.
 */
static int finishing;

static volatile sig_atomic_t stop;

static void on_signal(int sig) { (void)sig; stop = 1; }

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ------------------------------------------------------------------ font */

/*
 * PSF1 and PSF2 both, because the console fonts KDOS ships are PSF1 (Terminus
 * 8x16) but nothing stops a later font from being PSF2.
 */
static int font_load(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;

	unsigned char hdr[32];
	if (fread(hdr, 1, 4, f) != 4) {
		fclose(f);
		return -1;
	}

	if (hdr[0] == 0x36 && hdr[1] == 0x04) {          /* PSF1 */
		gw = 8;
		gh = hdr[3];
		gcount = (hdr[2] & 0x01) ? 512 : 256;
		gstride = 1;
		fseek(f, 4, SEEK_SET);
	} else if (hdr[0] == 0x72 && hdr[1] == 0xb5 &&
		   hdr[2] == 0x4a && hdr[3] == 0x86) {   /* PSF2 */
		if (fread(hdr + 4, 1, 28, f) != 28) {
			fclose(f);
			return -1;
		}
		uint32_t headersize = *(uint32_t *)(hdr + 8);
		gcount  = *(uint32_t *)(hdr + 16);
		gh      = *(uint32_t *)(hdr + 24);
		gw      = *(uint32_t *)(hdr + 28);
		gstride = (gw + 7) / 8;
		fseek(f, headersize, SEEK_SET);
	} else {
		fclose(f);
		return -1;
	}

	if (gw <= 0 || gh <= 0 || gcount <= 0 || gh > 64 || gw > 32) {
		fclose(f);
		return -1;
	}

	size_t n = (size_t)gcount * gstride * gh;
	glyphs = malloc(n);
	if (!glyphs || fread(glyphs, 1, n, f) != n) {
		free(glyphs);
		glyphs = NULL;
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

/* ---------------------------------------------------------------- canvas */

static void px(int x, int y, uint32_t c)
{
	if (x < 0 || y < 0 || x >= fbw || y >= fbh)
		return;
	canvas[(size_t)y * fbw + x] = c;
}

static void fill(int x, int y, int w, int h, uint32_t c)
{
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			px(i, j, c);
}

static void draw_glyph(int x, int y, unsigned char ch, int scale, uint32_t c)
{
	if (!glyphs || ch >= gcount)
		return;
	const unsigned char *g = glyphs + (size_t)ch * gstride * gh;

	for (int row = 0; row < gh; row++) {
		for (int col = 0; col < gw; col++) {
			int byte = g[row * gstride + (col >> 3)];
			if (!(byte & (0x80 >> (col & 7))))
				continue;
			fill(x + col * scale, y + row * scale, scale, scale, c);
		}
	}
}

static int text_width(const char *s, int scale, int tracking)
{
	int n = (int)strlen(s);
	if (!n)
		return 0;
	return n * gw * scale + (n - 1) * tracking;
}

static void draw_text(int x, int y, const char *s, int scale, int tracking, uint32_t c)
{
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		draw_glyph(x, y, *p, scale, c);
		x += gw * scale + tracking;
	}
}

/* ----------------------------------------------------------------- frame */

static inline uint32_t mix_rgb(uint32_t a, uint32_t b, double t);

/*
 * The block-art wordmark, the same one every KDOS text file carries, encoded
 * with one ASCII token per character cell so the C string stays single-byte:
 *   '#' = █    '=' = ═    '|' = ║    '{' = ╔    '}' = ╗    '[' = ╚    ']' = ╝
 */
#define ART_ROWS 6
#define ART_COLS 33
static const char *art[ART_ROWS] = {
	"##}  ##}######}  ######} #######}",
	"##| ##{]##{==##}##{===##}##{====]",
	"#####{] ##|  ##|##|   ##|#######}",
	"##{=##} ##|  ##|##|   ##|[====##|",
	"##|  ##}######{][######{]#######|",
	"[=]  [=][=====]  [=====] [======]",
};

/* Mascot palette. Index 0 is transparent; order matches penguin.h. */
static const uint32_t peng_pal[6] = {
	0, 0x000000, 0xe8ffee, C_AMBER, C_PHOS, C_PHOSDIM
};

static int art_cw, art_ch, art_lt, art_x, art_y;
static int peng_scale, peng_x, peng_y;
static int sub_y, tag_y, rule_y, bar_y;
static int body_scale, body_x, body_y, body_w, body_step, body_max;

#define BAR_SEGS 26

/* One character cell of the wordmark: a solid block, or the box-drawing
 * borders reduced to single thick strokes. */
static void draw_art_cell(int x, int y, char c, uint32_t col)
{
	int mx = x + art_cw / 2 - art_lt / 2;
	int my = y + art_ch / 2 - art_lt / 2;

	switch (c) {
	case '#': fill(x, y, art_cw, art_ch, col); break;
	case '=': fill(x, my, art_cw, art_lt, col); break;
	case '|': fill(mx, y, art_lt, art_ch, col); break;
	case '{': fill(mx, my, x + art_cw - mx, art_lt, col);
		  fill(mx, my, art_lt, y + art_ch - my, col); break;
	case '}': fill(x, my, mx + art_lt - x, art_lt, col);
		  fill(mx, my, art_lt, y + art_ch - my, col); break;
	case '[': fill(mx, my, x + art_cw - mx, art_lt, col);
		  fill(mx, y, art_lt, my + art_lt - y, col); break;
	case ']': fill(x, my, mx + art_lt - x, art_lt, col);
		  fill(mx, y, art_lt, my + art_lt - y, col); break;
	default: break;
	}
}

static void draw_art(int x, int y, uint32_t bright, uint32_t dim)
{
	for (int j = 0; j < ART_ROWS; j++) {
		/* logo.txt renders the top half bold-green and the bottom half
		 * plain-green; keep that split. */
		uint32_t col = bright;
		if (dim != bright && j >= ART_ROWS / 2)
			col = mix_rgb(bright, dim, 0.35);
		for (int i = 0; i < ART_COLS; i++)
			draw_art_cell(x + i * art_cw, y + j * art_ch,
				      art[j][i], col);
	}
}

static void draw_penguin(int x, int y, int scale)
{
	int px2 = 0, py2 = 0;
	for (size_t i = 0; i + 1 < sizeof(penguin_rle); i += 2) {
		int idx = penguin_rle[i];
		int run = penguin_rle[i + 1];
		if (idx)
			fill(x + px2 * scale, y + py2 * scale,
			     run * scale, scale, peng_pal[idx]);
		px2 += run;
		if (px2 >= PENGUIN_W) {
			px2 = 0;
			py2++;
		}
	}
}

/*
 * Everything is stacked from the top down and measured in glyph cells, so the
 * whole screen scales with the framebuffer instead of assuming 1280x800.
 */
static void layout(void)
{
	body_scale = fbw / 640;
	if (body_scale < 1)
		body_scale = 1;
	if (body_scale > 3)
		body_scale = 3;

	int cell = gh * body_scale;   /* one line of body text */

	/* Wordmark cells: the mascot + wordmark group takes ~55% of the width. */
	art_cw = fbw / 85;
	if (art_cw < 4)
		art_cw = 4;
	if (art_cw > 60)
		art_cw = 60;
	art_ch = art_cw * 2;
	art_lt = art_cw / 3;
	if (art_lt < 1)
		art_lt = 1;

	int art_w = ART_COLS * art_cw;
	int art_h = ART_ROWS * art_ch;

	peng_scale = (art_h * 115 / 100 + PENGUIN_H / 2) / PENGUIN_H;
	if (peng_scale < 1)
		peng_scale = 1;
	int peng_w = PENGUIN_W * peng_scale;
	int peng_h = PENGUIN_H * peng_scale;
	int gap = art_cw * 2;

	int group_w = peng_w + gap + art_w;
	int group_h = peng_h > art_h ? peng_h : art_h;

	peng_x = (fbw - group_w) / 2;
	peng_y = fbh * 9 / 100;
	art_x  = peng_x + peng_w + gap;
	art_y  = peng_y + (group_h - art_h) / 2;

	int group_end = peng_y + group_h;
	sub_y  = group_end + cell;
	tag_y  = sub_y + cell * 2;
	rule_y = tag_y + cell * 2;

	body_step = cell + body_scale * 4;

	/*
	 * The status column is a fixed-width block — name dotted to LINE_COLS,
	 * a space, and up to "FAIL" — centered as a whole. Anchoring it at a
	 * screen percentage (the old way) only looked centered at 1280 wide.
	 */
	body_w = (LINE_COLS + 6) * gw * body_scale;
	body_x = (fbw - body_w) / 2;
	if (body_x < 0)
		body_x = 0;

	bar_y  = rule_y + body_step;
	body_y = bar_y + body_step + body_scale * 4;
	body_max = (fbh * 94 / 100 - body_y) / body_step;
	if (body_max > MAX_LINES)
		body_max = MAX_LINES;
	if (body_max < 1)
		body_max = 1;
}

/*
 * The retro progress bar: bracket caps, chunky segments, a percent readout.
 * With a known total it fills as stages close, the leading segment pulsing
 * amber; before any total arrives it plays a KITT sweep instead.
 */
static void draw_bar(int tick)
{
	int cell = gw * body_scale;
	int pct_w = 5 * cell;
	int seg_area_x = body_x + cell + body_scale * 2;
	int seg_area_w = body_w - cell * 2 - body_scale * 4 - pct_w;
	int gap = body_scale * 2;
	int seg_w = (seg_area_w - gap * (BAR_SEGS - 1)) / BAR_SEGS;
	int seg_h = gh * body_scale - body_scale * 6;
	int y = bar_y + body_scale * 3;

	if (seg_w < 2)
		return;

	draw_text(body_x - cell / 2, bar_y, "[", body_scale, 0, C_TEXT);
	draw_text(seg_area_x + BAR_SEGS * (seg_w + gap) - gap + body_scale * 2,
		  bar_y, "]", body_scale, 0, C_TEXT);

	int filled = -1, kitt = -1;
	if (total_steps > 0) {
		filled = done_steps * BAR_SEGS / total_steps;
		if (filled > BAR_SEGS)
			filled = BAR_SEGS;
		if (!finishing && filled >= BAR_SEGS)
			filled = BAR_SEGS - 1;
	} else {
		int span = 2 * (BAR_SEGS - 3);
		kitt = tick % span;
		if (kitt >= BAR_SEGS - 3)
			kitt = span - kitt;
	}

	for (int i = 0; i < BAR_SEGS; i++) {
		int x = seg_area_x + i * (seg_w + gap);
		uint32_t c = C_DIM;

		if (filled >= 0) {
			if (i < filled)
				c = C_PHOS;
			else if (i == filled && (done_steps < total_steps || !finishing))
				c = (tick & 2) ? C_AMBER : C_DIM;
		} else {
			int d = i - kitt;
			if (d < 0)
				d = -d;
			if (d == 0)
				c = C_PHOS;
			else if (d == 1)
				c = C_PHOSDIM;
			else if (d == 2)
				c = C_DIM;
			else
				c = mix_rgb(C_DEEP, C_DIM, 0.5);
		}
		fill(x, y, seg_w, seg_h, c);
	}

	char pct[16];
	if (total_steps > 0) {
		int p = done_steps * 100 / total_steps;
		if (p > 100)
			p = 100;
		if (!finishing && p > 99)
			p = 99;
		snprintf(pct, sizeof(pct), "%3d%%", p);
	} else {
		snprintf(pct, sizeof(pct), "BUSY");
	}
	draw_text(body_x + body_w - pct_w + cell, bar_y, pct, body_scale, 0,
		  total_steps > 0 ? C_PHOS : C_AMBER);
}

/* Compose the whole screen into the canvas. Effects come later, in present. */
static void compose(int tick)
{
	int cursor_on = (tick / 4) & 1;
	static const char spin[4] = { '|', '/', '-', '\\' };
	for (size_t i = 0; i < (size_t)fbw * fbh; i++)
		canvas[i] = C_DEEP;

	/*
	 * The block-art wordmark. Dim copies in four directions first: phosphor
	 * bleeds around a lit pixel, it does not cast a drop shadow to one side.
	 */
	int bleed = art_cw / 3;
	if (bleed < 1)
		bleed = 1;
	draw_art(art_x - bleed, art_y, C_PHOSDIM, C_PHOSDIM);
	draw_art(art_x + bleed, art_y, C_PHOSDIM, C_PHOSDIM);
	draw_art(art_x, art_y - bleed, C_PHOSDIM, C_PHOSDIM);
	draw_art(art_x, art_y + bleed, C_PHOSDIM, C_PHOSDIM);
	draw_art(art_x, art_y, C_PHOS, C_DIM);

	draw_penguin(peng_x, peng_y, peng_scale);

	const char *sub = "KD's Homebrew Linux Distro";
	draw_text((fbw - text_width(sub, body_scale, 0)) / 2, sub_y,
		  sub, body_scale, 0, C_TEXT);

	const char *tag = "musl . toybox . wlroots . no systemd";
	draw_text((fbw - text_width(tag, body_scale, 0)) / 2, tag_y,
		  tag, body_scale, 0, C_DIM);

	/* A rule between the identity and the machine talking. */
	fill(body_x, rule_y, body_w, body_scale, C_DIM);

	draw_bar(tick);

	/* Status lines, oldest first, scrolled to keep the newest visible. */
	int first = nlines > body_max ? nlines - body_max : 0;
	for (int i = first; i < nlines; i++) {
		int y = body_y + (i - first) * body_step;
		struct line *l = &lines[i];

		if (l->state == ST_PLAIN) {
			draw_text(body_x, y, l->text, body_scale, 0, C_TEXT);
			continue;
		}

		char buf[LINE_COLS + 8];
		int n = (int)strlen(l->text);
		if (n > LINE_COLS)
			n = LINE_COLS;
		memcpy(buf, l->text, n);
		buf[n] = ' ';
		for (int k = n + 1; k < LINE_COLS; k++)
			buf[k] = '.';
		buf[LINE_COLS] = '\0';
		draw_text(body_x, y, buf, body_scale, 0, C_TEXT);

		int sx = body_x + text_width(buf, body_scale, 0) + gw * body_scale;
		if (l->state == ST_OK) {
			draw_text(sx, y, "OK", body_scale, 0, C_PHOS);
		} else if (l->state == ST_FAIL) {
			draw_text(sx, y, "FAIL", body_scale, 0, C_ALARM);
		} else {
			/* A live stage gets a little rotor, not a mute block. */
			char s[2] = { spin[tick & 3], 0 };
			draw_text(sx, y, s, body_scale, 0, C_AMBER);
			if (cursor_on)
				fill(sx + gw * body_scale * 2, y,
				     gw * body_scale, gh * body_scale, C_PHOS);
		}
	}

	/*
	 * The failure panel. Drawn under the stage list, in alarm on the
	 * dimmest surface, at HALF the body scale — the point is to fit real
	 * log text on screen, and a boot log line is wider than a stage name.
	 *
	 * The stage list scrolls; this does not. Once something has failed the
	 * reason stays up for the rest of the boot, because the user's next
	 * move is to photograph it.
	 */
	if (failed && ndetail) {
		int ds = body_scale > 1 ? body_scale - 1 : 1;
		int dy = body_y + (nlines - first) * body_step + body_step;
		int lh = gh * ds + ds;

		draw_text(body_x, dy, "WHY:", ds, 0, C_ALARM);
		dy += lh;
		for (int i = 0; i < ndetail; i++, dy += lh) {
			if (dy + lh > fbh)
				break;	/* never scribble past the panel */
			draw_text(body_x + gw * ds * 2, dy, detail[i], ds, 0,
				  C_DIM);
		}
	}
}

/* --------------------------------------------------------------- present */

static inline uint32_t pack(uint32_t rgb)
{
	uint32_t r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
	return ((r >> (8 - vi.red.length)) << vi.red.offset) |
	       ((g >> (8 - vi.green.length)) << vi.green.offset) |
	       ((b >> (8 - vi.blue.length)) << vi.blue.offset);
}

static inline uint32_t scale_rgb(uint32_t c, double f)
{
	uint32_t r = (uint32_t)(((c >> 16) & 0xff) * f);
	uint32_t g = (uint32_t)(((c >> 8) & 0xff) * f);
	uint32_t b = (uint32_t)((c & 0xff) * f);
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return (r << 16) | (g << 8) | b;
}

static inline uint32_t mix_rgb(uint32_t a, uint32_t b, double t)
{
	double u = 1.0 - t;
	uint32_t r = (uint32_t)(((a >> 16) & 0xff) * u + ((b >> 16) & 0xff) * t);
	uint32_t g = (uint32_t)(((a >> 8) & 0xff) * u + ((b >> 8) & 0xff) * t);
	uint32_t bl = (uint32_t)((a & 0xff) * u + (b & 0xff) * t);
	return (r << 16) | (g << 8) | bl;
}

/*
 * Blit the canvas through the tube.
 *
 *   vopen  1.0 = full height, 0.0 = a hairline at the centre
 *   hopen  1.0 = full width,  0.0 = a dot
 *   flash  how much of the picture is washed out toward white-green
 *   gain   overall brightness (the decay at the end, and the idle flicker)
 *   hum    y position of the hum bar (the slow bright band real tubes get
 *          from ripple on the supply rail), or a large negative to disable
 */
static void present(double vopen, double hopen, double flash, double gain,
		    double hum)
{
	double cy = fbh / 2.0, cx = fbw / 2.0;
	if (vopen < 1e-4) vopen = 1e-4;
	if (hopen < 1e-4) hopen = 1e-4;

	double humw = fbh * 0.045 + 8.0;

	for (int y = 0; y < fbh; y++) {
		unsigned char *dst = fbmem + (size_t)y * fi.line_length;

		double dy = (y - cy) / vopen + cy;
		int sy = (int)dy;
		int inside = (sy >= 0 && sy < fbh);

		/* The beam edge: a hot seam sitting on the boundary of the unfold. */
		double edge = fabs(y - cy) / (fbh / 2.0);
		double seam = 0.0;
		if (vopen < 0.999) {
			double d = fabs(edge - vopen);
			if (d < 0.012)
				seam = 1.0 - d / 0.012;
		}

		/* Scanlines: every other row is dimmer. This is the whole look. */
		double scan = (y & 1) ? 0.78 : 1.0;

		double hd = fabs(y - hum);
		if (hd < humw)
			scan *= 1.0 + 0.10 * (1.0 - hd / humw);

		for (int x = 0; x < fbw; x++) {
			uint32_t c = C_DEEP;

			if (inside && hopen > 0.999) {
				c = canvas[(size_t)sy * fbw + x];
			} else if (inside) {
				double dxv = (x - cx) / hopen + cx;
				int sx = (int)dxv;
				if (sx >= 0 && sx < fbw)
					c = canvas[(size_t)sy * fbw + sx];
			}

			/*
			 * The wash only applies where the beam is actually
			 * painting. Outside the unfold the tube is dark, and
			 * flashing that area makes the whole screen look like
			 * fog instead of a picture opening up.
			 */
			if (inside) {
				if (flash > 0.0)
					c = mix_rgb(c, C_HOT, flash * 0.45);
				c = scale_rgb(c, scan * gain);
			} else {
				c = 0x000000;
			}
			if (seam > 0.0)
				c = mix_rgb(c, C_HOT, seam * 0.85);

			uint32_t v = pack(c);
			if (fbbypp == 4) {
				*(uint32_t *)(dst + (size_t)x * 4) = v;
			} else if (fbbypp == 2) {
				*(uint16_t *)(dst + (size_t)x * 2) = (uint16_t)v;
			} else {
				dst[x * 3 + 0] = v & 0xff;
				dst[x * 3 + 1] = (v >> 8) & 0xff;
				dst[x * 3 + 2] = (v >> 16) & 0xff;
			}
		}
	}
}

/* ------------------------------------------------------------------ init */

/*
 * Wake the console up, then tell it to sit still.
 *
 * The kernel is built with CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER, which
 * is the whole reason the framebuffer is free for us — but it also means the
 * DRM fbdev client has not done its modeset yet, so nothing we write to
 * /dev/fb0 is being scanned out. The display still shows whatever the firmware
 * and the bootloader left in the EFI framebuffer, and the splash paints
 * perfectly into a buffer nobody looks at.
 *
 * One byte to /dev/tty0 ends the deferral: fbcon takes over, the modeset runs,
 * and our buffer becomes the thing on screen. Clear it and hide the cursor in
 * the same write so fbcon does not leave a blinking block on top of the
 * animation.
 */
static void console_claim(void)
{
	ttyfd = open("/dev/tty0", O_RDWR | O_NOCTTY);
	if (ttyfd < 0)
		return;
	static const char seq[] = "\033[2J\033[H\033[?25l";
	(void)!write(ttyfd, seq, sizeof(seq) - 1);
}

static void console_release(void)
{
	if (ttyfd < 0)
		return;
	static const char seq[] = "\033[?25h";
	(void)!write(ttyfd, seq, sizeof(seq) - 1);
	close(ttyfd);
	ttyfd = -1;
}

static int fb_open(void)
{
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0)
		return -1;
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vi) < 0 ||
	    ioctl(fbfd, FBIOGET_FSCREENINFO, &fi) < 0)
		return -1;

	fbw = vi.xres;
	fbh = vi.yres;
	fbbypp = vi.bits_per_pixel / 8;
	if (fbbypp < 2 || fbbypp > 4 || fbw <= 0 || fbh <= 0)
		return -1;

	fbsize = (size_t)fi.line_length * fbh;
	fbmem = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fbmem == MAP_FAILED) {
		fbmem = NULL;
		return -1;
	}

	canvas = malloc(sizeof(uint32_t) * (size_t)fbw * fbh);
	return canvas ? 0 : -1;
}

/*
 * Push the frame out. Writes through the mmap are only flushed to the real
 * scanout when the fbdev's deferred-I/O worker gets round to it; panning to
 * the offset we are already at forces that to happen now, so the animation
 * runs at the rate we drew it rather than at the rate the kernel felt like.
 */
static void fb_flush(void)
{
	if (fbfd < 0)
		return;
	vi.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
	ioctl(fbfd, FBIOPAN_DISPLAY, &vi);
}

static void add_line(const char *text, int state)
{
	if (nlines >= MAX_LINES) {
		memmove(&lines[0], &lines[1], sizeof(lines[0]) * (MAX_LINES - 1));
		nlines = MAX_LINES - 1;
	}
	snprintf(lines[nlines].text, sizeof(lines[nlines].text), "%s", text);
	lines[nlines].state = state;
	nlines++;
}

static void close_line(int state)
{
	for (int i = nlines - 1; i >= 0; i--) {
		if (lines[i].state == ST_PENDING) {
			lines[i].state = state;
			done_steps++;
			return;
		}
	}
}

/* Returns 1 when told to quit. */
static int handle(const char *cmd)
{
	switch (cmd[0]) {
	case 'S': add_line(cmd + 1, ST_PENDING); break;
	case 'M': add_line(cmd + 1, ST_PLAIN); break;
	case 'O': close_line(ST_OK); break;
	case 'F': close_line(ST_FAIL); break;
	case 'D':
		/* Detail lines accumulate; the newest MAX_DETAIL are kept, so
		 * a script that fails after pages of output still shows the
		 * end, which is where the error is. */
		if (ndetail == MAX_DETAIL) {
			memmove(detail[0], detail[1],
				sizeof(detail) - sizeof(detail[0]));
			ndetail--;
		}
		snprintf(detail[ndetail], sizeof(detail[0]), "%s", cmd + 1);
		ndetail++;
		failed = 1;
		break;
	case 'T': total_steps += atoi(cmd + 1); break;
	case 'Q': finishing = 1; return 1;
	default: break;
	}
	return 0;
}

static int run(void)
{
	if (fb_open() < 0)
		return 0;   /* fail open: no framebuffer, no splash, no fuss */

	console_claim();

	if (font_load("/usr/share/kdos/splash.psf") < 0 &&
	    font_load("/usr/share/consolefonts/ter-v16n.psf") < 0)
		return 0;

	layout();

	unlink(FIFO_PATH);
	if (mkfifo(FIFO_PATH, 0600) < 0 && errno != EEXIST)
		return 0;

	/*
	 * O_RDWR, not O_RDONLY: a FIFO with no writer returns EOF on every read
	 * the moment a client closes it, and we would spin. Holding a write end
	 * ourselves means the pipe never reaches that state.
	 */
	int fifo = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
	if (fifo < 0)
		return 0;

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	double t0 = now_s();
	char buf[512];
	size_t used = 0;
	int quit = 0;

	/* Power-on, then a steady picture until someone says stop. */
	while (!stop) {
		double t = now_s() - t0;
		int tick = (int)(t * 10.0);

		double vopen = 1.0, flash = 0.0, gain = 1.0, hum = -1e6;
		if (t < INTRO_S) {
			double p = t / INTRO_S;
			double u = p / 0.55;
			if (u > 1.0) u = 1.0;
			vopen = 1.0 - pow(1.0 - u, 3.0);      /* ease out */
			double f = 1.0 - p / 0.6;
			flash = f > 0.0 ? f * f : 0.0;
		} else if (!quit) {
			/* Idle: a slow, shallow brightness wander. Tubes breathe. */
			gain = 0.97 + 0.03 * sin(t * 2.1);
			/* And the hum bar drifts down the raster, forever. */
			hum = fmod(t * fbh * 0.12, fbh * 1.3) - fbh * 0.15;
		}

		compose(tick);
		present(vopen, 1.0, flash, gain, hum);
		fb_flush();

		if (quit)
			break;

		struct pollfd pfd = { .fd = fifo, .events = POLLIN };
		int frame_ms = (t < INTRO_S) ? 16 : 90;
		if (poll(&pfd, 1, frame_ms) > 0 && (pfd.revents & POLLIN)) {
			ssize_t n = read(fifo, buf + used, sizeof(buf) - used - 1);
			if (n > 0) {
				used += (size_t)n;
				buf[used] = '\0';
				char *line = buf, *nl;
				while ((nl = strchr(line, '\n'))) {
					*nl = '\0';
					if (handle(line))
						quit = 1;
					line = nl + 1;
				}
				used = strlen(line);
				memmove(buf, line, used + 1);
			}
		}
	}

	/*
	 * Power-off: fold to a hairline, pinch that to a dot, let it decay. The
	 * screen is left black so whatever takes the console next starts clean.
	 */
	double q0 = now_s();
	for (;;) {
		double p = (now_s() - q0) / 0.45;
		if (p >= 1.0)
			break;

		double fold = 1.0 - p / 0.55;
		if (fold < 0.0) fold = 0.0;
		fold = fold * fold;

		double pinch = 1.0 - (p - 0.45) / 0.40;
		if (pinch > 1.0) pinch = 1.0;
		if (pinch < 0.0) pinch = 0.0;

		double decay = 1.0 - (p - 0.80) / 0.20;
		if (decay > 1.0) decay = 1.0;
		if (decay < 0.0) decay = 0.0;

		compose(0);
		present(fold, pinch, p * 0.5, decay, -1e6);
		fb_flush();
		usleep(16000);
	}

	memset(fbmem, 0, fbsize);
	fb_flush();
	munmap(fbmem, fbsize);
	console_release();
	/*
	 * Best effort only: after switch_root this path resolves inside the old
	 * initramfs root, which no longer has the FIFO (or anything else). The
	 * `quit` client does the real cleanup from the new root.
	 */
	unlink(FIFO_PATH);
	return 0;
}

/* --------------------------------------------------------------- preview */

/*
 * Render one frame to a PPM instead of to the screen, so the look can be
 * iterated on without booting anything:
 *
 *   kdos-splash preview 1280x800 0.35 out.ppm
 *
 * The second argument is the intro progress in seconds; anything past the
 * intro gives the settled picture.
 */
static int preview(const char *geom, const char *at, const char *out)
{
	int w = 1280, h = 800;
	sscanf(geom, "%dx%d", &w, &h);
	double t = atof(at);

	fbw = w;
	fbh = h;
	fbbypp = 4;
	fi.line_length = (uint32_t)w * 4;
	vi.red.offset = 16;   vi.red.length = 8;
	vi.green.offset = 8;  vi.green.length = 8;
	vi.blue.offset = 0;   vi.blue.length = 8;

	fbsize = (size_t)fi.line_length * h;
	fbmem = calloc(1, fbsize);
	canvas = malloc(sizeof(uint32_t) * (size_t)w * h);
	if (!fbmem || !canvas)
		return 1;

	if (font_load("/usr/share/kdos/splash.psf") < 0 &&
	    font_load("/usr/share/consolefonts/ter-v16n.psf") < 0 &&
	    font_load("splash.psf") < 0) {
		fprintf(stderr, "preview: no font\n");
		return 1;
	}
	layout();

	total_steps = 9;
	add_line("PROBING BLOCK DEVICES", ST_OK);
	add_line("MOUNTING SYSTEM IMAGE", ST_OK);
	add_line("OVERLAY ROOT", ST_OK);
	add_line("SWITCHING ROOT", ST_OK);
	add_line("DEVICE MANAGER", ST_OK);
	done_steps = 5;

	/*
	 * The fixture carries a FAILURE, because the failure panel is the part
	 * of this screen nobody can see without one — and a panel that has
	 * never been looked at is a panel with a geometry bug in it. Same
	 * reason kdosbuild's preview fixture uses a nine-digit file count and
	 * a port name longer than any pane.
	 *
	 * The detail lines are deliberately at and over DETAIL_COLS: real log
	 * output does not respect a column budget.
	 */
	add_line("NETWORK MANAGER", ST_FAIL);
	done_steps++;
	failed = 1;
	static const char *FIX[] = {
		"[KDOS] Starting NetworkManager...",
		"<info>  NetworkManager (version 1.56.0) is starting...",
		"<error> [1765.4] bus-manager: could not get the system bus: "
		"Could not connect: No such file or directory",
		"<error> [1765.4] Failed to initialize: no D-Bus connection",
	};
	for (size_t i = 0; i < sizeof(FIX) / sizeof(FIX[0]); i++)
		snprintf(detail[ndetail++], sizeof(detail[0]), "%s", FIX[i]);

	add_line("STARTING NETWORK", ST_PENDING);

	double vopen = 1.0, flash = 0.0, gain = 1.0;
	if (t < INTRO_S) {
		double p = t / INTRO_S;
		double u = p / 0.55;
		if (u > 1.0) u = 1.0;
		vopen = 1.0 - pow(1.0 - u, 3.0);
		double f = 1.0 - p / 0.6;
		flash = f > 0.0 ? f * f : 0.0;
	}

	compose(4);
	present(vopen, 1.0, flash, gain, h * 0.62);

	FILE *f = fopen(out, "wb");
	if (!f)
		return 1;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (int y = 0; y < h; y++) {
		uint32_t *row = (uint32_t *)(fbmem + (size_t)y * fi.line_length);
		for (int x = 0; x < w; x++) {
			unsigned char rgb[3] = { (row[x] >> 16) & 0xff,
						 (row[x] >> 8) & 0xff,
						 row[x] & 0xff };
			fwrite(rgb, 1, 3, f);
		}
	}
	fclose(f);
	return 0;
}

/* ---------------------------------------------------------------- client */

static int say(const char *fmt, const char *arg, int wait_for_exit)
{
	int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
	if (fd < 0)
		return 0;   /* nothing listening; that is fine */
	char buf[256];
	int n = snprintf(buf, sizeof(buf), fmt, arg ? arg : "");
	if (n > 0)
		(void)!write(fd, buf, (size_t)n);
	close(fd);

	/*
	 * Quitting is synchronous. rcS runs `kdos-splash quit` and returns, and
	 * init starts agetty straight after; if we did not block here, agetty
	 * would print to tty1 while the power-off animation was still drawing,
	 * and the screen would end up as console text interleaved with splash
	 * pixels.
	 *
	 * The daemon is NOT able to announce its own exit by removing the FIFO.
	 * It started in the initramfs and was never chroot'ed, so after
	 * switch_root its idea of "/" is still the old initramfs root — whose
	 * /dev was moved away, and whose files switch_root has since deleted.
	 * Every path it resolves from then on points into that ghost. Its fds
	 * (framebuffer, FIFO, tty) keep working because they were opened before
	 * the move; anything by name does not.
	 *
	 * So test for the daemon rather than for the file: opening a FIFO for
	 * writing fails with ENXIO exactly when no reader is left. That answer
	 * does not depend on whose root is whose. The client is running in the
	 * real root, so it can do the cleanup the daemon cannot.
	 */
	if (wait_for_exit) {
		for (int i = 0; i < 200; i++) {
			int probe = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
			if (probe < 0)
				break;
			close(probe);
			usleep(10000);
		}
		unlink(FIFO_PATH);
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : "run";
	const char *arg = argc > 2 ? argv[2] : "";

	if (!strcmp(cmd, "run"))
		return run();
	if (!strcmp(cmd, "preview") && argc > 4)
		return preview(argv[2], argv[3], argv[4]);
	if (!strcmp(cmd, "step"))
		return say("S%s\n", arg, 0);
	if (!strcmp(cmd, "msg"))
		return say("M%s\n", arg, 0);
	if (!strcmp(cmd, "total"))
		return say("T%s\n", arg, 0);
	if (!strcmp(cmd, "ok"))
		return say("O%s\n", NULL, 0);
	if (!strcmp(cmd, "fail"))
		return say("F%s\n", NULL, 0);
	if (!strcmp(cmd, "detail"))
		return say("D%s\n", arg, 0);
	if (!strcmp(cmd, "quit"))
		return say("Q%s\n", NULL, 1);

	fprintf(stderr, "usage: kdos-splash {run|step TEXT|msg TEXT|ok|fail|detail TEXT|\n"
		"                    total N|quit}\n");
	return 1;
}

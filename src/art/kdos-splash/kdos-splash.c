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
 * Draws straight to /dev/fb0. The kernel is built with
 * CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER, so fbcon does not claim the
 * framebuffer until a glyph is printed on a virtual terminal. /dev/console is
 * the LAST console=: the serial port on the live entries (`console=tty0
 * console=ttyS0,115200n8`), tty0 on an installed machine, where the
 * initramfs's first echo ends the deferral. Either way the splash puts the
 * foreground VT in KD_GRAPHICS while it draws (console_claim), and a VT in
 * graphics mode draws no text, so no boot message lands on the picture; the
 * messages still go to the VT's buffer and show once it is back in text mode.
 *
 * TWO MOTIONS, ONE PICTURE. `kdos.splash=phosphor` on the kernel command line
 * is Phosphor mode's CRT, the power-on a window gets from the desktop's CRT
 * effect applied to the whole machine: a flash, then the picture unfolding
 * vertically from a hairline as the deflection settles, scanline banding and a
 * hum bar drifting down the raster; quitting runs it backwards — collapse to a
 * line, pinch to a dot, decay. Anything else is Modern: the same picture
 * fading in from black and back out to it, flat, with no scanlines. Boot
 * reports itself as POST lines either way. Quitting to a text console ends
 * on a full black frame; handing off to the display manager ends on the
 * settled picture, held until the compositor takes the screen (`handoff`).
 * Modern's first frame is black, and the CRT's is black but for the
 * hairline it unfolds from. The command line is the one place the choice
 * can come from: the power-on runs in the initramfs, before any root
 * filesystem with a setting in it is mounted.
 *
 * Every failure to reach the display or the running instance exits 0 without
 * drawing. A boot animation that can stop a boot is a bug, not a feature. The
 * exceptions are no part of a boot: a command line it does not understand (an
 * unknown command, or `preview` with fewer than three arguments) prints usage
 * and exits 1, and `preview` itself exits 1 when it cannot render. Boot
 * messages still go to /dev/console either way, and a boot the initramfs
 * cannot finish puts its reason and a shell on tty1, so a machine that dies
 * behind the splash is still debuggable.
 *
 *   kdos-splash run                 draw, and read commands until told to quit;
 *                                   on a command line without `quiet` it draws
 *                                   nothing and exits (read_mode)
 *   kdos-splash step "PROBING..."   begin a stage (prints NAME ....... )
 *   kdos-splash ok | fail | skip    close the current stage: it started, it
 *                                   failed, or it chose not to run (a service
 *                                   script that printed a `[SKIP]` line)
 *   kdos-splash msg  "text"         a plain line
 *   kdos-splash detail "text"       a detail line; the newest few are kept
 *   kdos-splash total N             add N to the expected step count (the
 *                                   progress bar; senders add their share as
 *                                   soon as they know it, and the bar never
 *                                   moves back — until the first total
 *                                   arrives it sweeps)
 *   kdos-splash accent NAME         switch the running splash's colour scheme
 *   kdos-splash quit                power-off animation, then exit; the console
 *                                   is put back in text mode with its cursor
 *                                   shown, and draws the boot's messages; the
 *                                   client does that too, so it holds when no
 *                                   splash answers (vt_text)
 *   kdos-splash handoff             draw the settled frame once more and stop
 *                                   reading commands; tty1 is left in graphics
 *                                   mode with that frame on it for the display
 *                                   manager's compositor, and the splash
 *                                   blanks the framebuffer and exits once
 *                                   that compositor's first frame is on the
 *                                   display (its CRTC scans out another
 *                                   framebuffer), or 5 s after it became DRM
 *                                   master (a pan refused with EBUSY) when no
 *                                   CRTC can be read, and says in the kernel
 *                                   log what ended the hold (hold_frame)
 *   kdos-splash preview WxH T out.ppm [modern|phosphor] [ACCENT]
 *                                   render the frame at T seconds to a file,
 *                                   with no framebuffer and no FIFO, in the
 *                                   motion named or the command line's, and
 *                                   in the scheme named or the default
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
#include <linux/kd.h>
#include <linux/vt.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#include "penguin.h"

#define FIFO_PATH "/dev/.kdos-splash"

/*
 * EVERY SCHEME, COMPILED IN — the header is included for its table and this
 * binary still links nothing. `kcolor.h` is macros, a struct and declarations;
 * expanding KCOL_SCHEMES here costs 252 bytes and no symbol from libkcolor.
 *
 * IT HAS TO BE COMPILED IN BECAUSE OF WHEN THIS RUNS. The splash is started by
 * the INITRAMFS, before any root filesystem is mounted, so there is no
 * `/etc/kdos/accent` to read at the moment it starts. It comes up in the
 * default and `rcS` tells it the machine's real accent over the FIFO once /etc
 * is readable — which is why these are variables and not constants.
 */
#include "kcolor.h"

#define SPL_SCHEME(id, lbl, tname, p, dm, sec, urg, dp, txt, var, pd, bd)     \
	{ #id, KCOL_HEX(p), KCOL_HEX(dm), KCOL_HEX(sec), KCOL_HEX(urg),       \
	  KCOL_HEX(dp), KCOL_HEX(txt), KCOL_HEX(pd) },

static const struct spl_scheme {
	const char *name;
	uint32_t primary, dim, secondary, urgent, deep, text, pdark;
} SPL_SCHEMES[] = { KCOL_SCHEMES(SPL_SCHEME) };

#define SPL_NSCHEME ((int)(sizeof(SPL_SCHEMES) / sizeof(SPL_SCHEMES[0])))

/* The accent in force, projected onto the names the drawing code uses. The two
 * derived values are kcol_mix results, not free-hand colours. */
static uint32_t C_PHOS, C_PHOSDIM, C_DIM, C_AMBER, C_ALARM, C_TEXT, C_DEEP;
static uint32_t C_TAG;		/* pdark, lifted to SPL_TAG_FLOOR on deep   */
static uint32_t C_HOT;		/* mix(text, white, 60) — the beam flash    */
static uint32_t C_GLOW;		/* mix(text, white, 68) — the mascot rim    */

/* kcol_mix's blend, term for term: the unsigned weighted sum floored once, so
 * a value derived here and one derived by the library are the same number in
 * either direction. A signed step would truncate toward zero and land one
 * higher on a falling channel (171 against 170 for 255 to 0 at 33). */
static uint32_t spl_mix(uint32_t a, uint32_t b, int pct)
{
	uint32_t out = 0;

	for (int sh = 16; sh >= 0; sh -= 8) {
		unsigned x = (a >> sh) & 0xff, y = (b >> sh) & 0xff;
		out |= ((x * (100 - (unsigned)pct) + y * (unsigned)pct) / 100) << sh;
	}
	return out;
}

/* WCAG relative luminance and contrast ratio, the measure kcol_contrast
 * takes, in floating point: the splash links nothing from libkcolor, and the
 * selftest checks every scheme's tagline with kcol_contrast itself. */
static double spl_lin(unsigned c)
{
	double v = c / 255.0;

	return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double spl_contrast(uint32_t a, uint32_t b)
{
	double la = 0.2126 * spl_lin((a >> 16) & 0xff) +
		    0.7152 * spl_lin((a >> 8) & 0xff) +
		    0.0722 * spl_lin(a & 0xff) + 0.05;
	double lb = 0.2126 * spl_lin((b >> 16) & 0xff) +
		    0.7152 * spl_lin((b >> 8) & 0xff) +
		    0.0722 * spl_lin(b & 0xff) + 0.05;

	return la > lb ? la / lb : lb / la;
}

/*
 * THE TAGLINE IS TEXT, SO IT HOLDS WCAG AA ON THE GROUND. It is the scheme's
 * `pdark`, the accent's dark tone, where that reaches the floor on `deep`; on
 * a scheme where it does not (Amber at 3.56:1, Ice at 4.42:1) it is moved
 * toward `primary` one percent at a time until it does, so it stays the
 * accent's hue and is only as much brighter as legibility needs. The floor
 * carries a margin over 4.5 so that kcol_contrast's table, which rounds each
 * channel, measures the same verdict.
 */
#define SPL_TAG_FLOOR 4.52

static uint32_t spl_tag(uint32_t pdark, uint32_t primary, uint32_t deep)
{
	uint32_t c = pdark;
	int pct = 0;

	while (pct < 100 && spl_contrast(c, deep) < SPL_TAG_FLOOR)
		c = spl_mix(pdark, primary, ++pct);
	return c;
}

static void spl_peng_pal(uint32_t glow, uint32_t amber, uint32_t phos,
			 uint32_t phosdim);

/* Unknown names leave the palette exactly as it was: a splash is not the place
 * to report a typo, and half-applying one would be worse than ignoring it. */
static int spl_accent(const char *name)
{
	for (int i = 0; i < SPL_NSCHEME; i++) {
		const struct spl_scheme *s = &SPL_SCHEMES[i];

		if (strcmp(s->name, name))
			continue;
		C_PHOS = s->primary;
		C_PHOSDIM = s->pdark;
		C_TAG = spl_tag(s->pdark, s->primary, s->deep);
		C_DIM = s->dim;
		C_AMBER = s->secondary;
		C_ALARM = s->urgent;
		C_TEXT = s->text;
		C_DEEP = s->deep;
		C_HOT = spl_mix(s->text, 0xffffff, 60);
		C_GLOW = spl_mix(s->text, 0xffffff, 68);
		/* peng_pal is declared below; filled through the forward
		 * declaration so the palette has exactly one writer. */
		spl_peng_pal(C_GLOW, C_AMBER, C_PHOS, C_PHOSDIM);
		return 1;
	}
	return 0;
}

/* How long the tube takes to warm up. Boot is ~10s; this is the part of it
 * anyone actually watches, so it gets a full second. Modern's fade from black
 * is half that: there is no tube to warm. */
#define INTRO_S 1.0
#define FADE_IN_S 0.5
/* Power-off, in either motion. */
#define OUTRO_S 0.45

/* The VT SDDM's first greeter takes (its build's SDDM_INITIAL_VT), and the one
 * kdos-dm falls back to a prompt on. */
#define DM_VT_NAME "tty1"
#define DM_VT "/dev/" DM_VT_NAME

#define MAX_LINES 64
#define LINE_COLS 40   /* stage name is dotted out to this column */

enum { ST_PENDING = 0, ST_OK, ST_FAIL, ST_SKIP, ST_PLAIN };

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
 * not have (it never reads a key), and rollback is the A/B attempt counter's,
 * which a boot that fails before it is confirmed spends. Showing the reason is
 * the splash's part, and it is most of the value.
 */
#define MAX_DETAIL 10
/* A detail line is kept whole up to the client's command buffer (`say`), and
 * cut to the panel's width only when it is drawn, so one line fits each
 * screen's panel rather than the narrowest's. */
#define DETAIL_COLS 254

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
static int ellipsis_glyph = -1; /* the font's U+2026, -1 when it has none */

static struct line lines[MAX_LINES];
static int nlines;
static char detail[MAX_DETAIL][DETAIL_COLS + 1];
static int ndetail;
static int failed;
/* The newest FAIL stage, kept by value: the line table drops its oldest entry
 * past MAX_LINES, and a boot has more stages than that, so an index into it
 * could be shifted away while the stage is still the reason on screen. */
static char fail_text[72];
static long fail_seq = -1;	/* its add_line sequence number, -1 for none */
static long lines_added;	/* every add_line ever, the table's sequence */
static int total_steps, done_steps;
/*
 * THE BAR NEVER MOVES BACK. The total is additive and each sender adds its
 * share late, so a bare done/total would fall every time a total arrives
 * after a stage has closed. The fraction shown is instead rebased on each
 * total: what was on screen becomes the floor (bar_base, at bar_done steps),
 * and the steps still to come fill bar_take of the rest of the current span.
 * The initramfs adds its stages a few at a time and never knows which batch
 * is its last, so each of its batches fills half of what is left of its
 * share and none of them can use it up; rcS counts its stages up front, so
 * its total fills the rest of the bar.
 *
 * THE TWO HALVES OF A BOOT HAVE FIXED SHARES. The initramfs fills the bar to
 * INITRD_SHARE and rcS fills the rest. Their stage counts are nothing like
 * their durations — the initramfs's handful are short, rcS's one per service
 * are most of the boot — so a shared count would spend most of the bar on the
 * first seconds. The first total sent from the real root (in_initrd, below)
 * moves the floor to INITRD_SHARE and the span's end to the whole bar.
 *
 * Only quit fills the bar: until it arrives the boot is by definition not
 * finished, and the bar holds at 99% / one segment short so the user never
 * watches a "100%" boot wait. A hand-off holds its frame at that hold, the
 * value KSplash starts its own bar from, so the bar never moves backwards.
 */
#define INITRD_SHARE 0.15
static double bar_base, bar_end = 1.0, bar_take = 1.0;
static int bar_done;
static int finishing;
/* Started in the initramfs, and switch_root has not happened yet: the
 * initramfs's /init is still reachable from this process's root. switch_root
 * deletes every file of the old root, so the first lookup that misses after
 * it is the boundary between the two halves (see say() on the ghost root). */
static int in_initrd;
static const char *initrd_probe = "/init";

/* Phosphor's CRT motion rather than Modern's flat one (the header). */
static int crt;

/* The two text lines under the wordmark are left out to make room for the
 * failure panel (layout()). */
static int compact;

/* The last stop is a display manager, not a text console (`handoff`). */
static int handing_off;
/* The devtmpfs /dev, and the kernel log, opened while their names still
 * resolve: after switch_root this process resolves no path. switch_root moves
 * that devtmpfs into the new root rather than remounting it, so a node opened
 * at devfd is the real root's (hold_frame reopens DM_VT through it), and
 * kmsgfd is where hold_frame says what ended the hold. */
static int devfd = -1;
static int kmsgfd = -1;

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
 * The glyph that maps U+2026 in a font's unicode table, or -1. The text the
 * splash draws is ASCII and indexes glyphs by byte, so the table is read for
 * this one character: PSF1's is little-endian 16-bit code points, each glyph's
 * ended by 0xFFFF; PSF2's is UTF-8, each glyph's ended by 0xFF. In both, a
 * marker (0xFFFE, 0xFE) starts the glyph's multi-character sequences, which
 * are no single character and are skipped.
 */
static int font_ellipsis(FILE *f, int psf2)
{
	int g = 0, seq = 0, c;
	unsigned char u[3];
	int nu = 0;

	while (g < gcount && (c = fgetc(f)) != EOF) {
		if (!psf2) {
			int hi = fgetc(f);
			if (hi == EOF)
				break;
			c |= hi << 8;
			if (c == 0xffff) {
				g++, seq = 0;
				continue;
			}
			if (c == 0xfffe)
				seq = 1;
			else if (!seq && c == 0x2026)
				return g;
			continue;
		}
		if (c == 0xff) {
			g++, seq = 0, nu = 0;
			continue;
		}
		if (c == 0xfe) {
			seq = 1, nu = 0;
			continue;
		}
		/* A lead byte restarts the character; U+2026 is E2 80 A6. */
		if ((c & 0xc0) != 0x80)
			nu = 0;
		if (nu < 3)
			u[nu++] = (unsigned char)c;
		if (!seq && nu == 3 && u[0] == 0xe2 && u[1] == 0x80 && u[2] == 0xa6)
			return g;
	}
	return -1;
}

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
	int psf2 = 0, table = 0;
	if (fread(hdr, 1, 4, f) != 4) {
		fclose(f);
		return -1;
	}

	if (hdr[0] == 0x36 && hdr[1] == 0x04) {          /* PSF1 */
		gw = 8;
		gh = hdr[3];
		gcount = (hdr[2] & 0x01) ? 512 : 256;
		gstride = 1;
		table = hdr[2] & 0x02;
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
		psf2 = 1;
		table = *(uint32_t *)(hdr + 12) & 0x01;
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
	ellipsis_glyph = table ? font_ellipsis(f, psf2) : -1;
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

static void draw_glyph(int x, int y, int ch, int scale, uint32_t c)
{
	if (!glyphs || ch < 0 || ch >= gcount)
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

/*
 * A LINE WIDER THAN ITS ROOM ENDS IN AN ELLIPSIS. Up to `cols` cells of `s`
 * are drawn; a longer line is cut at the last space that leaves room for the
 * marker, its trailing spaces dropped, and the marker drawn after it: the
 * font's U+2026 in one cell, or "..." in three when the font has none. A
 * line with no space early enough (one long path, say) is cut at the room
 * itself. Without the marker a cut line reads as the whole message.
 */
static void draw_fitted(int x, int y, const char *s, int cols, int scale,
			uint32_t c)
{
	int n = (int)strlen(s);
	int step = gw * scale;

	if (n <= cols) {
		draw_text(x, y, s, scale, 0, c);
		return;
	}
	int mark = ellipsis_glyph >= 0 ? 1 : 3;
	int room = cols - mark;
	if (room < 0)
		room = 0;
	int cut = room;
	while (cut > 0 && s[cut] != ' ')
		cut--;
	while (cut > 0 && s[cut - 1] == ' ')
		cut--;
	if (!cut)
		cut = room;
	for (int i = 0; i < cut; i++, x += step)
		draw_glyph(x, y, (unsigned char)s[i], scale, c);
	if (ellipsis_glyph >= 0)
		draw_glyph(x, y, ellipsis_glyph, scale, c);
	else
		draw_text(x, y, "...", scale, 0, c);
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

/*
 * Mascot palette. Index 0 is transparent and index 1 is the mascot's own black;
 * the rest follow the accent, so it is FILLED by spl_accent() rather than
 * initialised — an accent arriving mid-boot has to move the penguin too, or it
 * is the one thing on the screen still wearing the scheme the machine booted
 * in. Order matches penguin.h.
 */
static uint32_t peng_pal[6];

static void spl_peng_pal(uint32_t glow, uint32_t amber, uint32_t phos,
			 uint32_t phosdim)
{
	peng_pal[0] = 0;
	peng_pal[1] = 0x000000;
	peng_pal[2] = glow;
	peng_pal[3] = amber;
	peng_pal[4] = phos;
	peng_pal[5] = phosdim;
}

static int art_cw, art_ch, art_lt, art_x, art_y;
static int peng_scale, peng_x, peng_y;
static int sub_y, tag_y, ver_y, rule_y, bar_y;
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
 * THE RELEASE IS NAMED BEFORE THE DESKTOP, in the one form every other surface
 * gives it (`kdos version`, the boot menu, the greeter, the installer, What's
 * New): /etc/kdos-release's VERSION, which 045_identity.sh writes as `0.2.1`
 * on a release build and `0.2.1+<BUILD_ID>` otherwise. A root with no record
 * gives os-release's VERSION_ID, with `+BUILD_ID` when it has one. The files
 * are read at start out of the initramfs's /etc, where 090_initramfs.sh
 * copies the image's; neither file there means no release, and the identity
 * line is then the name alone.
 *
 * THE ROOT THAT BOOTS HAS THE LAST WORD. The initramfs's copy is the release
 * of the image that built it, and an installed machine keeps that initramfs
 * across every update that is not a slot image, so it can name an older
 * release than the root it boots. Once the root is mounted at /newroot, the
 * initramfs runs `KDOS_SPLASH_ROOT=/newroot kdos-splash release`: the client
 * reads that root's record and sends it as `R<release>`, and the running
 * splash takes it in place of its own (an empty one is a root with no
 * record, and leaves the name alone). It has to come from a client: after
 * switch_root this process resolves every path in the emptied initramfs.
 *
 * The reader takes one fixed buffer of each file and keeps the printable
 * ASCII of the value, which is all the font is indexed by, cut at the room
 * in `release`; spl_set_release() holds a sent value to the same rule.
 * KDOS_SPLASH_ROOT names a directory whose etc/ is read in place of /etc,
 * so every form can be rendered without a boot.
 */
static char release[48];
/* The name under the mark. genlook.py reads this line, and the tagline in
 * compose(), into KSplash's copy of the picture. */
static const char *sub = "Kool Desktop Operating System";
static char ident[64 + sizeof(release)];

static int spl_key(const char *root, const char *file, const char *key,
		   char *out, size_t cap)
{
	char path[4096], buf[4096];
	size_t klen = strlen(key);
	int fd;
	ssize_t n;

	out[0] = '\0';
	if ((size_t)snprintf(path, sizeof(path), "%s/etc/%s", root, file) >=
	    sizeof(path))
		return 0;
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	for (char *l = buf, *nl; l; l = nl ? nl + 1 : NULL) {
		nl = strchr(l, '\n');
		if (strncmp(l, key, klen) || l[klen] != '=')
			continue;
		const char *v = l + klen + 1;
		size_t o = 0;
		if (*v == '"')
			v++;
		for (; *v && *v != '\n' && *v != '"' && o + 1 < cap; v++)
			if (*v >= 0x20 && *v < 0x7f)
				out[o++] = *v;
		out[o] = '\0';
		return o > 0;
	}
	return 0;
}

/* The release `root`'s etc/ names, in `out` (empty when it names none). */
static void spl_read_release(const char *root, char *out, size_t cap)
{
	char build[sizeof(release)];

	if (!spl_key(root, "kdos-release", "VERSION", out, cap) &&
	    spl_key(root, "os-release", "VERSION_ID", out, cap) &&
	    spl_key(root, "os-release", "BUILD_ID", build, sizeof(build))) {
		size_t l = strlen(out);
		const char *b = build;
		if (l + 1 < cap)
			out[l++] = '+';
		while (*b && l + 1 < cap)
			out[l++] = *b++;
		out[l] = '\0';
	}
}

/* `R<release>`, and the start: the release on screen, and the line it
 * shares with the name. */
static void spl_set_release(const char *v)
{
	size_t o = 0;

	for (; *v && *v != '\n' && o + 1 < sizeof(release); v++)
		if (*v >= 0x20 && *v < 0x7f)
			release[o++] = *v;
	release[o] = '\0';
	snprintf(ident, sizeof(ident), "%s%s%s", sub, *release ? " " : "",
		 release);
}

static void read_release(void)
{
	const char *root = getenv("KDOS_SPLASH_ROOT");
	char v[sizeof(release)];

	spl_read_release(root ? root : "", v, sizeof(v));
	spl_set_release(v);
}

/* The name and the release share a line when the pair fits nine tenths of
 * the width; past that the release takes a line of its own under the
 * tagline, in the tagline's colour, and the rule moves down to make room. */
static int ver_own(void)
{
	return *release && text_width(ident, body_scale, 0) > fbw * 9 / 10;
}

/*
 * Everything is stacked from the top down and measured in glyph cells, so the
 * whole screen scales with the framebuffer instead of assuming 1280x800.
 * `cw` is the wordmark's cell width; 0 is the width the screen gives it.
 */
static void layout_at(int cw)
{
	body_scale = fbw / 640;
	if (body_scale < 1)
		body_scale = 1;
	if (body_scale > 3)
		body_scale = 3;

	int cell = gh * body_scale;   /* one line of body text */

	/* Wordmark cells: the mascot + wordmark group takes ~55% of the width. */
	art_cw = cw > 0 ? cw : fbw / 85;
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
	ver_y  = ver_own() ? tag_y + cell * 2 : 0;
	rule_y = compact ? group_end + cell
			 : (ver_y ? ver_y : tag_y) + cell * 2;

	body_step = cell + body_scale * 4;

	/*
	 * The status column is a fixed-width block — name dotted to LINE_COLS,
	 * a space, and up to "FAIL" — centered as a whole. Anchored at a screen
	 * percentage instead, it only looks centered at 1280 wide.
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

/* The failure panel's text scale, line height and whole height: a gap of one
 * stage row, `WHY:`, and one line per detail. */
static int panel_ds(void) { return body_scale > 1 ? body_scale - 1 : 1; }
static int panel_lh(void) { return gh * panel_ds() + panel_ds(); }
static int panel_h(void)
{
	return failed && ndetail ? body_step + (1 + ndetail) * panel_lh() : 0;
}

/*
 * THE FAILURE TAKES ITS ROOM FROM THE PICTURE. The mascot, the wordmark and
 * the two lines of text under them fill the top of the screen, and on a wide
 * screen they leave too little under the bar for two stage rows and a panel
 * of reasons. With a panel up that does not fit, the two text lines go
 * first (`compact`); if it still does not fit, the wordmark's cell narrows
 * until the bar, two rows and the whole panel fit above the bottom margin,
 * down to the smallest cell that still draws, and below that the panel's
 * last lines are cut at the screen's edge. Without a panel the picture is the
 * one the screen gives.
 */
static void layout(void)
{
	compact = 0;
	layout_at(0);
	int need = panel_h();
	if (!need || fbh * 94 / 100 - body_y >= 2 * body_step + need)
		return;
	compact = 1;
	layout_at(0);
	for (int cw = art_cw - 1;
	     cw >= 4 && fbh * 94 / 100 - body_y < 2 * body_step + need; cw--)
		layout_at(cw);
}

/* The fraction the bar shows, 0..1, by the rule at bar_base. */
static double bar_frac(void)
{
	if (total_steps <= 0)
		return 0.0;
	double f = bar_base + (bar_end - bar_base) * bar_take;
	if (total_steps > bar_done)
		f = bar_base + (bar_end - bar_base) * bar_take *
		    (done_steps - bar_done) / (double)(total_steps - bar_done);
	if (f > 1.0)
		f = 1.0;
	return f < bar_base ? bar_base : f;
}

/* `total N`: add N steps and rebase, so the fraction on screen stays put. */
static void bar_total(int n)
{
	if (n <= 0)
		return;
	if (in_initrd && access(initrd_probe, F_OK) != 0) {
		/* The first word from the real root: the initramfs's half is
		 * over, whatever of it was left unclosed. */
		in_initrd = 0;
		double f = bar_frac();
		bar_base = f > INITRD_SHARE ? f : INITRD_SHARE;
		bar_end = 1.0;
		bar_take = 1.0;
		bar_done = done_steps;
		total_steps = done_steps + n;
		return;
	}
	if (total_steps > 0 && done_steps > 0) {
		bar_base = bar_frac();
		bar_done = done_steps;
	}
	total_steps += n;
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
		filled = (int)(bar_frac() * BAR_SEGS);
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
		int p = (int)(bar_frac() * 100.0);
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

/* One status line at y: a plain message, or a stage dotted out to its state. */
static void draw_line(const struct line *l, int y, int tick)
{
	int cursor_on = (tick / 4) & 1;
	static const char spin[4] = { '|', '/', '-', '\\' };

	if (l->state == ST_PLAIN) {
		draw_text(body_x, y, l->text, body_scale, 0, C_TEXT);
		return;
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
	} else if (l->state == ST_SKIP) {
		/* Neither started nor broken: the tagline's dim tone, which holds
		 * 4.5:1 on the ground, so it reads apart from OK and from FAIL. */
		draw_text(sx, y, "SKIP", body_scale, 0, C_TAG);
	} else {
		/* A live stage gets a little rotor, not a mute block. */
		char s[2] = { spin[tick & 3], 0 };
		draw_text(sx, y, s, body_scale, 0, C_AMBER);
		if (cursor_on)
			fill(sx + gw * body_scale * 2, y,
			     gw * body_scale, gh * body_scale, C_PHOS);
	}
}

/* Compose the whole screen into the canvas. Effects come later, in present. */
static void compose(int tick)
{
	for (size_t i = 0; i < (size_t)fbw * fbh; i++)
		canvas[i] = C_DEEP;

	/*
	 * The block-art wordmark. On the CRT, dim copies in four directions
	 * first: phosphor bleeds around a lit pixel, it does not cast a drop
	 * shadow to one side. Modern draws the mark flat.
	 */
	if (crt) {
		int bleed = art_cw / 3;
		if (bleed < 1)
			bleed = 1;
		draw_art(art_x - bleed, art_y, C_PHOSDIM, C_PHOSDIM);
		draw_art(art_x + bleed, art_y, C_PHOSDIM, C_PHOSDIM);
		draw_art(art_x, art_y - bleed, C_PHOSDIM, C_PHOSDIM);
		draw_art(art_x, art_y + bleed, C_PHOSDIM, C_PHOSDIM);
	}
	draw_art(art_x, art_y, C_PHOS, C_DIM);

	draw_penguin(peng_x, peng_y, peng_scale);

	if (!compact) {
		const char *name = ver_y ? sub : ident;
		draw_text((fbw - text_width(name, body_scale, 0)) / 2, sub_y,
			  name, body_scale, 0, C_TEXT);

		const char *tag = "musl . GNU . sysvinit . KDE Plasma";
		draw_text((fbw - text_width(tag, body_scale, 0)) / 2, tag_y,
			  tag, body_scale, 0, C_TAG);

		if (ver_y)
			draw_text((fbw - text_width(release, body_scale, 0)) / 2,
				  ver_y, release, body_scale, 0, C_TAG);
	}

	/* A rule between the identity and the machine talking. */
	fill(body_x, rule_y, body_w, body_scale, C_DIM);

	draw_bar(tick);

	/*
	 * THE FAILURE PANEL'S ROOM IS TAKEN FIRST. Once a stage has failed with
	 * a reason, the reason is drawn under the stage list, `WHY:` in alarm
	 * and the reason in the text colour, one scale step under the body
	 * (panel_ds) — the point is to fit real log text on screen, and a boot
	 * log line is wider than a stage name. Each detail line is cut to the
	 * columns that scale leaves inside the panel.
	 * Its height comes out of the list's rows before the list is laid out,
	 * so every detail line lands above the bottom edge on any screen that
	 * holds two stage rows and the panel; a list given its rows first would
	 * leave the panel whatever was under the newest stage, which on a long
	 * boot is one line.
	 *
	 * THE FAILED STAGE IS PINNED. The list scrolls to keep the newest
	 * stage in view; the newest FAIL stage, once out of that window, takes
	 * the first row and the newest stages fill the rest. Reason and stage
	 * stay up together for the rest of the boot, because the user's next
	 * move is to photograph them.
	 */
	int ds = panel_ds();
	int lh = panel_lh();
	int panel = panel_h();
	int rows = body_max;
	if (panel) {
		rows = (fbh * 94 / 100 - body_y - panel) / body_step;
		if (rows > body_max)
			rows = body_max;
		if (rows < 2)
			rows = 2;
	}

	/* Status lines, oldest first, scrolled to keep the newest visible. */
	int first = nlines > rows ? nlines - rows : 0;
	int row = 0;
	int pin = fail_seq >= 0 && fail_seq < lines_added - (nlines - first);
	if (pin) {
		struct line pinned;
		snprintf(pinned.text, sizeof(pinned.text), "%s", fail_text);
		pinned.state = ST_FAIL;
		draw_line(&pinned, body_y, tick);
		row = 1;
		first++;
	}
	for (int i = first; i < nlines; i++, row++)
		draw_line(&lines[i], body_y + row * body_step, tick);

	if (panel) {
		int dy = body_y + row * body_step + body_step;

		draw_text(body_x, dy, "WHY:", ds, 0, C_ALARM);
		dy += lh;
		/* Indented two cells under `WHY:`, inside the rule's width. */
		int cols = body_w / (gw * ds) - 2;
		for (int i = 0; i < ndetail; i++, dy += lh) {
			if (dy + lh > fbh)
				break;	/* never scribble past the panel */
			draw_fitted(body_x + gw * ds * 2, dy, detail[i], cols,
				    ds, C_TEXT);
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
 *
 * Modern passes a full-open tube, no flash and no hum, so only gain moves.
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

		/* Scanlines: every other row is dimmer. This is the whole CRT
		 * look, and Modern has none. */
		double scan = (crt && (y & 1)) ? 0.78 : 1.0;

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

/* ---------------------------------------------------------------- motion */

static double smoothstep(double p)
{
	if (p <= 0.0)
		return 0.0;
	if (p >= 1.0)
		return 1.0;
	return p * p * (3.0 - 2.0 * p);
}

/* How long the power-on lasts in the motion in force. */
static double intro_s(void)
{
	return crt ? INTRO_S : FADE_IN_S;
}

/* The tube at t seconds into the power-on. The CRT unfolds from a hairline
 * through a flash; Modern fades the settled picture in from black. Past the
 * power-on both are the settled picture. */
static void intro(double t, double *vopen, double *flash, double *gain)
{
	*vopen = 1.0;
	*flash = 0.0;
	*gain = 1.0;
	if (t >= intro_s())
		return;
	if (!crt) {
		*gain = smoothstep(t / FADE_IN_S);
		return;
	}
	double p = t / INTRO_S;
	double u = p / 0.55;
	if (u > 1.0)
		u = 1.0;
	*vopen = 1.0 - pow(1.0 - u, 3.0);	/* ease out */
	double f = 1.0 - p / 0.6;
	*flash = f > 0.0 ? f * f : 0.0;
}

/* The tube at fraction p of the power-off. The CRT folds to a hairline,
 * pinches that to a dot and lets it decay; Modern fades the picture out.
 * Both end on black at p = 1. */
static void outro(double p, double *vopen, double *hopen, double *flash,
		  double *gain)
{
	*vopen = 1.0;
	*hopen = 1.0;
	*flash = 0.0;
	if (!crt) {
		*gain = 1.0 - smoothstep(p);
		return;
	}
	double fold = 1.0 - p / 0.55;
	if (fold < 0.0)
		fold = 0.0;
	*vopen = fold * fold;

	double pinch = 1.0 - (p - 0.45) / 0.40;
	if (pinch > 1.0)
		pinch = 1.0;
	if (pinch < 0.0)
		pinch = 0.0;
	*hopen = pinch;

	double decay = 1.0 - (p - 0.80) / 0.20;
	if (decay > 1.0)
		decay = 1.0;
	if (decay < 0.0)
		decay = 0.0;
	*gain = decay;
	*flash = p * 0.5;
}

/*
 * THE MOTION, FROM THE KERNEL COMMAND LINE: `kdos.splash=phosphor` is the CRT,
 * any other value or none is Modern, and the last `kdos.splash=` wins, as it
 * does for the kernel's own parameters. A command line that cannot be read is
 * Modern too: the splash never stops a boot over a setting.
 *
 * THE SPLASH RUNS ONLY ON A QUIET BOOT. Every entry that boots to the desktop
 * carries `quiet`; the verbose and single-user entries leave it out, and they
 * exist to put the boot's text on the screen. A splash there would cover that
 * text, and a kernel line printed to the VT under it would end fbcon's
 * deferred take-over and be drawn over the picture. So `run` without `quiet`
 * draws nothing and exits 0, and every later command finds no FIFO and is a
 * no-op. A command line that cannot be read counts as quiet, the boot's
 * normal case.
 *
 * KDOS_SPLASH_CMDLINE names a file to read in place of /proc/cmdline, so the
 * choice can be exercised without a boot.
 */
static int quiet_boot = 1;

static void read_mode(void)
{
	const char *path = getenv("KDOS_SPLASH_CMDLINE");
	char buf[4096];
	int fd = open(path && *path ? path : "/proc/cmdline",
		      O_RDONLY | O_CLOEXEC);
	ssize_t n;

	crt = 0;
	quiet_boot = 1;
	if (fd < 0)
		return;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return;
	buf[n] = '\0';

	quiet_boot = 0;
	for (char *w = strtok(buf, " \t\n"); w; w = strtok(NULL, " \t\n")) {
		if (!strncmp(w, "kdos.splash=", 12))
			crt = !strcmp(w + 12, "phosphor");
		else if (!strcmp(w, "quiet"))
			quiet_boot = 1;
	}
}

/* ------------------------------------------------------------------ init */

/*
 * Take the foreground VT for the picture.
 *
 * The kernel is built with CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER, which
 * is the whole reason the framebuffer is free for us. Only a printed glyph
 * ends the deferral, and this write is escape sequences alone, so it claims
 * nothing: where fbcon has already taken over (an installed machine's
 * console=tty0) it clears the console and hides the cursor, and elsewhere it
 * changes nothing on screen. On virtio-gpu the framebuffer is scanned out with
 * the deferral still pending, measured in the rig.
 *
 * Then KD_GRAPHICS: fbcon draws nothing on a VT in graphics mode, so the
 * initramfs's and rcS's lines, a kernel message and a deferred take-over that
 * a later glyph ends all stay off the picture. The mode is the VT's and
 * outlives this process, so it is set only after every early return in run()
 * and console_release() is the splash's way out: a splash that set it and left
 * without putting it back would leave the console drawing nothing, which the
 * `quit` client's vt_text() undoes for a splash that was killed.
 */
static void console_claim(void)
{
	ttyfd = open("/dev/tty0", O_RDWR | O_NOCTTY);
	if (ttyfd < 0)
		return;
	static const char seq[] = "\033[2J\033[H\033[?25l";
	(void)!write(ttyfd, seq, sizeof(seq) - 1);
	(void)ioctl(ttyfd, KDSETMODE, KD_GRAPHICS);
}

/* The console back in text mode with its cursor shown, for whoever writes text
 * on it next; it redraws its buffer, the boot's messages included. The `quit`
 * client does the same after the splash is gone (vt_text), for a splash that
 * never got here. Not on a hand-off: the display manager's compositor takes
 * the VT from elogind in graphics mode, and a VT put back into text now would
 * draw rcS's last lines and a blinking cursor over the held frame until it
 * does (vt_handoff). */
static void console_release(void)
{
	if (ttyfd < 0)
		return;
	static const char seq[] = "\033[?25h";
	if (!handing_off) {
		(void)ioctl(ttyfd, KDSETMODE, KD_TEXT);
		(void)!write(ttyfd, seq, sizeof(seq) - 1);
	}
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
	lines_added++;
}

static void close_line(int state)
{
	for (int i = nlines - 1; i >= 0; i--) {
		if (lines[i].state == ST_PENDING) {
			lines[i].state = state;
			done_steps++;
			if (state == ST_FAIL) {
				memcpy(fail_text, lines[i].text,
				       sizeof(fail_text));
				fail_seq = lines_added - nlines + i;
			}
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
	case 'K': close_line(ST_SKIP); break;
	case 'D':
		/* Detail lines accumulate; the newest MAX_DETAIL are kept, so
		 * a script that fails after pages of output still shows the
		 * end, which is where the error is. */
		if (ndetail == MAX_DETAIL) {
			memmove(detail, detail + 1,
				sizeof(detail) - sizeof(detail[0]));
			ndetail--;
		}
		snprintf(detail[ndetail], sizeof(detail[0]), "%s", cmd + 1);
		ndetail++;
		failed = 1;
		layout();
		break;
	case 'T': bar_total(atoi(cmd + 1)); break;
	/*
	 * THE ACCENT, ARRIVING MID-BOOT. The initramfs half of this runs in the
	 * compiled-in default because no root filesystem is mounted yet; `rcS`
	 * sends this once /etc is readable. Every frame after it is repainted
	 * from the variables, so the change takes at the next frame and nothing
	 * needs redrawing here.
	 */
	case 'A': spl_accent(cmd + 1); break;
	/* The booted root's release (read_release() above). The layout
	 * follows it, because a longer one can need a line of its own. */
	case 'R': spl_set_release(cmd + 1); layout(); break;
	case 'Q': finishing = 1; return 1;
	case 'H': handing_off = 1; return 1;
	default: break;
	}
	return 0;
}

/*
 * THE HAND-OFF HOLDS THE PICTURE. A power-off to black here would leave the
 * screen black for the seconds SDDM and its compositor take to start; instead
 * the settled frame (STARTING SESSION closed, the bar at its hold, the value
 * KSplash starts its own bar from) is drawn once more and stays on the
 * framebuffer until the compositor's first modeset replaces it.
 *
 * Closing the FIFO is the answer to the client: its open then fails with
 * ENXIO, so `kdos-splash handoff` returns only once that frame is flushed, and
 * no later frame is drawn for the compositor's modeset to race.
 *
 * THEN THE FRAMEBUFFER IS BLANKED, after the compositor has replaced the
 * frame, so the fbdev contents the kernel shows again when that compositor
 * lets go of the display (a login, a logout back to SDDM) are black and not a
 * stale boot frame. The blank must land before that hand-back: the kernel
 * restores fbdev's buffer in the same instant the compositor drops master,
 * and a blank made after it is seen for a frame. THE FIRST SIGN IS A PAN
 * REFUSED WITH EBUSY, which DRM's fbdev emulation answers while a DRM master
 * holds the device; once rcS has handed off, nothing but a compositor becomes
 * master. The pan is tried on every poll. It does not mark the compositor's
 * first frame: a master exists before its first modeset, and until that
 * modeset the fbdev buffer is still what the display scans out, with fbdev's
 * deferred-I/O flush copying writes into it whoever is master, so a blank on
 * the first EBUSY would show black for the rest of the compositor's start.
 *
 * THE COMPOSITOR'S FIRST FRAME IS SEEN ON THE DISPLAY CONTROLLER. On the
 * first EBUSY the hold opens the DRM card (hold_scan_open: dri/card0 to
 * dri/card3 through devfd, the first with a lit CRTC) and records the
 * framebuffer each lit CRTC scans out, which is still fbdev's. The open
 * cannot make the splash master, since one exists, and DROP_MASTER is asked
 * anyway so a stray EBUSY with no compositor leaves no master behind. A CRTC
 * that then scans out another framebuffer, or none, has stopped showing
 * fbdev's buffer, and the blank lands unseen. On virtio-gpu the greeter's
 * KWin draws its first frame about 0.6 to 0.7 s after it becomes master
 * (a live session's about 1.0 to 1.1 s) and the greeter's picture 2.6 to
 * 3.4 s after it, so the frame is blanked before a login can be typed.
 *
 * WHEN NO CARD ANSWERS (no DRM device, or no CRTC lit), the blank waits
 * HOLD_MARGIN_S past the first EBUSY. KWin's first frame on virtio-gpu comes
 * 0.6 to 1.8 s after master, so 5 s leaves more than twice that; the margin
 * then ends 1.6 to 2.4 s after the greeter appears, and a login typed inside
 * that window ends the greeter's session, puts DM_VT back in text mode and
 * blanks on that exit, a poll after the kernel has shown fbdev's buffer
 * again, so the boot frame shows for one frame before black. A compositor
 * slower than the margin to its first frame shows black for the rest of its
 * start, not the held frame.
 *
 * DM_VT's VT_PROCESS mode, which elogind sets when the compositor takes
 * control of the session, is recorded for the log and ends nothing: the VT is
 * hung up when the greeter's session starts, a descriptor held across that
 * answers every ioctl with EIO, and the hold reopens DM_VT through devfd
 * whenever one does.
 *
 * The hold ends at HOLD_MAX_S whatever was seen, and on SIGTERM, with the
 * same blank. It ends at once, touching nothing, when the framebuffer device
 * is gone (FBIOGET_VSCREENINFO fails: a native GPU driver has replaced the
 * firmware framebuffer the splash drew on, and its modeset has already
 * replaced the frame). It ends at once when DM_VT is put back into text
 * mode: with no EBUSY seen that is kdos-getty's prompt when SDDM cannot run,
 * whose console then owns the framebuffer, and the frame is left untouched;
 * after an EBUSY it is a compositor letting go (a login inside the margin, or
 * SDDM failing after its compositor came up, where kdos-getty switches to
 * text before it draws), fbdev shows its own buffer again, and that buffer is
 * blanked.
 *
 * EVERY WAY OUT WRITES ONE LINE TO THE KERNEL LOG, which `dmesg | grep
 * kdos-splash` shows: what ended the hold (frame, margin, timeout, signal,
 * fb-gone, text), after how long, when VT_PROCESS and the first EBUSY were
 * seen, and what the last pan returned. Times count from the hand-off.
 */
#define HOLD_POLL_US  100000
#define HOLD_MARGIN_S 5.0
#define HOLD_MAX_S    60.0

/* The lit CRTCs of the card the compositor took, and the framebuffer each
 * scanned out when the hold first saw EBUSY. */
#define HOLD_CRTCS 8
struct hold_scan {
	int fd, n;
	uint32_t crtc[HOLD_CRTCS], fb[HOLD_CRTCS];
};

static uint32_t hold_crtc_fb(int fd, uint32_t crtc)
{
	struct drm_mode_crtc c;
	memset(&c, 0, sizeof(c));
	c.crtc_id = crtc;
	return ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &c) == 0 ? c.fb_id : 0;
}

static void hold_scan_open(struct hold_scan *s)
{
	s->fd = -1;
	s->n = 0;
	for (int i = 0; i < 4 && s->n == 0 && devfd >= 0; i++) {
		char name[16];
		snprintf(name, sizeof(name), "dri/card%d", i);
		int fd = openat(devfd, name, O_RDWR | O_NOCTTY | O_CLOEXEC);
		if (fd < 0)
			continue;
		(void)ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
		uint32_t ids[HOLD_CRTCS];
		struct drm_mode_card_res res;
		memset(&res, 0, sizeof(res));
		res.crtc_id_ptr = (uint64_t)(uintptr_t)ids;
		res.count_crtcs = HOLD_CRTCS;
		if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0) {
			uint32_t n = res.count_crtcs < HOLD_CRTCS ? res.count_crtcs : HOLD_CRTCS;
			for (uint32_t j = 0; j < n; j++) {
				uint32_t fb = hold_crtc_fb(fd, ids[j]);
				if (fb) {
					s->crtc[s->n] = ids[j];
					s->fb[s->n++] = fb;
				}
			}
		}
		if (s->n)
			s->fd = fd;
		else
			close(fd);
	}
}

/* A recorded CRTC scans out something other than fbdev's buffer now. */
static int hold_scan_moved(const struct hold_scan *s)
{
	for (int i = 0; i < s->n; i++)
		if (hold_crtc_fb(s->fd, s->crtc[i]) != s->fb[i])
			return 1;
	return 0;
}

static void hold_log(const char *why, double since, double t_vt,
		     double t_busy, int pan_err)
{
	if (kmsgfd < 0)
		return;
	char vt[24] = "never", busy[24] = "never", line[200];
	if (t_vt >= 0.0)
		snprintf(vt, sizeof(vt), "%.1fs", t_vt);
	if (t_busy >= 0.0)
		snprintf(busy, sizeof(busy), "%.1fs", t_busy);
	int n = snprintf(line, sizeof(line),
			 "<6>kdos-splash: hold ended by %s after %.1fs; "
			 "VT_PROCESS at %s, EBUSY at %s, last pan %s\n",
			 why, since, vt, busy, pan_err ? strerror(pan_err) : "ok");
	if (n > 0)
		(void)!write(kmsgfd, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1);
}

/* DM_VT's KD mode, reopening it through devfd when the descriptor held was
 * hung up. A VT that cannot be asked counts as graphics: the hold then ends
 * by the pan or the timeout, never by a text mode nobody saw. */
static int hold_vt_mode(int *vt)
{
	int mode = KD_GRAPHICS;
	if (*vt >= 0 && ioctl(*vt, KDGETMODE, &mode) == 0)
		return mode;
	if (*vt >= 0 && errno != EIO)
		return KD_GRAPHICS;
	if (*vt >= 0)
		close(*vt);
	*vt = devfd >= 0 ? openat(devfd, DM_VT_NAME, O_RDONLY | O_NOCTTY | O_CLOEXEC) : -1;
	mode = KD_GRAPHICS;
	if (*vt >= 0 && ioctl(*vt, KDGETMODE, &mode) < 0)
		mode = KD_GRAPHICS;
	return mode;
}

static void hold_frame(int fifo)
{
	compose(0);
	present(1.0, 1.0, 0.0, 1.0, -1e6);
	fb_flush();
	close(fifo);
	unlink(FIFO_PATH);
	console_release();

	int vt = devfd >= 0 ? openat(devfd, DM_VT_NAME, O_RDONLY | O_NOCTTY | O_CLOEXEC) : -1;
	double h0 = now_s(), t = h0, t_vt = -1.0, t_busy = -1.0;
	int pan_err = 0, blank = 1;
	const char *why = "signal";
	struct hold_scan scan = { .fd = -1, .n = 0 };

	for (;;) {
		t = now_s();
		if (stop)
			break;
		if (t_busy >= 0.0 && t - h0 - t_busy >= HOLD_MARGIN_S) {
			why = "margin";
			break;
		}
		if (t - h0 >= HOLD_MAX_S) {
			why = "timeout";
			break;
		}

		struct fb_var_screeninfo cur;
		struct vt_mode vm;

		if (ioctl(fbfd, FBIOGET_VSCREENINFO, &cur) < 0) {
			why = "fb-gone";
			blank = 0;
			break;
		}
		if (hold_vt_mode(&vt) != KD_GRAPHICS) {
			why = "text";
			blank = t_busy >= 0.0;
			break;
		}
		if (t_vt < 0.0 && vt >= 0 &&
		    ioctl(vt, VT_GETMODE, &vm) == 0 && vm.mode == VT_PROCESS)
			t_vt = t - h0;
		vi.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
		pan_err = ioctl(fbfd, FBIOPAN_DISPLAY, &vi) < 0 ? errno : 0;
		if (pan_err == EBUSY && t_busy < 0.0) {
			t_busy = t - h0;
			hold_scan_open(&scan);
		} else if (scan.n && hold_scan_moved(&scan)) {
			why = "frame";
			break;
		}
		usleep(HOLD_POLL_US);
	}
	if (blank) {
		memset(fbmem, 0, fbsize);
		fb_flush();
	}
	munmap(fbmem, fbsize);
	if (vt >= 0)
		close(vt);
	if (scan.fd >= 0)
		close(scan.fd);
	hold_log(why, t - h0, t_vt, t_busy, pan_err);
}

static int run(void)
{
	if (fb_open() < 0)
		return 0;   /* fail open: no framebuffer, no splash, no fuss */

	if (font_load("/usr/share/kdos/splash.psf") < 0 &&
	    font_load("/usr/share/consolefonts/ter-v16n.psf") < 0)
		return 0;

	layout();

	in_initrd = access(initrd_probe, F_OK) == 0;
	if (in_initrd) {
		bar_end = INITRD_SHARE;
		bar_take = 0.5;
	}

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

	devfd = open("/dev", O_PATH | O_DIRECTORY | O_CLOEXEC);
	kmsgfd = open("/dev/kmsg", O_WRONLY | O_NOCTTY | O_CLOEXEC);
	console_claim();

	double t0 = now_s();
	char buf[512];
	size_t used = 0;
	int quit = 0;

	/* Power-on, then a steady picture until someone says stop. */
	while (!stop) {
		double t = now_s() - t0;
		int tick = (int)(t * 10.0);

		double vopen, flash, gain, hum = -1e6;
		intro(t, &vopen, &flash, &gain);
		if (crt && t >= INTRO_S && !quit) {
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
		int frame_ms = (t < intro_s()) ? 16 : 90;
		if (poll(&pfd, 1, frame_ms) > 0 && (pfd.revents & POLLIN)) {
			/* A line that fills the buffer is dropped whole. Kept,
			 * it would leave no room to read into, and no later
			 * command — quit included — could ever arrive. */
			if (used >= sizeof(buf) - 1)
				used = 0;
			ssize_t n = read(fifo, buf + used, sizeof(buf) - 1 - used);
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

	if (handing_off) {
		hold_frame(fifo);
		return 0;
	}

	/*
	 * Power-off, in the motion in force. The screen is left black so
	 * the text console that takes it next starts from black.
	 */
	double q0 = now_s();
	for (;;) {
		double p = (now_s() - q0) / OUTRO_S;
		if (p >= 1.0)
			break;

		double vopen, hopen, flash, gain;
		outro(p, &vopen, &hopen, &flash, &gain);
		compose(0);
		present(vopen, hopen, flash, gain, -1e6);
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
 *   kdos-splash preview 1280x800 0.35 out.ppm [modern|phosphor] [ACCENT]
 *
 * The second argument is the intro progress in seconds; anything past the
 * intro gives the settled picture. The motion is the one named, or the
 * command line's when none is; the scheme is the one named, or libkcolor's
 * default. A scheme name that does not exist is a usage error here, where a
 * person typed it, and not on the FIFO.
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

	/*
	 * The fixture is a boot, fed through the same commands the FIFO
	 * carries: both halves' totals, so the bar is the one a boot draws, and
	 * a FAILURE followed by more stages than any screen holds. The failure
	 * panel is the part of this screen nobody can see without one, and a
	 * panel that has never been looked at is a panel with a geometry bug in
	 * it; the stages after it are what push the failed one out of the
	 * scrolling window and onto its pinned row. Same reason kdosbuild's
	 * preview fixture uses a nine-digit file count and a port name longer
	 * than any pane. The newest service skipped itself, so a tall screen
	 * shows SKIP beside OK and FAIL.
	 *
	 * The detail lines are rcS's six, one of them deliberately wider than
	 * the panel at any scale: real log output does not respect a column
	 * budget, and that line is the one the ellipsis is seen on.
	 */
	static const char *INITRD[] = {
		"T2", "SPROBING BLOCK DEVICES", "O", "SDEVICE MANAGER", "O",
		"T4", "SBOOT MEDIA", "O", "SSYSTEM IMAGE", "O",
		"SOVERLAY ROOT", "O", "SSWITCHING ROOT", "O",
	};
	static const char *ROOT[] = {
		"T40", "SMOUNTING FILESYSTEMS", "O", "SNETWORK MANAGER", "F",
		"D[KDOS] Starting NetworkManager...",
		"D<info>  NetworkManager (version 1.58.1) is starting...",
		"D<error> [1765.4] bus-manager: could not get the system bus: "
		"Could not connect: No such file or directory",
		"D<error> [1765.4] Failed to initialize: no D-Bus connection",
		"D<warn>  [1765.4] exiting (error)",
		"Dnetworkmanager: exit 1",
	};
	static const char *SVC[] = {
		"SYSLOG", "UDEV TRIGGER", "HWCLOCK", "SYSCTL", "RANDOM SEED",
		"LOOPBACK", "HOSTNAME", "KMOD", "XFS HEALER", "DBUS", "ELOGIND",
		"POLKIT", "ACPID", "CRON", "CHRONY", "BLUETOOTH", "CUPS",
		"AVAHI", "FIREWALL", "SSHD", "UPOWER", "UDISKS", "MODEMMANAGER",
		"WPA SUPPLICANT", "ZRAM", "PLASMA SETUP", "FLATPAK", "PACKS",
		"BOXES", "STEP-CA", "-LLAMA-SERVER",
	};
	in_initrd = 1;
	bar_end = INITRD_SHARE;
	bar_take = 0.5;
	initrd_probe = ".";	/* the initramfs's root, still there */
	for (size_t i = 0; i < sizeof(INITRD) / sizeof(INITRD[0]); i++)
		handle(INITRD[i]);
	/* The root mounted at /newroot names its release, as the initramfs
	 * has it do before switch_root; KDOS_SPLASH_NEWROOT stands in for it. */
	const char *newroot = getenv("KDOS_SPLASH_NEWROOT");
	if (newroot) {
		char r[sizeof(release) + 1];
		r[0] = 'R';
		spl_read_release(newroot, r + 1, sizeof(r) - 1);
		handle(r);
	}
	initrd_probe = "";	/* switch_root: the initramfs's root is gone */
	for (size_t i = 0; i < sizeof(ROOT) / sizeof(ROOT[0]); i++)
		handle(ROOT[i]);
	for (size_t i = 0; i < sizeof(SVC) / sizeof(SVC[0]); i++) {
		char c[sizeof(lines[0].text) + 1];
		/* A leading '-' is a stage that skipped itself. */
		int skip = SVC[i][0] == '-';
		snprintf(c, sizeof(c), "S%s", SVC[i] + skip);
		handle(c);
		handle(skip ? "K" : "O");
	}
	handle("SSTARTING SESSION");

	double vopen, flash, gain;
	intro(t, &vopen, &flash, &gain);

	compose(4);
	present(vopen, 1.0, flash, gain, crt ? h * 0.62 : -1e6);

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
	/* ONE COMMAND IS ONE LINE. A text too long for the buffer is cut, and
	 * the cut line still ends in its newline: without it the command runs
	 * into the next one, and snprintf's return is the length it wanted,
	 * not the length it wrote. */
	if (n >= (int)sizeof(buf)) {
		n = (int)sizeof(buf) - 1;
		buf[n - 1] = '\n';
	}
	if (n > 0)
		(void)!write(fd, buf, (size_t)n);
	close(fd);

	/*
	 * Quitting is synchronous. rcS runs `kdos-splash handoff` and returns,
	 * and init starts the display manager straight after; if we did not
	 * block here until the splash has drawn its last frame, the greeter's
	 * compositor would modeset while the splash was still drawing, and the
	 * two would race for the screen — and on the way to a text console, the
	 * console's text would end up interleaved with splash pixels.
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
	 * writing fails with ENXIO exactly when no reader is left. A `quit`
	 * splash closes it on exit, a `handoff` one once its held frame is
	 * flushed (hold_frame). That answer does not depend on whose root is
	 * whose. The client is running in the real root, so it can do the
	 * cleanup the daemon cannot.
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

/*
 * tty1 FOR THE DISPLAY MANAGER: in graphics mode, its text cleared and its
 * cursor hidden. A VT in KD_GRAPHICS draws nothing — not the lines rcS and
 * init print after this, not a kernel message — so the frame the splash holds
 * (hold_frame) is what shows until the greeter's compositor takes the VT from
 * elogind, which keeps it in graphics mode, and draws its first frame. The
 * clear and the hidden cursor are for the VT's text buffer: whoever puts the
 * VT back into text mode (elogind when the greeter's session ends, kdos-getty
 * when SDDM cannot run) redraws it, and an empty buffer with no cursor
 * redraws as black. Escape sequences alone end no
 * deferred fbcon take-over, so a console fbcon has not claimed stays
 * unclaimed.
 *
 * Done by the client, in the real root: the running splash was started in the
 * initramfs and resolves every path in that root, which switch_root emptied.
 * Done whether or not a splash answered, because the VT is the display
 * manager's either way. O_NOCTTY keeps tty1 free to become the greeter
 * session's controlling terminal.
 */
static void vt_handoff(void)
{
	static const char seq[] = "\033[2J\033[H\033[?25l";
	int fd = open(DM_VT, O_RDWR | O_NOCTTY | O_CLOEXEC);

	if (fd < 0)
		return;
	(void)ioctl(fd, KDSETMODE, KD_GRAPHICS);
	(void)!write(fd, seq, sizeof(seq) - 1);
	close(fd);
}

/*
 * THE FOREGROUND VT BACK IN TEXT MODE for a `quit`, whether or not a splash
 * answered. console_release() does this on the splash's own way out, but a
 * splash killed without its outro leaves the VT in KD_GRAPHICS, where the
 * rescue shell, sulogin and kdos-getty would draw nothing. Only a VT in
 * VT_AUTO mode is touched: a VT in VT_PROCESS mode belongs to a session
 * elogind handed to a compositor, and putting that one into text mode would
 * draw the console over the desktop.
 */
static void vt_text(void)
{
	static const char seq[] = "\033[?25h";
	struct vt_mode vm;
	int mode = KD_TEXT;
	int fd = open("/dev/tty0", O_RDWR | O_NOCTTY | O_CLOEXEC);

	if (fd < 0)
		return;
	if (ioctl(fd, VT_GETMODE, &vm) == 0 && vm.mode == VT_AUTO &&
	    ioctl(fd, KDGETMODE, &mode) == 0 && mode == KD_GRAPHICS) {
		(void)ioctl(fd, KDSETMODE, KD_TEXT);
		(void)!write(fd, seq, sizeof(seq) - 1);
	}
	close(fd);
}

int main(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : "run";
	const char *arg = argc > 2 ? argv[2] : "";

	/* BEFORE ANY ENTRY POINT, because the colours are variables set from a
	 * scheme and an unset one is black on black. The default is libkcolor's
	 * own, so the splash and an unconfigured desktop come up in the same
	 * scheme. */
	spl_accent(KCOL_DEFAULT_NAME);
	read_release();

	if (!strcmp(cmd, "run")) {
		read_mode();
		return quiet_boot ? run() : 0;
	}
	if (!strcmp(cmd, "preview") && argc > 4 && argc < 8) {
		if (argc >= 6 && strcmp(argv[5], "modern") &&
		    strcmp(argv[5], "phosphor"))
			goto usage;
		if (argc == 7 && !spl_accent(argv[6]))
			goto usage;
		read_mode();
		if (argc >= 6)
			crt = !strcmp(argv[5], "phosphor");
		return preview(argv[2], argv[3], argv[4]);
	}
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
	if (!strcmp(cmd, "skip"))
		return say("K%s\n", NULL, 0);
	if (!strcmp(cmd, "detail"))
		return say("D%s\n", arg, 0);
	if (!strcmp(cmd, "accent"))
		return say("A%s\n", arg, 0);
	if (!strcmp(cmd, "release") && argc == 2)
		return say("R%s\n", release, 0);
	if (!strcmp(cmd, "quit")) {
		say("Q%s\n", NULL, 1);
		vt_text();
		return 0;
	}
	if (!strcmp(cmd, "handoff")) {
		say("H%s\n", NULL, 1);
		vt_handoff();
		return 0;
	}

usage:
	fprintf(stderr, "usage: kdos-splash {run|step TEXT|msg TEXT|ok|fail|skip|detail TEXT|\n"
		"                    total N|accent NAME|release|quit|handoff|\n"
		"                    preview WxH T OUT [modern|phosphor] [ACCENT]}\n");
	return 1;
}

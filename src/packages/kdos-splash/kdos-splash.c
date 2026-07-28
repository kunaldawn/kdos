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
 *   kdos-splash quit                power-off animation, then exit
 *
 * Commands reach the running instance through a FIFO in /dev, which is the
 * one filesystem that survives switch_root intact (devtmpfs is moved, not
 * remounted), so a single splash process spans the initramfs and the real
 * root without a seam.
 */

#define _GNU_SOURCE
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

static int logo_scale, logo_track, logo_x, logo_y;
static int sub_y, tag_y, rule_y;
static int body_scale, body_x, body_y, body_step, body_max;

/*
 * Everything is stacked from the top down and measured in glyph cells, so the
 * whole screen scales with the framebuffer instead of assuming 1280x800.
 */
static void layout(void)
{
	const char *word = "KDOS";
	int n = (int)strlen(word);

	body_scale = fbw / 640;
	if (body_scale < 1)
		body_scale = 1;
	if (body_scale > 3)
		body_scale = 3;

	int cell = gh * body_scale;   /* one line of body text */

	/* The wordmark takes a bit under half the screen width. */
	int target = fbw * 46 / 100;
	logo_scale = target / (n * gw + (n - 1) * 2);
	if (logo_scale < 2)
		logo_scale = 2;
	if (logo_scale > 24)
		logo_scale = 24;
	logo_track = logo_scale * 2;

	int lw = text_width(word, logo_scale, logo_track);
	logo_x = (fbw - lw) / 2;
	logo_y = fbh * 14 / 100;

	int logo_end = logo_y + gh * logo_scale;
	sub_y  = logo_end + cell;
	tag_y  = sub_y + cell * 2;
	rule_y = tag_y + cell * 2;

	body_step = cell + body_scale * 4;
	body_y = rule_y + body_step;
	body_x = fbw * 22 / 100;
	body_max = (fbh * 94 / 100 - body_y) / body_step;
	if (body_max > MAX_LINES)
		body_max = MAX_LINES;
	if (body_max < 1)
		body_max = 1;
}

/* Compose the whole screen into the canvas. Effects come later, in present. */
static void compose(int cursor_on)
{
	for (size_t i = 0; i < (size_t)fbw * fbh; i++)
		canvas[i] = C_DEEP;

	/*
	 * The wordmark, in the console font blown up. Dim copies in four
	 * directions first: phosphor bleeds around a lit pixel, it does not cast
	 * a drop shadow to one side.
	 */
	int bleed = logo_scale / 3;
	if (bleed < 1)
		bleed = 1;
	draw_text(logo_x - bleed, logo_y, "KDOS", logo_scale, logo_track, C_PHOSDIM);
	draw_text(logo_x + bleed, logo_y, "KDOS", logo_scale, logo_track, C_PHOSDIM);
	draw_text(logo_x, logo_y - bleed, "KDOS", logo_scale, logo_track, C_PHOSDIM);
	draw_text(logo_x, logo_y + bleed, "KDOS", logo_scale, logo_track, C_PHOSDIM);
	draw_text(logo_x, logo_y, "KDOS", logo_scale, logo_track, C_PHOS);

	const char *sub = "KD's Homebrew Linux Distro";
	draw_text((fbw - text_width(sub, body_scale, 0)) / 2, sub_y,
		  sub, body_scale, 0, C_TEXT);

	const char *tag = "musl . toybox . niri . no systemd";
	draw_text((fbw - text_width(tag, body_scale, 0)) / 2, tag_y,
		  tag, body_scale, 0, C_DIM);

	/* A rule between the identity and the machine talking. */
	fill(body_x, rule_y, fbw - body_x * 2, body_scale, C_DIM);

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
		if (l->state == ST_OK)
			draw_text(sx, y, "OK", body_scale, 0, C_PHOS);
		else if (l->state == ST_FAIL)
			draw_text(sx, y, "FAIL", body_scale, 0, C_ALARM);
		else if (cursor_on)
			fill(sx, y, gw * body_scale, gh * body_scale, C_PHOS);
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
 */
static void present(double vopen, double hopen, double flash, double gain)
{
	double cy = fbh / 2.0, cx = fbw / 2.0;
	if (vopen < 1e-4) vopen = 1e-4;
	if (hopen < 1e-4) hopen = 1e-4;

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
	case 'Q': return 1;
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
	int quit = 0, cursor = 1;
	double cursor_t = t0;

	/* Power-on, then a steady picture until someone says stop. */
	while (!stop) {
		double t = now_s() - t0;

		double vopen = 1.0, flash = 0.0, gain = 1.0;
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
		}

		if (now_s() - cursor_t > 0.45) {
			cursor = !cursor;
			cursor_t = now_s();
		}

		compose(cursor);
		present(vopen, 1.0, flash, gain);
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
		present(fold, pinch, p * 0.5, decay);
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

	add_line("PROBING BLOCK DEVICES", ST_OK);
	add_line("MOUNTING SYSTEM IMAGE", ST_OK);
	add_line("OVERLAY ROOT", ST_OK);
	add_line("SWITCHING ROOT", ST_OK);
	add_line("DEVICE MANAGER", ST_OK);
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

	compose(1);
	present(vopen, 1.0, flash, gain);

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
	if (!strcmp(cmd, "ok"))
		return say("O%s\n", NULL, 0);
	if (!strcmp(cmd, "fail"))
		return say("F%s\n", NULL, 0);
	if (!strcmp(cmd, "quit"))
		return say("Q%s\n", NULL, 1);

	fprintf(stderr, "usage: kdos-splash {run|step TEXT|msg TEXT|ok|fail|quit}\n");
	return 1;
}

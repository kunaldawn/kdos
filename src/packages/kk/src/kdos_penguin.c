/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * The mascot. The pixels are NOT a copy: penguin.h is included straight out of
 * src/packages/kdos-splash, the same quantised crop of kdos.png that the boot
 * splash paints to /dev/fb0 and that genlogo.py turns into the login banner.
 * Three programs, one array — a second copy here is exactly how the banner and
 * the splash would end up wearing different birds.
 *
 * The RLE decodes to palette INDICES; the splash maps them onto phosphor
 * colours, and this maps the same indices onto luminance, because aalib
 * renders brightness and the VT palette supplies the green.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#include "penguin.h"

#define STATE (TIME - starttime)

/* Index 0 is transparent. 1 is the outline, 2 the belly and eyes, 3 the amber
 * beak and feet, 4 and 5 the body and its shadow. */
static const unsigned char peng_lum[6] = { 0, 25, 255, 205, 165, 105 };

static unsigned char *peng;	/* PENGUIN_W * PENGUIN_H, decoded once */
static double zoom_from, zoom_to, spin;

static void peng_decode(void)
{
	size_t i;
	int x = 0, y = 0;

	if (peng)
		return;
	peng = calloc(1, (size_t)PENGUIN_W * PENGUIN_H);
	for (i = 0; i + 1 < sizeof(penguin_rle); i += 2) {
		int idx = penguin_rle[i], run = penguin_rle[i + 1];
		while (run-- > 0 && y < PENGUIN_H) {
			peng[y * PENGUIN_W + x] = peng_lum[idx];
			if (++x >= PENGUIN_W)
				x = 0, y++;
		}
	}
}

/* Nearest-neighbour sample with a rotation, straight into the image buffer.
 * No filtering on purpose: aalib is about to quantise this to 26 brightness
 * levels and a character grid, and a smoothed source only costs time. */
static void draw_peng(void)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	double t = (double)STATE / (double)(endtime - starttime + 1);
	double z = zoom_from + (zoom_to - zoom_from) * t;
	double a = spin * sin(STATE / 900000.0);
	double ca = cos(a), sa = sin(a);
	int px, py;

	if (z < 0.05)
		z = 0.05;
	clrscr();
	for (py = 0; py < h; py++)
		for (px = 0; px < w; px++) {
			/* image space is 2:1 per cell, so x is halved to keep
			 * the bird from coming out fat */
			double dx = (px - w / 2.0) * 0.5 / z;
			double dy = (py - h / 2.0) / z;
			int sx = (int)(dx * ca - dy * sa) + PENGUIN_W / 2;
			int sy = (int)(dx * sa + dy * ca) + PENGUIN_H / 2;

			if (sx < 0 || sx >= PENGUIN_W || sy < 0 || sy >= PENGUIN_H)
				continue;
			b[py * w + px] = peng[sy * PENGUIN_W + sx];
		}
}

void kdos_penguin(void)
{
	peng_decode();
	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->randomval = 0;
	params->dither = AA_FLOYD_S;

	drawptr = draw_peng;

	/* far away and spinning */
	zoom_from = 0.05;
	zoom_to = 0.9;
	spin = 0.9;
	timestuff(0, NULL, draw, 3 * 1000000);

	/* settles, still breathing */
	zoom_from = zoom_to;
	zoom_to = 1.0;
	spin = 0.12;
	timestuff(0, NULL, draw, 2.5 * 1000000);

	drawptr = NULL;
	clrscr();
	centerprint(aa_imgwidth(context) / 2, aa_imgheight(context) / 2, 8, 255,
		    "no systemd", 0);
	draw();
	bbwait(1.5 * 1000000);

	/* straight through the bird */
	drawptr = draw_peng;
	zoom_from = 1.0;
	zoom_to = 14.0;
	spin = 0.05;
	timestuff(0, NULL, draw, 2 * 1000000);

	params->bright = 0;
	params->contrast = 0;
}

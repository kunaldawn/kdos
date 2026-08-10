/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * Rain. This is the transition the portrait arrives through — the same
 * crystallise the author's GitHub profile card animates, which is why the four
 * baked head frames are rain-formation stages rather than four photographs.
 *
 * Drawn in image space, not as characters: a bright head with a decaying
 * trail is a brightness field, and letting aalib choose the glyph for each
 * cell is the whole point of rendering through it.
 */

#include <stdlib.h>
#include <string.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#define MAXCOL 512

static struct {
	double y;	/* head position, in rows                       */
	double v;	/* fall speed, rows per second                  */
	int len;	/* trail length                                 */
} drop[MAXCOL];

static int ncol, lastw, last_time;
static double converge;	/* 0 = pure rain, 1 = every column parked        */

/* Seconds since the previous frame. The scenes here free-run — on a stdout
 * driver they hit thousands of frames a second and on a TTY tens — so
 * anything that integrates has to integrate against the clock, the way every
 * upstream scene interpolates against TIME. A fixed per-frame step makes the
 * rain fall at the speed of the terminal. */
static double frame_dt(void)
{
	double dt = (TIME - last_time) / 1000000.0;

	last_time = TIME;
	if (dt <= 0 || dt > 0.25)	/* first frame, or a long stall */
		dt = 1.0 / 40.0;
	return dt;
}

static void seed(int i, int h)
{
	drop[i].y = -(rand() % (h * 2));
	drop[i].v = 6.0 + (rand() % 100) / 4.0;
	drop[i].len = 4 + rand() % 14;
}

static void draw_rain(void)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	int i, k;
	double dt = frame_dt();

	if (w != lastw) {
		lastw = w;
		ncol = w > MAXCOL ? MAXCOL : w;
		for (i = 0; i < ncol; i++)
			seed(i, h);
	}

	clrscr();
	for (i = 0; i < ncol; i++) {
		int x = i * w / ncol;

		drop[i].y += drop[i].v * dt * (1.0 - converge * 0.85);
		if (drop[i].y - drop[i].len > h) {
			if (converge > 0.9)
				continue;	/* let the screen empty out */
			seed(i, h);
			drop[i].y = -(rand() % 20);
		}
		for (k = 0; k < drop[i].len; k++) {
			int y = (int)drop[i].y - k;
			int v;

			if (y < 0 || y >= h)
				continue;
			/* head white-hot, trail falling off linearly */
			v = k == 0 ? 255 : 200 - (200 * k) / drop[i].len;
			if (v < 0)
				v = 0;
			b[y * w + x] = v;
		}
	}
}

void fx_matrix(void)
{
	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->dither = AA_FLOYD_S;
	params->randomval = 0;
	lastw = 0;
	last_time = TIME;
	converge = 0;

	drawptr = draw_rain;
	timestuff(0, NULL, draw, 4 * 1000000);

	/* the rain slows and thins — the portrait comes out of this */
	converge = 0.6;
	timestuff(0, NULL, draw, 2 * 1000000);
	converge = 1.0;
	timestuff(0, NULL, draw, 1.5 * 1000000);

	drawptr = NULL;
	params->bright = 0;
	params->contrast = 0;
}

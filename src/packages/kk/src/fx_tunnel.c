/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * A tunnel, the 1994 kind: per-pixel polar coordinates into a checker texture,
 * with the depth term animated. Upstream BB has a plasma and a zoomer but no
 * tunnel, so this adds rather than repeats.
 *
 * The angle/depth tables are rebuilt only when the terminal is resized, which
 * on a TTY means once — the per-frame cost is the texture lookup and nothing
 * else, and that matters when the whole point is measuring how fast a
 * character-cell display can go.
 */

#include <math.h>
#include <stdlib.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#define STATE (TIME - starttime)

static int *tab_depth, *tab_angle;
static int tw, th;

static void tables(int w, int h)
{
	int x, y;

	if (tab_depth && w == tw && h == th)
		return;
	free(tab_depth);
	free(tab_angle);
	tw = w;
	th = h;
	tab_depth = malloc(sizeof(int) * w * h);
	tab_angle = malloc(sizeof(int) * w * h);

	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* halve x: an image-buffer pixel is half a cell wide */
			double dx = (x - w / 2.0) * 0.5;
			double dy = y - h / 2.0;
			double r = sqrt(dx * dx + dy * dy);
			double a = atan2(dy, dx);

			if (r < 1.0)
				r = 1.0;
			tab_depth[y * w + x] = (int)(2048.0 / r);
			tab_angle[y * w + x] = (int)(a / (2 * M_PI) * 256.0 + 256.0);
		}
}

static void draw_tunnel(void)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	int t = STATE / 12000;
	int i, n = w * h;

	tables(w, h);
	for (i = 0; i < n; i++) {
		int d = tab_depth[i] + t;
		int a = tab_angle[i] + t / 3;
		int on = ((d >> 4) ^ (a >> 4)) & 1;
		/* fade the far end out so the mouth of the tunnel reads */
		int fade = tab_depth[i] > 255 ? 255 : tab_depth[i];

		b[i] = on ? fade : fade / 5;
	}
}

void fx_tunnel(void)
{
	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->randomval = 0;
	params->dither = AA_FLOYD_S;

	drawptr = draw_tunnel;
	timestuff(0, NULL, draw, 6 * 1000000);

	drawptr = NULL;
	params->bright = 0;
	params->contrast = 0;
}

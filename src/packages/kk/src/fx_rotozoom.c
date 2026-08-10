/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * A rotozoomer over a generated texture. The texture is not an asset: it is
 * the KDOS wordmark tiled under a plasma, computed once at startup, so the
 * scene costs nothing in the repo and still shows something that is ours.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#define STATE (TIME - starttime)
#define TEX 128

static unsigned char tex[TEX * TEX];
static int made;

/* Coarse 5x19 "KDOS", stamped into the tile. Same glyph grid the intro uses,
 * kept local rather than shared: this one is a texture, and wanting it bolder
 * or dimmer here should not move the wordmark on the title screen. */
static const char *const KDOS[] = {
	"#  # ###   ##   ###",
	"# #  #  # #  # #   ",
	"##   #  # #  #  ## ",
	"# #  #  # #  #    #",
	"#  # ###   ##  ### ",
};

static void make_tex(void)
{
	int x, y, r, c;

	if (made)
		return;
	made = 1;
	for (y = 0; y < TEX; y++)
		for (x = 0; x < TEX; x++) {
			double v = sin(x / 7.0) + sin(y / 9.0) +
				   sin((x + y) / 11.0);
			tex[y * TEX + x] = (unsigned char)(70 + v * 22);
		}
	for (r = 0; r < 5; r++)
		for (c = 0; c < 19; c++) {
			if (KDOS[r][c] != '#')
				continue;
			for (y = 0; y < 12; y++)
				for (x = 0; x < 6; x++) {
					int px = 10 + c * 6 + x;
					int py = 34 + r * 12 + y;
					if (px < TEX && py < TEX)
						tex[py * TEX + px] = 255;
				}
		}
}

static void draw_roto(void)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	double a = STATE / 700000.0;
	double z = 1.6 + 1.3 * sin(STATE / 1100000.0);
	double ca = cos(a) * z, sa = sin(a) * z;
	int px, py;

	for (py = 0; py < h; py++)
		for (px = 0; px < w; px++) {
			double dx = (px - w / 2.0) * 0.5;
			double dy = py - h / 2.0;
			int sx = (int)(dx * ca - dy * sa) & (TEX - 1);
			int sy = (int)(dx * sa + dy * ca) & (TEX - 1);

			b[py * w + px] = tex[sy * TEX + sx];
		}
}

void fx_rotozoom(void)
{
	make_tex();
	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->randomval = 0;
	params->dither = AA_FLOYD_S;

	drawptr = draw_roto;
	timestuff(0, NULL, draw, 6 * 1000000);

	drawptr = NULL;
	params->bright = 0;
	params->contrast = 0;
}

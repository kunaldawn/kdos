/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * The opening: a CRT waking up. Same three beats as kdos-splash's power-on —
 * a hairline that snaps open into a full raster, the wordmark burning in, then
 * the tagline — because the boot splash, the login banner and this demo are
 * meant to read as one machine.
 *
 * Everything is drawn into aalib's greyscale imagebuffer and rendered as
 * characters; the phosphor colour comes from the VT palette KDOS already
 * loaded, so there is nothing to tint here.
 */

#include <math.h>
#include <string.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#define STATE (TIME - starttime)

static double open_from, open_to;

/* The wordmark, one row per scanline of block art. Deliberately not the
 * six-line ANSI banner from the repo header: at 80 columns that would need
 * every cell, and the demo renders text as IMAGE, not as characters. */
static const char *const KDOS[] = {
	"#  # ###   ##   ###",
	"# #  #  # #  # #   ",
	"##   #  # #  #  ## ",
	"# #  #  # #  #    #",
	"#  # ###   ##  ### ",
};
#define KDOS_ROWS 5
#define KDOS_COLS 19

static void hline(int y, int x1, int x2, int v)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), x;

	if (y < 0 || y >= aa_imgheight(context))
		return;
	if (x1 < 0)
		x1 = 0;
	if (x2 > w)
		x2 = w;
	for (x = x1; x < x2; x++)
		b[y * w + x] = v;
}

/* The raster opening up: a bright line at mid-height that grows into the
 * full frame, with the leading edges kept brighter than the fill — the same
 * "beam ahead of the sweep" the login banner draws. */
static void draw_open(void)
{
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	int mid = h / 2, y;
	double f = open_from + (open_to - open_from) *
		   (double)STATE / (double)(endtime - starttime + 1);
	int half;

	if (f < 0)
		f = 0;
	if (f > 1)
		f = 1;
	half = (int)(f * h / 2);

	clrscr();
	for (y = mid - half; y <= mid + half; y++) {
		int d = abs(y - mid);
		int v = 90 + (int)(60.0 * sin(STATE / 90000.0 + y * 0.3));
		if (half && d > half - 2)
			v = 255;	/* the sweeping edge */
		hline(y, 0, w, v);
	}
	if (!half)
		hline(mid, 0, w, 255);
}

/* Burn the wordmark in over the raster, cell by cell, left to right. */
static void draw_wordmark(void)
{
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	int cw = w / (KDOS_COLS + 4), ch = h / (KDOS_ROWS + 6);
	int x0 = (w - cw * KDOS_COLS) / 2, y0 = (h - ch * KDOS_ROWS) / 2;
	int r, c, x, y;
	int lit = (int)((double)STATE / 40000.0);

	draw_open();
	for (r = 0; r < KDOS_ROWS; r++)
		for (c = 0; c < KDOS_COLS; c++) {
			int v;
			if (KDOS[r][c] != '#')
				continue;
			if (c > lit)
				continue;
			/* The cell that just lit is hotter than the rest —
			 * a phosphor that has not settled yet. */
			v = (c == lit) ? 255 : 200;
			for (y = 0; y < ch; y++)
				for (x = 0; x < cw; x++)
					hline(y0 + r * ch + y, x0 + c * cw + x,
					      x0 + c * cw + x + 1, v);
		}
}

void kdos_intro(void)
{
	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->randomval = 0;
	params->dither = AA_FLOYD_S;

	/* 1. the hairline snaps open */
	drawptr = draw_open;
	open_from = 0.0;
	open_to = 1.0;
	timestuff(0, NULL, draw, 1.2 * 1000000);

	/* 2. the wordmark burns in and holds */
	drawptr = draw_wordmark;
	open_from = open_to = 1.0;
	timestuff(0, NULL, draw, 2.5 * 1000000);

	/* 3. the tagline, on the raster, with the demo's own text pipeline */
	drawptr = NULL;
	clrscr();
	centerprint(aa_imgwidth(context) / 2, aa_imgheight(context) / 2, 6, 255,
		    "I use KDOS btw.", 0);
	draw();
	bbwait(2 * 1000000);

	params->bright = 0;
	params->contrast = 0;
}

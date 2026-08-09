/*
 * KK — the KDOS demo.  Forked from BB (C) 1997 AA-group; see LICENSE.notice.
 *
 * The one scene that listens. One bar per module voice, height from
 * Voice_RealVolume(), plus the pattern/row read straight off the player so the
 * flash lands on the beat rather than on a wall clock the music has drifted
 * away from.
 *
 * It has to work with the sound off too — `kk --no-sound`, a machine with no
 * card, an appbox with no /dev/snd. When there is no player the bars are
 * driven by a synthetic envelope instead, which is honest: the scene claims to
 * visualize the audio, and with no audio it says so by looking generated.
 */

#include <math.h>
#include <string.h>
#include "bb.h"
#include "kk.h"

/* draw() centres this caption every frame. Declared here rather than in bb.h
 * because three upstream files use `text` as a local of a different type. */
extern char *text;

#ifdef HAVE_LIBMIKMOD
#include <mikmod.h>
extern MODULE *module;
#endif

#define STATE (TIME - starttime)
#define BARS 32

static double level[BARS];
static int beat_row = -1, last_time;
static double flash;

/* See fx_matrix.c: these scenes free-run, so decay is per second, not per
 * frame. At 3000 fps a per-frame 0.82 kills a bar in three milliseconds. */
static double frame_dt(void)
{
	double dt = (TIME - last_time) / 1000000.0;

	last_time = TIME;
	if (dt <= 0 || dt > 0.25)
		dt = 1.0 / 40.0;
	return dt;
}

static void sample_levels(void)
{
	int i;
	double dt = frame_dt();
	double keep = exp(-dt * 6.0);	/* ~0.82 at 40 fps */

	if (flash > 0)
		flash -= dt;

#ifdef HAVE_LIBMIKMOD
	if (bbsound && module && Player_Active()) {
		int voices = module->numchn > 0 ? module->numchn : 1;
		int row = Player_GetRow();

		for (i = 0; i < BARS; i++) {
			int v = i * voices / BARS;
			double lv = (double)Voice_RealVolume(v) / 16384.0;

			if (lv > 1.0)
				lv = 1.0;
			/* attack instantly, decay slowly: a bar that tracks
			 * the mixer exactly just flickers */
			level[i] = lv > level[i] ? lv : level[i] * keep;
		}
		if (row != beat_row) {
			beat_row = row;
			if (!(row & 3))
				flash = 0.15;
		}
		return;
	}
#endif
	for (i = 0; i < BARS; i++) {
		double p = STATE / 260000.0 + i * 0.4;
		double lv = 0.5 + 0.5 * sin(p) * sin(p * 0.31 + 1.0);

		level[i] = lv > level[i] ? lv : level[i] * 0.85;
	}
	if (!((STATE / 500000) % 2) && flash <= 0)
		flash = 0.15;
}

static void draw_scope(void)
{
	unsigned char *b = context->imagebuffer;
	int w = aa_imgwidth(context), h = aa_imgheight(context);
	int i, x, y;
	int base = h - h / 8;

	sample_levels();
	clrscr();

	/* baseline — the scope's own graticule */
	for (x = 0; x < w; x++)
		b[base * w + x] = 60;

	for (i = 0; i < BARS; i++) {
		int x0 = i * w / BARS, x1 = (i + 1) * w / BARS - 1;
		int top = base - (int)(level[i] * (base - h / 10));

		if (x1 <= x0)
			x1 = x0 + 1;
		for (y = top; y < base; y++) {
			int v = y == top ? 255 : 120 + (base - y) * 90 / (base + 1);

			for (x = x0; x < x1 && x < w; x++)
				b[y * w + x] = v;
		}
		/* mirrored below the line, dimmer — a reflection on glass */
		for (y = base + 1; y < base + (base - top) / 4 && y < h; y++)
			for (x = x0; x < x1 && x < w; x++)
				b[y * w + x] = 40;
	}

	params->bright = flash > 0 ? 60 : 0;
}

void fx_scope(void)
{
	int i;

	for (i = 0; i < BARS; i++)
		level[i] = 0;
	beat_row = -1;
	last_time = TIME;
	flash = 0;

	text = "";
	params->bright = 0;
	params->contrast = 0;
	params->randomval = 0;
	params->dither = AA_FLOYD_S;

	drawptr = draw_scope;
	timestuff(0, NULL, draw, 7 * 1000000);

	drawptr = NULL;
	params->bright = 0;
	params->contrast = 0;
}

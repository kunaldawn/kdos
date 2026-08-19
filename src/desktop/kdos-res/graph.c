/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The one chart in this program, drawn at two tiers.
 *
 * CELLS FIRST. ktui_sparkline at whatever glyph tier ktui_ramp_init() chose is
 * the whole chart on tty1 and in every golden, and it is what the layout is
 * measured against. The pixel tile is an ENHANCEMENT laid over exactly the
 * same rectangle: kch_tile_slot() answering -1 is a terminal, `icons = no`, a
 * missing font and a full sprite table, and every one of those must produce
 * the same layout with the glyph chart in it.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kcell.h"
#include "res.h"

/*
 * A gridline every ten seconds, keyed to the ABSOLUTE sample number so it
 * marches left with the samples beside it. A flat band with a stationary
 * gridline reads as a program that has stopped.
 */
static int grid_period(void)
{
	int n = 10000 / (RC.interval_ms > 0 ? RC.interval_ms : 1000);
	return n < 2 ? 2 : n;
}

static void graph_tile(int id, KRect r, const KprHist *h, double vmax)
{
	KCellCanvas *cv = kch_tile_begin(id, r.w, r.h, (uint64_t)h->seq);
	if (!cv)
		return;

	int W = kcell_canvas_w(cv), H = kcell_canvas_h(cv);
	kcell_canvas_clear(cv);

	/*
	 * The plot area is drawn even when the plot is not: six percent of a
	 * band is a few pixels along the bottom edge, and without a backdrop
	 * and a baseline an idle machine shows a label, a number and nothing
	 * else. Both go down BEFORE the trace, or they paint over the very
	 * line they exist to make visible.
	 */
	kcell_canvas_fill(cv, 0, 0, W, H, KT_SURFACE, 40);
	kcell_canvas_fill(cv, 0, H - 1, W, 1, KT_DIM, 160);

	int period = grid_period();
	for (int i = 0; i < W; i++) {
		unsigned long long abs = h->seq > (unsigned long long)(W - i)
					 ? h->seq - (unsigned long long)(W - i) : 0;
		if (abs && abs % (unsigned long long)period == 0)
			kcell_canvas_fill(cv, i, 0, 1, H, KT_DIM, 60);
	}

	if (vmax <= 0.0)
		vmax = 1.0;

	int prev_top = -1;
	for (int i = 0; i < W; i++) {
		/* One sample per pixel, oldest at the left. */
		int idx = h->n - W + i;
		if (idx < 0)
			continue;
		double v = kpr_hist_smooth(h, idx);
		if (v < 0.0)
			v = 0.0;
		int px = (int)((v / vmax) * (double)(H - 1) + 0.5);
		if (px > H - 1)
			px = H - 1;
		/*
		 * A sample worth less than a pixel is drawn as one: a dribble
		 * would otherwise be a row of isolated dots blinking in and
		 * out, and every column puts the line down even at zero so
		 * the trace is continuous.
		 */
		if (px < 1 && v > 0.0)
			px = 1;
		int top = H - 1 - px;

		kcell_canvas_fill(cv, i, top, 1, H - top, KT_ACCENT, 80);
		/* A steep edge draws down to the previous column's top, so the
		 * line is continuous rather than a stack of dots. */
		if (prev_top >= 0 && prev_top != top) {
			int a = top < prev_top ? top : prev_top;
			int b = top < prev_top ? prev_top : top;
			kcell_canvas_fill(cv, i, a, 1, b - a + 1, KT_ACCENT, 255);
		} else {
			kcell_canvas_fill(cv, i, top, 1, 1, KT_ACCENT, 255);
		}
		prev_top = top;
	}
	kch_tile_commit(id);
}

/*
 * Draw one chart into `r`. `id` is the tile identity and must be stable for
 * a given chart across frames, or the sprite slot churns.
 */
void res_graph(int id, KRect r, const KprHist *h, const char *label,
	       const char *reading)
{
	if (r.w < 4 || r.h < 2)
		return;

	double vmax = h->pinned ? 100.0 : kpr_hist_scale((KprHist *)h);

	/* The cells. This is the chart on tty1 and in the goldens. */
	double v[KPR_HIST];
	int n = h->n < r.w ? h->n : r.w;
	for (int i = 0; i < n; i++)
		v[i] = kpr_hist_at(h, h->n - n + i);
	ktui_sparkline(r, v, n, vmax, KT_BG);

	/*
	 * The tile is asked for, never required. -1 is a terminal, `icons =
	 * no`, no font and a full table, and the cell chart above is already
	 * the whole picture in every one of those.
	 */
	if (kch_tile_slot(id) >= 0)
		graph_tile(id, r, h, vmax);

	/*
	 * The label goes top-left and the reading top-right, and the label is
	 * DROPPED rather than overlapped when the band is too narrow for both:
	 * a meter whose unit is not obvious is the one that needs its name,
	 * and two strings colliding read as a rendering fault.
	 */
	if (label && r.w >= 12)
		ktui_draw_text(r.x, r.y, r.w / 2, label, KT_MID, KT_BG, 0);
	if (reading)
		ktui_draw_text_right(r.x, r.y, r.w, reading, KT_TEXT, KT_BG, 0);
}

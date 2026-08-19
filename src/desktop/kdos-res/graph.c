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
 * The one chart in this program, and it is CELLS all the way down.
 *
 * A FULL-WIDTH CHART CANNOT BE A SPRITE TILE. libktui encodes a sprite cell's
 * sub-cell coordinate in four bits each way, so `kch_tile_begin()` refuses
 * anything past 16x16 CELLS — that is what the panel's fifteen-cell meters
 * strip is sized around. A page-wide chart here is seventy cells and more, so
 * the pixel tier is not merely unused, it is unreachable: there is no tile to
 * enhance. The area chart below is therefore the whole picture on tty1, in a
 * window and in every golden, which is also what makes the goldens worth
 * having.
 *
 * `ktui_sparkline` is ONE ROW — one ramp cell per column, drawn at `r.y`. It
 * is right for a status line and wrong for a band: handed ten rows it draws in
 * the first of them and leaves nine empty, which on a real screen reads as a
 * chart that is not working. This file draws the band itself.
 */

#include <stdio.h>
#include <string.h>

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

/*
 * One series as an AREA, bottom-aligned, newest sample at the RIGHT.
 *
 * Right-aligned for `ktui_sparkline`'s reason: a window that is still filling
 * must not slide its history sideways as it fills. Whole rows are the full
 * block and the top row of each column is the ramp glyph for what is left
 * over, so the resolution is `rows * ktui_ramp_levels()` — eight levels a row
 * in a modern terminal, three on the console font, and the shape survives
 * both.
 */
static void graph_cells(KRect r, const KprHist *h, double vmax, int colour,
			int bg)
{
	if (r.w < 1 || r.h < 1 || h->n < 1)
		return;
	if (vmax <= 0.0)
		vmax = 1.0;

	int cols = h->n < r.w ? h->n : r.w;
	int from = h->n - cols;
	int x0 = r.x + (r.w - cols);
	int period = grid_period();
	int levels = ktui_ramp_levels();

	for (int i = 0; i < cols; i++) {
		int cx = x0 + i;

		/*
		 * The gridline goes down FIRST and only where the area will
		 * not cover it: drawn after, it would paint over the very
		 * trace it exists to give a scale to.
		 */
		unsigned long long abs = h->seq > (unsigned long long)(cols - i)
					 ? h->seq - (unsigned long long)(cols - i)
					 : 0;
		int grid = abs && abs % (unsigned long long)period == 0;

		double v = kpr_hist_smooth(h, from + i);
		if (v < 0.0)
			v = 0.0;
		double f = v / vmax;
		if (f > 1.0)
			f = 1.0;

		double total = f * (double)r.h;		/* in rows */
		int full = (int)total;
		double frac = total - (double)full;

		if (full > r.h) {
			full = r.h;
			frac = 0.0;
		}
		/*
		 * A sample worth less than one ramp level is drawn as one: a
		 * dribble would otherwise be indistinguishable from silence,
		 * and "is anything happening at all" is the first question a
		 * chart is asked.
		 */
		if (full == 0 && frac * levels < 1.0 && v > 0.0)
			frac = 1.0 / (double)levels;

		if (grid)
			for (int rr = 0; rr < r.h; rr++)
				ktui_draw_text(cx, r.y + rr, 1,
					       ktui_glyph[KT_G_DOT], KT_DIM,
					       bg, 0);

		for (int rr = 0; rr < full && rr < r.h; rr++)
			ktui_draw_text(cx, r.y + r.h - 1 - rr, 1,
				       ktui_ramp_v(1.0), colour, bg, 0);
		if (frac > 0.0 && full < r.h)
			ktui_draw_text(cx, r.y + r.h - 1 - full, 1,
				       ktui_ramp_v(frac), colour, bg, 0);
	}
}

/*
 * Draw one chart into `r`. `id` is kept in the signature because every caller
 * names its chart and a stable identity is what a future renderer would need;
 * nothing here uses it.
 *
 * THE TOP ROW IS THE LABEL'S. The plot starts one row below it — sharing the
 * row put a run of ramp glyphs through the reading, which is what a chart
 * drawn by a one-row widget into a ten-row band looked like.
 */
void res_graph(int id, KRect r, const KprHist *h, const char *label,
	       const char *reading)
{
	(void)id;
	if (r.w < 4 || r.h < 2)
		return;

	double vmax = h->pinned ? 100.0 : kpr_hist_scale((KprHist *)h);

	graph_cells(krect(r.x, r.y + 1, r.w, r.h - 1), h, vmax, KT_ACCENT,
		    KT_BG);

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

/*
 * ── the pair ─────────────────────────────────────────────────────────────
 *
 * Two series on ONE shared axis: received above in the accent, sent below in
 * the secondary. Summing them instead is what hides the only thing anybody
 * watches a disk or a link for — "269 kB/s" does not say whether this machine
 * is reading or being read from, and those have different answers.
 *
 * ONE AXIS, not two. Scaled separately, a quiet direction is drawn at the same
 * height as a busy one and the picture says they are equal.
 *
 * STACKED, NOT MIRRORED, and that is the cell grid being honest. A mirror
 * needs the lower series to grow DOWNWARD from a midline, and the ramp is
 * bottom-aligned by construction — `ktui_ramp_v` has no upside-down twin, and
 * inventing one would mean glyphs the 512-glyph console font does not carry.
 * Two bands in a fixed order on one scale answer the same question.
 */
struct pair_scale { int id; double scale; };
static struct pair_scale g_pair[16];

static double *pair_scale_of(int id)
{
	for (int i = 0; i < (int)(sizeof(g_pair) / sizeof(g_pair[0])); i++) {
		if (g_pair[i].id == id)
			return &g_pair[i].scale;
		if (!g_pair[i].id) {
			g_pair[i].id = id;
			return &g_pair[i].scale;
		}
	}
	/* A full table is a chart that rescales every frame rather than a
	 * chart that is not drawn: the last slot is shared, which is wrong
	 * quietly rather than absent loudly. */
	return &g_pair[0].scale;
}

void res_graph2(int id, KRect r, const KprHist *a, const KprHist *b,
		const char *label, const char *reading)
{
	if (r.w < 4 || r.h < 3)
		return;

	double *kept = pair_scale_of(id);
	double pa = kpr_hist_peak(a), pb = kpr_hist_peak(b);
	double vmax = kpr_scale_step(pa > pb ? pa : pb, *kept);

	*kept = vmax;

	int plot_h = r.h - 1;			/* the top row is the label's */
	int top_h = plot_h / 2;
	int bot_h = plot_h - top_h;

	if (top_h < 1) {
		top_h = plot_h;
		bot_h = 0;
	}

	graph_cells(krect(r.x, r.y + 1, r.w, top_h), a, vmax, KT_ACCENT,
		    KT_BG);
	if (bot_h > 0)
		graph_cells(krect(r.x, r.y + 1 + top_h, r.w, bot_h), b, vmax,
			    KT_WARN, KT_BG);

	if (label && r.w >= 12)
		ktui_draw_text(r.x, r.y, r.w / 2, label, KT_MID, KT_BG, 0);
	if (reading)
		ktui_draw_text_right(r.x, r.y, r.w, reading, KT_TEXT, KT_BG, 0);
}

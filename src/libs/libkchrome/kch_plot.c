/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkchrome — the chart, drawn as pixels in a tile
 *
 * ONE CHART RENDERER FOR THE WHOLE DESKTOP. The panel's meters and every
 * chart in kdos-res are this file, so an area chart means the same picture
 * wherever it is: a faint plate, gridlines that march with the samples, a
 * base line, the area at a third of the colour's weight and the trace at
 * full weight over it.
 *
 * THE LAYERS, BOTTOM UP, and the order is the argument:
 *
 *   - The PLATE, so an idle chart is still a box with something in it. An
 *     area chart at a few percent is a few pixels along an edge, and without
 *     the plate a quiet machine shows a label, a number and nothing at all.
 *   - The GRIDLINES, keyed to the ABSOLUTE sample number so they travel left
 *     with the samples they were drawn beside. Flat data draws the same
 *     picture whatever it does next; the grid is what moves on a quiet chart,
 *     and on a scrolling plot the scale is what shows the scroll.
 *   - The BASE line (the midline of a pair), under the trace: at rest the two
 *     share a row, and drawn afterwards it would paint over the very trace the
 *     chart exists to show.
 *   - The SERIES, through kcell_canvas_series(): the only antialiased pixels
 *     here. The plate, grid and base are chrome and stay hard-edged fills.
 *   - The MARK, last and over the trace: one sample a reading is about (a
 *     pointer resting on the chart, a keyboard scrub), a hard-edged line
 *     like the gridlines it is the width of.
 *
 * A PAIR IS MIRRORED, on ONE axis: the first series grows up from a midline
 * and the second grows down from it. Scaled separately, a trickle and a
 * torrent would be drawn the same height and the picture would say they were
 * equal; summed, "269 kB/s" would not say which direction it was.
 *
 * THE TILE IS NEVER GUARANTEED. kch_plot() answers -1 on a terminal, in a
 * dump, under `icons = no` and on a full table, and every caller keeps its
 * cell chart for that answer — which is also what the goldens assert.
 * ---------------------------------
 */

#include <string.h>

#include "kcell.h"
#include "kchrome.h"

static uint64_t fnv_u64(uint64_t h, uint64_t v)
{
	for (int i = 0; i < 8; i++) {
		h ^= (v >> (i * 8)) & 0xff;
		h *= 0x100000001b3ULL;
	}
	return h;
}

static uint64_t fnv_dbl(uint64_t h, double d)
{
	uint64_t v;

	memcpy(&v, &d, sizeof(v));
	return fnv_u64(h, v);
}

/*
 * EVERY INPUT THE RASTER READS, AND NOTHING QUANTISED. The raster is a pure
 * function of these, so a hash over their exact bits re-rasters exactly when
 * the picture could have changed and never otherwise. The sample NUMBER is in
 * it because the gridlines move with it — that is what re-rasters a flat
 * chart. The palette is NOT: a theme change drops every tile through
 * kch_tile_reset(), one mechanism for icons and tiles alike.
 */
uint64_t kch_plot_hash(const KchPlot *p)
{
	uint64_t h = 0xcbf29ce484222325ULL;

	if (!p)
		return h;
	h = fnv_u64(h, (uint64_t)(unsigned)p->na);
	for (int i = 0; p->a && i < p->na; i++)
		h = fnv_dbl(h, p->a[i]);
	h = fnv_u64(h, (uint64_t)(unsigned)(p->b ? p->nb : -1));
	for (int i = 0; p->b && i < p->nb; i++)
		h = fnv_dbl(h, p->b[i]);
	h = fnv_dbl(h, p->vmax);
	h = fnv_dbl(h, p->step);
	h = fnv_dbl(h, p->line_w);
	h = fnv_u64(h, p->seq);
	{
		const int k[] = { p->slot_a, p->slot_b, p->per_cell, p->hold,
				  p->area_alpha, p->line_alpha, p->rest_alpha,
				  p->grid, p->grid_slot, p->grid_alpha,
				  p->plate_slot, p->plate_alpha, p->base_slot,
				  p->base_alpha, p->mark, p->mark_slot,
				  p->mark_alpha };

		for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); i++)
			h = fnv_u64(h, (uint64_t)(unsigned)k[i]);
	}
	return h;
}

/*
 * One gridline per `grid` samples, at the sample's own column. The virtual
 * samples left of the oldest are gridded too when the series is HELD to the
 * left edge — a scale that stopped where the samples ran out would say the
 * left of the chart was outside time — and are not when it is not, because
 * that part of the band is empty.
 */
static void plot_grid(KCellCanvas *cv, int x, int y, int w, int h,
		      const KchPlot *p, int n, double step)
{
	double s = step > 0.0 ? step : 1.0;
	int span = (int)((double)w / s) + 2;
	int gw = s >= 4.0 ? 2 : 1;

	if (p->grid <= 0 || p->grid_alpha <= 0 || n < 1)
		return;
	if (!p->hold && span > n)
		span = n;
	for (int back = 0; back < span; back++) {
		unsigned long long abs;
		int gx;

		if (p->seq < (unsigned long long)back + 1)
			break;
		abs = p->seq - (unsigned long long)back;
		if (abs % (unsigned long long)p->grid)
			continue;
		/* The sample's centre, counted back from the right edge — the
		 * same centre kcell_canvas_series() puts the sample at. */
		double c = (double)w - (double)back * s - s / 2.0;

		if (c < 0.0)
			break;
		gx = x + (int)c - gw / 2;
		kcell_canvas_fill(cv, gx < x ? x : gx, y,
				  gx < x ? gw - (x - gx) : gw, h, p->grid_slot,
				  p->grid_alpha);
	}
}

/* The marked sample's line, at the centre plot_grid() and the series use,
 * as wide as a gridline. */
static void plot_mark(KCellCanvas *cv, int x, int y, int w, int h,
		      const KchPlot *p, double step)
{
	double s = step > 0.0 ? step : 1.0;
	int gw = s >= 4.0 ? 2 : 1;
	double c = (double)w - (double)(p->mark - 1) * s - s / 2.0;
	int gx;

	if (p->mark < 1 || p->mark_alpha <= 0 || c < 0.0)
		return;
	gx = x + (int)c - gw / 2;
	kcell_canvas_fill(cv, gx < x ? x : gx, y, gx < x ? gw - (x - gx) : gw,
			  h, p->mark_slot, p->mark_alpha);
}

static void plot_series(KCellCanvas *cv, int x, int y, int w, int h,
			const double *v, int n, const KchPlot *p, double step,
			int slot, int mirror)
{
	KCellSeries s = {
		.v = v,
		.n = n,
		.vmax = p->vmax,
		.step = step,
		.mode = KCELL_SERIES_BOTH | (mirror ? KCELL_SERIES_MIRROR : 0) |
			(p->hold ? KCELL_SERIES_HOLD : 0),
		.area_alpha = p->area_alpha,
		.line_alpha = p->line_alpha,
		.rest_alpha = p->rest_alpha,
		.line_w = p->line_w,
	};

	if (v && n > 0 && w > 0 && h > 0)
		kcell_canvas_series(cv, x, y, w, h, &s, slot);
}

void kch_plot_draw(KCellCanvas *cv, int x, int y, int w, int h,
		   const KchPlot *p)
{
	double step = p && p->step > 0.0 ? p->step : 1.0;

	if (!cv || !p || w < 1 || h < 1)
		return;
	/* Clipped to the plot, so a gridline or a wide trace cannot reach the
	 * neighbour a caller put beside it in the same canvas. */
	if (kcell_canvas_clip_push(cv, x, y, w, h) != 0)
		return;
	if (p->plate_alpha > 0)
		kcell_canvas_fill(cv, x, y, w, h, p->plate_slot,
				  p->plate_alpha);
	plot_grid(cv, x, y, w, h, p,
		  p->na > p->nb || !p->b ? p->na : p->nb, step);

	if (!p->b) {
		if (p->base_alpha > 0)
			kcell_canvas_fill(cv, x, y + h - 1, w, 1, p->base_slot,
					  p->base_alpha);
		plot_series(cv, x, y, w, h, p->a, p->na, p, step, p->slot_a,
			    0);
	} else {
		/* The midline is the zero of BOTH halves, and the lower half
		 * starts on the row under it, so a quiet pair is a dim
		 * midline with a trace either side of it rather than one line
		 * wearing two colours. */
		int half = h / 2;

		if (p->base_alpha > 0)
			kcell_canvas_fill(cv, x, y + half, w, 1, p->base_slot,
					  p->base_alpha);
		plot_series(cv, x, y, w, half, p->a, p->na, p, step,
			    p->slot_a, 0);
		plot_series(cv, x, y + half + 1, w, h - half - 1, p->b, p->nb,
			    p, step, p->slot_b, 1);
	}
	plot_mark(cv, x, y, w, h, p, step);
	kcell_canvas_clip_pop(cv);
}

/*
 * The chart as a tile. The geometry and the hash are settled BEFORE the tile
 * is claimed: a tile handed out and then not drawn believes it holds this
 * content, and the next frame would present a stale slot.
 */
int kch_plot(int id, KRect r, const KchPlot *p, int fg, int bg)
{
	KCellCanvas *cv;

	/* The hash reads every sample, and a terminal would pay it every
	 * frame for a tile it can never show. */
	if (!p || r.w < 1 || r.h < 1 || !kicon_enabled())
		return -1;
	cv = kch_tile_begin(id, r.w, r.h, kch_plot_hash(p));
	if (cv) {
		int W = kcell_canvas_w(cv), H = kcell_canvas_h(cv);
		KchPlot q = *p;

		/* One sample per CELL column is the cell chart's own time
		 * scale, so the two tiers show the same window of history. */
		if (p->per_cell)
			q.step = (double)(W / r.w);
		kch_plot_draw(cv, 0, 0, W, H, &q);
		kch_tile_commit(id);
	}
	return kch_tile_draw(id, r, fg, bg) == 0 ? 0 : -1;
}

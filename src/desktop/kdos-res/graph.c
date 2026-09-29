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
 * The one chart in this program, in two tiers: PIXELS where the display has
 * them, CELLS everywhere else.
 *
 * THE PIXEL TIER is libkchrome's kch_plot() — the renderer the panel's meters
 * use too, so an area chart is one picture on this desktop: an antialiased
 * trace over its area, a plate, gridlines that march with the samples. It is
 * drawn as a tile over the plot's cells, one sample per CELL column, so both
 * tiers show the same stretch of history and the layout never depends on
 * which one drew.
 *
 * THE CELL TIER below is the whole picture on tty1, in `--dump` and in every
 * golden, which is also what makes the goldens worth having — and it is the
 * answer whenever a tile is not up: no pixel cell, no icon artwork, a full
 * sprite table. A tile is never guaranteed, so these cells are never dead
 * code.
 *
 * ONE SAMPLE, READ OFF THE CHART: a pointer resting on a chart with a
 * formatter, or a keyboard scrub on it, picks a sample; its column is marked
 * — the cells reversed, a line through the tile — and the reading in the
 * label row becomes that sample's age and value. The reading is cells in both
 * tiers, so tty1 and a dump say it too.
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
 * in a modern terminal, three on a VT font, and the shape survives both.
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

/* ── the pixel tier ────────────────────────────────────────────────────── */

/*
 * WHICH CHARTS HOLD A TILE, AND WHICH WERE DRAWN SINCE THE LAST SWEEP.
 *
 * A tile keeps two canvases the size of its chart for as long as it lives —
 * megabytes for a page-wide chart, four times that on a doubled output — and
 * a page that is not on the screen draws none of its charts. So a chart not
 * drawn in a frame gives its tile back after that frame is presented, and
 * pays one raster when its page comes back.
 */
/* Bounds the chart ids ever drawn at once, not the tiles held: an id past it
 * would never enter g_tiled and so never be swept, keeping its canvases until
 * a reset. The ids in use are 23 (see kdos-res.md, Adding a chart). */
#define GRAPH_TILES 32
static int g_tiled[GRAPH_TILES], g_ntiled;
static int g_seen[GRAPH_TILES], g_nseen;

/* Where the pointer rests, and the keyboard's scrub — see res_graph(). */
static int g_ptr_x = -1, g_ptr_y = -1;
/* The scrub: which chart, and which sample by its ABSOLUTE number, so it
 * travels left with the samples as new ones arrive. `g_scrub_seen` is that
 * the chart was drawn since the last sweep: a scrub on a chart that is not on
 * the screen — its page left — ends there. */
static int g_scrub_id = -1, g_scrub_seen;
static unsigned long long g_scrub_abs;

static int id_in(const int *set, int n, int id)
{
	for (int i = 0; i < n; i++)
		if (set[i] == id)
			return 1;
	return 0;
}

static void graph_seen(int id)
{
	if (!id_in(g_seen, g_nseen, id) && g_nseen < GRAPH_TILES)
		g_seen[g_nseen++] = id;
	if (!id_in(g_tiled, g_ntiled, id) && g_ntiled < GRAPH_TILES)
		g_tiled[g_ntiled++] = id;
}

/*
 * AFTER the flush, never before it: the frame being presented must not name a
 * slot the table has given back. And a drop repaints the whole surface next
 * frame, because a freed slot is handed straight to the next tile that asks
 * — possibly one drawn in the same cells as the dropped one — and libkwl
 * diffs each buffer against the cells it last painted, which would then match
 * and leave the old chart's pixels up.
 */
void res_graph_sweep(void)
{
	int dropped = 0;

	for (int i = 0; i < g_ntiled;) {
		if (!id_in(g_seen, g_nseen, g_tiled[i])) {
			kch_tile_drop(g_tiled[i]);
			g_tiled[i] = g_tiled[--g_ntiled];
			dropped = 1;
		} else {
			i++;
		}
	}
	g_nseen = 0;
	if (dropped)
		ktui_draw_invalidate();
	if (!g_scrub_seen)
		g_scrub_id = -1;
	g_scrub_seen = 0;
}

/*
 * At start and on every reload: every tile goes and every icon is retinted —
 * each was rasterised in the palette a retint replaces, and the icon cache is
 * keyed by name and size, not palette, so a header icon kept across a reload
 * would wear the old accent over charts drawn in the new one — and `icons` is
 * read again, because `icons = no` is a person asking for the character grid,
 * charts included. The caller invalidates the frame, which covers the slots
 * this frees.
 */
void res_graph_reset(void)
{
	if (kicon_enabled())
		kicon_retint();
	kch_tile_reset();
	kch_tile_enable(RC.icons);
	g_ntiled = 0;
	g_nseen = 0;
}

/*
 * One chart — or a pair, `b` mirrored under a midline on the same axis — as
 * pixels over the cells `r`. 0 when it is up, -1 when the caller must draw
 * its cells.
 *
 * The samples handed over are the ones the cells would show and the one to
 * the left of them, which the first column's trace leans towards; more would
 * only put history nobody sees into the tile's hash. The smoothing is the
 * cell tier's own, kpr_hist_smooth(), and the gridlines are keyed to the
 * same absolute sample number, so the two tiers agree about where every
 * gridline is.
 */
static int graph_px(int id, KRect r, const KprHist *a, const KprHist *b,
		    double vmax, int mark)
{
	static double sa[KPR_HIST], sb[KPR_HIST];
	int scale = kdisp_scale() > 0 ? kdisp_scale() : 1;
	KchPlot p = {
		.vmax = vmax,
		.slot_a = KT_ACCENT,
		.slot_b = KT_WARN,
		.per_cell = 1,
		.line_w = 1.5 * scale,
		.area_alpha = 90,
		.line_alpha = 255,
		.rest_alpha = 90,
		.seq = a->seq ? a->seq - 1 : 0,
		.grid = grid_period(),
		.grid_slot = KT_MID,
		.grid_alpha = 60,
		.plate_slot = KT_DIM,
		.plate_alpha = 55,
		.base_slot = KT_DIM,
		.base_alpha = 255,
		.mark = mark,
		.mark_slot = KT_MID,
		.mark_alpha = 255,
	};
	const KprHist *h[2] = { a, b };
	double *out[2] = { sa, sb };

	if (!kicon_enabled() || r.w < 1 || r.h < 1)
		return -1;
	for (int q = 0; q < 2 && h[q]; q++) {
		int from = h[q]->n - (r.w + 1);
		int n;

		if (from < 0)
			from = 0;
		n = h[q]->n - from;
		for (int i = 0; i < n; i++)
			out[q][i] = kpr_hist_smooth(h[q], from + i);
		if (q == 0) {
			p.a = sa;
			p.na = n;
		} else {
			p.b = sb;
			p.nb = n;
		}
	}
	graph_seen(id);
	return kch_plot(id, r, &p, KT_ACCENT, KT_BG);
}

/* ── one sample, read off the chart ─────────────────────────────────── */

/* How many samples each chart showed when last drawn, which is how far back
 * a scrub on it may go. */
static struct { int id, cols; } g_shown[16];

void res_graph_pointer(int mx, int my)
{
	g_ptr_x = mx;
	g_ptr_y = my;
}

void res_fmt_pct(double v, char *out, size_t n)
{
	snprintf(out, n, "%.0f%%", v);
}

static int *shown_of(int id)
{
	int n = (int)(sizeof(g_shown) / sizeof(g_shown[0]));

	for (int i = 0; i < n; i++)
		if (g_shown[i].id == id || !g_shown[i].id) {
			g_shown[i].id = id;
			return &g_shown[i].cols;
		}
	return &g_shown[n - 1].cols;
}

int res_graph_scrub(int id, const KprHist *h, int key)
{
	unsigned long long newest, oldest;
	int cols = *shown_of(id);

	if (!h || h->n < 1 || h->seq < 1)
		return 0;
	if (cols < 1 || cols > h->n)
		cols = h->n;
	newest = h->seq - 1;
	oldest = newest + 1 - (unsigned long long)cols;
	switch (key) {
	case KT_K_LEFT:
		if (g_scrub_id != id || g_scrub_abs > newest)
			g_scrub_abs = newest;
		else if (g_scrub_abs > oldest)
			g_scrub_abs--;
		g_scrub_id = id;
		return 1;
	case KT_K_RIGHT:
		if (g_scrub_id != id)
			return 0;
		if (g_scrub_abs >= newest)
			g_scrub_id = -1;	/* off the newest end */
		else
			g_scrub_abs++;
		return 1;
	}
	return 0;
}

int res_graph_scrubbing(void)
{
	return g_scrub_id >= 0;
}

void res_graph_scrub_end(void)
{
	g_scrub_id = -1;
}

/* How long ago `back` samples was, the way the reading says it. */
static void ago(int back, char *out, size_t n)
{
	long long ms = (long long)back * (RC.interval_ms > 0 ? RC.interval_ms
							     : 1000);
	long long s = (ms + 500) / 1000;

	if (!back)
		snprintf(out, n, "now");
	else if (s < 120)
		snprintf(out, n, "%llds ago", s);
	else
		snprintf(out, n, "%lldm ago", s / 60);
}

/*
 * Draw one chart into `r`. `id` names its tile, so it must be stable for the
 * chart and unique among the charts this program draws.
 *
 * THE TOP ROW IS THE LABEL'S. The plot starts one row below it — sharing the
 * row put a run of ramp glyphs through the reading, which is what a chart
 * drawn by a one-row widget into a ten-row band looked like.
 *
 * THE POINTER WINS OVER THE SCRUB while it rests on the chart: it is what the
 * hand is doing now. A scrub whose sample has scrolled out of the window
 * ends, rather than pointing at a column that is no longer that sample.
 */
void res_graph(int id, KRect r, const KprHist *h, const char *label,
	       const char *reading, ResFmt fmt)
{
	if (r.w < 4 || r.h < 2)
		return;

	double vmax = h->pinned ? 100.0 : kpr_hist_scale((KprHist *)h);

	KRect plot = krect(r.x, r.y + 1, r.w, r.h - 1);
	int cols = h->n < plot.w ? h->n : plot.w;
	int x0 = plot.x + plot.w - cols;
	int back = -1;
	char said[64];

	*shown_of(id) = cols;
	if (id == g_scrub_id)
		g_scrub_seen = 1;
	if (fmt && cols > 0) {
		if (g_ptr_y >= plot.y && g_ptr_y < plot.y + plot.h &&
		    g_ptr_x >= x0 && g_ptr_x < plot.x + plot.w) {
			back = plot.x + plot.w - 1 - g_ptr_x;
		} else if (g_scrub_id == id) {
			unsigned long long newest = h->seq ? h->seq - 1 : 0;

			if (g_scrub_abs <= newest &&
			    newest - g_scrub_abs < (unsigned long long)cols)
				back = (int)(newest - g_scrub_abs);
			else
				g_scrub_id = -1;
		}
	}

	if (graph_px(id, plot, h, NULL, vmax, back + 1) != 0) {
		graph_cells(plot, h, vmax, KT_ACCENT, KT_BG);
		if (back >= 0)
			ktui_draw_reverse(krect(x0 + cols - 1 - back, plot.y, 1,
						plot.h));
	}
	if (back >= 0) {
		char when[24], val[32];

		ago(back, when, sizeof(when));
		fmt(kpr_hist_at(h, h->n - 1 - back), val, sizeof(val));
		snprintf(said, sizeof(said), "%s  %s", when, val);
	}

	/*
	 * The label goes top-left and the reading top-right, and the label is
	 * DROPPED rather than overlapped when the band is too narrow for both:
	 * a meter whose unit is not obvious is the one that needs its name,
	 * and two strings colliding read as a rendering fault. A sample read
	 * off the chart takes the reading's place, in the accent, so it is
	 * never mistaken for the value now.
	 */
	if (label && r.w >= 12)
		ktui_draw_text(r.x, r.y, r.w / 2, label, KT_MID, KT_BG, 0);
	if (back >= 0)
		ktui_draw_text_right(r.x, r.y, r.w, said, KT_ACCENT, KT_BG, 0);
	else if (reading)
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
 * MIRRORED IN PIXELS, STACKED IN CELLS. The pixel tier grows the sent series
 * DOWNWARD from a midline, the shape every rate monitor since MRTG has used.
 * The cell tier cannot: the ramp is bottom-aligned by construction —
 * `ktui_ramp_v` has no upside-down twin, and inventing one would mean glyphs
 * the 512-glyph console font does not carry — so it draws two bands in a
 * fixed order on one scale, which answers the same question.
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

	if (graph_px(id, krect(r.x, r.y + 1, r.w, plot_h), a, b, vmax, 0) != 0) {
		graph_cells(krect(r.x, r.y + 1, r.w, top_h), a, vmax,
			    KT_ACCENT, KT_BG);
		if (bot_h > 0)
			graph_cells(krect(r.x, r.y + 1 + top_h, r.w, bot_h), b,
				    vmax, KT_WARN, KT_BG);
	}

	if (label && r.w >= 12)
		ktui_draw_text(r.x, r.y, r.w / 2, label, KT_MID, KT_BG, 0);
	if (reading)
		ktui_draw_text_right(r.x, r.y, r.w, reading, KT_TEXT, KT_BG, 0);
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — ramps and the charts drawn out of them
 *
 * No libm here, the same rule the rest of the library keeps: a level is a
 * multiply and a truncation, an autoscale is a max and a divide.
 * ---------------------------------
 */

#include <stddef.h>

#include "ktui.h"

/* Index 0 is the empty cell and index ktui_ramp_levels() the full one, so a
 * ramp table holds levels+1 entries; ramp_n counts the NON-empty steps. */
static const char *RAMP_H_RICH[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉",
				     "█" };
static const char *RAMP_V_RICH[] = { " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇",
				     "█" };
static const char *RAMP_VT[] = { " ", "░", "▒", "█" };
static const char *RAMP_ASCII[] = { " ", ".", ":", "#" };

#define RAMP_MAX 9		/* the longest table, levels+1 entries      */

static const char *const *ramp_h = RAMP_ASCII;
static const char *const *ramp_v = RAMP_ASCII;
static int ramp_n = 3;

/*
 * THE RAMPS DECODED ONCE. A chart writes one cell per column per frame, and
 * handing ktui_draw_text a three-byte block glyph makes it decode the same
 * codepoint and re-walk the character-width tables for every one of them.
 *
 * Every entry in every ramp is a SINGLE codepoint one column wide — a two-cell
 * glyph in one of these tables would desync the tty diff's cursor arithmetic,
 * because nothing below here writes a continuation cell. Kept in step with the
 * string tables above and started on the ASCII ramp those default to, so a
 * chart drawn before ktui_ramp_init() still paints the tier it claims.
 */
static uint32_t cp_h[RAMP_MAX] = { ' ', '.', ':', '#' };
static uint32_t cp_v[RAMP_MAX] = { ' ', '.', ':', '#' };

void ktui_ramp_init(void)
{
	if (!(ktui_caps & KT_CAP_UTF8)) {
		ramp_h = ramp_v = RAMP_ASCII;
		ramp_n = 3;
	} else if (ktui_caps & KT_CAP_LINUXVT) {
		ramp_h = ramp_v = RAMP_VT;
		ramp_n = 3;
	} else {
		ramp_h = RAMP_H_RICH;
		ramp_v = RAMP_V_RICH;
		ramp_n = 8;
	}
	for (int i = 0; i <= ramp_n; i++) {
		ktui_utf8_next(ramp_h[i], &cp_h[i]);
		ktui_utf8_next(ramp_v[i], &cp_v[i]);
	}
}

int ktui_ramp_levels(void)
{
	return ramp_n;
}

/* 0 is empty and 1 is full, and both have to be EXACT: a bar that shows a
 * sliver at zero or stops a shade short at one is read as a stalled build.
 * The tip cell is computed as ceiling: any non-zero f rounds to at least 1,
 * otherwise a rounded-down tip reads as empty and the bar lies about how
 * full it is — exactly the invariant this function protects. */
static int ramp_index(double f)
{
	if (f <= 0)
		return 0;
	if (f >= 1)
		return ramp_n;
	int i = (int)(f * ramp_n);
	if ((double)i < f * ramp_n)
		i++;
	if (i > ramp_n)
		i = ramp_n;
	return i;
}

const char *ktui_ramp_h(double f)
{
	return ramp_h[ramp_index(f)];
}

const char *ktui_ramp_v(double f)
{
	return ramp_v[ramp_index(f)];
}

static uint32_t ramp_h_cp(double f)
{
	return cp_h[ramp_index(f)];
}

static uint32_t ramp_v_cp(double f)
{
	return cp_v[ramp_index(f)];
}

static double window_max(const double *v, int from, int to)
{
	double m = 0;
	for (int i = from; i < to; i++)
		if (v[i] > m)
			m = v[i];
	return m;
}

/* Must window exactly as ktui_sparkline does, or the number printed beside a
 * chart describes a different set of samples than the one on screen. */
double ktui_sparkline_peak(const double *v, int n, int cols)
{
	if (!v || n <= 0 || cols <= 0)
		return 0;
	return window_max(v, n > cols ? n - cols : 0, n);
}

void ktui_sparkline(KRect r, const double *v, int n, double vmax, int bg)
{
	if (r.w <= 0 || n <= 0)
		return;
	int from = n > r.w ? n - r.w : 0;
	int cols = n - from;
	if (vmax <= 0)
		vmax = window_max(v, from, n);
	/* A flat-zero window is drawn as a baseline rather than as a full bar:
	 * dividing by a zero max would paint the HUD solid at idle. */
	int x0 = r.x + (r.w - cols);
	for (int i = 0; i < cols; i++) {
		double f = vmax > 0 ? v[from + i] / vmax : 0;

		/*
		 * A SAMPLE OF ZERO IS A BASELINE, NOT A HOLE.
		 *
		 * `ramp_index(0)` is 0 and the ramp's zeroth entry is a SPACE
		 * — exact empty, which is what a gauge needs and what a chart
		 * must not have. An idle meter drew ten spaces between its
		 * label and its reading, so the track vanished and the wing
		 * read as three words with gaps rather than as charts: `NET`
		 * then nothing then the rate. The lowest ramp step in the
		 * muted text colour is the axis the samples sit on.
		 */
		if (f <= 0)
			ktui_draw_cell(x0 + i, r.y, cp_v[1], KT_MID, bg, 0);
		else
			ktui_draw_cell(x0 + i, r.y, ramp_v_cp(f), KT_ACCENT,
				       bg, 0);
	}
}

void ktui_gauge(int x, int y, int w, double frac, int fg, int bg)
{
	if (w <= 0)
		return;
	double tip = 0;
	int fill = ktui_bar_fill(w, frac, &tip);
	/* The filled run and the track are each ONE glyph repeated, so they go
	 * out as runs of a decoded codepoint; only the tip cell varies. */
	ktui_draw_hline(x, y, fill, KT_G_FULL, fg, bg);
	int rest = x + fill;
	if (tip > 0 && fill < w) {
		ktui_draw_cell(rest, y, ramp_h_cp(tip), fg, bg, 0);
		rest++;
	}
	ktui_draw_hline(rest, y, x + w - rest, KT_G_SHADE, KT_DIM, bg);
}

void ktui_heat(KRect r, const double *v, int n, double vmax, int bg)
{
	if (r.w <= 0 || n <= 0)
		return;
	if (vmax <= 0)
		vmax = window_max(v, 0, n);
	int cells = n < r.w ? n : r.w;
	for (int i = 0; i < cells; i++) {
		/* Bucket the samples across the cells rather than dropping the
		 * tail: a 400-step build in a 20-cell strip must still show its
		 * slow patches. */
		int lo = (int)((long long)i * n / cells);
		int hi = (int)((long long)(i + 1) * n / cells);
		if (hi <= lo)
			hi = lo + 1;
		double sum = 0;
		for (int k = lo; k < hi && k < n; k++)
			sum += v[k];
		double avg = sum / (hi - lo);
		double f = vmax > 0 ? avg / vmax : 0;
		ktui_draw_cell(r.x + i, r.y, ramp_v_cp(f), KT_MID, bg, 0);
	}
}

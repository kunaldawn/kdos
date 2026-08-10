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

/* Index 0 is the empty cell, index levels-1 the full one. */
static const char *RAMP_H_RICH[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉",
				     "█" };
static const char *RAMP_V_RICH[] = { " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇",
				     "█" };
static const char *RAMP_VT[] = { " ", "░", "▒", "█" };
static const char *RAMP_ASCII[] = { " ", ".", ":", "#" };

static const char *const *ramp_h = RAMP_ASCII;
static const char *const *ramp_v = RAMP_ASCII;
static int ramp_n = 3;

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
		ktui_draw_text(x0 + i, r.y, 1, ktui_ramp_v(f), KT_ACCENT, bg,
			       0);
	}
}

void ktui_gauge(int x, int y, int w, double frac, int fg, int bg)
{
	if (w <= 0)
		return;
	double tip = 0;
	int fill = ktui_bar_fill(w, frac, &tip);
	for (int i = 0; i < w; i++) {
		const char *g;
		int c;
		if (i < fill) {
			g = ktui_glyph[KT_G_FULL];
			c = fg;
		} else if (i == fill && tip > 0) {
			g = ktui_ramp_h(tip);
			c = fg;
		} else {
			g = ktui_glyph[KT_G_SHADE];
			c = KT_DIM;
		}
		ktui_draw_text(x + i, y, 1, g, c, bg, 0);
	}
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
		ktui_draw_text(r.x + i, r.y, 1, ktui_ramp_v(f), KT_MID, bg, 0);
	}
}

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
 * The sample ring, the axis and the smoothing.
 */

#include <string.h>

#include "kproc.h"

void kpr_hist_init(KprHist *h, int pinned)
{
	memset(h, 0, sizeof(*h));
	h->pinned = pinned;
	h->scale = pinned ? 100.0 : 0.0;
}

void kpr_hist_push(KprHist *h, double v)
{
	if (v < 0.0)
		v = 0.0;
	if (h->n < KPR_HIST) {
		h->v[h->n++] = v;
	} else {
		memmove(h->v, h->v + 1, sizeof(h->v[0]) * (KPR_HIST - 1));
		h->v[KPR_HIST - 1] = v;
	}
	/*
	 * The ABSOLUTE sample number, which never resets. A gridline keyed to
	 * it marches left with the samples beside it; one keyed to the ring
	 * index would stand still and make a flat band look like a frozen
	 * program.
	 */
	h->seq++;
}

double kpr_hist_at(const KprHist *h, int i)
{
	if (i < 0 || i >= h->n)
		return 0.0;
	return h->v[i];
}

double kpr_hist_peak(const KprHist *h)
{
	double p = 0.0;
	for (int i = 0; i < h->n; i++)
		if (h->v[i] > p)
			p = h->v[i];
	return p;
}

/*
 * The axis: a ladder of round numbers with hysteresis.
 *
 * A chart autoscaled to its own window rescales on every sample, so a
 * perfectly steady stream still draws a shape that jumps — the data is flat
 * and the axis is moving.
 *
 * Grow the moment a sample does not fit, because a clipped chart is a lie.
 * Shrink only when the peak has been under a THIRD of the scale: one
 * threshold in each direction oscillates between two rungs for a series
 * sitting on the boundary, which is the same flicker wearing a different hat.
 *
 * A pinned ring is a percentage and never rescales — 0..100 is the axis, and
 * a CPU chart that rescaled to its own peak would make 3% look like 100%.
 */
double kpr_hist_scale(KprHist *h)
{
	static const double LADDER[] = {
		16e3, 64e3, 256e3, 1e6, 4e6, 16e6, 64e6, 256e6, 1e9, 4e9,
	};
	const int n = (int)(sizeof(LADDER) / sizeof(LADDER[0]));

	if (h->pinned)
		return 100.0;

	double peak = kpr_hist_peak(h);
	double cur = h->scale;

	if (cur <= 0)
		cur = LADDER[0];
	if (peak > cur) {
		for (int i = 0; i < n; i++)
			if (LADDER[i] > peak) {
				h->scale = LADDER[i];
				return h->scale;
			}
		h->scale = LADDER[n - 1];
		return h->scale;
	}
	if (peak < cur / 3.0) {
		for (int i = n - 1; i >= 0; i--)
			if (LADDER[i] < cur && LADDER[i] > peak) {
				h->scale = LADDER[i];
				return h->scale;
			}
		h->scale = LADDER[0];
		return h->scale;
	}
	h->scale = cur;
	return h->scale;
}

/*
 * A three-point mean, for the PLOTTED series only.
 *
 * Smoothing takes the hash off a line while a one-second spike still moves it
 * by a third of its height. The PRINTED number is never smoothed: that would
 * be lying about the instant, which is the one thing a reading has to be
 * honest about.
 */
double kpr_hist_smooth(const KprHist *h, int i)
{
	if (i < 0 || i >= h->n)
		return 0.0;
	if (h->n < 3 || i == 0 || i == h->n - 1)
		return h->v[i];
	return (h->v[i - 1] + h->v[i] + h->v[i + 1]) / 3.0;
}

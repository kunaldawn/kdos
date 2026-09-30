/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   report.c — what the numbers are allowed to say
 *
 * macOS, Windows 11 and Android all ship per-app battery attribution and all
 * three show a RELATIVE score. That is not a simplification for users; it is
 * the only defensible thing to print. RAPL measures the CPU package. It cannot
 * see the panel — the largest single draw on a laptop — nor the radio, the SSD,
 * or a discrete GPU. "GIMP was 41% of attributable CPU energy today" is a
 * measurement. "GIMP used 12% of your battery" is a guess wearing a unit.
 *
 * So no watt-hours leave this file, and three quantities are printed beside the
 * shares because each one is a way the shares could mislead:
 *
 *   - the IDLE FLOOR, in watts, because the shares are of the energy above it
 *     and a floor that has not settled makes them all read low;
 *   - the SHORT-LIVED residue, because a build that ran entirely between two
 *     samples is real energy belonging to no surviving process;
 *   - whether the GPU is INSIDE the domain that was measured, because on a
 *     discrete card it is not, and the answer for a gaming session would
 *     otherwise be wrong by most of the machine.
 *
 * The GPU column is engine TIME, labelled as time. Nothing here converts it to
 * energy, because nothing on the machine measures what it cost.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "energyd.h"

#define KE_BAR 20

typedef struct {
	const KeApp *a;
	double energy;
} KeRow;

static int by_energy(const void *x, const void *y)
{
	double a = ((const KeRow *)x)->energy;
	double b = ((const KeRow *)y)->energy;
	return a < b ? 1 : a > b ? -1 : 0;
}

static void human_span(double s, char *out, size_t cap)
{
	if (s < 90)
		snprintf(out, cap, "%.0f s", s);
	else if (s < 5400)
		snprintf(out, cap, "%.0f min", s / 60);
	else
		snprintf(out, cap, "%.1f h", s / 3600);
}

static void append(char *dst, size_t cap, const char *s)
{
	size_t n = strlen(dst);
	if (n < cap)
		kb_strlcpy(dst + n, s, cap - n);
}

void ke_ledger_report(const KeLedger *l, const KeRapl *r, KbBuf *out, bool json)
{
	double floor_uw = l->have_floor ? l->floor_uw : 0;

	KeRow *row = kb_calloc((size_t)(l->n ? l->n : 1), sizeof(*row));
	for (int i = 0; i < l->n; i++) {
		row[i].a = &l->app[i];
		/* we − floor·wt: this app's share of every window's energy,
		 * minus its share of the idle floor over the same windows. It
		 * can go slightly negative when a window sat AT the floor and
		 * rounding pushes it under; that is zero, not a negative. */
		row[i].energy = l->app[i].we - floor_uw * l->app[i].wt;
		if (row[i].energy < 0)
			row[i].energy = 0;
	}
	qsort(row, (size_t)l->n, sizeof(*row), by_energy);

	double shortlived = l->short_we - floor_uw * l->short_wt;
	if (shortlived < 0)
		shortlived = 0;

	double attributable = l->total_uj - floor_uw * l->window;
	if (attributable < 0)
		attributable = 0;

	double gpu_total = 0;
	for (int i = 0; i < l->n; i++)
		gpu_total += l->app[i].gpu_ns;

	char domains[128] = "";
	for (int i = 0; i < r->n; i++) {
		if (*domains)
			append(domains, sizeof(domains), " + ");
		append(domains, sizeof(domains), r->d[i].name);
	}
	if (!*domains)
		kb_strlcpy(domains, "none", sizeof(domains));

	if (json) {
		kb_buf_printf(out, "{\"window_s\":%.1f,\"samples\":%d,"
			      "\"domains\":", l->window, l->samples);
		kb_json_str(out, domains);
		kb_buf_printf(out, ",\"gpu_in_domain\":%s,\"gpu_accounting\":%s,"
			      "\"idle_floor_w\":%.3f,\"floor_settled\":%s,"
			      "\"attributable_fraction\":%.4f,\"apps\":[",
			      r->gpu_inside ? "true" : "false",
			      l->gpu_seen ? "true" : "false", floor_uw / 1e6,
			      l->have_floor ? "true" : "false",
			      l->total_uj > 0 ? attributable / l->total_uj : 0.0);
		for (int i = 0; i < l->n; i++) {
			kb_buf_printf(out, "%s{\"name\":", i ? "," : "");
			kb_json_str(out, row[i].a->name);
			kb_buf_printf(out, ",\"impact\":%.4f,\"cpu_s\":%.1f",
				      attributable > 0
					? row[i].energy / attributable : 0.0,
				      row[i].a->cpu_ticks / (double)ke_hz());
			if (l->gpu_seen)
				kb_buf_printf(out, ",\"gpu_share\":%.4f",
					      gpu_total > 0
						? row[i].a->gpu_ns / gpu_total
						: 0.0);
			kb_buf_str(out, "}");
		}
		kb_buf_printf(out, "],\"short_lived\":%.4f}\n",
			      attributable > 0 ? shortlived / attributable : 0.0);
		free(row);
		return;
	}

	char span[32];
	human_span(l->window, span, sizeof(span));
	kb_buf_printf(out, "KDOS energy  \xe2\x80\x94  %s of samples, RAPL %s\n\n",
		      span, domains);

	if (l->samples < 1 || attributable <= 0) {
		kb_buf_str(out, "  nothing attributable yet \xe2\x80\x94 the "
				"machine has not been above its idle floor\n");
		free(row);
		return;
	}

	for (int i = 0; i < l->n; i++) {
		double share = row[i].energy / attributable;
		if (share < 0.001)
			continue;
		int fill = (int)(share * KE_BAR + 0.5);
		char bar[KE_BAR * 3 + 1];
		int at = 0;
		for (int c = 0; c < KE_BAR; c++)
			at += snprintf(bar + at, sizeof(bar) - (size_t)at, "%s",
				       c < fill ? "\xe2\x96\x88" : " ");
		kb_buf_printf(out, "  %-38.38s %5.1f%%  %s", row[i].a->name,
			      share * 100.0, bar);
		if (l->gpu_seen && gpu_total > 0)
			kb_buf_printf(out, "  gpu %4.1f%%",
				      100.0 * row[i].a->gpu_ns / gpu_total);
		kb_buf_str(out, "\n");
	}

	if (shortlived / attributable >= 0.001)
		kb_buf_printf(out, "  %-38.38s %5.1f%%\n",
			      "short-lived and exited processes",
			      100.0 * shortlived / attributable);

	kb_buf_printf(out, "\n  shares are of ATTRIBUTABLE energy \xe2\x80\x94 "
		      "%.0f%% of the package total; the rest is the idle floor\n",
		      100.0 * attributable / l->total_uj);
	kb_buf_printf(out, "  idle floor %.2f W, the lowest average power seen "
		      "in %d samples%s\n", floor_uw / 1e6, l->samples,
		      l->samples < 30 ? " (still falling \xe2\x80\x94 early "
					"shares read low)" : "");
	if (r->gpu_inside)
		kb_buf_str(out, "  an uncore domain is present, so the "
				"integrated GPU's energy is already counted\n");
	else
		kb_buf_str(out, "  no uncore domain: a discrete GPU is OUTSIDE "
				"RAPL and none of its energy is here\n");
	if (l->gpu_seen)
		kb_buf_str(out, "  the gpu column is engine TIME from drm "
				"fdinfo, never energy\n");
	else
		kb_buf_str(out, "  no per-client GPU accounting from this "
				"driver \xe2\x80\x94 no gpu column\n");
	kb_buf_str(out, "  relative only. RAPL cannot see the panel, the radio "
			"or the disk, so there are no watt-hours here.\n");

	free(row);
}

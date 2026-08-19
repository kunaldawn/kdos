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
 * The CPU page.
 *
 * Utilisation as an area chart with /proc/pressure/cpu's `some avg10` under it
 * as a second, dimmer series. That pairing is the point: "busy" and "starved"
 * are different questions, a machine can be either without the other, and no
 * other monitor here puts them on one axis.
 *
 * Per core: up to sixteen get their own small chart. Above that the page draws
 * a heat strip, one column per core — 128 sparklines is a wall rather than a
 * reading.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

enum { CPU_MAX_GRID = 16 };

static void facts(int x, int y, int w)
{
	char line[256];
	const KprCpu *c = &R.cpu;

	ktui_draw_text(x, y, w, c->model, KT_TEXT, KT_BG, 0);

	/*
	 * Logical, physical and packages come from topology/, never from
	 * counting cpuinfo blocks: a block is a LOGICAL cpu, so counting them
	 * and calling the answer cores is wrong on every SMT machine.
	 */
	const char *dot = ktui_glyph[KT_G_DOT];
	snprintf(line, sizeof(line),
		 "%d logical %s %d core%s %s %d package%s",
		 c->ncpu, dot, c->ncore, c->ncore == 1 ? "" : "s",
		 dot, c->npkg, c->npkg == 1 ? "" : "s");
	ktui_draw_text(x, y + 1, w, line, KT_MID, KT_BG, 0);

	long khz = 0;
	for (int i = 0; i < c->ncpu; i++)
		if (c->khz[i] > khz)
			khz = c->khz[i];

	char freq[64], maxf[64];
	kb_strlcpy(freq, res_none(), sizeof(freq));
	kb_strlcpy(maxf, res_none(), sizeof(maxf));
	if (khz > 0)
		snprintf(freq, sizeof(freq), "%.2f GHz", (double)khz / 1e6);
	if (c->khz_max > 0)
		snprintf(maxf, sizeof(maxf), "%.2f GHz", (double)c->khz_max / 1e6);

	char gov[48] = "";
	if (c->governor[0])
		snprintf(gov, sizeof(gov), " %s %s", dot, c->governor);
	snprintf(line, sizeof(line), "%s of %s %s %s%s",
		 freq, maxf, dot, res_temp(c->temp_c), gov);
	ktui_draw_text(x, y + 2, w, line, KT_MID, KT_BG, 0);

	static const char *VIRT[] = { "bare metal", "a hypervisor", "KVM",
				      "QEMU", "VMware", "VirtualBox",
				      "Hyper-V", "Xen" };
	const char *v = (c->virt >= 0 && c->virt < 8) ? VIRT[c->virt] : "?";
	snprintf(line, sizeof(line), "running on %s", v);
	ktui_draw_text(x, y + 3, w, line, KT_DIM, KT_BG, 0);
}

const char *res_cpu_headline(void)
{
	static char s[64];
	double busy = R.h_cpu.n ? kpr_hist_at(&R.h_cpu, R.h_cpu.n - 1) : 0.0;
	snprintf(s, sizeof(s), "%.0f%% busy", busy);
	return s;
}

void res_draw_cpu(int x, int y, int w, int h)
{
	char reading[64];
	double busy = R.h_cpu.n ? kpr_hist_at(&R.h_cpu, R.h_cpu.n - 1) : 0.0;
	snprintf(reading, sizeof(reading), "%.0f%%", busy);

	/*
	 * `y` is the first body ROW and `h` is a HEIGHT, so the last usable
	 * row is y + h - 1. Comparing a row against `h` treats a height as an
	 * absolute coordinate and silently loses everything below the band.
	 */
	const int bottom = y + h;
	int row = y;
	int avail = h - 1;
	if (avail < 3)
		return;

	int chart_h = avail / 2;
	if (chart_h > 10)
		chart_h = 10;
	if (chart_h < 3)
		chart_h = 3;

	res_graph(1, krect(x + 1, row, w - 2, chart_h), &R.h_cpu,
		  "utilisation", reading);
	row += chart_h;

	/*
	 * Pressure under the chart, on its own row, in the secondary colour.
	 * `some avg10` is the share of the last ten seconds in which at least
	 * one task was stalled waiting for a runnable CPU — which is what
	 * "the machine feels slow" is, and it is not the same as busy.
	 */
	if (R.psi_cpu.present && row < bottom - 1) {
		char psi[96];
		snprintf(psi, sizeof(psi),
			 "pressure  some %.2f%% (10s)  %.2f%% (60s)",
			 R.psi_cpu.some10, R.psi_cpu.some60);
		ktui_draw_text(x + 1, row, w - 2, psi, KT_WARN, KT_BG, 0);
		row++;
	} else if (row < bottom - 1) {
		char off[64];
		snprintf(off, sizeof(off), "pressure  %s  this kernel has "
			 "PSI off", res_none());
		ktui_draw_text(x + 1, row, w - 2, off, KT_DIM, KT_BG, 0);
		row++;
	}

	if (row + 5 < bottom)
		facts(x + 1, row + 1, w - 2), row += 6;

	/* Per core, if there is room left. */
	int left = bottom - row - 1;
	if (left < 2 || R.cpu.ncpu <= 0 || !R.h_core)
		return;

	kch_group(x + 1, row, w - 2, "per core");
	row++;
	left = bottom - row - 1;
	if (left < 1)
		return;

	if (R.cpu.ncpu > CPU_MAX_GRID) {
		/*
		 * A heat strip, one column per core. Above sixteen the grid
		 * stops being readable and starts being wallpaper.
		 */
		double v[512];
		int n = R.cpu.ncpu > 512 ? 512 : R.cpu.ncpu;
		for (int i = 0; i < n; i++)
			v[i] = R.h_core[i].n
			       ? kpr_hist_at(&R.h_core[i], R.h_core[i].n - 1)
			       : 0.0;
		ktui_heat(krect(x + 1, row, w - 2, left < 2 ? 1 : 2), v, n,
			  100.0, KT_BG);
		return;
	}

	int cols = w >= 60 ? 4 : 2;
	int cw = (w - 2) / cols;
	int ch = left / ((R.cpu.ncpu + cols - 1) / cols);
	if (ch < 1)
		ch = 1;
	for (int i = 0; i < R.cpu.ncpu; i++) {
		int cx = x + 1 + (i % cols) * cw;
		int cy = row + (i / cols) * ch;
		if (cy + ch > bottom)
			break;
		char lbl[16];
		snprintf(lbl, sizeof(lbl), "%d", i);
		res_graph(100 + i, krect(cx, cy, cw - 1, ch), &R.h_core[i],
			  lbl, NULL);
	}
}

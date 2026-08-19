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
 * The Memory page.
 *
 * RAM and swap as two charts, with /proc/pressure/memory under them: a machine
 * that is STALLING on memory is the thing this page is opened to find out
 * about, and the used figure alone does not say it.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

const char *res_mem_headline(void)
{
	static char s[64];
	unsigned long long used = R.mem.total > R.mem.available
				  ? R.mem.total - R.mem.available : 0;
	snprintf(s, sizeof(s), "%s of %s", res_size(used), res_size(R.mem.total));
	return s;
}

void res_draw_mem(int x, int y, int w, int h)
{
	const KprMem *m = &R.mem;
	char reading[64];

	/*
	 * MemAvailable, never MemTotal - MemFree. Linux spends every spare
	 * page on cache, so that subtraction reports a healthy machine at 95%
	 * and makes this the page nobody believes.
	 */
	unsigned long long used = m->total > m->available
				  ? m->total - m->available : 0;
	snprintf(reading, sizeof(reading), "%s of %s",
		 res_size(used), res_size(m->total));

	/* `y` is the first body row and `h` a height; the last usable row is
	 * y + h - 1. */
	const int bottom = y + h;
	int row = y;
	int avail = h - 1;
	if (avail < 3)
		return;

	int chart_h = avail / 3;
	if (chart_h > 8)
		chart_h = 8;
	if (chart_h < 2)
		chart_h = 2;

	res_graph(2, krect(x + 1, row, w - 2, chart_h), &R.h_mem,
		  "RAM", reading);
	row += chart_h;

	if (m->swap_total) {
		char sw[64];
		unsigned long long su = m->swap_total - m->swap_free;
		snprintf(sw, sizeof(sw), "%s of %s", res_size(su),
			 res_size(m->swap_total));
		if (row + chart_h < bottom) {
			res_graph(3, krect(x + 1, row, w - 2, chart_h),
				  &R.h_swap, "swap", sw);
			row += chart_h;
		}
	} else if (row < bottom - 1) {
		ktui_draw_text(x + 1, row, w - 2, "swap  none configured",
			       KT_DIM, KT_BG, 0);
		row++;
	}

	if (row < bottom - 1) {
		char psi[96];
		if (R.psi_mem.present)
			snprintf(psi, sizeof(psi),
				 "pressure  some %.2f%%  full %.2f%% (10s)",
				 R.psi_mem.some10, R.psi_mem.full10);
		else
			snprintf(psi, sizeof(psi),
				 "pressure  %s  this kernel has PSI off",
				 res_none());
		ktui_draw_text(x + 1, row, w - 2, psi,
			       R.psi_mem.present ? KT_WARN : KT_DIM, KT_BG, 0);
		row += 2;
	}

	if (row + 4 >= bottom)
		return;

	kch_group(x + 1, row, w - 2, "breakdown");
	row++;

	struct { const char *k; unsigned long long v; } f[] = {
		{ "available", m->available },
		{ "free",      m->free },
		{ "cached",    m->cached },
		{ "buffers",   m->buffers },
		{ "shared",    m->shmem },
		{ "dirty",     m->dirty },
		{ "reclaimable", m->reclaimable },
	};
	for (size_t i = 0; i < sizeof(f) / sizeof(f[0]) && row < bottom - 1; i++, row++) {
		ktui_draw_text(x + 2, row, 14, f[i].k, KT_MID, KT_BG, 0);
		ktui_draw_text(x + 17, row, w - 19, res_size(f[i].v),
			       KT_TEXT, KT_BG, 0);
	}

	/*
	 * The per-slot DIMM table lives behind kdos-resctl: SMBIOS type 17 is
	 * in /sys/firmware/dmi/tables/DMI at mode 0400. Without the helper the
	 * section says so and the rest of the page is unaffected.
	 */
	if (row < bottom - 1)
		ktui_draw_text(x + 1, row + 1, w - 2,
			       "memory device details need kdos-resctl",
			       KT_DIM, KT_BG, 0);
}

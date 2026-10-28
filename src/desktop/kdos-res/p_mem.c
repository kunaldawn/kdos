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

/* The share in use, which the headline's two sizes leave as arithmetic. */
const char *res_mem_figure(void)
{
	static char s[16];
	unsigned long long used = R.mem.total > R.mem.available
				  ? R.mem.total - R.mem.available : 0;
	if (!R.mem.total || R.mem.total == KPR_UNREADABLE)
		return NULL;
	snprintf(s, sizeof(s), "%.0f%%", 100.0 * (double)used /
					 (double)R.mem.total);
	return s;
}

/* ←, → scrub the RAM chart — see res_graph_scrub(). */
int res_mem_key(int k)
{
	return res_graph_scrub(2, &R.h_mem, k);
}

/*
 * THE PER-SLOT TABLE COMES FROM kdos-resctl: SMBIOS type 17 is in
 * /sys/firmware/dmi/tables/DMI at mode 0400, and the helper is the one thing
 * that may open it. It is asked ONCE, the first time the page is drawn, and
 * the answer kept: modules do not change under a running machine, and a
 * setuid exec per frame would make this page the load it is measuring. A
 * fixture is a recorded machine and asks nothing, so its frame is the same on
 * every host whatever is installed.
 */
static char g_dimm[1024];
static const char *g_dimm_why;	/* why there is no table, or NULL */
static int g_dimm_asked;

static void dimm_ask(void)
{
	KbArgv a = { 0 };

	g_dimm_asked = 1;
	if (R.fixture) {
		g_dimm_why = "memory device details need kdos-resctl";
		return;
	}
	g_dimm_why = res_act_helper_why();
	if (g_dimm_why)
		return;
	kb_argv_add(&a, RES_RESCTL);
	kb_argv_add(&a, "dmi");
	kb_argv_end(&a);
	/* Exit 1 is the helper's refusal: the caller is not in wheel, or the
	 * table it opened was empty. Any other failure is the open itself. */
	int rc = kb_run_capture(&a, g_dimm, sizeof(g_dimm));
	if (rc != 0) {
		g_dimm[0] = '\0';
		g_dimm_why = rc == 1
			     ? "memory device details: kdos-resctl refused "
			       "(not in wheel, or an empty table)"
			     : "memory device details: the SMBIOS table could "
			       "not be read";
	}
}

static void draw_dimms(int x, int row, int w, int bottom)
{
	if (!g_dimm_asked)
		dimm_ask();
	if (g_dimm_why) {
		ktui_draw_text(x + 1, row, w - 2, g_dimm_why, KT_MID, KT_BG, 0);
		return;
	}
	if (row + 1 >= bottom - 1)
		return;
	kch_group(x + 1, row, w - 2, "memory devices");
	row++;

	/* One `slot<TAB>size<TAB>speed` line per populated device, or one
	 * sentence saying there is none. */
	const char *p = g_dimm;
	for (int slot = 0; *p && row < bottom - 1; slot++, row++) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		char line[128], f[3][48] = { "", "", "" };
		int nf = 0;
		size_t i = 0;

		while (i < len && nf < 3) {
			size_t j = i;
			while (j < len && p[j] != '\t')
				j++;
			snprintf(f[nf++], sizeof(f[0]), "%.*s", (int)(j - i),
				 p + i);
			i = j + 1;
		}
		if (nf == 3 && !strcmp(f[0], "slot"))
			snprintf(line, sizeof(line), "slot %-3d %-10s %s",
				 slot, f[1], f[2]);
		else
			snprintf(line, sizeof(line), "%.*s", (int)len, p);
		ktui_draw_text(x + 2, row, w - 4, line, KT_TEXT, KT_BG, 0);
		p = nl ? nl + 1 : p + len;
	}
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
		  "RAM", reading, res_fmt_pct);
	row += chart_h;

	if (m->swap_total) {
		char sw[64];
		unsigned long long su = m->swap_total - m->swap_free;
		snprintf(sw, sizeof(sw), "%s of %s", res_size(su),
			 res_size(m->swap_total));
		if (row + chart_h < bottom) {
			res_graph(3, krect(x + 1, row, w - 2, chart_h),
				  &R.h_swap, "swap", sw, res_fmt_pct);
			row += chart_h;
		}
	} else if (row < bottom - 1) {
		ktui_draw_text(x + 1, row, w - 2, "swap  none configured",
			       KT_MID, KT_BG, 0);
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
			       R.psi_mem.present ? KT_WARN : KT_MID, KT_BG, 0);
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

	if (row < bottom - 1)
		draw_dimms(x, row + 1, w, bottom);
}

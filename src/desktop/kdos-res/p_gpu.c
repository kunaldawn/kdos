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
 * The GPU page.
 *
 * "UTILISATION" IS ONLY EVER PRINTED WHERE A DRIVER REPORTED ONE. Everywhere
 * else the heading is ENGINE TIME, in nanoseconds of engine work per second of
 * wall clock, and it is not converted into a percentage: an engine can be busy
 * while the GPU idles, so that ratio would be a number nobody measured wearing
 * a unit everybody trusts.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

static KprGpu *g_gpu;
static int g_ngpu;
static int g_sel;

void res_gpu_prepare(void)
{
	kpr_drm_free(g_gpu);
	g_gpu = NULL;
	g_ngpu = kpr_drm_list(&g_gpu);

	/*
	 * NVML fills in what the kernel driver cannot for the proprietary
	 * stack. On a stock KDOS the probe fails and nothing here changes,
	 * which is the correct answer rather than an error.
	 */
	if (kpr_nvml_probe())
		for (int i = 0; i < g_ngpu; i++)
			if (!strcmp(g_gpu[i].driver, "nvidia"))
				kpr_nvml_read(i, &g_gpu[i]);
}

/* Engine time this tick, summed over every process's DRM fds. */
static unsigned long long engine_delta(void)
{
	if (!R.have_prev)
		return 0;
	unsigned long long now = 0, before = 0;
	for (int i = 0; i < R.sample.n; i++)
		now += R.sample.p[i].gpu_ns;
	for (int i = 0; i < R.prev.n; i++)
		before += R.prev.p[i].gpu_ns;
	return now > before ? now - before : 0;
}

const char *res_gpu_headline(void)
{
	static char s[96];
	if (!g_ngpu)
		return "no GPU found";
	const KprGpu *g = &g_gpu[g_sel < g_ngpu ? g_sel : 0];
	if (g->busy_percent >= 0)
		snprintf(s, sizeof(s), "%s %d%% busy", g->driver,
			 g->busy_percent);
	else
		snprintf(s, sizeof(s), "%s, engine time only", g->driver);
	return s;
}

void res_draw_gpu(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y + 1;

	if (!g_ngpu) {
		ktui_draw_text(x + 1, row, w - 2,
			       "No DRM device under /sys/class/drm.",
			       KT_DIM, KT_BG, 0);
		return;
	}

	for (int i = 0; i < g_ngpu && row < bottom - 2; i++) {
		const KprGpu *g = &g_gpu[i];
		char head[96];
		snprintf(head, sizeof(head), "%s  (%s)", g->name, g->driver);
		kch_group(x, row, w, head);
		row++;

		char line[192];
		if (g->busy_percent >= 0) {
			snprintf(line, sizeof(line), "utilisation  %d%%",
				 g->busy_percent);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT,
				       KT_BG, 0);
		} else {
			/*
			 * The honest alternative. i915 and xe have never had a
			 * utilisation counter, so this is not a fallback for a
			 * read that failed — it is the only measurement that
			 * exists for those drivers.
			 */
			unsigned long long ns = engine_delta();
			snprintf(line, sizeof(line),
				 "engine time  %.1f ms/s  (this driver "
				 "publishes no utilisation)",
				 (double)ns / 1e6);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_TEXT,
				       KT_BG, 0);
		}

		if (g->mem_total != KPR_UNREADABLE) {
			snprintf(line, sizeof(line), "memory       %s of %s",
				 res_size(g->mem_used), res_size(g->mem_total));
			ktui_draw_text(x + 1, row++, w - 2, line, KT_MID,
				       KT_BG, 0);
		}
		if (g->freq_mhz > 0) {
			snprintf(line, sizeof(line), "clock        %ld MHz",
				 g->freq_mhz);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_MID,
				       KT_BG, 0);
		}
		if (g->temp_c >= 0.0 || g->power_w >= 0.0) {
			char pw[32] = "";
			if (g->power_w >= 0.0)
				snprintf(pw, sizeof(pw), "  %.1f W", g->power_w);
			snprintf(line, sizeof(line), "sensors      %s%s",
				 res_temp(g->temp_c), pw);
			ktui_draw_text(x + 1, row++, w - 2, line, KT_MID,
				       KT_BG, 0);
		}
		row++;
	}

	if (row < bottom - 1 && !kpr_nvml_probe())
		ktui_draw_text(x + 1, bottom - 1, w - 2,
			       "NVIDIA management library not present",
			       KT_DIM, KT_BG, 0);
}

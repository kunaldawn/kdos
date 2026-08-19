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
 * GPUs, from /sys/class/drm.
 *
 * WHAT CAN BE SAID DEPENDS ON THE DRIVER, and the honest answer is often "this
 * driver publishes no utilisation figure". amdgpu has gpu_busy_percent; i915
 * and xe have never had one and never will — for those the only measurement
 * available is ENGINE TIME summed out of each process's fdinfo, which is a
 * different quantity and must be labelled as one.
 *
 * Nothing here converts engine time into a percentage. A ratio of engine
 * nanoseconds to wall nanoseconds looks like a utilisation and is not one: an
 * engine can be busy while the GPU idles and vice versa, and the number would
 * be believed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kproc.h"

static void read_link_base(const char *path, char *out, size_t cap)
{
	out[0] = 0;
	char target[1024];
	ssize_t n = readlink(path, target, sizeof(target) - 1);
	if (n <= 0)
		return;
	target[n] = 0;
	kb_strlcpy(out, kb_basename(target), cap);
}

int kpr_drm_list(KprGpu **out)
{
	*out = NULL;
	char dir[512];
	snprintf(dir, sizeof(dir), "%s/class/drm", kpr_sys());
	int count = 0;
	char **names = kb_listdir(dir, &count);
	if (!names)
		return 0;

	KprGpu *v = kb_calloc((size_t)(count > 0 ? count : 1), sizeof(*v));
	int n = 0;
	for (char **e = names; *e; e++) {
		/*
		 * card0 and not card0-DP-1 (a connector) and not renderD128
		 * (the render node of a card already listed). A page that
		 * counted those would report three GPUs on a machine with one.
		 */
		if (strncmp(*e, "card", 4))
			continue;
		if (strchr(*e, '-'))
			continue;

		KprGpu *g = &v[n];
		memset(g, 0, sizeof(*g));
		kb_strlcpy(g->name, *e, sizeof(g->name));
		g->busy_percent = -1;
		g->temp_c = -1.0;
		g->power_w = -1.0;
		g->mem_total = KPR_UNREADABLE;
		g->mem_used = KPR_UNREADABLE;
		g->freq_mhz = -1;

		char p[1024];
		snprintf(p, sizeof(p), "%s/class/drm/%s/device/driver",
			 kpr_sys(), *e);
		read_link_base(p, g->driver, sizeof(g->driver));
		if (!g->driver[0])
			kb_strlcpy(g->driver, "unknown", sizeof(g->driver));

		/*
		 * The one driver here that publishes a real utilisation. -1
		 * everywhere else, and -1 is what makes the page say "engine
		 * time" rather than print a percentage it did not measure.
		 */
		long long busy = kpr_num_sys(-1,
			"class/drm/%s/device/gpu_busy_percent", *e);
		if (busy >= 0)
			g->busy_percent = (int)busy;

		long long vt = kpr_num_sys(-1,
			"class/drm/%s/device/mem_info_vram_total", *e);
		long long vu = kpr_num_sys(-1,
			"class/drm/%s/device/mem_info_vram_used", *e);
		if (vt > 0)
			g->mem_total = (unsigned long long)vt;
		if (vu >= 0)
			g->mem_used = (unsigned long long)vu;

		/* Frequency: each driver spells it differently, and a driver
		 * that spells it none of these ways gets -1. */
		long long f = kpr_num_sys(-1, "class/drm/%s/gt_cur_freq_mhz", *e);
		if (f < 0)
			f = kpr_num_sys(-1,
				"class/drm/%s/device/tile0/gt0/freq0/cur_freq", *e);
		if (f > 0)
			g->freq_mhz = (long)f;

		for (int i = 0; i < 8; i++) {
			long long mc = kpr_num_sys(-1,
				"class/drm/%s/device/hwmon/hwmon%d/temp1_input",
				*e, i);
			if (mc > 0) {
				g->temp_c = (double)mc / 1000.0;
				long long uw = kpr_num_sys(-1,
					"class/drm/%s/device/hwmon/hwmon%d/power1_average",
					*e, i);
				if (uw > 0)
					g->power_w = (double)uw / 1e6;
				break;
			}
		}
		n++;
	}
	kb_strv_free(names);
	*out = v;
	return n;
}

void kpr_drm_free(KprGpu *g) { free(g); }

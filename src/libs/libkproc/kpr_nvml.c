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
 * NVIDIA's management library, if somebody installed one.
 *
 * NOTHING IS LINKED AGAINST NVIDIA AT BUILD TIME and no NVIDIA header is
 * vendored: every entry point is resolved by dlsym from a library opened by
 * name at first use. A stock KDOS has no such library, so the probe fails and
 * the GPU page says the driver publishes no statistics — which is the truth on
 * that machine rather than an empty table.
 *
 * The proprietary driver is the one case where a real utilisation figure
 * exists for a card that publishes no fdinfo engine statistics at all, which
 * is why this is worth a dlopen and not worth a dependency.
 */

#include <stdio.h>
#include <string.h>

#include "kproc.h"

#ifdef KPR_NO_DLOPEN

int kpr_nvml_probe(void) { return 0; }
int kpr_nvml_read(int idx, KprGpu *g) { (void)idx; (void)g; return -1; }

#else
#include <dlfcn.h>

static void *g_lib;
static int g_tried;
static unsigned g_count;

static int (*nv_init)(void);
static int (*nv_count)(unsigned *);
static int (*nv_handle)(unsigned, void **);
static int (*nv_util)(void *, void *);
static int (*nv_mem)(void *, void *);
static int (*nv_temp)(void *, int, unsigned *);
static int (*nv_power)(void *, unsigned *);
static int (*nv_name)(void *, char *, unsigned);

int kpr_nvml_probe(void)
{
	if (g_tried)
		return g_lib != NULL;
	g_tried = 1;

	g_lib = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
	if (!g_lib)
		return 0;

	/* The v2 symbols, which is what every driver since 2015 exports. */
	nv_init   = dlsym(g_lib, "nvmlInit_v2");
	nv_count  = dlsym(g_lib, "nvmlDeviceGetCount_v2");
	nv_handle = dlsym(g_lib, "nvmlDeviceGetHandleByIndex_v2");
	nv_util   = dlsym(g_lib, "nvmlDeviceGetUtilizationRates");
	nv_mem    = dlsym(g_lib, "nvmlDeviceGetMemoryInfo");
	nv_temp   = dlsym(g_lib, "nvmlDeviceGetTemperature");
	nv_power  = dlsym(g_lib, "nvmlDeviceGetPowerUsage");
	nv_name   = dlsym(g_lib, "nvmlDeviceGetName");

	if (!nv_init || !nv_count || !nv_handle || nv_init() != 0) {
		dlclose(g_lib);
		g_lib = NULL;
		return 0;
	}
	if (nv_count(&g_count) != 0)
		g_count = 0;
	return 1;
}

int kpr_nvml_read(int idx, KprGpu *g)
{
	if (!kpr_nvml_probe() || idx < 0 || (unsigned)idx >= g_count)
		return -1;

	void *dev = NULL;
	if (nv_handle((unsigned)idx, &dev) != 0 || !dev)
		return -1;

	/* The struct NVML fills is two unsigned ints; declaring that much
	 * rather than vendoring nvml.h is the whole of the ABI used here. */
	struct { unsigned gpu, mem; } util = { 0, 0 };
	if (nv_util && nv_util(dev, &util) == 0)
		g->busy_percent = (int)util.gpu;

	struct { unsigned long long total, free, used; } mi = { 0, 0, 0 };
	if (nv_mem && nv_mem(dev, &mi) == 0) {
		g->mem_total = mi.total;
		g->mem_used = mi.used;
	}
	unsigned t = 0;
	if (nv_temp && nv_temp(dev, 0, &t) == 0)
		g->temp_c = (double)t;
	unsigned mw = 0;
	if (nv_power && nv_power(dev, &mw) == 0)
		g->power_w = (double)mw / 1000.0;
	if (nv_name)
		nv_name(dev, g->model, sizeof(g->model));
	return 0;
}
#endif

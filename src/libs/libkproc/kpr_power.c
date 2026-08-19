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
 * Batteries and AC adapters, from /sys/class/power_supply.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

static void slurp_trim(char *dst, size_t cap, const char *fmt, const char *a1,
		       const char *a2)
{
	dst[0] = 0;
	char *d = kpr_slurp_sys(fmt, a1, a2);
	if (!d)
		return;
	char *nl = strchr(d, '\n');
	if (nl)
		*nl = 0;
	kb_strlcpy(dst, d, cap);
	free(d);
}

int kpr_power_list(KprBattery **out)
{
	*out = NULL;
	int count = 0;
	char path[512];
	snprintf(path, sizeof(path), "%s/class/power_supply", kpr_sys());
	char **names = kb_listdir(path, &count);
	if (!names)
		return 0;

	KprBattery *v = kb_calloc((size_t)(count > 0 ? count : 1), sizeof(*v));
	int n = 0;
	for (char **e = names; *e; e++) {
		if ((*e)[0] == '.')
			continue;
		KprBattery *b = &v[n++];
		memset(b, 0, sizeof(*b));
		b->health = -1.0;
		b->cycles = -1;
		b->capacity = -1;
		kb_strlcpy(b->name, *e, sizeof(b->name));

		char type[24];
		slurp_trim(type, sizeof(type), "class/power_supply/%s/type%s", *e, "");
		b->is_battery = !strcmp(type, "Battery");

		if (!b->is_battery) {
			b->online = (int)kpr_num_sys(-1,
				"class/power_supply/%s/online", *e);
			continue;
		}

		slurp_trim(b->state, sizeof(b->state),
			   "class/power_supply/%s/status%s", *e, "");
		slurp_trim(b->tech, sizeof(b->tech),
			   "class/power_supply/%s/technology%s", *e, "");
		b->capacity = (int)kpr_num_sys(-1, "class/power_supply/%s/capacity", *e);
		b->cycles = (int)kpr_num_sys(-1, "class/power_supply/%s/cycle_count", *e);
		b->voltage_uv = (long)kpr_num_sys(0, "class/power_supply/%s/voltage_now", *e);

		/*
		 * A driver publishes EITHER the energy_* triple (µWh) or the
		 * charge_* one (µAh), never both, and which one is not a
		 * property of the battery — it is a property of the driver.
		 * Reading only energy_* is why a monitor shows an empty
		 * battery page on perfectly ordinary hardware.
		 */
		b->energy_now = (unsigned long long)kpr_num_sys(0,
			"class/power_supply/%s/energy_now", *e);
		b->energy_full = (unsigned long long)kpr_num_sys(0,
			"class/power_supply/%s/energy_full", *e);
		b->energy_full_design = (unsigned long long)kpr_num_sys(0,
			"class/power_supply/%s/energy_full_design", *e);
		if (!b->energy_full) {
			b->energy_now = (unsigned long long)kpr_num_sys(0,
				"class/power_supply/%s/charge_now", *e);
			b->energy_full = (unsigned long long)kpr_num_sys(0,
				"class/power_supply/%s/charge_full", *e);
			b->energy_full_design = (unsigned long long)kpr_num_sys(0,
				"class/power_supply/%s/charge_full_design", *e);
		}

		/*
		 * Health is full / full_design: what the cells hold now
		 * against what they held new. A battery reporting 100% charge
		 * can be at 70% health, and the second number is the one that
		 * says whether it needs replacing.
		 */
		if (b->energy_full && b->energy_full_design)
			b->health = (double)b->energy_full /
				    (double)b->energy_full_design;

		/*
		 * Draw in watts. power_now is µW where the driver has it;
		 * otherwise current × voltage, which is the same quantity the
		 * long way round.
		 */
		b->power_uw = (long)kpr_num_sys(0, "class/power_supply/%s/power_now", *e);
		b->current_ua = (long)kpr_num_sys(0, "class/power_supply/%s/current_now", *e);
		if (!b->power_uw && b->current_ua && b->voltage_uv)
			b->power_uw = (long)((double)b->current_ua *
					     (double)b->voltage_uv / 1e6);
	}
	kb_strv_free(names);
	*out = v;
	return n;
}

void kpr_power_free(KprBattery *b) { free(b); }

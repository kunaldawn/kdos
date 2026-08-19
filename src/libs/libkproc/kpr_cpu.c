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
 * /proc/stat, cpufreq, topology and the temperature sensors.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

static int parse_times(const char *line, KprCpuTimes *t)
{
	memset(t, 0, sizeof(*t));
	return sscanf(line, "%llu %llu %llu %llu %llu %llu %llu %llu",
		      &t->user, &t->nice, &t->sys, &t->idle, &t->iowait,
		      &t->irq, &t->softirq, &t->steal) >= 4 ? 0 : -1;
}

static unsigned long long times_sum(const KprCpuTimes *t)
{
	return t->user + t->nice + t->sys + t->idle + t->iowait + t->irq +
	       t->softirq + t->steal;
}

/*
 * Busy over the DELTA. idle AND iowait both count as not-busy: a machine
 * blocked on a disk is not burning CPU, and folding iowait into busy is how a
 * monitor reports 100% for a process that is asleep.
 */
double kpr_cpu_busy(const KprCpuTimes *prev, const KprCpuTimes *cur)
{
	unsigned long long tp = times_sum(prev), tc = times_sum(cur);
	if (tc <= tp)
		return 0.0;
	unsigned long long ip = prev->idle + prev->iowait;
	unsigned long long ic = cur->idle + cur->iowait;
	double dt = (double)(tc - tp);
	double di = ic > ip ? (double)(ic - ip) : 0.0;
	double busy = 1.0 - di / dt;
	if (busy < 0.0)
		busy = 0.0;
	if (busy > 1.0)
		busy = 1.0;
	return busy;
}

/*
 * The topology, and the reason it is not counted from /proc/cpuinfo: a cpuinfo
 * block is a LOGICAL cpu, so counting them and calling the answer "cores"
 * reports an 8-core machine with SMT as 16 cores. physical_package_id and
 * core_id are the kernel's own answer.
 */
static void read_topology(KprCpu *c)
{
	int pkg_seen[64] = { 0 }, npkg = 0;
	struct { int pkg, core; } cores[512];
	int ncore = 0;

	for (int i = 0; i < c->ncpu && i < 512; i++) {
		long pkg = kpr_num_sys(-1,
			"devices/system/cpu/cpu%d/topology/physical_package_id", i);
		long core = kpr_num_sys(-1,
			"devices/system/cpu/cpu%d/topology/core_id", i);
		if (pkg < 0 || core < 0)
			continue;
		if (pkg >= 0 && pkg < 64 && !pkg_seen[pkg]) {
			pkg_seen[pkg] = 1;
			npkg++;
		}
		int dup = 0;
		for (int k = 0; k < ncore; k++)
			if (cores[k].pkg == (int)pkg && cores[k].core == (int)core) {
				dup = 1;
				break;
			}
		if (!dup && ncore < 512) {
			cores[ncore].pkg = (int)pkg;
			cores[ncore].core = (int)core;
			ncore++;
		}
	}
	/* No topology/ at all (a container, some VMs): say so by falling back
	 * to the logical count rather than reporting zero cores. */
	c->ncore = ncore ? ncore : c->ncpu;
	c->npkg = npkg ? npkg : 1;
}

/*
 * Temperature, in the order worth trying. -1 when nothing answered, which the
 * renderer must draw as an em dash: a machine with no sensor is not a machine
 * running at 0 °C.
 */
static double read_temp(void)
{
	static const char *want[] = { "coretemp", "k10temp", "zenpower",
				      "cpu_thermal", "acpitz", NULL };
	char *names = NULL;
	for (int i = 0; i < 32; i++) {
		names = kpr_slurp_sys("class/hwmon/hwmon%d/name", i);
		if (!names)
			continue;
		for (int k = 0; want[k]; k++) {
			if (strncmp(names, want[k], strlen(want[k]))) 
				continue;
			for (int t = 1; t <= 4; t++) {
				long long mc = kpr_num_sys(-1,
					"class/hwmon/hwmon%d/temp%d_input", i, t);
				if (mc > 0) {
					free(names);
					return (double)mc / 1000.0;
				}
			}
		}
		free(names);
		names = NULL;
	}

	/* thermal_zone by type, for the boards with no hwmon entry. */
	for (int i = 0; i < 16; i++) {
		char *type = kpr_slurp_sys("class/thermal/thermal_zone%d/type", i);
		if (!type)
			continue;
		int match = strstr(type, "cpu") || strstr(type, "x86_pkg") ||
			    strstr(type, "acpitz");
		free(type);
		if (!match)
			continue;
		long long mc = kpr_num_sys(-1, "class/thermal/thermal_zone%d/temp", i);
		if (mc > 0)
			return (double)mc / 1000.0;
	}
	return -1.0;
}

static void read_model(KprCpu *c)
{
	char *ci = kpr_slurp_proc("cpuinfo");
	kb_strlcpy(c->arch, "x86_64", sizeof(c->arch));
	if (!ci)
		return;

	for (char *line = ci, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!c->model[0] && !strncmp(line, "model name", 10)) {
			char *v = strchr(line, ':');
			if (v) {
				v++;
				while (*v == ' ' || *v == '\t')
					v++;
				kb_strlcpy(c->model, v, sizeof(c->model));
			}
		}
		/* The hypervisor flag is the honest "am I virtualised" signal
		 * that needs no DMI read; sys_vendor refines WHICH below. */
		if (strstr(line, "hypervisor") && c->virt == KPR_VIRT_NONE)
			c->virt = KPR_VIRT_UNKNOWN;
		if (nl)
			*nl = '\n';
	}
	free(ci);

	char *vendor = kpr_slurp_sys("class/dmi/id/sys_vendor");
	if (vendor) {
		if (strstr(vendor, "QEMU"))
			c->virt = KPR_VIRT_QEMU;
		else if (strstr(vendor, "KVM"))
			c->virt = KPR_VIRT_KVM;
		else if (strstr(vendor, "VMware"))
			c->virt = KPR_VIRT_VMWARE;
		else if (strstr(vendor, "innotek") || strstr(vendor, "VirtualBox"))
			c->virt = KPR_VIRT_VBOX;
		else if (strstr(vendor, "Microsoft"))
			c->virt = KPR_VIRT_HYPERV;
		else if (strstr(vendor, "Xen"))
			c->virt = KPR_VIRT_XEN;
		free(vendor);
	}
	if (!c->model[0])
		kb_strlcpy(c->model, "unknown", sizeof(c->model));
}

int kpr_cpu_read(KprCpu *c)
{
	memset(c, 0, sizeof(*c));
	c->temp_c = -1.0;

	char *stat = kpr_slurp_proc("stat");
	if (!stat)
		return -1;

	/* Count the per-cpu lines first so the arrays are sized once. */
	for (char *p = stat; (p = strstr(p, "cpu")); p++)
		if (isdigit((unsigned char)p[3]))
			c->ncpu++;
	if (c->ncpu <= 0)
		c->ncpu = 1;

	c->per = kb_calloc((size_t)c->ncpu, sizeof(*c->per));
	c->khz = kb_calloc((size_t)c->ncpu, sizeof(*c->khz));
	for (int i = 0; i < c->ncpu; i++)
		c->khz[i] = -1;

	for (char *line = stat, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;
		if (!strncmp(line, "cpu ", 4)) {
			parse_times(line + 4, &c->total);
		} else if (!strncmp(line, "cpu", 3) &&
			   isdigit((unsigned char)line[3])) {
			int idx = atoi(line + 3);
			char *sp = strchr(line, ' ');
			if (sp && idx >= 0 && idx < c->ncpu)
				parse_times(sp + 1, &c->per[idx]);
		}
		if (nl)
			*nl = '\n';
	}
	free(stat);

	read_topology(c);
	read_model(c);
	c->temp_c = read_temp();

	/* Frequency: scaling_cur_freq, then cpuinfo_cur_freq. Both are kHz. */
	for (int i = 0; i < c->ncpu; i++) {
		long long k = kpr_num_sys(-1,
			"devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		if (k <= 0)
			k = kpr_num_sys(-1,
				"devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", i);
		c->khz[i] = k > 0 ? (long)k : -1;
	}
	c->khz_max = (long)kpr_num_sys(-1,
		"devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");

	char *gov = kpr_slurp_sys("devices/system/cpu/cpu0/cpufreq/scaling_governor");
	if (gov) {
		char *nl = strchr(gov, '\n');
		if (nl)
			*nl = 0;
		kb_strlcpy(c->governor, gov, sizeof(c->governor));
		free(gov);
	}
	return 0;
}

void kpr_cpu_free(KprCpu *c)
{
	if (!c)
		return;
	free(c->per);
	free(c->khz);
	c->per = NULL;
	c->khz = NULL;
}

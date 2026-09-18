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
 *
 * THE EARLIEST NAME IN temp_want[] WINS, WHATEVER ITS hwmon INDEX. The index is
 * allocated from one system-wide ida and says nothing about what the chip
 * measures: acpitz registers with the ACPI tables and coretemp only when the
 * CPU driver loads, so taking the first index that matches anything reports
 * the chassis sensor — a flat figure that barely moves under load — on the
 * commonest laptop there is. One pass over the chips, keeping the best rank
 * seen, and an early exit once the top rank answers because nothing can beat
 * it.
 *
 * A die sensor's location is then LATCHED: it is the best answer that
 * exists, so re-searching for a better one every tick can only find what was
 * already found. acpitz is not latched — it is the fallback, and the die
 * driver may still load. The latch re-checks the chip's name, because an
 * hwmon index freed by a module unload is handed to the next chip to
 * register and the file at that path would then be another device's
 * temperature.
 */
static const char *const temp_want[] = { "coretemp", "k10temp", "zenpower",
					 "cpu_thermal", "acpitz", NULL };
#define TEMP_FALLBACK_RANK 4		/* acpitz: re-searched every tick */

static unsigned g_temp_gen;		/* 0 = nothing latched            */
static int g_temp_chip, g_temp_chan, g_temp_rank;

static double read_temp(void)
{
	int idx[256];
	int nchip, best = -1, best_chan = 0, best_rank = TEMP_FALLBACK_RANK + 1;
	double best_val = -1.0;

	if (g_temp_gen == kpr_root_gen()) {
		char name[64];
		int n = kpr_read_into_sys(name, sizeof(name),
					  "class/hwmon/hwmon%d/name",
					  g_temp_chip);
		long long mc = -1;

		if (n > 0 && !strncmp(name, temp_want[g_temp_rank],
				      strlen(temp_want[g_temp_rank])))
			mc = kpr_num_sys(-1, "class/hwmon/hwmon%d/temp%d_input",
					 g_temp_chip, g_temp_chan);
		if (mc > 0)
			return (double)mc / 1000.0;
		g_temp_gen = 0;
	}

	nchip = kpr_sysfs_indices("class/hwmon", "hwmon", idx,
				  (int)(sizeof(idx) / sizeof(*idx)));
	for (int c = 0; c < nchip && best_rank > 0; c++) {
		int i = idx[c];
		char name[64];
		int n = kpr_read_into_sys(name, sizeof(name),
					  "class/hwmon/hwmon%d/name", i);

		if (n <= 0)
			continue;
		for (int k = 0; temp_want[k] && k < best_rank; k++) {
			if (strncmp(name, temp_want[k], strlen(temp_want[k])))
				continue;
			for (int t = 1; t <= 4; t++) {
				long long mc = kpr_num_sys(-1,
					"class/hwmon/hwmon%d/temp%d_input", i, t);
				if (mc <= 0)
					continue;
				best = i;
				best_chan = t;
				best_rank = k;
				best_val = (double)mc / 1000.0;
				break;
			}
			break;
		}
	}
	if (best >= 0) {
		if (best_rank < TEMP_FALLBACK_RANK) {
			g_temp_chip = best;
			g_temp_chan = best_chan;
			g_temp_rank = best_rank;
			g_temp_gen = kpr_root_gen();
		}
		return best_val;
	}

	/* thermal_zone by type, for the boards with no hwmon entry. */
	nchip = kpr_sysfs_indices("class/thermal", "thermal_zone", idx,
				  (int)(sizeof(idx) / sizeof(*idx)));
	for (int c = 0; c < nchip; c++) {
		int i = idx[c];
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

/*
 * The half of a snapshot that cannot change while the machine runs: the core
 * and package counts, the model string, the architecture, the virtualisation
 * kind and the maximum frequency.
 *
 * Re-deriving them is two topology files per logical CPU plus the whole of
 * /proc/cpuinfo — 26 KB on sixteen cores — and a monitor samples once a tick
 * inside its draw loop. The latch is keyed on the logical CPU COUNT and not
 * on a bare flag, because hotplug changes ncpu and with it ncore and npkg,
 * and a flag would report the old topology for ever. It is keyed on the root
 * generation too, or a fixture switch reports this host's CPU under a
 * recorded machine.
 */
static unsigned g_static_gen;		/* 0 = empty                      */
static int g_static_ncpu;
static int g_ncore, g_npkg, g_virt;
static char g_model[96], g_arch[16];
static long g_khz_max;

int kpr_cpu_read(KprCpu *c)
{
	memset(c, 0, sizeof(*c));
	c->temp_c = -1.0;

	char *stat = kpr_slurp_proc("stat");
	if (!stat)
		return -1;

	/*
	 * SIZED BY THE HIGHEST CPU NUMBER, not by how many lines there are.
	 * /proc/stat lists only the online CPUs and keeps their real numbers,
	 * so with cpu1 offline the file holds cpu0, cpu2 and cpu3: a count
	 * would size three slots, drop cpu3's times entirely and leave slot 1
	 * all zeroes, which reads as a core pinned at 0%. The hole is marked
	 * in `online` instead.
	 */
	int maxidx = -1;
	for (char *p = stat; (p = strstr(p, "cpu")); p++)
		if (isdigit((unsigned char)p[3])) {
			int idx = atoi(p + 3);
			if (idx > maxidx)
				maxidx = idx;
		}
	c->ncpu = maxidx + 1;
	if (c->ncpu <= 0)
		c->ncpu = 1;

	c->per = kb_calloc((size_t)c->ncpu, sizeof(*c->per));
	c->khz = kb_calloc((size_t)c->ncpu, sizeof(*c->khz));
	c->online = kb_calloc((size_t)c->ncpu, sizeof(*c->online));
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
			if (sp && idx >= 0 && idx < c->ncpu) {
				parse_times(sp + 1, &c->per[idx]);
				c->online[idx] = 1;
			}
		}
		if (nl)
			*nl = '\n';
	}
	free(stat);

	if (g_static_gen != kpr_root_gen() || g_static_ncpu != c->ncpu) {
		read_topology(c);
		read_model(c);
		c->khz_max = (long)kpr_num_sys(-1,
			"devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
		g_ncore = c->ncore;
		g_npkg = c->npkg;
		g_virt = c->virt;
		g_khz_max = c->khz_max;
		kb_strlcpy(g_model, c->model, sizeof(g_model));
		kb_strlcpy(g_arch, c->arch, sizeof(g_arch));
		g_static_ncpu = c->ncpu;
		g_static_gen = kpr_root_gen();
	} else {
		c->ncore = g_ncore;
		c->npkg = g_npkg;
		c->virt = g_virt;
		c->khz_max = g_khz_max;
		kb_strlcpy(c->model, g_model, sizeof(c->model));
		kb_strlcpy(c->arch, g_arch, sizeof(c->arch));
	}
	c->temp_c = read_temp();

	/* Frequency: scaling_cur_freq, then cpuinfo_cur_freq. Both are kHz.
	 * An offline CPU has no cpufreq directory and keeps -1, which is the
	 * same answer as a machine with no cpufreq at all and is what the
	 * renderer already draws as absent. */
	for (int i = 0; i < c->ncpu; i++) {
		if (!c->online[i])
			continue;
		long long k = kpr_num_sys(-1,
			"devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		if (k <= 0)
			k = kpr_num_sys(-1,
				"devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", i);
		c->khz[i] = k > 0 ? (long)k : -1;
	}

	/* The governor is NOT latched with the rest: it is the one of these a
	 * user changes while the machine runs. */
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

/*
 * HOW MANY CPUs ARE RUNNING, which `ncpu` is not: that is the array length,
 * the highest CPU number plus one. Anything dividing by "the number of CPUs"
 * — a per-core percentage rescaled to percent-of-machine — must use this, or
 * a machine with a CPU offline below the highest index reports every figure
 * scaled down by the missing core's share.
 *
 * Never zero, so it is safe as a divisor: a KprCpu that has not been read yet
 * has no `online` array and counts as one CPU.
 */
int kpr_cpu_online(const KprCpu *c)
{
	int n = 0;

	if (!c)
		return 1;
	for (int i = 0; i < c->ncpu; i++)
		if (!c->online || c->online[i])
			n++;
	return n ? n : 1;
}

void kpr_cpu_free(KprCpu *c)
{
	if (!c)
		return;
	free(c->per);
	free(c->khz);
	free(c->online);
	c->per = NULL;
	c->khz = NULL;
	c->online = NULL;
}

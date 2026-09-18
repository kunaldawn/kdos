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
 * Block devices, from /proc/diskstats and /sys/class/block.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kproc.h"

/*
 * Virtual devices are FLAGGED, not dropped. A user who asked to see loop and
 * zram devices still gets them; the caller filters. Dropping them here would
 * make the library the place that decides, and it is not.
 */
static int is_virtual(const char *name)
{
	return !strncmp(name, "loop", 4) || !strncmp(name, "ram", 3) ||
	       !strncmp(name, "zram", 4) || !strncmp(name, "dm-", 3);
}

/*
 * What a block device node cannot change while it exists: whether it is a
 * partition, whether it rotates, whether it is removable, its model string
 * and where its temperature lives.
 *
 * The Drives page frees and rebuilds the whole list on every tick inside the
 * draw loop, so re-reading these is a dozen opens per disk per frame to
 * re-learn constants.
 *
 * `size` IS NOT HERE AND MUST NOT BE. An optical drive keeps its name across
 * a disc change, an LVM volume across an lvresize and a loop device across a
 * losetup -c: a cached capacity is a wrong number on screen with nothing to
 * say so, and it costs one read to be right.
 *
 * An entry lives while its name is in /proc/diskstats and dies with it, so a
 * name reused by a different device re-reads. kpr_root_gen() drops the lot,
 * or a fixture switch describes this host's disks under a recorded machine.
 */
struct blkc {
	char name[32];
	int partition;			/* its parent carries the io       */
	int rotational, removable;
	char model[64];
	char temp[80];			/* path under device/, "" = none   */
	int filled;			/* the constants above are read    */
	int seen;			/* this device is still in diskstats */
};

static struct blkc *g_bc;
static int g_bc_n, g_bc_cap;
static unsigned g_bc_gen;

static struct blkc *blkc_get(const char *name)
{
	for (int i = 0; i < g_bc_n; i++)
		if (!strcmp(g_bc[i].name, name))
			return &g_bc[i];
	if (g_bc_n == g_bc_cap) {
		int want = g_bc_cap ? g_bc_cap * 2 : 16;
		struct blkc *bigger = realloc(g_bc, (size_t)want * sizeof(*bigger));

		if (!bigger)
			return NULL;
		g_bc = bigger;
		g_bc_cap = want;
	}
	struct blkc *e = &g_bc[g_bc_n++];
	memset(e, 0, sizeof(*e));
	kb_strlcpy(e->name, name, sizeof(e->name));
	return e;
}

/*
 * Drive temperature, where the driver publishes one: nvme exposes hwmon
 * under the device, and SATA disks do only with the drivetemp module loaded.
 * Empty when nothing answered, and -1 then renders as an em dash.
 *
 * BOTH LAYOUTS EXIST and neither is a bounded index scan. The hwmon number
 * comes from one system-wide ida, so a drive on a laptop that already has
 * acpitz, coretemp, a wireless card, two power supplies and a dGPU lands at
 * hwmon7 or above and a 0..7 probe finds nothing at all. Some drivers hang
 * the chip directly under `device` as `hwmonN`, others under a glue
 * directory as `device/hwmon/hwmonN`; the readdir answers for both and
 * invents neither.
 */
static void find_temp(const char *name, char *out, size_t cap)
{
	char dir[640];
	int n = 0;
	char **v;

	*out = 0;
	snprintf(dir, sizeof(dir), "%s/class/block/%s/device", kpr_sys(), name);
	v = kb_listdir(dir, &n);
	if (!v)
		return;
	for (int i = 0; i < n && !*out; i++) {
		if (strncmp(v[i], "hwmon", 5))
			continue;
		if (v[i][5]) {
			if (kpr_num_sys(-1, "class/block/%s/device/%s/temp1_input",
					name, v[i]) > 0)
				snprintf(out, cap, "%s", v[i]);
			continue;
		}
		char sub[768];
		int m = 0;
		char **w;

		snprintf(sub, sizeof(sub), "%s/hwmon", dir);
		w = kb_listdir(sub, &m);
		for (int k = 0; w && k < m && !*out; k++) {
			if (strncmp(w[k], "hwmon", 5))
				continue;
			if (kpr_num_sys(-1,
					"class/block/%s/device/hwmon/%s/temp1_input",
					name, w[k]) > 0)
				snprintf(out, cap, "hwmon/%s", w[k]);
		}
		kb_strv_free(w);
	}
	kb_strv_free(v);
}

static double disk_temp(struct blkc *e, const char *name)
{
	long long mc;

	/* A miss is re-probed rather than remembered: drivetemp can be loaded
	 * after the monitor started, and the readdir is microseconds. */
	if (!e->temp[0])
		find_temp(name, e->temp, sizeof(e->temp));
	if (!e->temp[0])
		return -1.0;
	mc = kpr_num_sys(-1, "class/block/%s/device/%s/temp1_input", name,
			 e->temp);
	if (mc > 0)
		return (double)mc / 1000.0;
	e->temp[0] = 0;			/* the chip went away */
	return -1.0;
}

int kpr_block_list(KprDisk **out)
{
	*out = NULL;
	char *ds = kpr_slurp_proc("diskstats");
	if (!ds)
		return 0;

	if (g_bc_gen != kpr_root_gen()) {
		g_bc_n = 0;
		g_bc_gen = kpr_root_gen();
	}
	for (int i = 0; i < g_bc_n; i++)
		g_bc[i].seen = 0;

	int cap = 16, n = 0;
	KprDisk *d = kb_calloc((size_t)cap, sizeof(*d));

	for (char *line = ds, *next; line && *line; line = next) {
		char *nl = strchr(line, '\n');
		next = nl ? nl + 1 : NULL;
		if (nl)
			*nl = 0;

		char name[32] = "";
		unsigned long long rd_s = 0, wr_s = 0, ticks = 0;
		/*
		 * major minor name  rd_ios rd_merges rd_sectors rd_ticks
		 * wr_ios wr_merges wr_sectors wr_ticks in_flight io_ticks ...
		 *
		 * io_ticks is field 10 of the per-device counters (the 13th
		 * column overall) and is milliseconds the queue was busy,
		 * which is what gives a utilisation percentage.
		 */
		if (sscanf(line, " %*u %*u %31s %*u %*u %llu %*u %*u %*u %llu"
				 " %*u %*u %llu",
			   name, &rd_s, &wr_s, &ticks) != 4)
			goto cont;
		if (!name[0])
			goto cont;

		struct blkc *e = blkc_get(name);
		if (!e)
			goto cont;
		e->seen = 1;
		if (!e->filled) {
			char probe[512];

			e->filled = 1;
			/*
			 * WHOLE DISKS ONLY. diskstats lists sda beside sda1
			 * and sda2, so summing every line counts each byte
			 * two or three times. A partition has a `partition`
			 * attribute; its parent does not. That test is the
			 * kernel's own answer and does not depend on parsing
			 * the name.
			 *
			 * A device with no sysfs entry at all is not a disk
			 * we can describe; diskstats can carry stale rows,
			 * and such a row gets no cache entry so the next tick
			 * looks again.
			 */
			snprintf(probe, sizeof(probe),
				 "%s/class/block/%s/partition", kpr_sys(), name);
			e->partition = kb_path_exists(probe);
			if (!e->partition) {
				snprintf(probe, sizeof(probe),
					 "%s/class/block/%s", kpr_sys(), name);
				if (!kb_path_exists(probe)) {
					/* swap-remove; a self-copy when e is
					 * the last entry */
					*e = g_bc[--g_bc_n];
					goto cont;
				}
				e->rotational = (int)kpr_num_sys(-1,
					"class/block/%s/queue/rotational", name);
				e->removable = (int)kpr_num_sys(0,
					"class/block/%s/removable", name);

				char *model = kpr_slurp_sys(
					"class/block/%s/device/model", name);
				if (model) {
					char *x = strchr(model, '\n');
					if (x)
						*x = 0;
					/* trailing pad is normal in the SCSI
					 * inquiry string */
					for (int i = (int)strlen(model) - 1;
					     i >= 0 && model[i] == ' '; i--)
						model[i] = 0;
					kb_strlcpy(e->model, model,
						   sizeof(e->model));
					free(model);
				}
			}
		}
		if (e->partition)
			goto cont;

		if (n == cap) {
			cap *= 2;
			KprDisk *nd = kb_calloc((size_t)cap, sizeof(*nd));
			memcpy(nd, d, (size_t)n * sizeof(*nd));
			free(d);
			d = nd;
		}
		KprDisk *k = &d[n++];
		memset(k, 0, sizeof(*k));
		kb_strlcpy(k->name, name, sizeof(k->name));
		k->rd_sectors = rd_s;
		k->wr_sectors = wr_s;
		k->io_ticks = ticks;
		k->virt = is_virtual(name);
		k->rotational = e->rotational;
		k->removable = e->removable;
		kb_strlcpy(k->model, e->model, sizeof(k->model));
		/* size is in 512-byte units in sysfs, always, whatever the
		 * drive's own sector size. Read every tick: it is the one of
		 * these that moves under a name that does not. */
		k->size = (unsigned long long)kpr_num_sys(0, "class/block/%s/size", name)
			  * KPR_SECTOR;
		k->temp_c = disk_temp(e, name);
cont:
		if (nl)
			*nl = '\n';
	}
	free(ds);

	/* A name that left diskstats left the machine. Dropping the entry is
	 * what keeps a re-used name — a second USB stick arriving as sdb —
	 * from inheriting the first one's model and temperature. */
	for (int i = 0; i < g_bc_n; ) {
		if (g_bc[i].seen) {
			i++;
			continue;
		}
		g_bc[i] = g_bc[--g_bc_n];
	}

	*out = d;
	return n;
}

void kpr_block_free(KprDisk *d) { free(d); }

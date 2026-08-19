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

/* Drive temperature, where the driver publishes one: nvme exposes hwmon under
 * the device, and SATA disks do only with the drivetemp module loaded. -1
 * otherwise, and -1 renders as an em dash. */
static double disk_temp(const char *name)
{
	for (int i = 0; i < 8; i++) {
		long long mc = kpr_num_sys(-1,
			"class/block/%s/device/hwmon%d/temp1_input", name, i);
		if (mc > 0)
			return (double)mc / 1000.0;
	}
	long long mc = kpr_num_sys(-1,
		"class/block/%s/device/hwmon/temp1_input", name);
	return mc > 0 ? (double)mc / 1000.0 : -1.0;
}

int kpr_block_list(KprDisk **out)
{
	*out = NULL;
	char *ds = kpr_slurp_proc("diskstats");
	if (!ds)
		return 0;

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

		/*
		 * WHOLE DISKS ONLY. diskstats lists sda beside sda1 and sda2,
		 * so summing every line counts each byte two or three times.
		 * A partition has a `partition` attribute; its parent does
		 * not. That test is the kernel's own answer and does not
		 * depend on parsing the name.
		 */
		char probe[512];
		snprintf(probe, sizeof(probe), "%s/class/block/%s/partition",
			 kpr_sys(), name);
		if (kb_path_exists(probe))
			goto cont;

		/* A device with no sysfs entry at all is not a disk we can
		 * describe; diskstats can carry stale rows. */
		snprintf(probe, sizeof(probe), "%s/class/block/%s",
			 kpr_sys(), name);
		if (!kb_path_exists(probe))
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
		k->rotational = (int)kpr_num_sys(-1, "class/block/%s/queue/rotational", name);
		k->removable = (int)kpr_num_sys(0, "class/block/%s/removable", name);
		/* size is in 512-byte units in sysfs, always, whatever the
		 * drive's own sector size. */
		k->size = (unsigned long long)kpr_num_sys(0, "class/block/%s/size", name)
			  * KPR_SECTOR;
		k->temp_c = disk_temp(name);

		char *model = kpr_slurp_sys("class/block/%s/device/model", name);
		if (model) {
			char *e = strchr(model, '\n');
			if (e)
				*e = 0;
			/* trailing pad is normal in the SCSI inquiry string */
			for (int i = (int)strlen(model) - 1; i >= 0 && model[i] == ' '; i--)
				model[i] = 0;
			kb_strlcpy(k->model, model, sizeof(k->model));
			free(model);
		}
cont:
		if (nl)
			*nl = '\n';
	}
	free(ds);
	*out = d;
	return n;
}

void kpr_block_free(KprDisk *d) { free(d); }

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — what the machine actually is
 *
 * Everything here is read straight from /sys, /proc and the block device.
 * lsblk and blkid exist on KDOS, but parsing their output means the
 * installer inherits their formatting and their exit codes; a superblock is
 * a stable ABI and a 200-line reader is cheaper than either.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "kinstall.h"

Disk ki_disk[MAX_DISKS];
int ki_ndisk;
SysInfo ki_sys;

static const unsigned char GUID_ESP[16] = {
	0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
	0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

Disk *disk_by_path(const char *path)
{
	for (int i = 0; i < ki_ndisk; i++)
		if (!strcmp(ki_disk[i].path, path))
			return &ki_disk[i];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */

static unsigned long long sysfs_ull(const char *fmt, ...)
{
	char path[256], buf[64];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0)
		return 0;
	return strtoull(buf, NULL, 10);
}

static void sysfs_str(char *out, size_t cap, const char *fmt, ...)
{
	char path[256], buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);
	out[0] = 0;
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0)
		return;
	char *p = buf;
	while (*p == ' ')
		p++;
	size_t n = strlen(p);
	while (n && (p[n - 1] == ' ' || p[n - 1] == '\n'))
		p[--n] = 0;
	kb_strlcpy(out, p, cap);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void uuid_fmt(char *out, const unsigned char *u)
{
	snprintf(out, 40,
		 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		 u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
		 u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

static void trim_label(char *s)
{
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t'))
		s[--n] = 0;
	for (char *p = s; *p; p++)
		if ((unsigned char)*p < 0x20)
			*p = ' ';
}

/* A small blkid: enough of every superblock KDOS can create or meet. */
static void sniff_fs(const char *path, Part *p)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;

	unsigned char sb[4096];
	unsigned char boot[512];

	if (pread(fd, boot, sizeof(boot), 0) == (ssize_t)sizeof(boot)) {
		if (!memcmp(boot + 3, "NTFS    ", 8)) {
			kb_strlcpy(p->fstype, "ntfs", sizeof(p->fstype));
		} else if (!memcmp(boot + 0x52, "FAT32", 5)) {
			kb_strlcpy(p->fstype, "vfat", sizeof(p->fstype));
			snprintf(p->uuid, sizeof(p->uuid), "%02X%02X-%02X%02X",
				 boot[0x46], boot[0x45], boot[0x44], boot[0x43]);
			memcpy(p->label, boot + 0x47, 11);
			p->label[11] = 0;
			trim_label(p->label);
		} else if (!memcmp(boot + 0x36, "FAT", 3)) {
			kb_strlcpy(p->fstype, "vfat", sizeof(p->fstype));
			snprintf(p->uuid, sizeof(p->uuid), "%02X%02X-%02X%02X",
				 boot[0x2a], boot[0x29], boot[0x28], boot[0x27]);
			memcpy(p->label, boot + 0x2b, 11);
			p->label[11] = 0;
			trim_label(p->label);
		} else if (!memcmp(boot, "XFSB", 4)) {
			kb_strlcpy(p->fstype, "xfs", sizeof(p->fstype));
			uuid_fmt(p->uuid, boot + 32);
		} else if (!memcmp(boot, "hsqs", 4)) {
			kb_strlcpy(p->fstype, "squashfs", sizeof(p->fstype));
		}
	}

	/* ext2/3/4 superblock lives at 1 KiB, magic 0xEF53 at +0x38. */
	if (!p->fstype[0] &&
	    pread(fd, sb, 1024, 1024) == 1024 &&
	    sb[0x38] == 0x53 && sb[0x39] == 0xef) {
		unsigned incompat = (unsigned)sb[0x60] | ((unsigned)sb[0x61] << 8) |
				    ((unsigned)sb[0x62] << 16) | ((unsigned)sb[0x63] << 24);
		kb_strlcpy(p->fstype, (incompat & 0x40) ? "ext4" : "ext2",
			 sizeof(p->fstype));
		uuid_fmt(p->uuid, sb + 0x68);
		memcpy(p->label, sb + 0x78, 16);
		p->label[16] = 0;
		trim_label(p->label);
	}

	if (!p->fstype[0] && pread(fd, sb, 4096, 0x10000) == 4096 &&
	    !memcmp(sb + 0x40, "_BHRfS_M", 8)) {
		kb_strlcpy(p->fstype, "btrfs", sizeof(p->fstype));
		uuid_fmt(p->uuid, sb + 0x20);
	}

	if (!p->fstype[0] && pread(fd, sb, 4096, 0) == 4096 &&
	    !memcmp(sb + 4086, "SWAPSPACE2", 10)) {
		kb_strlcpy(p->fstype, "swap", sizeof(p->fstype));
		uuid_fmt(p->uuid, sb + 0x40c);
	}

	close(fd);
}

/* Re-read one device after mkfs has changed it under us. */
void probe_part(const char *path, Part *p)
{
	memset(p, 0, sizeof(*p));
	kb_strlcpy(p->path, path, sizeof(p->path));
	kb_strlcpy(p->name, kb_basename(path), sizeof(p->name));
	sniff_fs(path, p);
}

/* GPT: read the header, then the entry that matches this partition number, so
 * an ESP is identified by its type GUID rather than by "it looks like FAT". */
static void read_gpt(Disk *d)
{
	/* Set before the open, not after: a disk we could not open at all
	 * (running unprivileged, or a device that vanished mid-probe) has an
	 * UNKNOWN table, and an empty string renders as a blank column that
	 * reads like "no partition table". */
	kb_strlcpy(d->table, "-", sizeof(d->table));

	int fd = open(d->path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;

	unsigned char hdr[512], mbr[512];

	if (pread(fd, mbr, 512, 0) == 512 && mbr[510] == 0x55 && mbr[511] == 0xaa)
		kb_strlcpy(d->table, "dos", sizeof(d->table));

	if (pread(fd, hdr, 512, 512) != 512 || memcmp(hdr, "EFI PART", 8)) {
		close(fd);
		return;
	}
	kb_strlcpy(d->table, "gpt", sizeof(d->table));

	unsigned long long ent_lba = 0;
	memcpy(&ent_lba, hdr + 72, 8);
	unsigned nent = 0, esz = 0;
	memcpy(&nent, hdr + 80, 4);
	memcpy(&esz, hdr + 84, 4);
	if (esz < 128 || esz > 4096 || nent > 512) {
		close(fd);
		return;
	}

	unsigned char *ent = malloc((size_t)esz);
	if (!ent) {
		close(fd);
		return;
	}
	for (unsigned i = 0; i < nent; i++) {
		if (pread(fd, ent, esz, (off_t)(ent_lba * 512 + i * esz)) != (ssize_t)esz)
			break;
		int empty = 1;
		for (int k = 0; k < 16; k++)
			if (ent[k]) {
				empty = 0;
				break;
			}
		if (empty)
			continue;
		unsigned long long first = 0;
		memcpy(&first, ent + 32, 8);
		for (int k = 0; k < d->nparts; k++) {
			if (d->part[k].start != first)
				continue;
			if (!memcmp(ent, GUID_ESP, 16))
				d->part[k].is_esp = 1;
		}
	}
	free(ent);
	close(fd);
}

/* ──────────────────────────────────────────────────────────────────────── */

static void apply_mounts(void)
{
	/* Read to real EOF: a truncated /proc/mounts loses the tail, and the
	 * boot media is on it as often as not — marking the stick we are
	 * running from as an ordinary install target. */
	size_t len = 0;
	char *buf = kb_read_all("/proc/mounts", &len);
	if (!buf)
		return;

	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[128], mnt[192], type[32];
		if (sscanf(line, "%127s %191s %31s", dev, mnt, type) != 3)
			continue;

		int boot_media = !strcmp(type, "iso9660") || !strcmp(type, "squashfs");

		for (int i = 0; i < ki_ndisk; i++) {
			if (!strcmp(ki_disk[i].path, dev) && boot_media)
				ki_disk[i].is_boot_media = 1;
			for (int k = 0; k < ki_disk[i].nparts; k++) {
				if (strcmp(ki_disk[i].part[k].path, dev))
					continue;
				ki_disk[i].part[k].mounted = 1;
				kb_strlcpy(ki_disk[i].part[k].mountpoint, mnt,
					 sizeof(ki_disk[i].part[k].mountpoint));
				if (boot_media)
					ki_disk[i].is_boot_media = 1;
			}
		}
	}
	free(buf);
}

static int is_disk_name(const char *n)
{
	static const char *skip[] = { "loop", "ram", "sr", "zram", "dm-", "md",
				      "fd", "nbd", NULL };
	for (int i = 0; skip[i]; i++)
		if (!strncmp(n, skip[i], strlen(skip[i])))
			return 0;
	return 1;
}

static void disk_transport(Disk *d)
{
	char link[512];
	char path[256];
	snprintf(path, sizeof(path), "/sys/block/%s", d->name);
	ssize_t n = readlink(path, link, sizeof(link) - 1);
	if (n > 0) {
		link[n] = 0;
		if (strstr(link, "/usb"))
			kb_strlcpy(d->tran, "usb", sizeof(d->tran));
		else if (strstr(link, "/virtio"))
			kb_strlcpy(d->tran, "virtio", sizeof(d->tran));
		else if (strstr(link, "/nvme"))
			kb_strlcpy(d->tran, "nvme", sizeof(d->tran));
		else if (strstr(link, "/mmc"))
			kb_strlcpy(d->tran, "mmc", sizeof(d->tran));
	}
	if (!d->tran[0]) {
		if (!strncmp(d->name, "nvme", 4))
			kb_strlcpy(d->tran, "nvme", sizeof(d->tran));
		else if (!strncmp(d->name, "vd", 2))
			kb_strlcpy(d->tran, "virtio", sizeof(d->tran));
		else if (!strncmp(d->name, "mmcblk", 6))
			kb_strlcpy(d->tran, "mmc", sizeof(d->tran));
		else
			kb_strlcpy(d->tran, "sata", sizeof(d->tran));
	}
}

void probe_disks(void)
{
	ki_ndisk = 0;

	DIR *dir = opendir("/sys/block");
	if (!dir)
		return;

	struct dirent *e;
	while ((e = readdir(dir)) && ki_ndisk < MAX_DISKS) {
		if (e->d_name[0] == '.' || !is_disk_name(e->d_name))
			continue;

		Disk *d = &ki_disk[ki_ndisk];
		memset(d, 0, sizeof(*d));
		kb_strlcpy(d->name, e->d_name, sizeof(d->name));
		char devpath[300];
		snprintf(devpath, sizeof(devpath), "/dev/%s", e->d_name);
		kb_strlcpy(d->path, devpath, sizeof(d->path));
		if (!kb_path_exists(d->path))
			continue;

		d->sectors = sysfs_ull("/sys/block/%s/size", d->name);
		if (!d->sectors)
			continue;
		d->sector_size = (int)sysfs_ull("/sys/block/%s/queue/logical_block_size",
						d->name);
		if (d->sector_size <= 0)
			d->sector_size = 512;
		d->rotational = (int)sysfs_ull("/sys/block/%s/queue/rotational", d->name);
		d->removable = (int)sysfs_ull("/sys/block/%s/removable", d->name);
		d->readonly = (int)sysfs_ull("/sys/block/%s/ro", d->name);

		sysfs_str(d->model, sizeof(d->model), "/sys/block/%s/device/model",
			  d->name);
		if (!d->model[0])
			sysfs_str(d->model, sizeof(d->model),
				  "/sys/block/%s/device/name", d->name);
		if (!d->model[0])
			kb_strlcpy(d->model, "Unknown device", sizeof(d->model));
		disk_transport(d);

		char sub[256];
		snprintf(sub, sizeof(sub), "/sys/block/%s", d->name);
		DIR *pd = opendir(sub);
		if (pd) {
			struct dirent *pe;
			while ((pe = readdir(pd)) && d->nparts < MAX_PARTS) {
				if (strncmp(pe->d_name, d->name, strlen(d->name)))
					continue;
				/* Explicit precisions, so the bound is visible to
				 * the compiler as well as to the reader. */
				char chk[320];
				snprintf(chk, sizeof(chk),
					 "/sys/block/%.31s/%.255s/partition",
					 d->name, pe->d_name);
				if (!kb_path_exists(chk))
					continue;

				Part *p = &d->part[d->nparts];
				memset(p, 0, sizeof(*p));
				kb_strlcpy(p->name, pe->d_name, sizeof(p->name));
				char pp[300];
				snprintf(pp, sizeof(pp), "/dev/%s", pe->d_name);
				kb_strlcpy(p->path, pp, sizeof(p->path));
				p->start = sysfs_ull("/sys/block/%s/%s/start",
						     d->name, pe->d_name);
				p->sectors = sysfs_ull("/sys/block/%s/%s/size",
						       d->name, pe->d_name);
				sniff_fs(p->path, p);
				d->nparts++;
			}
			closedir(pd);
		}

		/* Partition order in sysfs is directory order, not disk order. */
		for (int i = 1; i < d->nparts; i++) {
			Part t = d->part[i];
			int k = i - 1;
			while (k >= 0 && d->part[k].start > t.start) {
				d->part[k + 1] = d->part[k];
				k--;
			}
			d->part[k + 1] = t;
		}

		read_gpt(d);
		ki_ndisk++;
	}
	closedir(dir);

	for (int i = 1; i < ki_ndisk; i++) {
		Disk t = ki_disk[i];
		int k = i - 1;
		while (k >= 0 && strcmp(ki_disk[k].name, t.name) > 0) {
			ki_disk[k + 1] = ki_disk[k];
			k--;
		}
		ki_disk[k + 1] = t;
	}

	apply_mounts();
}

/* ──────────────────────────────────────────────────────────────────────── */

static unsigned long long walk_kb, walk_appbox_kb;
static size_t appbox_len;
static const char appbox_path[] = "/home/kdos/.local/share/containers";

static int walk_cb(const char *path, const struct stat *st, int type,
		   struct FTW *ftw)
{
	(void)type;
	(void)ftw;
	unsigned long long kb = (unsigned long long)st->st_blocks / 2;
	walk_kb += kb;
	if (!strncmp(path, appbox_path, appbox_len))
		walk_appbox_kb += kb;
	return 0;
}

/* One pass over the live tree, once, before the screen is taken over. It is
 * the only honest input to "will this fit" — the squashfs knows its
 * compressed size and nothing on the system knows the expanded one.
 *
 * FTW_MOUNT does all the excluding that matters: /proc /sys /dev /run /tmp
 * are each their own filesystem, so the walk never enters them and there is
 * no path blacklist to keep in sync with fstab. */
static void measure_payload(void)
{
	walk_kb = walk_appbox_kb = 0;
	appbox_len = sizeof(appbox_path) - 1;
	nftw("/", walk_cb, 24, FTW_PHYS | FTW_MOUNT);
	ki_sys.payload_kb = walk_kb;
	ki_sys.appbox_kb = walk_appbox_kb;
}

void probe_system(void)
{
	memset(&ki_sys, 0, sizeof(ki_sys));

	ki_sys.uefi = kb_path_exists("/sys/firmware/efi");

	DIR *d = opendir("/sys/firmware/efi/efivars");
	if (d) {
		struct dirent *e;
		while ((e = readdir(d))) {
			if (strncmp(e->d_name, "SecureBoot-", 11))
				continue;
			char p[320], b[16];
			snprintf(p, sizeof(p), "/sys/firmware/efi/efivars/%s",
				 e->d_name);
			int fd = open(p, O_RDONLY | O_CLOEXEC);
			if (fd >= 0) {
				ssize_t n = read(fd, b, sizeof(b));
				if (n >= 5)
					ki_sys.secure_boot = b[4];
				close(fd);
			}
			break;
		}
		closedir(d);
	}

	char buf[8192];
	if (kb_read_file("/proc/meminfo", buf, sizeof(buf)) > 0) {
		char *p = strstr(buf, "MemTotal:");
		if (p)
			ki_sys.mem_kb = strtoull(p + 9, NULL, 10);
	}
	if (kb_read_file("/proc/cpuinfo", buf, sizeof(buf)) > 0) {
		char *p = strstr(buf, "model name");
		if (p && (p = strchr(p, ':'))) {
			p++;
			while (*p == ' ')
				p++;
			char *nl = strchr(p, '\n');
			if (nl)
				*nl = 0;
			kb_strlcpy(ki_sys.cpu, p, sizeof(ki_sys.cpu));
		}
	}
	if (!ki_sys.cpu[0])
		kb_strlcpy(ki_sys.cpu, "unknown", sizeof(ki_sys.cpu));

	/*
	 * The count comes from /sys, not from counting "processor" in
	 * /proc/cpuinfo — which reported 1 on every machine and was found by
	 * looking at `--dump probe`. Two reasons it could not work: the model
	 * name parse above NUL-terminates the buffer at the end of the first
	 * block, so there is nothing left to count, and cpuinfo on a 16-thread
	 * part is 26 KB against an 8 KB buffer anyway. `present` is one short
	 * line in the documented "0-3,8-11" form.
	 */
	if (kb_read_file("/sys/devices/system/cpu/present", buf,
			 sizeof(buf)) > 0) {
		for (const char *q = buf; *q;) {
			char *end = NULL;
			long lo = strtol(q, &end, 10), hi = lo;
			if (end == q)
				break;
			q = end;
			if (*q == '-') {
				hi = strtol(q + 1, &end, 10);
				q = end;
			}
			if (hi >= lo)
				ki_sys.cores += (int)(hi - lo + 1);
			while (*q == ',')
				q++;
			if (*q == '\n' || *q == '\0')
				break;
		}
	}
	if (ki_sys.cores < 1)
		ki_sys.cores = 1;

	/* A machine with many mounts overruns a fixed buffer, and a truncated
	 * /proc/mounts is how "am I live?" gets answered wrong. */
	size_t mlen = 0;
	char *mounts = kb_read_all("/proc/mounts", &mlen);
	if (mounts) {
		char *save = NULL;
		for (char *line = strtok_r(mounts, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char dev[128], mnt[192], type[32];
			if (sscanf(line, "%127s %191s %31s", dev, mnt, type) != 3)
				continue;
			if (!strcmp(mnt, "/") && !strcmp(type, "overlay"))
				ki_sys.live = 1;
		}
		free(mounts);
	}

	measure_payload();
}

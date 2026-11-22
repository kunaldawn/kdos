/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — what the machine actually is
 *
 * Everything here is read straight from /sys, /proc and the block device,
 * and from udev's database for a device this process may not open. lsblk and
 * blkid exist on KDOS, but parsing their output means the installer inherits
 * their formatting and their exit codes; a superblock is a stable ABI and a
 * 200-line reader is cheaper than either.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "kinstall-core.h"
/* The catalogue reader, compiled into this program rather than shelled out to.
 * It uses kb_* alone, so it costs none of the libraries kinstall deliberately
 * does not link — and a live installer cannot assume anything is on $PATH in
 * the target it is building. */
#include "kdos-appbox.h"

Disk ki_disk[MAX_DISKS];
int ki_ndisk;
SysInfo ki_sys;

static const unsigned char GUID_ESP[16] = {
	0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
	0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
};

/* 21686148-6449-6E6F-744E-656564454649 in GPT's mixed-endian byte order,
 * which spells "Hah!IdontNeedEFI": the BIOS boot partition. */
static const unsigned char GUID_BIOS_BOOT[16] = {
	0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6f, 0x6e,
	0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49
};

/* By its /dev node, or by any link to one: an answer file may name the disk
 * as /dev/disk/by-id/…, and every size and layout decision reads the disk
 * this returns. */
Disk *disk_by_path(const char *path)
{
	char real[PATH_MAX];

	for (int i = 0; i < ki_ndisk; i++)
		if (!strcmp(ki_disk[i].path, path))
			return &ki_disk[i];
	if (!path[0] || !realpath(path, real))
		return NULL;
	for (int i = 0; i < ki_ndisk; i++)
		if (!strcmp(ki_disk[i].path, real))
			return &ki_disk[i];
	return NULL;
}

unsigned long long ki_disk_bytes(const Disk *d)
{
	return d->sectors * 512ULL;
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

/*
 * WHAT UDEV RECORDED ABOUT A DEVICE, for a probe that may not open it.
 * /run/udev/data/b<major>:<minor> is world-readable and carries the
 * properties udev's own blkid run found, one `E:KEY=value` line each; the
 * graphical installer's dumps run as the session's user, who is not in
 * `disk`, and without this every partition would read as empty. The device
 * node is stat()ed, never opened, so its mode does not matter.
 */
static int udev_prop(const char *dev, const char *key, char *out, size_t cap)
{
	struct stat st;
	char path[64], line[512];
	size_t kl = strlen(key);
	FILE *f;
	int found = 0;

	out[0] = 0;
	if (stat(dev, &st) < 0 || !S_ISBLK(st.st_mode))
		return 0;
	snprintf(path, sizeof(path), "/run/udev/data/b%u:%u",
		 major(st.st_rdev), minor(st.st_rdev));
	f = fopen(path, "re");
	if (!f)
		return 0;
	while (!found && fgets(line, sizeof(line), f)) {
		if (strncmp(line, "E:", 2) || strncmp(line + 2, key, kl) ||
		    line[2 + kl] != '=')
			continue;
		line[strcspn(line, "\n")] = 0;
		kb_strlcpy(out, line + 3 + kl, cap);
		found = 1;
	}
	fclose(f);
	return found;
}

/* The superblock facts out of udev's database, for a device the probe
 * cannot read: the type, label and uuid, and the partition type that marks
 * an ESP (GPT's type GUID, or MBR's 0xef as udev writes it) or a BIOS boot
 * partition. */
static void sniff_udev(const char *path, Part *p)
{
	char type[64];

	udev_prop(path, "ID_FS_TYPE", p->fstype, sizeof(p->fstype));
	udev_prop(path, "ID_FS_LABEL", p->label, sizeof(p->label));
	udev_prop(path, "ID_FS_UUID", p->uuid, sizeof(p->uuid));
	if (udev_prop(path, "ID_PART_ENTRY_TYPE", type, sizeof(type))) {
		if (!strcasecmp(type, "c12a7328-f81f-11d2-ba4b-00a0c93ec93b") ||
		    !strcasecmp(type, "0xef"))
			p->is_esp = 1;
		if (!strcasecmp(type, KI_GUID_BIOS_BOOT))
			p->is_bios_boot = 1;
	}
}

/* A small blkid: enough of every superblock KDOS can create or meet. */
static void sniff_fs(const char *path, Part *p)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		sniff_udev(path, p);
		return;
	}

	unsigned char sb[4096];
	unsigned char boot[512];

	if (pread(fd, boot, sizeof(boot), 0) == (ssize_t)sizeof(boot)) {
		if (!memcmp(boot, "LUKS\xba\xbe", 6)) {
			/* The container's uuid is ASCII in the header, the
			 * same text `cryptsetup luksUUID` prints. */
			kb_strlcpy(p->fstype, "crypto_LUKS", sizeof(p->fstype));
			memcpy(p->uuid, boot + 168, sizeof(p->uuid) - 1);
			p->uuid[sizeof(p->uuid) - 1] = 0;
			trim_label(p->uuid);
		} else if (!memcmp(boot + 3, "NTFS    ", 8)) {
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

	/* f2fs: the superblock is at 1 KiB, magic 0xF2F52010 little-endian at
	 * +0, uuid at +0x6c and a UTF-16LE volume name at +0x7c. Without it
	 * an f2fs root reads back with no UUID, and do_config cannot write
	 * fstab or the boot entry's root=. */
	if (!p->fstype[0] &&
	    pread(fd, sb, 1024, 1024) == 1024 &&
	    sb[0] == 0x10 && sb[1] == 0x20 && sb[2] == 0xf5 && sb[3] == 0xf2) {
		kb_strlcpy(p->fstype, "f2fs", sizeof(p->fstype));
		uuid_fmt(p->uuid, sb + 0x6c);
		size_t n = 0;
		for (int k = 0; k < 64 && n < sizeof(p->label) - 1; k++) {
			unsigned c = (unsigned)sb[0x7c + 2 * k] |
				     ((unsigned)sb[0x7c + 2 * k + 1] << 8);
			if (!c)
				break;
			p->label[n++] = c < 0x80 ? (char)c : '?';
		}
		p->label[n] = 0;
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

	/* An LVM physical volume: the label is in one of the first four
	 * sectors, and pvcreate puts it in the second. Named as blkid names
	 * it, so the Layout page and `blkid -t TYPE=LVM2_member` — what the
	 * initramfs asks — agree about which device is one. */
	if (!p->fstype[0] && pread(fd, sb, 2048, 0) == 2048)
		for (int s = 0; s < 4; s++)
			if (!memcmp(sb + s * 512, "LABELONE", 8) &&
			    !memcmp(sb + s * 512 + 24, "LVM2 001", 8)) {
				kb_strlcpy(p->fstype, "LVM2_member",
					   sizeof(p->fstype));
				break;
			}

	/* ISO 9660: the primary volume descriptor at 32 KiB, type 1 and
	 * "CD001", with its volume id at +40 in 32 space-padded bytes. Tried
	 * last, because a partition reformatted over an old image may keep
	 * the descriptor; on the whole device of a written stick nothing
	 * before it matches, and the stick reads as the image it holds. */
	if (!p->fstype[0] && pread(fd, sb, 2048, 0x8000) == 2048 &&
	    !memcmp(sb, "\x01" "CD001", 6)) {
		kb_strlcpy(p->fstype, "iso9660", sizeof(p->fstype));
		memcpy(p->label, sb + 40, 32);
		p->label[32] = 0;
		trim_label(p->label);
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

/* The partition table: GPT's header, then the entry that matches each
 * partition's start, so an ESP is identified by its type GUID rather than by
 * "it looks like FAT"; on an MBR disk the primary entries typed 0xef, which
 * is how a hybrid ISO written to a stick marks its ESP. */
void ki_disk_table(Disk *d)
{
	/* Set before the open, not after: a disk we could not open at all
	 * (running unprivileged, or a device that vanished mid-probe) has an
	 * UNKNOWN table, and an empty string renders as a blank column that
	 * reads like "no partition table". */
	kb_strlcpy(d->table, "-", sizeof(d->table));

	int fd = open(d->path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		/* The partitions' types came from udev in sniff_udev(); the
		 * table's own type is the disk's ID_PART_TABLE_TYPE. */
		char t[16];

		if (udev_prop(d->path, "ID_PART_TABLE_TYPE", t, sizeof(t)) &&
		    (!strcmp(t, "gpt") || !strcmp(t, "dos")))
			kb_strlcpy(d->table, t, sizeof(d->table));
		return;
	}

	unsigned char hdr[512], mbr[512];

	if (pread(fd, mbr, 512, 0) == 512 && mbr[510] == 0x55 && mbr[511] == 0xaa) {
		/* An MBR entry counts logical sectors; sysfs counts 512 bytes. */
		unsigned long long unit = d->sector_size > 512
					  ? (unsigned long long)d->sector_size / 512 : 1;

		kb_strlcpy(d->table, "dos", sizeof(d->table));
		for (int e = 0; e < 4; e++) {
			const unsigned char *m = mbr + 446 + 16 * e;
			unsigned long long lba = (unsigned long long)m[8] |
						 ((unsigned long long)m[9] << 8) |
						 ((unsigned long long)m[10] << 16) |
						 ((unsigned long long)m[11] << 24);

			if (m[4] != 0xef)
				continue;
			for (int k = 0; k < d->nparts; k++)
				if (d->part[k].start == lba * unit)
					d->part[k].is_esp = 1;
		}
	}

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
			if (!memcmp(ent, GUID_BIOS_BOOT, 16))
				d->part[k].is_bios_boot = 1;
		}
	}
	free(ent);
	close(fd);
}

/* ──────────────────────────────────────────────────────────────────────── */

/* The kernel name (dm-N) of the device-mapper block device whose node is
 * dev, read through /sys/dev/block/MAJ:MIN; 0 when dev is not a block node
 * or not a dm device. */
static int dm_kernel_name(const char *dev, char *out, size_t outsz)
{
	struct stat st;
	char path[64], real[PATH_MAX];

	if (strncmp(dev, "/dev/", 5) || stat(dev, &st) || !S_ISBLK(st.st_mode))
		return 0;
	snprintf(path, sizeof(path), "/sys/dev/block/%u:%u",
		 major(st.st_rdev), minor(st.st_rdev));
	if (!realpath(path, real) || strncmp(kb_basename(real), "dm-", 3))
		return 0;
	kb_strlcpy(out, kb_basename(real), outsz);
	return 1;
}

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

		/* A MEDIUM ON A dm DEVICE IS TRACED TO ITS DISK. A Ventoy stick's
		 * medium is mounted as /dev/mapper/ventoy, a mapping over the ISO
		 * file on the stick's data partition, and names no disk or
		 * partition in /proc/mounts; without the walk down its slaves the
		 * stick is offered as an install target. The kernel name comes
		 * from the node's device number, not its path: /dev/mapper/ventoy
		 * may be a node of its own rather than a link to dm-N. */
		char kname[64];
		if (boot_media && dm_kernel_name(dev, kname, sizeof(kname)))
			for (int i = 0; i < ki_ndisk; i++)
				if (ki_dev_on_disk(kname, ki_disk[i].name))
					ki_disk[i].is_boot_media = 1;

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

/*
 * EVERY TRANSPORT CLASSIFIED HERE IS ONE THE INSTALLER WILL INSTALL TO, so
 * its driver must be in 090_initramfs.sh's MODULES, the way every row of
 * ki_filesystems[] must be. A disk the initramfs has no driver for installs
 * fine and its root is never found on the first boot.
 */
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

/* A name for a disk whose driver publishes no model — virtio-blk is one —
 * from its transport, and its serial where the driver has one, so two such
 * disks are told apart by more than their size. */
static void disk_fallback_name(Disk *d)
{
	static const struct {
		const char *tran, *name;
	} names[] = {
		{ "virtio", "Virtual disk" },
		{ "nvme", "NVMe disk" },
		{ "mmc", "Memory card" },
		{ "usb", "USB disk" },
	};
	const char *name = "Disk";
	char serial[32];

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (!strcmp(d->tran, names[i].tran))
			name = names[i].name;
	sysfs_str(serial, sizeof(serial), "/sys/block/%s/serial", d->name);
	if (serial[0])
		snprintf(d->model, sizeof(d->model), "%s %s", name, serial);
	else
		kb_strlcpy(d->model, name, sizeof(d->model));
}

/* ──────────────────────────────────────────────────────────────────────── */

Lv ki_lv[MAX_LVS];
int ki_nlv;

static int has_holders(const char *name)
{
	char path[256];
	char **ents;
	int n = 0;

	snprintf(path, sizeof(path), "/sys/class/block/%.64s/holders", name);
	ents = kb_listdir(path, NULL);
	for (char **e = ents; e && *e; e++)
		n++;
	kb_strv_free(ents);
	return n > 0;
}

/*
 * THE STACK IS WALKED DOWN THROUGH `slaves`, which is what every dm device
 * and array lists: an LV's are the PVs, a container's is the partition it
 * was opened on. A partition's disk is its parent directory in sysfs. The
 * depth bound is for a loop no kernel builds and a fixture could.
 */
static int on_disk(const char *dev, const char *disk, int depth)
{
	char path[256], real[PATH_MAX];
	char **ents;
	int hit = 0;

	if (!strcmp(dev, disk))
		return 1;
	if (depth > 8)
		return 0;
	snprintf(path, sizeof(path), "/sys/class/block/%.64s/partition", dev);
	if (kb_path_exists(path)) {
		snprintf(path, sizeof(path), "/sys/class/block/%.64s", dev);
		if (!realpath(path, real))
			return 0;
		char *slash = strrchr(real, '/');
		if (!slash)
			return 0;
		*slash = 0;
		return !strcmp(kb_basename(real), disk);
	}
	snprintf(path, sizeof(path), "/sys/class/block/%.64s/slaves", dev);
	ents = kb_listdir(path, NULL);
	for (char **e = ents; e && *e && !hit; e++)
		hit = on_disk(*e, disk, depth + 1);
	kb_strv_free(ents);
	return hit;
}

int ki_dev_on_disk(const char *dev, const char *disk)
{
	return on_disk(dev, disk, 0);
}

/*
 * AN ACTIVE VOLUME OF THE GROUP STANDING ON ANOTHER DISK, read from sysfs:
 * every LVM volume is a dm device whose uuid starts `LVM-`, whose name is
 * `vg-lv` and whose `slaves` are the physical volumes it lies on. sysfs needs
 * no privilege, so this is what an unprivileged front end's plan sees, where
 * `pvs` cannot read the labels.
 */
static int vg_active_off_disk(const char *vg, const char *disk)
{
	char **ents = kb_listdir("/sys/block", NULL);
	int hit = 0;

	for (char **e = ents; e && *e && !hit; e++) {
		char p[320], uuid[160] = "", name[160] = "", g[128], lv[128];
		char **sl;

		if (strncmp(*e, "dm-", 3))
			continue;
		snprintf(p, sizeof(p), "/sys/block/%.64s/dm/uuid", *e);
		kb_read_line_file(p, uuid, sizeof(uuid));
		snprintf(p, sizeof(p), "/sys/block/%.64s/dm/name", *e);
		kb_read_line_file(p, name, sizeof(name));
		if (strncmp(uuid, "LVM-", 4) ||
		    ki_dm_split(name, g, sizeof(g), lv, sizeof(lv)) ||
		    strcmp(g, vg))
			continue;
		snprintf(p, sizeof(p), "/sys/block/%.64s/slaves", *e);
		sl = kb_listdir(p, NULL);
		for (char **s = sl; s && *s && !hit; s++)
			hit = !ki_dev_on_disk(*s, disk);
		kb_strv_free(sl);
	}
	kb_strv_free(ents);
	return hit;
}

/*
 * THE GROUP BY NAME, FROM LVM'S OWN LIST OF PHYSICAL VOLUMES. The active
 * volumes in ki_lv miss a group with no volume, one whose volumes did not
 * activate, and the other half of a group that spans the target disk and
 * another. `pvs` reads every label, so each of those still shows as a PV of
 * the group on a device off the disk. `pvs` reads labels only as root, so
 * the group's active volumes are asked of sysfs as well, and a plan made
 * unprivileged refuses the clash the install would. With no lvm at all
 * there is no vgcreate for the name to stop.
 */
int ki_vg_off_disk(const char *vg, const char *disk)
{
	KbArgv a = {0};
	KbBuf out = {0};
	char *save = NULL, *line;
	int hit = 0;

	if (!kb_have_prog("lvm"))
		return 0;
	if (vg_active_off_disk(vg, disk))
		return 1;
	kb_argv_add(&a, "lvm");
	kb_argv_add(&a, "pvs");
	kb_argv_add(&a, "--noheadings");
	kb_argv_add(&a, "--separator");
	kb_argv_add(&a, "|");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "pv_name,vg_name");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out) != 0 || !out.p) {
		kb_buf_free(&out);
		return 0;
	}
	for (line = strtok_r(out.p, "\n", &save); line && !hit;
	     line = strtok_r(NULL, "\n", &save)) {
		char *bar = strchr(line, '|'), *name, *g, *end;
		char real[PATH_MAX];

		if (!bar)
			continue;
		*bar = 0;
		name = line;
		while (isspace((unsigned char)*name))
			name++;
		g = bar + 1;
		end = g + strlen(g);
		while (end > g && isspace((unsigned char)end[-1]))
			*--end = 0;
		if (strcmp(g, vg))
			continue;
		/* /dev/mapper/<name> and /dev/<vg>/<lv> are links; the kernel
		 * name sysfs knows is the target's. */
		if (!realpath(name, real))
			kb_strlcpy(real, name, sizeof(real));
		hit = !ki_dev_on_disk(kb_basename(real), disk);
	}
	kb_buf_free(&out);
	return hit;
}

/*
 * A PHYSICAL VOLUME'S GROUP, for a whole disk or a partition named `name`
 * (sda, sda3) at `path`. An active volume on it lists the device among its
 * `slaves`, which sysfs shows anybody; `lvm pvs` names the group of an
 * inactive one, and reads labels only as root.
 */
static void pv_vg(const char *name, const char *path, char *out, size_t cap)
{
	char **ents = kb_listdir("/sys/block", NULL);
	KbArgv a = {0};
	KbBuf out_b = {0};

	out[0] = 0;
	for (char **e = ents; e && *e && !out[0]; e++) {
		char p[320], uuid[160] = "", dname[160] = "", g[64], lv[128];

		if (strncmp(*e, "dm-", 3))
			continue;
		snprintf(p, sizeof(p), "/sys/block/%.64s/slaves/%.31s", *e,
			 name);
		if (!kb_path_exists(p))
			continue;
		snprintf(p, sizeof(p), "/sys/block/%.64s/dm/uuid", *e);
		kb_read_line_file(p, uuid, sizeof(uuid));
		snprintf(p, sizeof(p), "/sys/block/%.64s/dm/name", *e);
		kb_read_line_file(p, dname, sizeof(dname));
		if (!strncmp(uuid, "LVM-", 4) &&
		    !ki_dm_split(dname, g, sizeof(g), lv, sizeof(lv)))
			kb_strlcpy(out, g, cap);
	}
	kb_strv_free(ents);
	if (out[0] || geteuid() != 0 || !kb_have_prog("lvm"))
		return;
	kb_argv_add(&a, "lvm");
	kb_argv_add(&a, "pvs");
	kb_argv_add(&a, "--noheadings");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "vg_name");
	kb_argv_add(&a, path);
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &out_b) == 0 && out_b.p) {
		char *v = out_b.p, *end;

		while (isspace((unsigned char)*v))
			v++;
		end = v + strcspn(v, " \t\r\n");
		*end = 0;
		kb_strlcpy(out, v, cap);
	}
	kb_buf_free(&out_b);
}

/* An ISO 9660 image's own size: the primary volume descriptor's block
 * count (+80) times its block size (+128), both little-endian; 0 when the
 * device cannot be read. */
static unsigned long long iso_bytes(const char *path)
{
	unsigned char pvd[2048];
	unsigned long long blocks, bsize;
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t n;

	if (fd < 0)
		return 0;
	n = pread(fd, pvd, sizeof(pvd), 0x8000);
	close(fd);
	if (n != (ssize_t)sizeof(pvd) || memcmp(pvd, "\x01" "CD001", 6))
		return 0;
	blocks = (unsigned long long)pvd[80] | ((unsigned long long)pvd[81] << 8) |
		 ((unsigned long long)pvd[82] << 16) |
		 ((unsigned long long)pvd[83] << 24);
	bsize = (unsigned long long)pvd[128] | ((unsigned long long)pvd[129] << 8);
	return blocks * bsize;
}

/* Whether every partition of `d` is a hybrid window of the ISO 9660
 * image on its whole device. A window holds FAT (the image's ESP), ISO
 * 9660 or nothing of its own. With the descriptor read (`sized`), each
 * ends inside the image's d->fs_bytes; unread, as to the session's user,
 * some partition starts inside the first MiB, where xorriso and isohybrid
 * put the first window and where parted, fdisk and Windows never start
 * one. A disk repartitioned after it was written fails both, as the new
 * table leaves the descriptor at 32 KiB in place, and its partitions are
 * what an erase destroys. */
static int iso_windows(const Disk *d, int sized)
{
	int early = !d->nparts;

	for (int k = 0; k < d->nparts; k++) {
		const Part *p = &d->part[k];

		if (p->fstype[0] && strcmp(p->fstype, "vfat") &&
		    strcmp(p->fstype, "iso9660"))
			return 0;
		if (sized && (p->start + p->sectors) * 512 > d->fs_bytes)
			return 0;
		if (p->start < 2048)
			early = 1;
	}
	return sized || early;
}

void ki_disk_signature(Disk *d)
{
	Part p;

	memset(&p, 0, sizeof(p));
	d->fstype[0] = d->label[0] = d->vg[0] = 0;
	d->fs_bytes = 0;
	d->iso_image = 0;
	sniff_fs(d->path, &p);
	kb_strlcpy(d->fstype, p.fstype, sizeof(d->fstype));
	kb_strlcpy(d->label, p.label, sizeof(d->label));
	if (!strcmp(d->fstype, "LVM2_member"))
		pv_vg(d->name, d->path, d->vg, sizeof(d->vg));
	if (strcmp(d->fstype, "iso9660"))
		return;
	/* Unreadable, as to the session's user, the image ends where its
	 * last partition does: a hybrid image's partitions all lie inside
	 * it, and xorriso's last one, KDOS's among them, reaches its end. */
	d->fs_bytes = iso_bytes(d->path);
	d->iso_image = iso_windows(d, d->fs_bytes != 0);
	for (int k = 0; !d->fs_bytes && k < d->nparts; k++) {
		unsigned long long end = d->part[k].start + d->part[k].sectors;
		int last = 1;

		for (int j = 0; j < d->nparts; j++)
			if (d->part[j].start + d->part[j].sectors > end)
				last = 0;
		if (last)
			d->fs_bytes = end * 512;
	}
}

void ki_disk_contents(const Disk *d, int max, char *out, size_t cap)
{
	/* The room an entry leaves for the tail naming those that do not
	 * fit: ", and 2147483647 more partitions" and its NUL. */
	const size_t tail = 40;
	size_t at = 0;
	int i;

	out[0] = 0;
	/* A written stick's hybrid partitions are windows into the image on
	 * the whole device, and hold nothing of their own to name. */
	if (d->iso_image) {
		snprintf(out, cap, "ISO 9660 image%s%s%s on the whole disk%s%s",
			 d->label[0] ? " \u201c" : "", d->label,
			 d->label[0] ? "\u201d" : "", d->fs_bytes ? ", " : "",
			 d->fs_bytes ? kb_human_size(d->fs_bytes) : "");
		return;
	}
	if (!d->nparts) {
		if (!strcmp(d->fstype, "LVM2_member") && d->vg[0])
			snprintf(out, cap, "LVM physical volume (volume group %s)",
				 d->vg);
		else if (!strcmp(d->fstype, "LVM2_member"))
			snprintf(out, cap, "LVM physical volume");
		else if (d->fstype[0])
			snprintf(out, cap, "%s on the whole disk", d->fstype);
		return;
	}
	for (i = 0; i < d->nparts && (max <= 0 || i < max); i++) {
		const Part *p = &d->part[i];
		char kind[128], one[256];
		int n;

		if (p->is_esp)
			kb_strlcpy(kind, "EFI system", sizeof(kind));
		else if (p->is_bios_boot)
			kb_strlcpy(kind, "BIOS boot", sizeof(kind));
		else if (!strcmp(p->fstype, "LVM2_member") && p->vg[0])
			snprintf(kind, sizeof(kind),
				 "LVM physical volume (volume group %s)", p->vg);
		else if (!strcmp(p->fstype, "LVM2_member"))
			kb_strlcpy(kind, "LVM physical volume", sizeof(kind));
		else
			kb_strlcpy(kind, p->fstype[0] ? p->fstype : "empty",
				   sizeof(kind));
		n = snprintf(one, sizeof(one), "%s%s%s%s%s %s",
			     i ? ", " : "", kind,
			     p->label[0] ? " \u201c" : "", p->label,
			     p->label[0] ? "\u201d" : "",
			     kb_human_size(p->sectors * 512));
		/* An entry goes in whole or not at all, so the list never ends
		 * inside a label or a UTF-8 sequence; the last one may use the
		 * tail's room, as nothing follows it. */
		if (n < 0 || (size_t)n >= sizeof(one) ||
		    at + (size_t)n + (i + 1 < d->nparts ? tail : 0) >= cap)
			break;
		memcpy(out + at, one, (size_t)n + 1);
		at += (size_t)n;
	}
	if (i < d->nparts && at + tail <= cap)
		snprintf(out + at, cap - at, "%s%d more partition%s",
			 i ? ", and " : "", d->nparts - i,
			 d->nparts - i == 1 ? "" : "s");
}

int ki_udev_model(const char *data, char *model, size_t cap)
{
	char line[512], dec[256];
	size_t n = 0, ml = strlen(model);
	const char *v = NULL, *s;
	FILE *f = fopen(data, "re");

	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "E:ID_MODEL_ENC=", 15)) {
			line[strcspn(line, "\n")] = 0;
			v = line + 15;
			break;
		}
	}
	fclose(f);
	if (!v)
		return 0;
	for (s = v; *s && n + 1 < sizeof(dec); s++) {
		unsigned int c;

		if (s[0] == '\\' && s[1] == 'x' && isxdigit((unsigned char)s[2]) &&
		    isxdigit((unsigned char)s[3]) &&
		    sscanf(s + 2, "%2x", &c) == 1) {
			dec[n++] = (char)c;
			s += 3;
		} else {
			dec[n++] = *s;
		}
	}
	dec[n] = 0;
	while (n && isspace((unsigned char)dec[n - 1]))
		dec[--n] = 0;
	s = dec;
	while (isspace((unsigned char)*s))
		s++;
	if (strlen(s) <= ml || strncmp(s, model, ml) || strlen(s) >= cap)
		return 0;
	kb_strlcpy(model, s, cap);
	return 1;
}

/*
 * `vg-lv`, with every `-` inside either name doubled. The first single `-`
 * is the split; a name that has none is not an LVM volume's.
 */
int ki_dm_split(const char *name, char *vg, size_t vcap, char *lv,
		size_t lcap)
{
	size_t o = 0;
	const char *p = name;

	while (*p) {
		if (p[0] == '-' && p[1] == '-') {
			if (o + 1 >= vcap)
				return -1;
			vg[o++] = '-';
			p += 2;
			continue;
		}
		if (*p == '-')
			break;
		if (o + 1 >= vcap)
			return -1;
		vg[o++] = *p++;
	}
	vg[o] = 0;
	if (*p != '-' || !o)
		return -1;
	p++;
	o = 0;
	while (*p) {
		char c;

		if (p[0] == '-' && p[1] == '-') {
			c = '-';
			p += 2;
		} else if (*p == '-') {
			/* A single dash after the split: a layer device
			 * (`-tpool`, `-real`, `-cow`), never a volume. */
			return -1;
		} else {
			c = *p++;
		}
		if (o + 1 >= lcap)
			return -1;
		lv[o++] = c;
	}
	lv[o] = 0;
	return o ? 0 : -1;
}

/* LVM's own sub-volumes, by the suffixes it reserves for them. */
static int lv_internal(const char *lv)
{
	static const char *const sub[] = {
		"_tdata", "_tmeta", "_cdata", "_cmeta", "_corig", "_cpool",
		"_cvol", "_wcorig", "_rimage_", "_rmeta_", "_mimage_", "_mlog",
		"_vorigin", "_vdata", "_pmspare", "_imeta", "_iorig",
	};
	for (size_t i = 0; i < sizeof(sub) / sizeof(sub[0]); i++)
		if (strstr(lv, sub[i]))
			return 1;
	return 0;
}

/*
 * EVERY GROUP ACTIVE BEFORE ANYTHING IS LISTED. lvm2 is built with no hotplug
 * activation, and the initramfs of a live boot activates nothing, so without
 * this a disk carrying LVM shows a physical volume and no volume on it. Once
 * per process: a group does not appear between two pages, and a second
 * vgchange on every visit to the Disk page is a pause for nothing. Only as
 * root, because activation is a device-mapper ioctl, and `--dump probe` run by
 * a user must not fail on it. `udevadm settle` after, or /dev/<vg>/<lv> is not
 * there yet when the list is read.
 */
static void lvm_activate(void)
{
	static int done;
	KbArgv a = {0}, s = {0};

	if (done || geteuid() != 0 || !kb_have_prog("lvm"))
		return;
	done = 1;
	kb_argv_add(&a, "lvm");
	kb_argv_add(&a, "vgchange");
	kb_argv_add(&a, "-aay");
	kb_argv_end(&a);
	kb_run(&a);
	kb_argv_add(&s, "udevadm");
	kb_argv_add(&s, "settle");
	kb_argv_end(&s);
	kb_run(&s);
}

static void lv_mounts(void)
{
	size_t len = 0;
	char *buf = kb_read_all("/proc/mounts", &len);
	char *save = NULL;

	if (!buf)
		return;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[160], mnt[192];
		struct stat st;

		if (sscanf(line, "%159s %191s", dev, mnt) != 2 ||
		    strncmp(dev, "/dev/", 5) || stat(dev, &st) != 0 ||
		    !S_ISBLK(st.st_mode))
			continue;
		for (int i = 0; i < ki_nlv; i++) {
			char sys[64], num[32];
			unsigned ma, mi;

			snprintf(sys, sizeof(sys), "/sys/block/%s/dev",
				 ki_lv[i].dm);
			if (kb_read_line_file(sys, num, sizeof(num)) < 0 ||
			    sscanf(num, "%u:%u", &ma, &mi) != 2)
				continue;
			if (major(st.st_rdev) != ma || minor(st.st_rdev) != mi)
				continue;
			ki_lv[i].mounted = 1;
			kb_strlcpy(ki_lv[i].mountpoint, mnt,
				   sizeof(ki_lv[i].mountpoint));
		}
	}
	free(buf);
}

static void probe_lvs(void)
{
	DIR *dir;
	struct dirent *e;

	ki_nlv = 0;
	lvm_activate();
	dir = opendir("/sys/block");
	if (!dir)
		return;
	while ((e = readdir(dir)) && ki_nlv < MAX_LVS) {
		char uuid[160], name[160];
		Lv *l = &ki_lv[ki_nlv];

		if (strncmp(e->d_name, "dm-", 3))
			continue;
		sysfs_str(uuid, sizeof(uuid), "/sys/block/%.32s/dm/uuid",
			  e->d_name);
		sysfs_str(name, sizeof(name), "/sys/block/%.32s/dm/name",
			  e->d_name);
		/* `LVM-` and two 32-character uuids, and nothing after: a
		 * suffix is a layer (`-tpool`, `-pool`, `-real`, `-cow`). */
		if (strncmp(uuid, "LVM-", 4) || strlen(uuid) != 4 + 64)
			continue;
		memset(l, 0, sizeof(*l));
		if (ki_dm_split(name, l->vg, sizeof(l->vg), l->lv,
				  sizeof(l->lv)) != 0 ||
		    lv_internal(l->lv))
			continue;
		kb_strlcpy(l->dm, e->d_name, sizeof(l->dm));
		l->sectors = sysfs_ull("/sys/block/%s/size", l->dm);
		if (!l->sectors)
			continue;
		/* /dev/<vg>/<lv> is udev's, and what an answer file should
		 * name; /dev/dm-N is the kernel's and always there. */
		{
			char pth[sizeof(l->path)];

			snprintf(pth, sizeof(pth), "/dev/%s/%s", l->vg, l->lv);
			if (!kb_path_exists(pth))
				snprintf(pth, sizeof(pth), "/dev/%s", l->dm);
			kb_strlcpy(l->path, pth, sizeof(l->path));
		}
		l->held = has_holders(l->dm);
		{
			Part p;

			memset(&p, 0, sizeof(p));
			sniff_fs(l->path, &p);
			kb_strlcpy(l->fstype, p.fstype, sizeof(l->fstype));
			kb_strlcpy(l->label, p.label, sizeof(l->label));
			kb_strlcpy(l->uuid, p.uuid, sizeof(l->uuid));
		}
		ki_nlv++;
	}
	closedir(dir);

	for (int i = 1; i < ki_nlv; i++) {
		Lv t = ki_lv[i];
		int k = i - 1;
		while (k >= 0 && strcmp(ki_lv[k].path, t.path) > 0) {
			ki_lv[k + 1] = ki_lv[k];
			k--;
		}
		ki_lv[k + 1] = t;
	}
	lv_mounts();
}

Lv *lv_by_path(const char *path)
{
	for (int i = 0; i < ki_nlv; i++)
		if (!strcmp(ki_lv[i].path, path))
			return &ki_lv[i];
	return NULL;
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
		{
			char mm[32] = "", data[64];

			sysfs_str(mm, sizeof(mm), "/sys/block/%s/dev", d->name);
			if (mm[0]) {
				snprintf(data, sizeof(data), "/run/udev/data/b%s",
					 mm);
				ki_udev_model(data, d->model, sizeof(d->model));
			}
		}
		disk_transport(d);
		if (!d->model[0])
			disk_fallback_name(d);

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
				p->num = (int)sysfs_ull("/sys/block/%s/%s/partition",
							d->name, pe->d_name);
				p->start = sysfs_ull("/sys/block/%s/%s/start",
						     d->name, pe->d_name);
				p->sectors = sysfs_ull("/sys/block/%s/%s/size",
						       d->name, pe->d_name);
				p->held = has_holders(pe->d_name);
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

		ki_disk_table(d);
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
	probe_lvs();
	/* After the activation, so a whole-disk volume's group is in sysfs. */
	for (int i = 0; i < ki_ndisk; i++) {
		Disk *d = &ki_disk[i];

		ki_disk_signature(d);
		for (int k = 0; k < d->nparts; k++)
			if (!strcmp(d->part[k].fstype, "LVM2_member"))
				pv_vg(d->part[k].name, d->part[k].path,
				      d->part[k].vg, sizeof(d->part[k].vg));
	}
}

/* ──────────────────────────────────────────────────────────────────────── */

static unsigned long long walk_kb, walk_appbox_kb;
static unsigned long long walk_link_bytes, walk_link_appbox_bytes;
static size_t appbox_len;

/* The inodes with more than one name seen so far, by (st_dev, st_ino), in an
 * open-addressed table kept at most half full. */
static struct walk_ino {
	dev_t dev;
	ino_t ino;
	int used;
} *walk_inos;
static size_t walk_ino_cap, walk_ino_n;

static size_t walk_ino_slot(struct walk_ino *t, size_t cap, dev_t dev,
			    ino_t ino)
{
	size_t h = ((size_t)ino * 0x9E3779B97F4A7C15ull) ^ (size_t)dev;

	for (h &= cap - 1; t[h].used && (t[h].dev != dev || t[h].ino != ino);
	     h = (h + 1) & (cap - 1))
		;
	return h;
}

/* Non-zero when this inode was seen before; records it otherwise. */
static int walk_ino_seen(dev_t dev, ino_t ino)
{
	size_t h;

	if (2 * (walk_ino_n + 1) > walk_ino_cap) {
		size_t cap = walk_ino_cap ? walk_ino_cap * 2 : 4096;
		struct walk_ino *t = kb_calloc(cap, sizeof(*t));

		for (size_t i = 0; i < walk_ino_cap; i++)
			if (walk_inos[i].used)
				t[walk_ino_slot(t, cap, walk_inos[i].dev,
						walk_inos[i].ino)] = walk_inos[i];
		free(walk_inos);
		walk_inos = t;
		walk_ino_cap = cap;
	}
	h = walk_ino_slot(walk_inos, walk_ino_cap, dev, ino);
	if (walk_inos[h].used)
		return 1;
	walk_inos[h] = (struct walk_ino){ dev, ino, 1 };
	walk_ino_n++;
	return 0;
}
static const char appbox_path[] = "/home/kdos/.local/share/containers";
static const char livehome_path[] = "/home/kdos/";
static const char batcache_path[] = "/home/kdos/.cache/bat";

/* The kind of each entry the copy carries, in rsync's order (KI_ENT_*). */
static unsigned char *walk_kind;
static size_t walk_nkind, walk_kind_cap;

static void walk_kind_add(unsigned char k)
{
	if (walk_nkind == walk_kind_cap) {
		walk_kind_cap = walk_kind_cap ? walk_kind_cap * 2 : 65536;
		walk_kind = kb_realloc(walk_kind, walk_kind_cap);
	}
	walk_kind[walk_nkind++] = k;
}

/*
 * WHAT THE COPY CARRIES, AND NOTHING ELSE. Of the live account's home the
 * install keeps the box store and the syntax cache the image built; the rest
 * is the live session's own (its desktop layout, its wallet, its indexes) and
 * is left on the medium, so it is not counted either.
 */
static void walk_entry(const char *path, const struct stat *st)
{
	unsigned long long kb = (unsigned long long)st->st_blocks / 2;
	int store = !strncmp(path, appbox_path, appbox_len);
	int later = 0;

	if (!store &&
	    !strncmp(path, livehome_path, sizeof(livehome_path) - 1) &&
	    strncmp(path, batcache_path, sizeof(batcache_path) - 1))
		return;
	walk_kb += kb;
	if (store)
		walk_appbox_kb += kb;
	/* Every name of a file after its first: rsync -H counts each in the
	 * bytes of its percentage and makes it a link at no copy cost. */
	if (S_ISREG(st->st_mode) && st->st_nlink > 1 &&
	    walk_ino_seen(st->st_dev, st->st_ino)) {
		later = 1;
		walk_link_bytes += (unsigned long long)st->st_size;
		if (store)
			walk_link_appbox_bytes += (unsigned long long)st->st_size;
	}
	walk_kind_add((unsigned char)((S_ISREG(st->st_mode) && !later
					       ? KI_ENT_FILE
					       : KI_ENT_OTHER) |
				      (store ? KI_ENT_STORE : 0)));
}

typedef struct {
	char *name;
	struct stat st;
} WalkEnt;

/* rsync's order inside one directory: every entry that is not a directory,
 * then the directories, each by its name's bytes. */
static int walk_ent_cmp(const void *a, const void *b)
{
	const WalkEnt *x = a, *y = b;
	int dx = S_ISDIR(x->st.st_mode), dy = S_ISDIR(y->st.st_mode);

	if (dx != dy)
		return dx - dy;
	return strcmp(x->name, y->name);
}

/* The directory open at `fd`, whose path is `path` (`len` bytes, in a
 * PATH_MAX buffer): its entries in rsync's order, each directory's own
 * entries straight after it. Another filesystem is never entered. */
static void walk_dir(int fd, char *path, size_t len, dev_t dev)
{
	DIR *d = fdopendir(fd);
	struct dirent *de;
	WalkEnt *e = NULL;
	size_t n = 0, cap = 0;

	if (!d) {
		close(fd);
		return;
	}
	while ((de = readdir(d))) {
		struct stat st;

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (fstatat(dirfd(d), de->d_name, &st, AT_SYMLINK_NOFOLLOW) ||
		    st.st_dev != dev)
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 64;
			e = kb_realloc(e, cap * sizeof(*e));
		}
		e[n].name = kb_strdup(de->d_name);
		e[n].st = st;
		n++;
	}
	qsort(e, n, sizeof(*e), walk_ent_cmp);
	for (size_t i = 0; i < n; i++) {
		size_t nl = strlen(e[i].name);
		size_t at = len > 1 ? len : 0;
		int sub;

		if (at + 1 + nl >= PATH_MAX)
			continue;
		path[at] = '/';
		memcpy(path + at + 1, e[i].name, nl + 1);
		walk_entry(path, &e[i].st);
		if (S_ISDIR(e[i].st.st_mode)) {
			sub = openat(dirfd(d), e[i].name,
				     O_RDONLY | O_DIRECTORY | O_NOFOLLOW |
					     O_CLOEXEC);
			if (sub >= 0)
				walk_dir(sub, path, at + 1 + nl, dev);
		}
		path[len] = 0;
	}
	for (size_t i = 0; i < n; i++)
		free(e[i].name);
	free(e);
	closedir(d);
}

/* One pass over the live tree, once, before the screen is taken over. It is
 * the only honest input to "will this fit" — the squashfs knows its
 * compressed size and nothing on the system knows the expanded one.
 *
 * Staying on the root's filesystem does all the excluding that matters:
 * /proc /sys /dev /run /tmp are each their own filesystem, so the walk never
 * enters them and there is no path blacklist to keep in sync with fstab.
 *
 * IN RSYNC'S ORDER, so the copy's profile (KiCopyProfile) lines up with its
 * `to-chk` count: in each directory the entries that are not directories by
 * name, then each directory by name followed by everything under it.
 *
 * SEPARATE FROM probe_system() because it is the one slow probe: seconds on
 * a live medium and minutes on a development machine. `--dump plan` asks
 * for it only when the answer depends on it. */
void probe_payload(void)
{
	char path[PATH_MAX] = "/";
	struct stat st;
	int fd;

	walk_kb = walk_appbox_kb = 0;
	walk_link_bytes = walk_link_appbox_bytes = 0;
	walk_nkind = 0;
	appbox_len = sizeof(appbox_path) - 1;
	fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd >= 0 && !fstat(fd, &st)) {
		walk_entry(path, &st);
		walk_dir(fd, path, 1, st.st_dev);
	} else if (fd >= 0) {
		close(fd);
	}
	free(walk_inos);
	walk_inos = NULL;
	walk_ino_cap = walk_ino_n = 0;
	ki_sys.payload_kb = walk_kb;
	ki_sys.appbox_kb = walk_appbox_kb;
	ki_sys.link_extra_bytes = walk_link_bytes;
	ki_sys.link_extra_appbox_bytes = walk_link_appbox_bytes;
	ki_copy_profile(&ki_sys.copy_prof, walk_kind, walk_nkind, 1);
	ki_copy_profile(&ki_sys.copy_prof_sys, walk_kind, walk_nkind, 0);
	free(walk_kind);
	walk_kind = NULL;
	walk_nkind = walk_kind_cap = 0;
}

/* Whether interface `ifc` carries traffic: operstate up (or unknown, which a
 * tun device and some drivers report while working) and carrier 1. A cable
 * pulled leaves the route and reads operstate down. */
static int if_up(const char *root, const char *ifc)
{
	char p[PATH_MAX], v[32];

	if (!*ifc || !strcmp(ifc, "lo") || strchr(ifc, '/'))
		return 0;
	snprintf(p, sizeof(p), "%s/sys/class/net/%s/operstate", root, ifc);
	if (kb_read_file(p, v, sizeof(v)) <= 0)
		return 0;
	v[strcspn(v, "\n")] = 0;
	if (strcmp(v, "up") && strcmp(v, "unknown"))
		return 0;
	snprintf(p, sizeof(p), "%s/sys/class/net/%s/carrier", root, ifc);
	return kb_read_file(p, v, sizeof(v)) > 0 && v[0] == '1';
}

/* A default route, IPv4 (/proc/net/route: destination 00000000, RTF_UP) or
 * IPv6 (/proc/net/ipv6_route: ::/0, RTF_UP and not RTF_REJECT, the kernel's
 * own unreachable entry on lo), on an interface that is up. */
static int default_route(const char *root, int v6)
{
	char p[PATH_MAX];
	char *data, *l;
	int found = 0;

	snprintf(p, sizeof(p), "%s/proc/net/%s", root,
		 v6 ? "ipv6_route" : "route");
	data = kb_read_all(p, NULL);
	for (l = data; l && *l && !found;) {
		char *nl = strchr(l, '\n');
		char ifc[32] = "", dst[40] = "", plen[8] = "";
		unsigned long flags = 0;

		if (nl)
			*nl = 0;
		if (!v6) {
			if (sscanf(l, "%31s %39s %*s %lx", ifc, dst, &flags) == 3 &&
			    !strcmp(dst, "00000000") && (flags & 0x1) &&
			    if_up(root, ifc))
				found = 1;
		} else if (sscanf(l, "%39s %7s %*s %*s %*s %*s %*s %*s %lx %31s",
				  dst, plen, &flags, ifc) == 4 &&
			   !strcmp(dst, "00000000000000000000000000000000") &&
			   !strcmp(plen, "00") && (flags & 0x1) &&
			   !(flags & 0x200) && if_up(root, ifc)) {
			found = 1;
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return found;
}

void ki_probe_network(void)
{
	const char *root = getenv("KDOS_NET_ROOT");

	if (!root)
		root = "";
	ki_sys.network = default_route(root, 0) || default_route(root, 1);
}

void probe_system(void)
{
	memset(&ki_sys, 0, sizeof(ki_sys));

	ki_sys.uefi = kb_path_exists("/sys/firmware/efi");
	ki_probe_network();

	/*
	 * AND WHICH EFI BINARY THIS MACHINE CAN EXECUTE. The kernel publishes
	 * the firmware's own width, which on an early Atom tablet is 32 while
	 * the CPU and every byte of this image are 64. A kernel too old to
	 * publish it leaves this 0, which the bootloader step reads as
	 * "assume 64" — the overwhelming majority, and the case where being
	 * wrong is a machine that boots through the removable-media fallback
	 * rather than one that does not boot at all.
	 */
	if (ki_sys.uefi) {
		char fw[16];

		if (kb_read_file("/sys/firmware/efi/fw_platform_size", fw,
				 sizeof(fw)) > 0)
			ki_sys.fw_bits = atoi(fw);
	}

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
	 * /proc/cpuinfo, which would report 1 on every machine: the model
	 * name parse above NUL-terminates the buffer at the end of the first
	 * model name line, so only the first "processor" line is left to
	 * count, and cpuinfo on a 16-thread
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
}

/* ════════════════════════════════════════════════════════════════════════
 * The applications this medium knows how to build
 * ════════════════════════════════════════════════════════════════════════ */

KiGroup ki_group[MAX_APPGROUPS];
int ki_ngroup;
int ki_apps_present;

/*
 * WHERE THE CATALOGUE IS, on a live medium. $KDOS_CATALOGUE first so a dump
 * can be driven over a fixture; then the live root's own copy, which is what
 * an installer booted from the ISO actually reads.
 */
static const char *catalogue_path(void)
{
	static char path[512];
	const char *env = getenv("KDOS_CATALOGUE");
	static const char *const tries[] = {
		"/usr/share/kdos/appstore/catalogue",
		"/mnt/iso/appstore/catalogue",
	};

	if (env && *env)
		return env;
	for (size_t i = 0; i < sizeof(tries) / sizeof(tries[0]); i++)
		if (kb_path_exists(tries[i])) {
			kb_strlcpy(path, tries[i], sizeof(path));
			return path;
		}
	return tries[0];
}

/*
 * THE GROUPS, out of the catalogue, with a size estimate per group.
 *
 * A group's estimate counts each member ONCE and counts no runtime at all: the
 * shared layers under two GTK applications are stored once on disk, so summing
 * the per-application figures already over-counts, and adding the runtimes
 * would over-count again. It is labelled an estimate everywhere it is shown.
 */
void probe_apps(void)
{
	char err[256];

	ki_ngroup = 0;
	ki_apps_present = 0;

	if (cat_load(catalogue_path(), err, sizeof(err)) != 0)
		return;
	ki_apps_present = 1;

	for (int i = 0; i < cat_ngroups() && ki_ngroup < MAX_APPGROUPS; i++) {
		const CatGroup *g = cat_group_at(i);
		KiGroup *k = &ki_group[ki_ngroup];

		kb_strlcpy(k->id, g->id, sizeof(k->id));
		kb_strlcpy(k->desc, g->desc, sizeof(k->desc));
		k->napp = 0;
		k->bytes = 0;
		k->chosen = 0;
		for (int m = 0; m < g->nmember; m++) {
			const CatPack *a = cat_find(g->members[m]);

			if (!a)
				continue;
			k->napp++;
			k->bytes += a->bytes;
		}
		/* A group whose members are all absent from this catalogue is
		 * not offered: a tick that installs nothing is worse than a
		 * row that is not there. */
		if (k->napp)
			ki_ngroup++;
	}
}

unsigned long long ki_apps_bytes(void)
{
	unsigned long long n = 0;

	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			n += ki_group[i].bytes;
	return n;
}

int ki_apps_route(void)
{
	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			return APPS_PENDING;
	return APPS_NONE;
}

void ki_apps_collect(void)
{
	/* Composed rather than formatted, so the bound is the one checked: a
	 * group id that would not fit whole is left off, never cut. Nothing
	 * ticked is the word `none`, because an empty line reads back as the
	 * default and the default is `essential`. */
	cfg.apps[0] = '\0';
	for (int i = 0; i < ki_ngroup; i++) {
		size_t l = strlen(cfg.apps), il = strlen(ki_group[i].id);

		if (!ki_group[i].chosen || l + (l ? 1 : 0) + il + 1 >
						   sizeof(cfg.apps))
			continue;
		if (l)
			cfg.apps[l++] = ' ';
		memcpy(cfg.apps + l, ki_group[i].id, il + 1);
	}
	if (!cfg.apps[0])
		kb_strlcpy(cfg.apps, "none", sizeof(cfg.apps));
}

void ki_apps_enter(void)
{
	probe_apps();
	if (!ki_apps_present)
		return;

	/*
	 * THE ANSWER FILE WINS WHERE IT NAMED SOMETHING, and `essential` is
	 * the default where it did not. An unknown name falls the whole
	 * selection back to `essential` rather than refusing: this runs before
	 * the point of no return, and refusing there would leave a machine
	 * with no operating system on it over one misspelling.
	 */
	if (!cfg.apps[0]) {
		for (int i = 0; i < ki_ngroup; i++)
			ki_group[i].chosen = !strcmp(ki_group[i].id,
						     "essential");
		ki_apps_collect();
		return;
	}
	for (int i = 0; i < ki_ngroup; i++)
		ki_group[i].chosen = 0;
	if (!strcmp(cfg.apps, "none"))
		return;
	{
		char want[1024];
		char *tok, *save;
		int matched = 0, asked = 0;

		kb_strlcpy(want, cfg.apps, sizeof(want));
		for (tok = strtok_r(want, " ", &save); tok;
		     tok = strtok_r(NULL, " ", &save)) {
			asked++;
			for (int i = 0; i < ki_ngroup; i++)
				if (!strcmp(ki_group[i].id, tok)) {
					ki_group[i].chosen = 1;
					matched++;
					break;
				}
		}
		if (asked && !matched)
			for (int i = 0; i < ki_ngroup; i++)
				ki_group[i].chosen =
					!strcmp(ki_group[i].id, "essential");
	}
	ki_apps_collect();
}

/* ── native groups ─────────────────────────────────────────────────────── */

KiNative ki_native[MAX_NATIVE];
int ki_nnative;

static const char *groups_path(void)
{
	const char *env = getenv("KDOS_GROUPS");
	return env && *env ? env : "/ports/groups";
}

/*
 * HOW MANY PORTS A MEMBER INSTALLS. A meta port is a recipe with no
 * `source =` line, which installs nothing of its own and pulls in its
 * `depends`; a group of one such port (kde-gear-games) holds a whole Gear
 * set, and counted by its line it would read as one program. The recipes
 * are beside the groups file, at <dir>/core/<shelf>/<name>/kpkgbuild, as
 * on the image's /ports; a member with no recipe there counts as one.
 */
static int member_ports(const char *dir, const char *name)
{
	char core[PATH_MAX / 2], path[PATH_MAX];
	char **shelves;
	char *data = NULL;
	int n = 1, meta = 1, deps = 0;

	snprintf(core, sizeof(core), "%.2000s/core", dir);
	shelves = kb_listdir(core, NULL);
	for (char **sh = shelves; sh && *sh && !data; sh++) {
		snprintf(path, sizeof(path), "%s/%.255s/%.255s/kpkgbuild",
			 core, *sh, name);
		data = kb_read_all(path, NULL);
	}
	kb_strv_free(shelves);
	if (!data)
		return 1;
	for (char *l = data, *nl; l && *l; l = nl ? nl + 1 : NULL) {
		char *key = l, *v;
		size_t kl;

		nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*key == ' ' || *key == '\t')
			key++;
		kl = strcspn(key, " \t=");
		v = key + kl;
		while (*v == ' ' || *v == '\t')
			v++;
		if (*v != '=' || *key == '#')
			continue;
		v++;
		if (kl == 6 && !strncmp(key, "source", 6))
			meta = 0;
		if (kl == 7 && !strncmp(key, "depends", 7))
			for (char *w = strtok(v, " \t\r"); w;
			     w = strtok(NULL, " \t\r"))
				deps++;
	}
	free(data);
	if (meta && deps)
		n = deps;
	return n;
}

int ki_binhost_named;

/* One `key = value` line's value in a file, into out; 0 when absent. */
static int conf_line(const char *path, const char *key, char *out, size_t cap)
{
	char *data = kb_read_all(path, NULL);
	size_t kl = strlen(key);
	int found = 0;

	for (char *l = data; l && *l && !found;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (*l != '#' && !strncmp(l, key, kl) &&
		    (l[kl] == ' ' || l[kl] == '\t' || l[kl] == '=')) {
			char *v = l + kl;
			while (*v == ' ' || *v == '\t' || *v == '=')
				v++;
			v[strcspn(v, " \t\r")] = 0;
			if (*v) {
				kb_strlcpy(out, v, cap);
				found = 1;
			}
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
	return found;
}

/* A release name is made of the characters `kdos update` takes in one. */
static int release_ok(const char *v)
{
	if (!*v || strlen(v) > 40)
		return 0;
	for (; *v; v++)
		if (!((*v >= '0' && *v <= '9') || (*v >= 'a' && *v <= 'z') ||
		      (*v >= 'A' && *v <= 'Z') || *v == '.' || *v == '+' ||
		      *v == '-' || *v == '_' || *v == '~'))
			return 0;
	return 1;
}

/*
 * WHETHER A BINHOST IS NAMED, in `kdos update`'s order: `binhost =` in
 * /etc/kdos/update.conf, then $KDOS_UPDATE_BINHOST, then a `version` line in
 * the TREE stamp beside the groups file, which only a published release's
 * tree carries. With none, the Packs step downloads nothing and the chosen
 * groups go to groups-pending, so the step's weight is its short one.
 */
static int binhost_named(const char *dir)
{
	char v[256];
	const char *e = getenv("KDOS_UPDATE_BINHOST");
	char *tree;
	int named;

	if (conf_line("/etc/kdos/update.conf", "binhost", v, sizeof(v)))
		return 1;
	if (e && *e)
		return 1;
	tree = kb_path_join(dir, "TREE");
	named = conf_line(tree, "version", v, sizeof(v)) && release_ok(v);
	free(tree);
	return named;
}

/* The format is ports/groups': a `group <id> <title>` line, then member
 * ports up to the next group line; `#` starts a comment. */
void probe_groups(void)
{
	const char *gp = groups_path();
	char *data = kb_read_all(gp, NULL);
	char dir[PATH_MAX];
	char *slash;

	kb_strlcpy(dir, gp, sizeof(dir));
	slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	else
		kb_strlcpy(dir, ".", sizeof(dir));

	ki_binhost_named = binhost_named(dir);
	ki_nnative = 0;
	for (char *l = data; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		char *h = strchr(l, '#');
		if (h)
			*h = 0;
		while (*l == ' ' || *l == '\t')
			l++;
		if (!strncmp(l, "group", 5) && (l[5] == ' ' || l[5] == '\t')) {
			if (ki_nnative == MAX_NATIVE)
				break;
			char *id = l + 6;
			while (*id == ' ' || *id == '\t')
				id++;
			size_t il = strcspn(id, " \t\r");
			char *t = id + il;
			while (*t == ' ' || *t == '\t')
				t++;
			id[il] = 0;
			if (*id && ki_name_ok(id)) {
				KiNative *g = &ki_native[ki_nnative++];
				kb_strlcpy(g->id, id, sizeof(g->id));
				kb_strlcpy(g->title, *t ? t : id, sizeof(g->title));
				g->title[strcspn(g->title, "\r")] = 0;
				g->nports = 0;
				g->chosen = 0;
			}
		} else if (ki_nnative > 0) {
			char *save = NULL;

			for (char *w = strtok_r(l, " \t\r", &save); w;
			     w = strtok_r(NULL, " \t\r", &save))
				ki_native[ki_nnative - 1].nports +=
					member_ports(dir, w);
		}
		l = nl ? nl + 1 : NULL;
	}
	free(data);
}

int ki_groups_chosen(void)
{
	int n = 0;

	for (int i = 0; i < ki_nnative; i++)
		n += ki_native[i].chosen;
	return n;
}

void ki_groups_collect(void)
{
	cfg.groups[0] = '\0';
	for (int i = 0; i < ki_nnative; i++) {
		size_t l = strlen(cfg.groups), il = strlen(ki_native[i].id);

		if (!ki_native[i].chosen || l + (l ? 1 : 0) + il + 1 >
						     sizeof(cfg.groups))
			continue;
		if (l)
			cfg.groups[l++] = ' ';
		memcpy(cfg.groups + l, ki_native[i].id, il + 1);
	}
	if (!cfg.groups[0])
		kb_strlcpy(cfg.groups, "none", sizeof(cfg.groups));
}

/*
 * THE LANGUAGE A CHOICE POINTS AT, one table: an XKB layout, a console keymap
 * or a time zone that is Chinese, Japanese or Korean names that language's
 * group of /ports/groups, which carries the input method and the fonts the
 * base image leaves out. Without it the person who typed in Japanese at the
 * keyboard page logs in to a desktop with no way to type it. The installer
 * asks for no locale (the first login's language is plasma-setup's or the
 * live session's), so the keyboard and the zone are what it has. The
 * keyboard is checked first, so its reason is the one a group reports.
 */
enum { LANG_XKB, LANG_KEYMAP, LANG_ZONE };

static const struct {
	int kind;
	const char *word;	/* a layout, a keymap, or a zone name     */
	const char *group;
	const char *language;
} lang_map[] = {
	{ LANG_XKB,    "cn",             "lang-zh", "Chinese"  },
	{ LANG_XKB,    "tw",             "lang-zh", "Chinese"  },
	{ LANG_XKB,    "jp",             "lang-ja", "Japanese" },
	{ LANG_XKB,    "kr",             "lang-ko", "Korean"   },
	{ LANG_KEYMAP, "jp106",          "lang-ja", "Japanese" },
	{ LANG_ZONE,   "Asia/Shanghai",  "lang-zh", "Chinese"  },
	{ LANG_ZONE,   "Asia/Urumqi",    "lang-zh", "Chinese"  },
	{ LANG_ZONE,   "Asia/Hong_Kong", "lang-zh", "Chinese"  },
	{ LANG_ZONE,   "Asia/Macau",     "lang-zh", "Chinese"  },
	{ LANG_ZONE,   "Asia/Taipei",    "lang-zh", "Chinese"  },
	{ LANG_ZONE,   "Asia/Tokyo",     "lang-ja", "Japanese" },
	{ LANG_ZONE,   "Asia/Seoul",     "lang-ko", "Korean"   },
	{ LANG_ZONE,   "Asia/Pyongyang", "lang-ko", "Korean"   },
};

/* Whether `word` is one of the comma-separated layouts of `list`: an XKB
 * answer of `us,jp` switches between both, and the second is still typed. */
static int in_list(const char *list, const char *word)
{
	size_t wl = strlen(word);

	for (const char *p = list; *p;) {
		size_t l = strcspn(p, ",");
		if (l == wl && !strncmp(p, word, wl))
			return 1;
		p += l;
		if (*p)
			p++;
	}
	return 0;
}

void ki_lang_why(const char *group, char *out, size_t n)
{
	out[0] = 0;
	for (int k = LANG_XKB; k <= LANG_ZONE && !out[0]; k++)
		for (size_t i = 0; i < sizeof(lang_map) / sizeof(lang_map[0]);
		     i++) {
			if (lang_map[i].kind != k ||
			    strcmp(lang_map[i].group, group))
				continue;
			if (k == LANG_XKB &&
			    in_list(cfg.xkb_layout, lang_map[i].word))
				snprintf(out, n, "the %s keyboard",
					 lang_map[i].language);
			else if (k == LANG_KEYMAP &&
				 !strcmp(cfg.keymap, lang_map[i].word))
				snprintf(out, n, "the %s keyboard",
					 lang_map[i].language);
			else if (k == LANG_ZONE &&
				 !strcmp(cfg.tz_label, lang_map[i].word))
				snprintf(out, n, "the time zone %s",
					 lang_map[i].word);
			if (out[0])
				break;
		}
}

/* Whether `id` is one of the space-separated words of `list`. */
static int in_words(const char *list, const char *id)
{
	size_t il = strlen(id);

	for (const char *p = list; *p;) {
		while (*p == ' ')
			p++;
		size_t l = strcspn(p, " ");
		if (l == il && l && !strncmp(p, id, il))
			return 1;
		p += l;
	}
	return 0;
}

void ki_groups_enter(void)
{
	/* What the language rule ticked at the last call. The wizard enters
	 * the page again after a trip back to the keyboard, with cfg.groups
	 * already the person's ticks: only what the rule's answer CHANGED is
	 * applied over them, so an untick survives and a new keyboard still
	 * ticks its language. */
	static char suggested[1024];
	char want[1024], now[1024] = "";
	char *tok, *save;
	int fresh = !cfg.groups[0];

	probe_groups();
	for (int i = 0; i < ki_nnative; i++) {
		ki_native[i].chosen = 0;
		ki_lang_why(ki_native[i].id, ki_native[i].why,
			    sizeof(ki_native[i].why));
	}
	kb_strlcpy(want, cfg.groups, sizeof(want));
	for (tok = strtok_r(want, " ", &save); tok;
	     tok = strtok_r(NULL, " ", &save))
		for (int i = 0; i < ki_nnative; i++)
			if (!strcmp(ki_native[i].id, tok))
				ki_native[i].chosen = 1;
	if (!cfg.groups_set) {
		for (int i = 0; i < ki_nnative; i++) {
			KiNative *g = &ki_native[i];
			int was = in_words(suggested, g->id);

			if (g->why[0] && (fresh || !was))
				g->chosen = 1;
			else if (!g->why[0] && was && !fresh)
				g->chosen = 0;
			if (g->why[0] && strlen(now) + strlen(g->id) + 2 <
						 sizeof(now)) {
				strcat(now, " ");
				strcat(now, g->id);
			}
		}
		kb_strlcpy(suggested, now, sizeof(suggested));
	}
	ki_groups_collect();
}

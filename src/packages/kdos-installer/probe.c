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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "kinstall.h"
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
 * THE GROUP BY NAME, FROM LVM'S OWN LIST OF PHYSICAL VOLUMES. The active
 * volumes in ki_lv miss a group with no volume, one whose volumes did not
 * activate, and the other half of a group that spans the target disk and
 * another. `pvs` reads every label, so each of those still shows as a PV of
 * the group on a device off the disk. A `pvs` that does not run is taken as
 * no group: without lvm there is no vgcreate for the name to stop.
 */
int ki_vg_off_disk(const char *vg, const char *disk)
{
	KbArgv a = {0};
	KbBuf out = {0};
	char *save = NULL, *line;
	int hit = 0;

	if (!kb_have_prog("lvm"))
		return 0;
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
	probe_lvs();
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

/* ════════════════════════════════════════════════════════════════════════
 * The applications this medium knows how to build
 * ════════════════════════════════════════════════════════════════════════ */

KiGroup ki_group[MAX_APPGROUPS];
int ki_ngroup;
int ki_apps_present;
char ki_apps_archive[512];

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
 * AN EXPORTED SET ON A MOUNTED DEVICE, which is the offline route and the only
 * one a machine with no network has. The first `.ktar` found under a mounted
 * removable filesystem wins, and it is the only one offered — a second medium
 * carrying a different set is not reachable from the page.
 *
 * It is looked for ONCE, at probe time. Walking every mount on every draw
 * would put a filesystem scan inside the paint path.
 */
static void find_archive(void)
{
	static const char *const roots[] = { "/mnt", "/media", "/run/media" };

	ki_apps_archive[0] = '\0';
	for (size_t r = 0; r < sizeof(roots) / sizeof(roots[0]); r++) {
		char **ents = kb_listdir(roots[r], NULL);

		for (char **e = ents; e && *e; e++) {
			char dir[512];
			char **f;

			snprintf(dir, sizeof(dir), "%s/%s", roots[r], *e);
			f = kb_listdir(dir, NULL);
			for (char **g = f; g && *g; g++) {
				size_t l = strlen(*g);

				if (l < 6 || strcmp(*g + l - 5, ".ktar"))
					continue;
				/* A PATH THAT WOULD NOT FIT IS SKIPPED, never
				 * truncated: a shortened path names a
				 * different file, and the failure would be an
				 * import of something nobody chose.
				 *
				 * Composed rather than formatted, so the bound
				 * is the one checked here — a compiler cannot
				 * see the guard through a format call and
				 * rejects it whatever the check says. */
				size_t dl = strlen(dir);

				if (dl + 1 + l >= sizeof(ki_apps_archive))
					continue;
				memcpy(ki_apps_archive, dir, dl);
				ki_apps_archive[dl] = '/';
				memcpy(ki_apps_archive + dl + 1, *g, l + 1);
				break;
			}
			kb_strv_free(f);
			if (ki_apps_archive[0])
				break;
		}
		kb_strv_free(ents);
		if (ki_apps_archive[0])
			break;
	}
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
	find_archive();

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

/*
 * WHICH ROUTE, AND THE PAGE SAYS SO. In order: an exported set on a stick is
 * offline and verified, so it wins; a network during the install is next; and
 * with neither, the selection is recorded and the first session offers it.
 *
 * THE NETWORK TEST IS A ROUTE, NOT A PING. `/proc/net/route` carries a default
 * gateway or it does not; opening a socket to somebody else's host to decide
 * what to draw would be an installer reaching the network to ask whether it
 * can reach the network.
 */
static int have_default_route(void)
{
	char buf[8192];
	char *line, *save;
	int n = 0;

	if (kb_read_file("/proc/net/route", buf, sizeof(buf)) <= 0)
		return 0;
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char iface[64], dest[64];

		if (n++ == 0)
			continue;	/* the header row */
		if (sscanf(line, "%63s %63s", iface, dest) != 2)
			continue;
		if (!strcmp(dest, "00000000"))
			return 1;
	}
	return 0;
}

int ki_apps_route(void)
{
	int any = 0;

	for (int i = 0; i < ki_ngroup; i++)
		any |= ki_group[i].chosen;
	if (!any)
		return APPS_NONE;
	if (ki_apps_archive[0])
		return APPS_IMPORT;
	if (have_default_route())
		return APPS_NETWORK;
	return APPS_PENDING;
}

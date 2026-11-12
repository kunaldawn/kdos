/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos persist — the live session remembers
 * ---------------------------------
 *
 * A live session's writes land in the overlay's upper layer, which is a tmpfs
 * and therefore gone at power-off. This makes that upper a real filesystem
 * instead: one partition, labelled KDOS_PERSIST, in the free space after the
 * image on the stick, or on a disk somebody names.
 *
 * THE LABEL IS THE WHOLE INTERFACE. The initramfs asks blkid for it by name
 * and uses whatever answers, so the store can be on the boot stick, on a
 * second stick or on an internal disk, and none of them have to be recorded
 * anywhere. Nothing here writes a path into a configuration file, because a
 * path is a promise about enumeration order that USB does not keep.
 *
 * IT MUST BE A FILESYSTEM THAT CARRIES XATTRS, HARDLINKS AND A d_type, which
 * is why this makes ext4 and reports an ntfs, iso9660 or squashfs store as
 * refused: overlayfs rejects such an upper with EINVAL and the boot falls
 * back to a tmpfs. FAT and exFAT never get that far, because neither can
 * carry a 12-character label.
 *
 * A STORE ON THE BOOT DISK IS REACHED THROUGH A LOOP DEVICE. The medium is
 * mounted as the whole disk (ISO9660 starts at its first sector, so no
 * partition carries it), and a mounted disk is claimed exclusively: the
 * kernel then refuses an exclusive open of any partition on it, so mkfs and
 * mount of the store's partition both fail with EBUSY. A loop over the disk
 * at the partition's offset and length opens the disk without that claim.
 * The initramfs mounts the store the same way, so /mnt/persist's source is
 * a /dev/loopN, which loop_part() maps back to the partition.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "kbase.h"
#include "kdos-tools.h"

#define PERSIST_LABEL "KDOS_PERSIST"
#define PERSIST_MNT   "/mnt/persist"
#define PERSIST_MAX   16

/* ── sysfs and small runs ───────────────────────────────────────────────── */

static const char *bname(const char *dev)
{
	const char *s = strrchr(dev, '/');
	return s ? s + 1 : dev;
}

/* /sys/class/block/<name>/<attr> as a number; -1 when it is not there. */
static long long blk_attr(const char *name, const char *attr)
{
	char path[256], buf[64];

	snprintf(path, sizeof(path), "/sys/class/block/%s/%s", name, attr);
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0)
		return -1;
	return strtoll(buf, NULL, 10);
}

static unsigned long long dev_bytes(const char *dev)
{
	long long s = blk_attr(bname(dev), "size");
	return s > 0 ? (unsigned long long)s * 512ULL : 0;
}

static int is_partition(const char *dev)
{
	char path[256];

	snprintf(path, sizeof(path), "/sys/class/block/%s/partition",
		 bname(dev));
	return kb_path_exists(path);
}

/* The kernel name of the disk a device is on: sda for sda4, nvme0n1 for
 * nvme0n1p2, the device itself when it is a whole disk. sysfs nests a
 * partition inside its disk's directory, so the answer is the parent's
 * name and no suffix is ever parsed. */
static void parent_disk(const char *dev, char *out, size_t cap)
{
	char path[256], real[PATH_MAX];

	kb_strlcpy(out, bname(dev), cap);
	if (!is_partition(dev))
		return;
	snprintf(path, sizeof(path), "/sys/class/block/%s/..", bname(dev));
	if (realpath(path, real))
		kb_strlcpy(out, bname(real), cap);
}

/* The first line a program prints, or "" when it fails or prints nothing. */
static void run_line(KbArgv *a, char *out, size_t cap)
{
	out[0] = 0;
	kb_argv_end(a);
	if (kb_run_capture(a, out, cap) != 0) {
		out[0] = 0;
		return;
	}
	char *nl = strchr(out, '\n');
	if (nl)
		*nl = 0;
}

/* ── finding things ─────────────────────────────────────────────────────── */

/* The source and type of the mount at `mnt`, or "" when nothing is there. */
static void mount_at(const char *mnt, char *src, size_t scap, char *type,
		     size_t tcap)
{
	char *buf = kb_read_all("/proc/mounts", NULL);

	src[0] = 0;
	if (type)
		type[0] = 0;
	if (!buf)
		return;
	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[128], m[192], t[32];
		if (sscanf(line, "%127s %191s %31s", dev, m, t) != 3)
			continue;
		if (strcmp(m, mnt))
			continue;
		kb_strlcpy(src, dev, scap);
		if (type)
			kb_strlcpy(type, t, tcap);
	}
	free(buf);
}

/*
 * The whole disk this system booted from, or "" when it did not boot from a
 * medium — an installed system has no stick to add a store to.
 *
 * THE MEDIUM IS THE DISK AND NOT A PARTITION ON IT. A written stick carries
 * ISO9660 from its first sector, so the initramfs mounts the DISK at
 * /mnt/iso; an optical boot mounts /dev/srN there, which is read-only.
 */
static void boot_medium(char *out, size_t cap)
{
	char type[32];

	mount_at("/mnt/iso", out, cap, type, sizeof(type));
	if (strcmp(type, "iso9660") || strncmp(out, "/dev/", 5))
		out[0] = 0;
}

/* Is this device on the disk the medium is mounted from? */
static int on_medium(const char *dev)
{
	char medium[128], disk[64];

	boot_medium(medium, sizeof(medium));
	if (!medium[0])
		return 0;
	parent_disk(dev, disk, sizeof(disk));
	return !strcmp(disk, bname(medium));
}

/*
 * A loop device over a partition's span maps back to that partition: the
 * loop's backing file is a block device, and the partition of it that starts
 * at the loop's offset is the store. Anything else is not mapped.
 */
static int loop_part(const char *dev, char *out, size_t cap)
{
	const char *base = bname(dev);
	char path[256], back[160], buf[64];
	struct stat st;

	if (strncmp(base, "loop", 4))
		return 0;
	snprintf(path, sizeof(path), "/sys/block/%s/loop/backing_file", base);
	if (kb_read_line_file(path, back, sizeof(back)) < 0)
		return 0;
	if (stat(back, &st) != 0 || !S_ISBLK(st.st_mode))
		return 0;
	snprintf(path, sizeof(path), "/sys/block/%s/loop/offset", base);
	if (kb_read_line_file(path, buf, sizeof(buf)) < 0)
		return 0;
	unsigned long long off = strtoull(buf, NULL, 10);

	char disk[64];
	int n = 0, hit = 0;
	parent_disk(back, disk, sizeof(disk));
	snprintf(path, sizeof(path), "/sys/block/%s", disk);
	char **names = kb_listdir(path, &n);
	for (int i = 0; names && i < n && !hit; i++) {
		char sub[256];
		snprintf(sub, sizeof(sub), "/sys/block/%s/%s/partition", disk,
			 names[i]);
		if (!kb_path_exists(sub))
			continue;
		long long start = blk_attr(names[i], "start");
		if (start >= 0 && (unsigned long long)start * 512ULL == off) {
			snprintf(out, cap, "/dev/%s", names[i]);
			hit = 1;
		}
	}
	kb_strv_free(names);
	return hit;
}

/* The device a person would recognise: a loop over a partition becomes the
 * partition, anything else stays itself. */
static void canon(const char *dev, char *out, size_t cap)
{
	if (!loop_part(dev, out, cap))
		kb_strlcpy(out, dev, cap);
}

/* The device the boot would take as the store — the one `blkid -L` answers,
 * as the initramfs asks — or "" when there is none. */
static void find_store(char *out, size_t cap)
{
	KbArgv a = { 0 };
	char dev[128];

	out[0] = 0;
	kb_argv_add(&a, "blkid");
	kb_argv_add(&a, "-L");
	kb_argv_add(&a, PERSIST_LABEL);
	run_line(&a, dev, sizeof(dev));
	if (dev[0])
		canon(dev, out, cap);
}

/* Every device carrying the label, loops folded onto their partitions. */
static int labelled(char list[][128], int max)
{
	KbArgv a = { 0 };
	KbBuf b = { 0 };
	int n = 0;

	kb_argv_add(&a, "blkid");
	kb_argv_add(&a, "-t");
	kb_argv_add(&a, "LABEL=" PERSIST_LABEL);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "device");
	kb_argv_end(&a);
	if (kb_run_capture_buf(&a, &b) != 0 || !b.p) {
		kb_buf_free(&b);
		return 0;
	}
	char *save = NULL;
	for (char *line = strtok_r(b.p, "\n", &save); line && n < max;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[128];
		int dup = 0;

		canon(line, dev, sizeof(dev));
		for (int i = 0; i < n; i++)
			if (!strcmp(list[i], dev))
				dup = 1;
		if (!dup)
			kb_strlcpy(list[n++], dev, 128);
	}
	kb_buf_free(&b);
	return n;
}

static void fs_type(const char *dev, char *out, size_t cap)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, "blkid");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "value");
	kb_argv_add(&a, "-s");
	kb_argv_add(&a, "TYPE");
	kb_argv_add(&a, dev);
	run_line(&a, out, cap);
}

/* The types the initramfs refuses as an upper by name. THIS LIST MUST MATCH
 * the case in 090_initramfs.sh's store mount: a type refused there and not
 * here is reported as becoming the upper layer at the next boot, and the boot
 * then comes up in RAM. */
static int refused_type(const char *t)
{
	static const char *const bad[] = {
		"vfat", "exfat", "ntfs", "iso9660", "squashfs", NULL
	};
	for (int i = 0; bad[i]; i++)
		if (!strcmp(t, bad[i]))
			return 1;
	return 0;
}

/*
 * The store's state, from what the session is RUNNING ON first: the device
 * at /mnt/persist is the store this session writes to, whatever blkid would
 * answer now. Only with nothing mounted there is the label asked about.
 */
struct pstate {
	const char *state;		/* none, present, refused, in-use  */
	char dev[128];
	char type[32];
};

static void store_state(struct pstate *s)
{
	char src[128];

	s->state = "none";
	s->dev[0] = s->type[0] = 0;
	mount_at(PERSIST_MNT, src, sizeof(src), s->type, sizeof(s->type));
	if (src[0]) {
		canon(src, s->dev, sizeof(s->dev));
		s->state = "in-use";
		return;
	}
	find_store(s->dev, sizeof(s->dev));
	if (!s->dev[0])
		return;
	fs_type(s->dev, s->type, sizeof(s->type));
	s->state = refused_type(s->type) ? "refused" : "present";
}

static int booted_nopersist(void)
{
	char buf[4096];
	char line[4100];

	if (kb_read_line_file("/proc/cmdline", buf, sizeof(buf)) < 0)
		return 0;
	snprintf(line, sizeof(line), " %s ", buf);
	return strstr(line, " nopersist ") != NULL;
}

/* Disks a store could be made on from a page: the boot medium when it is
 * writable, then every removable or USB disk of at least PERSIST_MIN that is
 * writable and has nothing of it mounted. A floppy drive reports itself
 * removable and writable and holds a few kilobytes. */
#define PERSIST_MIN (256ULL << 20)

static int is_removable(const char *disk)
{
	char path[256], real[PATH_MAX];

	if (blk_attr(disk, "removable") == 1)
		return 1;
	snprintf(path, sizeof(path), "/sys/block/%s", disk);
	return realpath(path, real) && strstr(real, "/usb");
}

static int disk_mounted(const char *disk)
{
	char *buf = kb_read_all("/proc/mounts", NULL);
	int hit = 0;

	if (!buf)
		return 0;
	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line && !hit;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[128], real[128], d[64];
		if (sscanf(line, "%127s", dev) != 1 || strncmp(dev, "/dev/", 5))
			continue;
		canon(dev, real, sizeof(real));
		parent_disk(real, d, sizeof(d));
		hit = !strcmp(d, disk);
	}
	free(buf);
	return hit;
}

static int candidates(char list[][128], int max)
{
	char medium[128];
	int n = 0, nd = 0;

	boot_medium(medium, sizeof(medium));
	if (medium[0] && blk_attr(bname(medium), "ro") == 0)
		kb_strlcpy(list[n++], medium, 128);

	char **disks = kb_listdir("/sys/block", &nd);
	for (int i = 0; disks && i < nd && n < max; i++) {
		const char *d = disks[i];
		if (!strncmp(d, "loop", 4) || !strncmp(d, "ram", 3) ||
		    !strncmp(d, "dm-", 3) || !strncmp(d, "zram", 4) ||
		    !strncmp(d, "sr", 2) || !strncmp(d, "md", 2) ||
		    !strncmp(d, "nbd", 3) || !strncmp(d, "fd", 2))
			continue;
		if (medium[0] && !strcmp(d, bname(medium)))
			continue;
		if (!is_removable(d) || blk_attr(d, "ro") != 0 ||
		    dev_bytes(d) < PERSIST_MIN || disk_mounted(d))
			continue;
		snprintf(list[n++], 128, "/dev/%s", d);
	}
	kb_strv_free(disks);
	return n;
}

/* ── loops ──────────────────────────────────────────────────────────────── */

/*
 * The node to open for a partition: the partition itself, or, on the boot
 * disk, a loop over the partition's span (the header says why). `loop` is
 * set when one was made and must be detached afterwards.
 */
static int open_node(const char *part, char *out, size_t cap, int *loop)
{
	char medium[128];

	*loop = 0;
	if (!on_medium(part)) {
		kb_strlcpy(out, part, cap);
		return 0;
	}
	boot_medium(medium, sizeof(medium));
	long long start = blk_attr(bname(part), "start");
	long long size = blk_attr(bname(part), "size");
	if (start < 0 || size <= 0) {
		fprintf(stderr, "kdos persist: the kernel has no start and size "
				"for %s\n", part);
		return -1;
	}
	KbArgv a = { 0 };
	kb_argv_add(&a, "losetup");
	kb_argv_add(&a, "--find");
	kb_argv_add(&a, "--show");
	kb_argv_add(&a, "-o");
	kb_argv_addf(&a, "%llu", (unsigned long long)start * 512ULL);
	kb_argv_add(&a, "--sizelimit");
	kb_argv_addf(&a, "%llu", (unsigned long long)size * 512ULL);
	kb_argv_add(&a, medium);
	run_line(&a, out, cap);
	if (strncmp(out, "/dev/", 5)) {
		fprintf(stderr, "kdos persist: losetup would not map %s on %s\n",
			part, medium);
		return -1;
	}
	*loop = 1;
	return 0;
}

static void close_node(const char *node, int loop)
{
	if (!loop)
		return;
	KbArgv a = { 0 };
	kb_argv_add(&a, "losetup");
	kb_argv_add(&a, "-d");
	kb_argv_add(&a, node);
	kb_argv_end(&a);
	kb_run(&a);
}

/* ── status ─────────────────────────────────────────────────────────────── */

static int cmd_status(void)
{
	struct pstate s;
	char others[PERSIST_MAX][128];
	int nothers = labelled(others, PERSIST_MAX);

	store_state(&s);
	if (!strcmp(s.state, "none")) {
		printf("no persistence store\n\n");
		printf("  This session's writes are in RAM and go when it is\n");
		printf("  powered off. `kdos persist create` makes a store in\n");
		printf("  the free space after the image on the boot medium.\n");
		return 0;
	}

	printf("store    %s  (%s, %s)\n", s.dev, kb_human_size(dev_bytes(s.dev)),
	       s.type[0] ? s.type : "unknown type");

	/*
	 * IN USE IS NOT THE SAME QUESTION AS PRESENT. A store made in this
	 * session is not the upper of the overlay this session is already
	 * running on, and will not be until the next boot — reporting it as
	 * live would tell somebody their work is being saved when it is not.
	 */
	struct statvfs vfs;
	if (!strcmp(s.state, "in-use") && statvfs(PERSIST_MNT, &vfs) == 0) {
		unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
		unsigned long long avail = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
		printf("state    in use by this session\n");
		printf("free     %s of %s\n", kb_human_size(avail),
		       kb_human_size(total));
	} else if (!strcmp(s.state, "refused")) {
		printf("state    refused: %s cannot hold an overlay upper\n",
		       s.type);
		printf("         the boot ignores it and the session runs in RAM;\n");
		printf("         reformat it ext4, or take the label off it.\n");
	} else {
		printf("state    present, NOT in use by this session\n");
		if (booted_nopersist())
			printf("         this boot was started with nopersist (the\n"
			       "         \"clean session\" entry);\n");
		printf("         it becomes the upper layer at the next boot;\n");
		printf("         the \"clean session\" menu entry ignores it.\n");
	}
	for (int i = 0; i < nothers; i++) {
		if (!strcmp(others[i], s.dev))
			continue;
		printf("also     %s carries the label %s\n", others[i],
		       PERSIST_LABEL);
	}
	if (nothers > 1)
		printf("         the boot uses the one blkid answers first, which\n"
		       "         is not a promise across reboots: take the label\n"
		       "         off every store but one.\n");
	return 0;
}

/*
 * THE SAME ANSWER FOR A PAGE, as one line so a reader needs no stream
 * parser: whether this is a live session (a boot medium is mounted), the
 * store's state as the page switches on it, the medium and whether it can be
 * written, and the disks a store could be made on.
 */
static int cmd_status_json(void)
{
	struct pstate s;
	char medium[128];
	char cand[PERSIST_MAX][128];
	int ncand = candidates(cand, PERSIST_MAX);
	KbBuf b = { 0 };

	store_state(&s);
	boot_medium(medium, sizeof(medium));
	kb_buf_printf(&b, "{\"live\": %s, \"store\": \"%s\", \"device\": ",
		      medium[0] ? "true" : "false", s.state);
	if (s.dev[0])
		kb_json_str(&b, s.dev);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ", \"type\": ");
	if (s.type[0])
		kb_json_str(&b, s.type);
	else
		kb_buf_str(&b, "null");
	kb_buf_str(&b, ", \"medium\": ");
	if (medium[0])
		kb_json_str(&b, medium);
	else
		kb_buf_str(&b, "null");
	kb_buf_printf(&b, ", \"writable\": %s, \"candidates\": [",
		      medium[0] && blk_attr(bname(medium), "ro") == 0
			      ? "true" : "false");
	for (int i = 0; i < ncand; i++) {
		if (i)
			kb_buf_str(&b, ", ");
		kb_json_str(&b, cand[i]);
	}
	kb_buf_str(&b, "]}\n");
	fwrite(b.p, 1, b.n, stdout);
	kb_buf_free(&b);
	return 0;
}

/* ── create ─────────────────────────────────────────────────────────────── */

/* Which partition numbers the kernel knows on a disk, as a bitmap. */
static void part_set(const char *disk, unsigned char set[129])
{
	const char *base = bname(disk);
	char path[256];

	memset(set, 0, 129);
	for (int i = 1; i <= 128; i++) {
		snprintf(path, sizeof(path), "/sys/class/block/%s%s%d",
			 base, isdigit((unsigned char)base[strlen(base) - 1])
			       ? "p" : "", i);
		set[i] = (unsigned char)kb_path_exists(path);
	}
}

static void partname(const char *disk, int n, char *out, size_t cap)
{
	size_t l = strlen(disk);
	int digit = l && isdigit((unsigned char)disk[l - 1]);
	snprintf(out, cap, "%s%s%d", disk, digit ? "p" : "", n);
}

static void blkid_probe(const char *dev, const char *tag, char *out, size_t cap)
{
	KbArgv a = { 0 };

	kb_argv_add(&a, "blkid");
	kb_argv_add(&a, "-p");
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, "value");
	kb_argv_add(&a, "-s");
	kb_argv_add(&a, tag);
	kb_argv_add(&a, dev);
	run_line(&a, out, cap);
}

static int confirm(void)
{
	printf("Type yes to go ahead: ");
	fflush(stdout);
	char ans[16] = { 0 };
	if (!fgets(ans, sizeof(ans), stdin) || strncmp(ans, "yes", 3)) {
		fprintf(stderr, "kdos persist: not confirmed\n");
		return 0;
	}
	return 1;
}

static int need_root(const char *verb)
{
	if (geteuid() == 0)
		return 1;
	fprintf(stderr, "kdos persist: must run as root — try: sudo kdos "
			"persist %s\n", verb);
	return 0;
}

static int cmd_create(const char *want, int assume_yes)
{
	char disk[128], medium[128];
	char stores[PERSIST_MAX][128];

	if (labelled(stores, PERSIST_MAX) > 0) {
		fprintf(stderr, "kdos persist: a store already exists on %s\n",
			stores[0]);
		fprintf(stderr, "              remove it first, or use it as "
				"it is\n");
		return 1;
	}

	boot_medium(medium, sizeof(medium));
	if (want) {
		char real[PATH_MAX];
		kb_strlcpy(disk, realpath(want, real) ? real : want,
			   sizeof(disk));
	} else {
		kb_strlcpy(disk, medium, sizeof(disk));
	}

	if (!disk[0]) {
		fprintf(stderr, "kdos persist: this system did not boot from a "
				"medium — name a device to put the store on\n");
		return 1;
	}

	struct stat st;
	if (stat(disk, &st) != 0 || !S_ISBLK(st.st_mode)) {
		fprintf(stderr, "kdos persist: %s is not a block device\n", disk);
		return 1;
	}
	if (is_partition(disk)) {
		char whole[64];
		parent_disk(disk, whole, sizeof(whole));
		fprintf(stderr, "kdos persist: %s is a partition — name the disk "
				"it is on: /dev/%s\n", disk, whole);
		return 1;
	}
	if (blk_attr(bname(disk), "ro") == 1) {
		fprintf(stderr, "kdos persist: %s is read-only (a disc) — name a "
				"USB stick or disk: sudo kdos persist create "
				"/dev/sdX\n", disk);
		return 1;
	}

	if (!need_root("create"))
		return 1;

	static const char *const need[] = { "sfdisk", "mkfs.ext4", "blkid",
					    "losetup", NULL };
	for (int i = 0; need[i]; i++)
		if (!kb_have_prog(need[i])) {
			fprintf(stderr, "kdos persist: %s is not installed\n",
				need[i]);
			return 1;
		}

	/*
	 * A DISK WITH NO PARTITION TABLE GETS ONE, AND A DISK THAT IS ONE
	 * FILESYSTEM IS LEFT ALONE. `sfdisk --append` needs a table to append
	 * to; a blank disk has none and is given a GPT holding only the store.
	 * A disk formatted whole carries somebody's data across every sector
	 * and has no free space to take, so it is refused with its type named.
	 */
	char pttype[32], wholefs[32];
	blkid_probe(disk, "PTTYPE", pttype, sizeof(pttype));
	int fresh = !pttype[0];
	if (fresh) {
		blkid_probe(disk, "TYPE", wholefs, sizeof(wholefs));
		if (wholefs[0]) {
			fprintf(stderr, "kdos persist: %s is one %s filesystem "
					"with no partition table — nothing on it "
					"is free to take\n", disk, wholefs);
			return 1;
		}
	}

	unsigned char before[129];
	int nbefore = 0;
	part_set(disk, before);
	for (int i = 1; i <= 128; i++)
		nbefore += before[i];

	printf("  disk       %s  (%s)\n", disk, kb_human_size(dev_bytes(disk)));
	if (fresh)
		printf("  partitions none; a GPT is written holding the store\n");
	else
		printf("  partitions %d now; the store is added after the last\n",
		       nbefore);
	printf("  filesystem ext4, labelled %s\n", PERSIST_LABEL);
	if (!fresh)
		printf("\n  Nothing already on %s is moved or rewritten: the "
		       "store is\n  made from the free space AFTER the last "
		       "partition.\n", disk);
	printf("\n");

	if (!assume_yes && !confirm())
		return 1;

	/*
	 * `--append` AND NOT A REWRITTEN TABLE. sfdisk given a whole script
	 * replaces every entry, and the entries on a written stick describe the
	 * ISO and the ESP the machine is running from. Appending adds one and
	 * leaves the rest byte for byte. `--wipe never` keeps sfdisk off the
	 * ISO9660 signature at the start of the disk, which is the running
	 * medium. `--no-reread` because the boot disk is in use and sfdisk
	 * refuses one without it; partx below tells the kernel instead.
	 */
	KbArgv a = { 0 };
	char err[1024];
	const char *script = fresh ? "label: gpt\n,,L\n" : ",,L\n";
	kb_argv_add(&a, "sfdisk");
	kb_argv_add(&a, "--no-reread");
	kb_argv_add(&a, "--wipe");
	kb_argv_add(&a, "never");
	if (!fresh)
		kb_argv_add(&a, "--append");
	kb_argv_add(&a, disk);
	kb_argv_end(&a);
	if (kb_run_feed_env(&a, NULL, 0, script, strlen(script), err,
			    sizeof(err)) != 0) {
		size_t el = strlen(err);
		fprintf(stderr, "kdos persist: sfdisk would not add a partition "
				"to %s:\n%s%s", disk, el ? err : "(it said nothing)",
			el && err[el - 1] == '\n' ? "" : "\n");
		return 1;
	}

	/*
	 * THE KERNEL IS TOLD ABOUT THE NEW PARTITION WITHOUT REREADING THE
	 * TABLE, because the boot disk is held by the medium's mount and a
	 * full reread is refused while it is — BLKRRPART answers EBUSY and the
	 * node never appears. `partx -a` adds the new entry and leaves the
	 * known ones alone.
	 */
	if (kb_have_prog("partx")) {
		KbArgv px = { 0 };
		kb_argv_add(&px, "partx");
		kb_argv_add(&px, "-a");
		kb_argv_add(&px, disk);
		kb_argv_end(&px);
		kb_run(&px);
	}
	if (kb_have_prog("udevadm")) {
		KbArgv ud = { 0 };
		kb_argv_add(&ud, "udevadm");
		kb_argv_add(&ud, "settle");
		kb_argv_end(&ud);
		kb_run(&ud);
	}

	unsigned char after[129];
	int num = 0;
	part_set(disk, after);
	for (int i = 1; i <= 128 && !num; i++)
		if (after[i] && !before[i])
			num = i;
	char part[160];
	if (!num) {
		/* The partition is in the table now but carries no label, so
		 * `create` after a reboot would not find it as the store and
		 * would try to append another. Formatting it is the one step
		 * left. */
		fprintf(stderr, "kdos persist: the new partition on %s did not "
				"appear — reboot, then format it with "
				"`mkfs.ext4 -m 0 -L %s <partition>`\n",
			disk, PERSIST_LABEL);
		return 1;
	}
	partname(disk, num, part, sizeof(part));

	char node[128];
	int loop = 0;
	if (open_node(part, node, sizeof(node), &loop) != 0)
		return 1;

	printf("making ext4 on %s%s%s...\n", part, loop ? " through " : "",
	       loop ? node : "");
	KbArgv mk = { 0 };
	kb_argv_add(&mk, "mkfs.ext4");
	kb_argv_add(&mk, "-q");
	/* -F: the partition was made a moment ago, so whatever an older
	 * signature in its sectors claims is not somebody's data, and with no
	 * terminal mke2fs would read its question's answer as no. */
	kb_argv_add(&mk, "-F");
	/* No reserved blocks: this is not a root filesystem and five per cent
	 * of a stick is a lot to hold back from the only thing using it. */
	kb_argv_add(&mk, "-m");
	kb_argv_add(&mk, "0");
	kb_argv_add(&mk, "-L");
	kb_argv_add(&mk, PERSIST_LABEL);
	kb_argv_add(&mk, node);
	kb_argv_end(&mk);
	int mkrc = kb_run(&mk);
	sync();
	close_node(node, loop);
	if (mkrc != 0) {
		fprintf(stderr, "kdos persist: mkfs.ext4 failed on %s\n", part);
		return 1;
	}

	printf("\nstore ready on %s (%s).\n", part,
	       kb_human_size(dev_bytes(part)));
	printf("It becomes the session's upper layer at the next boot.\n");
	return 0;
}

/* ── reset ──────────────────────────────────────────────────────────────── */

enum { RESET_ALL, RESET_HOME, RESET_SYSTEM };

/* Empty a directory, keeping the directory. Returns how many entries could
 * not be removed. `keep` names one entry to leave, or NULL. */
static int empty_dir(const char *dir, const char *keep)
{
	int n = 0, bad = 0;
	char **names = kb_listdir(dir, &n);

	for (int i = 0; names && i < n; i++) {
		if (keep && !strcmp(names[i], keep))
			continue;
		char *p = kb_path_join(dir, names[i]);
		if (kb_rmtree(p) != 0)
			bad++;
		free(p);
	}
	kb_strv_free(names);
	return bad;
}

/*
 * A CHANGE IS UNDONE BY DELETING IT FROM upper/, and only while nothing is
 * running on it. The upper of a mounted overlay must not be changed beneath
 * it (overlayfs's rule: the result is undefined), so a store in use is
 * refused and the reset runs from the clean session, where the store is not
 * mounted at all. Deleting a whiteout from upper/ brings back the image's
 * file it hid, which is what resetting the system part means.
 */
static int cmd_reset(int what, int assume_yes)
{
	struct pstate s;
	static const char *const said[] = {
		"every change it holds: upper/ is emptied, and the next boot "
		"starts as the image shipped",
		"the home directories: upper/home is emptied",
		"every system change: upper/ is emptied except home/",
	};

	store_state(&s);
	if (!strcmp(s.state, "none")) {
		fprintf(stderr, "kdos persist: there is no persistence store\n");
		return 1;
	}
	if (!strcmp(s.state, "in-use")) {
		fprintf(stderr, "kdos persist: %s is this session's upper layer "
				"and cannot be changed under it — boot \"KDOS "
				"Live (clean session)\" and run this there\n",
			s.dev);
		return 1;
	}
	if (!strcmp(s.state, "refused")) {
		fprintf(stderr, "kdos persist: %s is %s, which the boot never "
				"uses — there is nothing of a session on it\n",
			s.dev, s.type);
		return 1;
	}
	if (!need_root("reset"))
		return 1;

	printf("  store      %s  (%s)\n", s.dev, kb_human_size(dev_bytes(s.dev)));
	printf("  removes    %s\n\n", said[what]);
	if (!assume_yes && !confirm())
		return 1;

	char dir[] = "/run/kdos-persist.XXXXXX";
	if (!mkdtemp(dir)) {
		fprintf(stderr, "kdos persist: no mount point under /run\n");
		return 1;
	}

	char node[128];
	int loop = 0, rc = 1;
	if (open_node(s.dev, node, sizeof(node), &loop) != 0) {
		rmdir(dir);
		return 1;
	}
	KbArgv m = { 0 };
	kb_argv_add(&m, "mount");
	kb_argv_add(&m, node);
	kb_argv_add(&m, dir);
	kb_argv_end(&m);
	if (kb_run(&m) != 0) {
		fprintf(stderr, "kdos persist: %s would not mount\n", s.dev);
		close_node(node, loop);
		rmdir(dir);
		return 1;
	}

	char upper[64], work[64];
	snprintf(upper, sizeof(upper), "%s/upper", dir);
	snprintf(work, sizeof(work), "%s/work", dir);
	int bad = 0;
	if (kb_is_dir(upper)) {
		if (what == RESET_HOME) {
			char home[80];
			snprintf(home, sizeof(home), "%s/home", upper);
			if (kb_is_dir(home))
				bad += empty_dir(home, NULL);
		} else {
			bad += empty_dir(upper,
					 what == RESET_SYSTEM ? "home" : NULL);
		}
	}
	/* work/ holds only the overlay's scratch, which it may leave behind
	 * at a power cut; it is never a change anybody made. */
	if (kb_is_dir(work))
		bad += empty_dir(work, NULL);
	sync();

	KbArgv u = { 0 };
	kb_argv_add(&u, "umount");
	kb_argv_add(&u, dir);
	kb_argv_end(&u);
	if (kb_run(&u) != 0)
		fprintf(stderr, "kdos persist: %s would not unmount from %s\n",
			s.dev, dir);
	else
		rmdir(dir);
	close_node(node, loop);
	sync();

	if (bad) {
		fprintf(stderr, "kdos persist: %d entr%s could not be removed\n",
			bad, bad == 1 ? "y" : "ies");
	} else {
		printf("reset %s. The next persistent boot starts from it.\n",
		       s.dev);
		rc = 0;
	}
	return rc;
}

/* ── entry ──────────────────────────────────────────────────────────────── */

static int usage(void)
{
	printf("usage: kdos persist [--json]\n"
	       "       kdos persist create [<device>] [--yes]\n"
	       "       kdos persist reset [--home | --system] [--yes]\n"
	       "\n"
	       "  With no arguments, report whether a persistence store\n"
	       "  exists and whether this session is writing to it.\n"
	       "  --json        the same, as one line for a settings page\n"
	       "\n"
	       "  create        make the store in the free space after the\n"
	       "                image, on the boot medium unless a disk is\n"
	       "                named. Nothing already there is rewritten;\n"
	       "                a disk with no partition table gets a GPT.\n"
	       "  reset         delete the changes the store holds: all of\n"
	       "                them, only --home, or all but home (--system).\n"
	       "                Run it from the \"clean session\" boot entry.\n"
	       "  --yes         do not ask for confirmation\n"
	       "\n"
	       "  The store is an ext4 filesystem labelled " PERSIST_LABEL ".\n"
	       "  The initramfs finds it by that label, so it may live on any\n"
	       "  disk. Boot the \"clean session\" menu entry to ignore it.\n");
	return 0;
}

int persist_main(int argc, char **argv)
{
	const char *dev = NULL;
	int create = 0, reset = 0, yes = 0, json = 0, what = RESET_ALL;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-h") || !strcmp(a, "--help"))
			return usage();
		if (!strcmp(a, "--yes") || !strcmp(a, "-y")) {
			yes = 1;
		} else if (!strcmp(a, "--json")) {
			json = 1;
		} else if (!strcmp(a, "create") && !create && !reset) {
			create = 1;
		} else if (!strcmp(a, "reset") && !create && !reset) {
			reset = 1;
		} else if (reset && !strcmp(a, "--home") && what == RESET_ALL) {
			what = RESET_HOME;
		} else if (reset && !strcmp(a, "--system") && what == RESET_ALL) {
			what = RESET_SYSTEM;
		} else if (a[0] == '-') {
			fprintf(stderr, "kdos persist: unknown option '%s'\n", a);
			return 1;
		} else if (create && !dev) {
			dev = a;
		} else {
			fprintf(stderr, "kdos persist: unexpected '%s'\n", a);
			return 1;
		}
	}

	if (json && (create || reset)) {
		fprintf(stderr, "kdos persist: --json is a status\n");
		return 1;
	}
	if (json)
		return cmd_status_json();
	if (reset)
		return cmd_reset(what, yes);
	return create ? cmd_create(dev, yes) : cmd_status();
}

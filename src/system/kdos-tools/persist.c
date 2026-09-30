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
 * image on the stick.
 *
 * THE LABEL IS THE WHOLE INTERFACE. The initramfs asks blkid for it by name
 * and uses whatever answers, so the store can be on the boot stick, on a
 * second stick or on an internal disk, and none of them have to be recorded
 * anywhere. Nothing here writes a path into a configuration file, because a
 * path is a promise about enumeration order that USB does not keep.
 *
 * IT MUST BE A FILESYSTEM THAT CARRIES XATTRS, HARDLINKS AND A d_type, which
 * is why this makes ext4 and refuses to treat a vfat partition as a store:
 * overlayfs rejects such an upper with EINVAL and the boot would fall back to
 * a tmpfs having said nothing a user could act on.
 */

#include <ctype.h>
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

/* ── finding things ─────────────────────────────────────────────────────── */

/* The device holding the store, or "" if there is none. */
static void find_store(char *out, size_t cap)
{
	KbArgv a = { 0 };
	char buf[256] = { 0 };

	out[0] = 0;
	kb_argv_add(&a, "blkid");
	kb_argv_add(&a, "-L");
	kb_argv_add(&a, PERSIST_LABEL);
	kb_argv_end(&a);
	if (kb_run_capture(&a, buf, sizeof(buf)) != 0)
		return;
	char *nl = strchr(buf, '\n');
	if (nl)
		*nl = 0;
	kb_strlcpy(out, buf, cap);
}

/*
 * The whole disk this system booted from, or "" when it did not boot from a
 * medium — an installed system has no stick to add a store to.
 *
 * THE MEDIUM IS THE DISK AND NOT A PARTITION ON IT. A written stick carries
 * ISO9660 from its first sector, so /proc/mounts names the DISK; taking the
 * mount's device and stripping a trailing digit would turn /dev/sda into
 * /dev/sd and find nothing.
 */
static void boot_medium(char *out, size_t cap)
{
	size_t len = 0;
	char *buf = kb_read_all("/proc/mounts", &len);

	out[0] = 0;
	if (!buf)
		return;
	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char dev[128], mnt[192], type[32];
		if (sscanf(line, "%127s %191s %31s", dev, mnt, type) != 3)
			continue;
		if (strcmp(type, "iso9660"))
			continue;
		if (strncmp(dev, "/dev/", 5))
			continue;
		kb_strlcpy(out, dev, cap);
		break;
	}
	free(buf);
}

static unsigned long long dev_bytes(const char *dev)
{
	const char *base = strrchr(dev, '/');
	char path[256];
	char *s;
	size_t n = 0;
	unsigned long long v = 0;

	base = base ? base + 1 : dev;
	snprintf(path, sizeof(path), "/sys/class/block/%s/size", base);
	s = kb_read_all(path, &n);
	if (s) {
		v = strtoull(s, NULL, 10) * 512ULL;
		free(s);
	}
	return v;
}

/* ── status ─────────────────────────────────────────────────────────────── */

static int cmd_status(void)
{
	char store[128];
	find_store(store, sizeof(store));

	if (!store[0]) {
		printf("no persistence store\n\n");
		printf("  This session's writes are in RAM and go when it is\n");
		printf("  powered off. `kdos persist create` makes a store in\n");
		printf("  the free space after the image on the boot medium.\n");
		return 0;
	}

	printf("store    %s  (%s)\n", store, kb_human_size(dev_bytes(store)));

	/*
	 * IN USE IS NOT THE SAME QUESTION AS PRESENT. A store made in this
	 * session is not the upper of the overlay this session is already
	 * running on, and will not be until the next boot — reporting it as
	 * live would tell somebody their work is being saved when it is not.
	 */
	struct statvfs vfs;
	int mounted = statvfs(PERSIST_MNT, &vfs) == 0;
	size_t len = 0;
	char *mounts = kb_read_all("/proc/mounts", &len);
	int live = mounts && strstr(mounts, " " PERSIST_MNT " ") != NULL;
	free(mounts);

	if (live && mounted) {
		unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
		unsigned long long avail = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
		printf("state    in use by this session\n");
		printf("free     %s of %s\n", kb_human_size(avail),
		       kb_human_size(total));
	} else {
		printf("state    present, NOT in use by this session\n");
		printf("         it becomes the upper layer at the next boot;\n");
		printf("         the \"clean session\" menu entry ignores it.\n");
	}
	return 0;
}

/* ── create ─────────────────────────────────────────────────────────────── */

/* The partitions of a disk, newest last, as /sys knows them. */
static int part_count(const char *disk)
{
	const char *base = strrchr(disk, '/');
	char path[256];
	int n = 0;

	base = base ? base + 1 : disk;
	for (int i = 1; i <= 128; i++) {
		snprintf(path, sizeof(path), "/sys/class/block/%s%s%d",
			 base, isdigit((unsigned char)base[strlen(base) - 1])
			       ? "p" : "", i);
		if (kb_path_exists(path))
			n = i;
	}
	return n;
}

static void partname(const char *disk, int n, char *out, size_t cap)
{
	size_t l = strlen(disk);
	int digit = l && isdigit((unsigned char)disk[l - 1]);
	snprintf(out, cap, "%s%s%d", disk, digit ? "p" : "", n);
}

static int cmd_create(const char *want, int assume_yes)
{
	char disk[128], store[128];

	find_store(store, sizeof(store));
	if (store[0]) {
		fprintf(stderr, "kdos persist: a store already exists on %s\n",
			store);
		fprintf(stderr, "              remove it first, or use it as "
				"it is\n");
		return 1;
	}

	if (want)
		kb_strlcpy(disk, want, sizeof(disk));
	else
		boot_medium(disk, sizeof(disk));

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

	if (geteuid() != 0) {
		fprintf(stderr, "kdos persist: must run as root — try: sudo "
				"kdos persist create\n");
		return 1;
	}

	static const char *const need[] = { "sfdisk", "mkfs.ext4", NULL };
	for (int i = 0; need[i]; i++)
		if (!kb_have_prog(need[i])) {
			fprintf(stderr, "kdos persist: %s is not installed\n",
				need[i]);
			return 1;
		}

	int before = part_count(disk);
	printf("  disk       %s  (%s)\n", disk, kb_human_size(dev_bytes(disk)));
	printf("  partitions %d now; the store becomes number %d\n",
	       before, before + 1);
	printf("  filesystem ext4, labelled %s\n", PERSIST_LABEL);
	printf("\n  Nothing already on %s is moved or rewritten: the store is\n",
	       disk);
	printf("  made from the free space AFTER the last partition.\n\n");

	if (!assume_yes) {
		printf("Type yes to go ahead: ");
		fflush(stdout);
		char ans[16] = { 0 };
		if (!fgets(ans, sizeof(ans), stdin) ||
		    strncmp(ans, "yes", 3)) {
			fprintf(stderr, "kdos persist: not confirmed\n");
			return 1;
		}
	}

	/*
	 * `--append` AND NOT A REWRITTEN TABLE. sfdisk given a whole script
	 * replaces every entry, and the entries on a written stick describe the
	 * ISO and the ESP the machine is running from. Appending adds one and
	 * leaves the rest byte for byte.
	 */
	KbArgv a = { 0 };
	kb_argv_add(&a, "sfdisk");
	kb_argv_add(&a, "--append");
	kb_argv_add(&a, disk);
	kb_argv_end(&a);
	if (kb_run_feed(&a, ",,L\n", 4) != 0) {
		fprintf(stderr, "kdos persist: sfdisk would not add a "
				"partition — is there free space after the "
				"image?\n");
		return 1;
	}

	/*
	 * THE KERNEL IS TOLD ABOUT THE NEW PARTITION WITHOUT REREADING THE
	 * TABLE, because a stick this system booted from has a mounted
	 * partition on it and a full reread is refused while one exists —
	 * BLKRRPART answers EBUSY and the node never appears. `partx -a` adds
	 * the one new entry and leaves the mounted ones alone.
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

	char part[160];
	partname(disk, before + 1, part, sizeof(part));
	if (!kb_path_exists(part)) {
		fprintf(stderr, "kdos persist: %s did not appear — reboot and "
				"run this again\n", part);
		return 1;
	}

	printf("making ext4 on %s...\n", part);
	KbArgv mk = { 0 };
	kb_argv_add(&mk, "mkfs.ext4");
	kb_argv_add(&mk, "-q");
	/* No reserved blocks: this is not a root filesystem and five per cent
	 * of a stick is a lot to hold back from the only thing using it. */
	kb_argv_add(&mk, "-m");
	kb_argv_add(&mk, "0");
	kb_argv_add(&mk, "-L");
	kb_argv_add(&mk, PERSIST_LABEL);
	kb_argv_add(&mk, part);
	kb_argv_end(&mk);
	if (kb_run(&mk) != 0) {
		fprintf(stderr, "kdos persist: mkfs.ext4 failed on %s\n", part);
		return 1;
	}

	sync();
	printf("\nstore ready on %s (%s).\n", part,
	       kb_human_size(dev_bytes(part)));
	printf("It becomes the session's upper layer at the next boot.\n");
	return 0;
}

/* ── entry ──────────────────────────────────────────────────────────────── */

static int usage(void)
{
	printf("usage: kdos persist [create [<device>]] [--yes]\n"
	       "\n"
	       "  With no arguments, report whether a persistence store\n"
	       "  exists and whether this session is writing to it.\n"
	       "\n"
	       "  create        make the store in the free space after the\n"
	       "                image, on the boot medium unless a device is\n"
	       "                named. Nothing already there is rewritten.\n"
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
	int create = 0, yes = 0;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-h") || !strcmp(a, "--help"))
			return usage();
		if (!strcmp(a, "--yes") || !strcmp(a, "-y")) {
			yes = 1;
		} else if (!strcmp(a, "create")) {
			create = 1;
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

	return create ? cmd_create(dev, yes) : cmd_status();
}

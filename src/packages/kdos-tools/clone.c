/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos clone — the stick writes the stick
 *
 * A machine with no network makes another machine. `kdos rebuild` reproduces
 * the ISO from the sources on the medium; this reproduces the MEDIUM, which is
 * the operation somebody standing in front of two USB sticks actually wants.
 *
 * It is a raw copy of the image and deliberately nothing cleverer. The boot
 * arrangement — El Torito, the EFI image, the partition layout — is whatever
 * the medium already carries, so a copy boots exactly what the original boots
 * and there is no second opinion about how a KDOS stick is laid out.
 *
 * THE IMAGE'S LENGTH COMES FROM THE IMAGE, NEVER FROM THE DEVICE. A 3 GB image
 * written to a 64 GB stick leaves the device reporting 64 GB, and copying that
 * copies 61 GB of whatever was on the stick before. Same principle as the pack
 * format reading EROFS's own superblock extent — except that here there are
 * TWO self-descriptions and the obvious one is a trap. See img_extent().
 * ---------------------------------
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kdos-tools.h"

#define CL_MAX_TARGETS 32
#define CL_CHUNK (4u << 20)		/* 4 MiB: one read, one write, one hash */

struct cl_target {
	char kname[64];			/* sda                              */
	char node[128];			/* /dev/sda                         */
	char model[80];
	unsigned long long bytes;
};

/* ── the image's own extent ────────────────────────────────────────────── */

static int read_at(int fd, void *buf, size_t n, off_t off)
{
	return pread(fd, buf, n, off) == (ssize_t)n;
}

static unsigned long long le64(const unsigned char *p)
{
	unsigned long long v = 0;
	for (int i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return v;
}

static unsigned int le32(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
	       ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned int le16(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

/*
 * How long is the image on this device or in this file?
 *
 * TWO RECORDS DESCRIBE IT AND THE ISO9660 ONE IS THE SHORTER. The Primary
 * Volume Descriptor at sector 16 carries the volume space size, and on an
 * optical-only image that is the whole thing to the byte — measured against
 * the shipped ISO, 4970509 blocks x 2048 = its exact file size. But a hybrid
 * image built with `-append_partition` puts the EFI System Partition AFTER the
 * ISO9660 volume, and the PVD does not count it: measured on a test image, the
 * PVD stopped 4.5 MB short, and those 4.5 MB were precisely the ESP. Trusting
 * the PVD alone would truncate away the thing that makes the copy boot.
 *
 * The GPT's backup header is the other record and it is the one that spans an
 * appended partition: the header at LBA 1 names `alternate_lba`, and the image
 * ends one sector past it. Measured: (17187 + 1) x 512 was the test image's
 * exact size.
 *
 * So both are read and the LARGER wins. An image with only one gets that one;
 * an image with neither is not something to copy blindly, and is refused.
 */
static unsigned long long img_extent(int fd)
{
	unsigned char sec[2048];
	unsigned long long iso = 0, gpt = 0;

	/* ISO9660: "CD001" at the start of the PVD, sector 16. Volume space
	 * size is a both-endian 32-bit at +80, block size at +128. */
	if (read_at(fd, sec, sizeof(sec), 16 * 2048) &&
	    sec[0] == 1 && !memcmp(sec + 1, "CD001", 5)) {
		unsigned int blocks = le32(sec + 80);
		unsigned int bs = le16(sec + 128);
		if (blocks && bs)
			iso = (unsigned long long)blocks * bs;
	}

	/* GPT: "EFI PART" at LBA 1, alternate_lba at +32. The backup header
	 * occupies that LBA, so the image ends at the end of it. */
	if (read_at(fd, sec, 512, 512) && !memcmp(sec, "EFI PART", 8)) {
		unsigned long long alt = le64(sec + 32);
		if (alt)
			gpt = (alt + 1) * 512ULL;
	}

	return iso > gpt ? iso : gpt;
}

/* ── what must never be written to ─────────────────────────────────────── */

/*
 * The disk a partition belongs to. The target of a clone is a whole DISK, but
 * everything that identifies the running system names a partition — so a check
 * that compared node for node would happily overwrite the disk it is running
 * from because the mount says `/dev/sda2` and the target says `/dev/sda`.
 */
static void parent_disk(const char *node, char *out, size_t n)
{
	const char *base = kb_basename(node);
	char path[512];
	size_t len = strlen(base);

	/* A whole disk has a directory under /sys/block and a partition does
	 * not — partitions live INSIDE their disk's. That is the test rather
	 * than the presence of a `partition` attribute, because an fstab may
	 * name a device that is not plugged in and the suffix still has to come
	 * off: answering `sda1` for a stale entry would let `sda` be written.
	 * nvme0n1p3 -> nvme0n1, mmcblk0p2 -> mmcblk0, sda1 -> sda. */
	snprintf(path, sizeof(path), "/sys/block/%s", base);
	if (kb_is_dir(path)) {
		snprintf(out, n, "%s", base);
		return;
	}
	while (len > 1 && base[len - 1] >= '0' && base[len - 1] <= '9')
		len--;
	if (len > 1 && base[len - 1] == 'p' &&
	    (!strncmp(base, "nvme", 4) || !strncmp(base, "mmcblk", 6) ||
	     !strncmp(base, "loop", 4)))
		len--;
	snprintf(out, n, "%.*s", (int)len, base);
}

/*
 * Every disk the running system depends on, as kernel names.
 *
 * A live session's boot medium is at /mnt/iso — the initramfs mounts it and
 * `switch_root` MS_MOVEs it across, which is what makes it visible here at all.
 * An installed system has `/` and `/boot`. Anything else with a filesystem
 * mounted anywhere is somebody's data and is not offered either: that is a
 * wider rule than kdos-mountd's, because mountd is choosing something to MOUNT
 * and this is choosing something to DESTROY.
 */
static int in_use_disks(char out[][64], int max)
{
	char *data = kb_read_all("/proc/mounts", NULL);
	int n = 0;

	if (!data)
		return 0;
	for (char *p = data; *p && n < max;) {
		char *nl = strchr(p, '\n');
		char dev[256] = "", mnt[256] = "";

		if (nl)
			*nl = '\0';
		if (sscanf(p, "%255s %255s", dev, mnt) == 2 &&
		    !strncmp(dev, "/dev/", 5)) {
			char disk[64];
			int dup = 0;

			parent_disk(dev, disk, sizeof(disk));
			for (int i = 0; i < n; i++)
				if (!strcmp(out[i], disk))
					dup = 1;
			if (!dup)
				snprintf(out[n++], 64, "%s", disk);
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return n;
}

/* An fstab entry is a decision somebody already made. Only device paths are
 * matched: a UUID= line names a filesystem this has not opened. */
static int in_fstab(const char *node)
{
	char *data = kb_read_all("/etc/fstab", NULL);
	char disk[64], want[64];
	int hit = 0;

	if (!data)
		return 0;
	parent_disk(node, want, sizeof(want));
	for (char *p = data; *p && !hit;) {
		char *nl = strchr(p, '\n');
		char dev[256] = "";

		if (nl)
			*nl = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '#' && sscanf(p, "%255s", dev) == 1 &&
		    !strncmp(dev, "/dev/", 5)) {
			parent_disk(dev, disk, sizeof(disk));
			if (!strcmp(disk, want))
				hit = 1;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return hit;
}

static char *sys_str(const char *fmt, const char *a, const char *b)
{
	char path[512], buf[256];

	snprintf(path, sizeof(path), fmt, a, b);
	if (kb_read_line_file(path, buf, sizeof(buf)) != 0)
		return NULL;
	for (int i = (int)strlen(buf) - 1; i >= 0 && (buf[i] == ' ' ||
	     buf[i] == '\n'); i--)
		buf[i] = '\0';
	return kb_strdup(buf);
}

static int sys_int(const char *fmt, const char *a, const char *b)
{
	char *s = sys_str(fmt, a, b);
	int v = s ? atoi(s) : 0;
	free(s);
	return v;
}

/* Removable, or on USB — kdos-mountd's rule, and for the same reason: an
 * external drive in an enclosure reports removable = 0 and is exactly as much
 * somebody else's disk as a stick is. */
static int is_removable(const char *disk)
{
	char path[512], real[1024];
	ssize_t n;

	if (sys_int("/sys/block/%s/removable%s", disk, "") == 1)
		return 1;
	snprintf(path, sizeof(path), "/sys/block/%s", disk);
	n = readlink(path, real, sizeof(real) - 1);
	if (n <= 0)
		return 0;
	real[n] = '\0';
	return strstr(real, "/usb") != NULL;
}

static int list_targets(struct cl_target *t, int max, char used[][64], int nused)
{
	int n = 0, ndisk = 0;
	char **disks = kb_listdir("/sys/block", &ndisk);

	if (!disks)
		return 0;
	for (int i = 0; i < ndisk && n < max; i++) {
		const char *d = disks[i];
		char *model;

		/* None of these is a thing somebody plugged in, and `sr` is
		 * read-only: offering to write a disc nothing can write is a
		 * row that fails when it is chosen. */
		if (!strncmp(d, "loop", 4) || !strncmp(d, "ram", 3) ||
		    !strncmp(d, "dm-", 3) || !strncmp(d, "zram", 4) ||
		    !strncmp(d, "sr", 2) || !strncmp(d, "md", 2))
			continue;
		if (!is_removable(d))
			continue;

		int skip = 0;
		for (int k = 0; k < nused; k++)
			if (!strcmp(used[k], d))
				skip = 1;
		if (skip)
			continue;

		snprintf(t[n].node, sizeof(t[n].node), "/dev/%s", d);
		if (in_fstab(t[n].node))
			continue;
		snprintf(t[n].kname, sizeof(t[n].kname), "%s", d);
		t[n].bytes = (unsigned long long)
			sys_int("/sys/block/%s/size%s", d, "") * 512ULL;
		model = sys_str("/sys/block/%s/device/model%s", d, "");
		snprintf(t[n].model, sizeof(t[n].model), "%s",
			 model ? model : "");
		free(model);
		n++;
	}
	kb_strv_free(disks);
	return n;
}

/* ── the copy ──────────────────────────────────────────────────────────── */

static void progress(unsigned long long done, unsigned long long total,
		     double t0, const char *what)
{
	double el = kb_now_s() - t0;
	int pct = total ? (int)((double)done * 100.0 / (double)total) : 0;
	char rate[64] = "";

	if (el > 0.5)
		snprintf(rate, sizeof(rate), "  %s/s",
			 kb_human_size((unsigned long long)((double)done / el)));
	printf("\r  %s %3d%%  %s%s   ", what, pct, kb_human_size(done), rate);
	fflush(stdout);
}

/*
 * A READ-BACK THAT READS THE PAGE CACHE VERIFIES NOTHING. Everything just
 * written is still in the block device's buffer cache, so re-reading it hands
 * back the bytes this process produced rather than the bytes the flash stored —
 * which is exactly the failure a counterfeit stick presents, and exactly the
 * one the verify exists to catch. BLKFLSBUF drops that cache.
 */
static void drop_cache(int fd)
{
	fsync(fd);
	ioctl(fd, BLKFLSBUF, 0);
	posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
}

static int hash_stream(int fd, unsigned long long len, char out[65],
		       const char *what)
{
	unsigned char *buf = kb_calloc(1, CL_CHUNK);
	unsigned long long done = 0;
	double t0 = kb_now_s();
	KbSha256 s;

	kb_sha256_init(&s);
	while (done < len) {
		size_t want = (len - done) < CL_CHUNK ? (size_t)(len - done)
						     : CL_CHUNK;
		ssize_t got = read(fd, buf, want);

		if (got <= 0) {
			free(buf);
			return -1;
		}
		kb_sha256_update(&s, buf, (size_t)got);
		done += (unsigned long long)got;
		progress(done, len, t0, what);
	}
	kb_sha256_final(&s, out);
	free(buf);
	printf("\n");
	return 0;
}

static int copy_stream(int in, int out, unsigned long long len, char hash[65])
{
	unsigned char *buf = kb_calloc(1, CL_CHUNK);
	unsigned long long done = 0;
	double t0 = kb_now_s();
	KbSha256 s;

	kb_sha256_init(&s);
	while (done < len) {
		size_t want = (len - done) < CL_CHUNK ? (size_t)(len - done)
						     : CL_CHUNK;
		ssize_t got = read(in, buf, want);
		ssize_t put;

		if (got <= 0)
			break;
		put = write(out, buf, (size_t)got);
		if (put != got) {
			free(buf);
			printf("\n");
			fprintf(stderr, "kdos clone: short write at %llu: %s\n",
				done, strerror(errno));
			return -1;
		}
		kb_sha256_update(&s, buf, (size_t)got);
		done += (unsigned long long)got;
		progress(done, len, t0, "writing ");
	}
	kb_sha256_final(&s, hash);
	free(buf);
	printf("\n");
	if (done != len) {
		fprintf(stderr, "kdos clone: the source ended %llu bytes "
				"early\n", len - done);
		return -1;
	}
	return 0;
}

/* ── f3probe ───────────────────────────────────────────────────────────── */

/*
 * A counterfeit stick reports a capacity it does not have and silently wraps,
 * so the copy succeeds, the verify fails somewhere in the middle, and the
 * person is left thinking the image is broken. f3probe answers it directly.
 * `--destructive` because this is about to overwrite the device anyway, and
 * the non-destructive mode's save-and-restore is slower for nothing.
 */
static int counterfeit(const char *node)
{
	KbArgv a = {0};
	KbBuf out = {0};
	int rc, bad = 0;

	if (!kb_have_prog("f3probe"))
		return 0;		/* nothing to ask */
	kb_argv_add(&a, "f3probe");
	kb_argv_add(&a, "--destructive");
	kb_argv_add(&a, "--time-ops");
	kb_argv_add(&a, node);
	kb_argv_end(&a);
	rc = kb_run_capture_buf(&a, &out);
	if (rc == 0 && out.p) {
		/* f3probe names the verdict in a line of its own. "Good" is the
		 * only one that is not a refusal. */
		if (strstr(out.p, "counterfeit") ||
		    strstr(out.p, "Bad news") ||
		    strstr(out.p, "damaged"))
			bad = 1;
	}
	kb_buf_free(&out);
	return bad;
}

/* ── the medium this system booted from ────────────────────────────────── */

/*
 * The source, when nothing was named: the device mounted at /mnt/iso.
 *
 * `01_initramfs.sh` mounts the boot medium there and MOVES the mount across
 * `switch_root`, which is the only reason a booted KDOS can read the stick it
 * came from at all. An installed system has no /mnt/iso and must be given a
 * source; there is no image on its disk to copy.
 */
static int boot_medium(char *out, size_t n)
{
	char *data = kb_read_all("/proc/mounts", NULL);
	int found = 0;

	if (!data)
		return 0;
	for (char *p = data; *p && !found;) {
		char *nl = strchr(p, '\n');
		char dev[256] = "", mnt[256] = "";

		if (nl)
			*nl = '\0';
		if (sscanf(p, "%255s %255s", dev, mnt) == 2 &&
		    !strcmp(mnt, "/mnt/iso") && !strncmp(dev, "/dev/", 5)) {
			snprintf(out, n, "%s", dev);
			found = 1;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return found;
}

/* ── the command ───────────────────────────────────────────────────────── */

static int usage(void)
{
	printf("usage: kdos clone [<device>]\n"
	       "\n"
	       "  With no device, lists what may be written to. With one,\n"
	       "  copies this machine's boot medium onto it and verifies the\n"
	       "  copy by reading it back.\n"
	       "\n"
	       "  --source <path>   an image file or device to clone instead\n"
	       "  --extent          print the image's exact length and stop\n"
	       "  -y, --yes         do not ask before overwriting\n"
	       "  --no-probe        skip f3probe's counterfeit test\n"
	       "  --no-verify       skip the read-back (says what that costs)\n");
	return 0;
}

static int confirmed(const char *node, unsigned long long have)
{
	char buf[16];

	if (!isatty(0)) {
		fprintf(stderr, "kdos clone: %s holds %s and every byte of it "
				"will be destroyed — pass -y to mean it\n",
			node, kb_human_size(have));
		return 0;
	}
	printf("\n  %s holds %s. Everything on it will be destroyed.\n",
	       node, kb_human_size(have));
	printf("  Type the device name to confirm: ");
	fflush(stdout);
	if (!fgets(buf, sizeof(buf), stdin))
		return 0;
	buf[strcspn(buf, "\n")] = '\0';
	return !strcmp(buf, kb_basename(node));
}

int clone_main(int argc, char **argv)
{
	const char *target = NULL, *source = NULL;
	int yes = 0, probe = 1, verify = 1, extent_only = 0;
	char used[64][64];
	int nused;
	char medium[256];
	unsigned long long len;
	char whash[65], rhash[65];
	int in, out, rc = 1;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help"))
			return usage();
		else if (!strcmp(a, "-y") || !strcmp(a, "--yes"))
			yes = 1;
		else if (!strcmp(a, "--no-probe"))
			probe = 0;
		else if (!strcmp(a, "--no-verify"))
			verify = 0;
		else if (!strcmp(a, "--extent"))
			extent_only = 1;
		else if (!strcmp(a, "--source") && i + 1 < argc)
			source = argv[++i];
		else if (a[0] == '-') {
			fprintf(stderr, "kdos clone: unknown option '%s'\n", a);
			return 2;
		} else if (!target)
			target = a;
		else
			return usage();
	}

	/* The source, and its length, before anything is listed: a target is
	 * only eligible if it is big enough, and that is not knowable without
	 * the image. */
	if (!source) {
		if (!boot_medium(medium, sizeof(medium))) {
			fprintf(stderr, "kdos clone: this system did not boot "
				"from a KDOS medium — name one with "
				"--source <file.iso>\n");
			return 1;
		}
		source = medium;
	}
	in = open(source, O_RDONLY);
	if (in < 0) {
		fprintf(stderr, "kdos clone: %s: %s\n", source, strerror(errno));
		return 1;
	}
	len = img_extent(in);
	if (!len) {
		fprintf(stderr, "kdos clone: %s carries no ISO9660 volume and "
			"no GPT — it is not a KDOS medium\n", source);
		close(in);
		return 1;
	}

	/* The exact byte count, which is what a human size cannot carry and
	 * what the two-record rule above is actually about. */
	if (extent_only) {
		printf("%llu\n", len);
		close(in);
		return 0;
	}

	nused = in_use_disks(used, 64);

	if (!target) {
		struct cl_target t[CL_MAX_TARGETS];
		int n = list_targets(t, CL_MAX_TARGETS, used, nused);

		printf("source  %s  %s\n", source, kb_human_size(len));
		printf("\n");
		if (!n) {
			printf("  nothing to write to — plug in a stick.\n"
			       "  The medium this system booted from and every "
			       "disk it has mounted\n  are refused.\n");
			close(in);
			return 0;
		}
		printf("  %-12s %10s  %s\n", "DEVICE", "SIZE", "MODEL");
		for (int i = 0; i < n; i++)
			printf("  %-12s %10s  %s%s\n", t[i].node,
			       kb_human_size(t[i].bytes), t[i].model,
			       t[i].bytes < len ? "   (too small)" : "");
		printf("\n  kdos clone <device>\n");
		close(in);
		return 0;
	}

	/* Every refusal, before a byte is written. */
	{
		char disk[64];

		parent_disk(target, disk, sizeof(disk));
		for (int i = 0; i < nused; i++)
			if (!strcmp(used[i], disk)) {
				fprintf(stderr, "kdos clone: %s is in use by "
					"this system — it is the medium it "
					"booted from, or it has a filesystem "
					"mounted\n", target);
				close(in);
				return 1;
			}
		if (in_fstab(target)) {
			fprintf(stderr, "kdos clone: %s is named in "
				"/etc/fstab\n", target);
			close(in);
			return 1;
		}
	}

	out = open(target, O_RDWR);
	if (out < 0) {
		fprintf(stderr, "kdos clone: %s: %s%s\n", target,
			strerror(errno),
			errno == EACCES ? " — try sudo" : "");
		close(in);
		return 1;
	}
	{
		unsigned long long have = 0;

		if (ioctl(out, BLKGETSIZE64, &have) != 0 || have == 0) {
			fprintf(stderr, "kdos clone: %s is not a block "
				"device\n", target);
			goto done;
		}
		if (have < len) {
			fprintf(stderr, "kdos clone: %s holds %s and the image "
				"is %s\n", target, kb_human_size(have),
				kb_human_size(len));
			goto done;
		}
		printf("source  %s  %s\n", source, kb_human_size(len));
		printf("target  %s  %s\n", target, kb_human_size(have));
		if (!yes && !confirmed(target, have)) {
			fprintf(stderr, "kdos clone: not confirmed\n");
			goto done;
		}
	}

	if (probe) {
		if (!kb_have_prog("f3probe"))
			printf("\n  f3probe is not installed — the capacity "
			       "this device claims is untested\n");
		else {
			printf("\n  probing for a counterfeit capacity "
			       "(f3probe)...\n");
			if (counterfeit(target)) {
				fprintf(stderr, "kdos clone: f3probe says %s "
					"is counterfeit or damaged — it does "
					"not hold what it claims to\n", target);
				goto done;
			}
		}
	}

	printf("\n");
	if (copy_stream(in, out, len, whash) != 0)
		goto done;
	drop_cache(out);

	if (!verify) {
		printf("  not verified — --no-verify was given, so nothing has "
		       "read back what the flash stored\n");
		rc = 0;
		goto done;
	}
	if (lseek(out, 0, SEEK_SET) != 0) {
		fprintf(stderr, "kdos clone: cannot rewind %s\n", target);
		goto done;
	}
	if (hash_stream(out, len, rhash, "verify  ") != 0) {
		fprintf(stderr, "kdos clone: %s could not be read back\n",
			target);
		goto done;
	}
	if (strcmp(whash, rhash) != 0) {
		fprintf(stderr, "kdos clone: the copy does not match — %s was "
			"written and %s read back. The device did not store "
			"what it accepted.\n", whash, rhash);
		goto done;
	}
	printf("\n  %s\n  verified: %s\n", target, whash);
	rc = 0;
done:
	close(out);
	close(in);
	return rc;
}

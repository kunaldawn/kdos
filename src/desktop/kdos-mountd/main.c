/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-mountd / kdos-mount — the USB stick
 *
 *     $ kdos-mount list
 *     0  sdb1  KDOS       vfat    28.7G
 *     1  sdb2  backup     ext4   120.0G  /media/kdos/backup
 *     $ kdos-mount mount 0
 *     /media/kdos/KDOS
 *
 * Plugging a stick into this machine did NOTHING. Not "opened the wrong
 * program" — nothing at all: there is no udisks here, mounting is root's, and
 * the desktop is not root. That is the most visible gap on the KDE comparison
 * and it is the only one on it that genuinely needs a privileged daemon.
 *
 * THE CLIENT NEVER NAMES A PATH. It asks for an INDEX out of a list the daemon
 * itself published, and the daemon decides the device, the mountpoint and the
 * options. Every "just take a path and a mountpoint" design ends at
 * `mount /dev/sda2 /etc` from a shell as any user in wheel; there is nothing
 * here to aim, because there is no argument that means anything except a row
 * number the daemon wrote a moment ago.
 *
 * WHAT IS ELIGIBLE, and each clause is a refusal that matters:
 *
 *   - the device must be REMOVABLE or on USB (`/sys/block/<disk>/removable`,
 *     or a `usb` on the path to it). An internal disk is the admin's.
 *   - it must be a PARTITION or a whole disk with a filesystem, and it must
 *     have a filesystem this kernel can mount — probed by name from
 *     /proc/filesystems, so a kernel without `ntfs3` refuses rather than
 *     failing halfway.
 *   - it must not already be mounted anywhere, and it must not appear in
 *     /etc/fstab. An fstab entry is a decision somebody already made and this
 *     daemon does not get to second-guess it.
 *   - it must not be the medium THIS SYSTEM BOOTED FROM. Offering to unmount
 *     the live ISO is offering to kill the session.
 *
 * THE MOUNT IS `nosuid,nodev` ALWAYS and `noexec` by default. A stick is
 * somebody else's filesystem; a setuid root binary on one is a local root hole
 * that predates every other consideration. `noexec` is a default rather than a
 * law because running an AppImage off a stick is a real thing people do, and
 * `exec = yes` in /etc/kdos/mountd.conf is how you say you meant it.
 *
 * THE MOUNTPOINT IS THE DAEMON'S: /media/<user>/<label or device>, created
 * 0700 owned by the caller, and removed on unmount. A label with a slash or a
 * `..` in it is not a label — it is somebody's idea of a joke — so the name is
 * sanitised to [A-Za-z0-9._-] and truncated.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE	/* struct ucred */
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>	/* major(), minor() — the device check */
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kbase.h"

#define KM_SOCKET "/run/kdos-mountd.sock"
#define KM_GROUP "wheel"
#define KM_MAX 128
/* The largest second frame: a passphrase, or a typed device name. Long enough
 * for a real passphrase and short enough that a client cannot make this daemon
 * hold anything. */
#define KM_SECRET_MAX 512
#define KM_DEVS 32
#define KM_NAME 64

struct kmdev {
	/* 256, not 64: `/dev/sdb1` fits in a dozen bytes and the FIXTURE's dev
	 * root is a scratch directory a hundred characters deep — a truncated
	 * node opens nothing, probes nothing, and drops the device with no
	 * error anywhere. Found by the fixture reporting `0 eligible` for a
	 * stick that was plainly there. */
	char node[256];
	char kname[32];		/* sdb1                                   */
	/* The PARENT DISK this partition is on — `sdb` for `sdb1`, and its own
	 * name for a whole disk. A destructive verb is refused by the disk and
	 * not by the partition: a live medium's ESP is removable, probes as
	 * vfat, is unmounted and is in nobody's fstab, so every per-partition
	 * rule offers it and a format there destroys the running session. */
	char disk[32];
	char label[KM_NAME];
	char fstype[32];
	unsigned long long bytes;
	char mnt[256];		/* where it is mounted, or "" — /proc/mounts
				 * can hold a longer path than this daemon
				 * ever creates, which is why it is the wider
				 * of the two buffers */
};

static struct kmdev devs[KM_DEVS];
static int ndev;

/*
 * FIXTURE MODE, AND EVERY PATH OVERRIDE IS GATED ON IT.
 *
 * This daemon runs as root under `supervise`, and once it can spawn `mkfs` an
 * environment variable that moves its idea of `/dev` is a way to point a
 * format at any node on the machine. The overrides exist for the fixture and
 * for nothing else, so they are read only when `--fixture` set this — a
 * variable inherited from an init environment then names nothing.
 *
 * The SOCKET path is deliberately not gated: moving the socket is how the
 * self-test drives the daemon unprivileged, it grants no access the caller did
 * not already have to the directory it names, and the daemon refuses a
 * non-root caller on the real socket regardless.
 */
static int km_fixture;

static const char *sock_path(void)
{
	const char *p = getenv("KDOS_MOUNTD_SOCKET");
	return p && *p ? p : KM_SOCKET;
}

/* Overridable for the fixture, like the /sys, /dev and fstab roots. Without
 * it this one reading comes from the machine running the test rather than
 * from the recorded one, and any host whose own root is an overlay — a
 * container, for instance — makes the daemon refuse optical media that the
 * fixture says it should offer. */
static const char *mounts_path(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_MOUNTS") : NULL;
	return p && *p ? p : "/proc/mounts";
}

/* The /sys tree, overridable for the fixture — the same seam
 * `kdos stutter --fixture` and KDOS_PRIVACY_PROC use, and the only way the
 * eligibility rules get tested on a machine with no stick in it. */
static const char *sysroot(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_SYS") : NULL;
	return p && *p ? p : "/sys";
}

/* Overridable for the fixture, like the other roots: `format` is gated on a
 * key in this file, and a gate that cannot be opened in a test is a gate whose
 * refusal is the only half ever exercised. */
static const char *conf_path(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_CONF") : NULL;
	return p && *p ? p : "/etc/kdos/mountd.conf";
}

static const char *devroot(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_DEV") : NULL;
	return p && *p ? p : "/dev";
}

/* ── the allowed set ───────────────────────────────────────────────────── */

static bool uid_allowed(uid_t uid)
{
	/*
	 * FIXTURE MODE ADMITS ANYBODY, and grants nothing: it is reachable
	 * only from `--fixture-serve` on the command line, its paths are a
	 * scratch directory, and every child it would spawn is printed instead
	 * of run. The service script starts this daemon with no arguments, so
	 * there is no path from a running system into here.
	 */
	if (km_fixture)
		return true;
	if (uid == 0)
		return true;
	struct passwd *pw = getpwuid(uid);
	if (!pw || !pw->pw_name)
		return false;
	return kb_user_in_group(pw->pw_name, pw->pw_gid, KM_GROUP) != 0;
}

/* ── reading the machine ───────────────────────────────────────────────── */

static char *read_trim(const char *path)
{
	char *s = kb_read_all(path, NULL);
	if (!s)
		return NULL;
	s[strcspn(s, "\r\n")] = '\0';
	return s;
}

static int read_int(const char *path)
{
	char *s = read_trim(path);
	int v = s ? atoi(s) : 0;
	free(s);
	return v;
}

/*
 * Is this block device removable? `removable` on the DISK covers a card reader
 * and a USB stick; the `usb` component in the sysfs path covers an external
 * drive in a USB enclosure, which reports removable = 0 and is exactly as much
 * "somebody's else's disk" as a stick.
 */
static bool is_removable(const char *disk)
{
	char path[512];

	snprintf(path, sizeof(path), "%s/block/%s/removable", sysroot(), disk);
	if (read_int(path) == 1)
		return true;

	snprintf(path, sizeof(path), "%s/block/%s", sysroot(), disk);
	char real[1024];
	ssize_t n = readlink(path, real, sizeof(real) - 1);
	if (n <= 0)
		return false;
	real[n] = '\0';
	return strstr(real, "/usb") != NULL;
}

/* Whether the kernel can mount this at all, from /proc/filesystems — refusing
 * here beats failing inside mount() with an errno nobody can read. */
static bool fs_supported(const char *fstype)
{
	char *data = kb_read_all("/proc/filesystems", NULL);
	bool ok = false;

	if (!fstype || !*fstype)
		return false;
	if (!data)
		return true;		/* cannot tell: let mount(2) decide */
	for (char *p = data; *p && !ok;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		char *name = strrchr(p, '\t');
		name = name ? name + 1 : p;
		if (!strcmp(name, fstype))
			ok = true;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return ok;
}

/* Where a device is mounted right now, from /proc/mounts. */
static void find_mount(const char *node, char *out, size_t n)
{
	char *data = kb_read_all(mounts_path(), NULL);

	out[0] = '\0';
	if (!data)
		return;
	for (char *p = data; *p;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		char dev[256] = "", mnt[256] = "";
		if (sscanf(p, "%255s %255s", dev, mnt) == 2 &&
		    !strcmp(dev, node))
			snprintf(out, n, "%s", mnt);
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
}

/* Overridable for the fixture, like the /sys and /dev roots — the refusal an
 * fstab entry earns is a rule worth testing, and it cannot be tested against
 * the developer's own /etc/fstab. */
static const char *fstab_path(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_FSTAB") : NULL;
	return p && *p ? p : "/etc/fstab";
}

static bool in_fstab(const char *node, const char *label)
{
	char *data = kb_read_all(fstab_path(), NULL);
	bool found = false;

	if (!data)
		return false;
	for (char *p = data; *p && !found;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p && *p != '#') {
			if (strstr(p, node))
				found = true;
			if (label && *label) {
				char want[KM_NAME + 8];
				snprintf(want, sizeof(want), "LABEL=%s", label);
				if (strstr(p, want))
					found = true;
			}
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return found;
}

/*
 * Is this a LIVE session — root on an overlay, the shape 01_initramfs.sh
 * leaves behind?
 */
static bool root_is_overlay(void)
{
	char *data = kb_read_all(mounts_path(), NULL);
	bool live = false;

	if (!data)
		return false;
	for (char *p = data; *p && !live;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		char dev[256] = "", mnt[256] = "", type[64] = "";
		if (sscanf(p, "%255s %255s %63s", dev, mnt, type) == 3 &&
		    !strcmp(mnt, "/") && !strcmp(type, "overlay"))
			live = true;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return live;
}

/*
 * The medium this system booted from, so it is never offered.
 *
 * THE LIVE ISO CANNOT BE FOUND IN /proc/mounts AT ALL, and that is the trap
 * this function exists around. The initramfs mounts the ISO and the squashfs,
 * then `switch_root` MS_MOVEs the new root — so those mounts stay in the OLD
 * namespace and the running system's /proc/mounts has one line for the whole
 * arrangement: `overlay / overlay ... lowerdir=/mnt/system`, naming a path
 * that no longer exists. Measured on a booted ISO, where kdos-mountd cheerfully
 * offered `sr0 iso9660 8.8G` — the disc it was running from.
 *
 * So a live session refuses every ISO9660 medium. The cost is that a second
 * data CD cannot be mounted while running live; the alternative is a desktop
 * that offers to eject the operating system. An INSTALLED system has a real
 * root and offers optical media normally.
 */
static bool is_boot_medium(const char *node, const char *fstype)
{
	if (fstype && !strcmp(fstype, "iso9660") && root_is_overlay())
		return true;

	char *data = kb_read_all(mounts_path(), NULL);
	bool boot = false;

	if (!data)
		return false;
	for (char *p = data; *p && !boot;) {
		char *nl = strchr(p, '\n');
		if (nl)
			*nl = '\0';
		char dev[256] = "", mnt[256] = "";
		if (sscanf(p, "%255s %255s", dev, mnt) == 2 &&
		    !strcmp(dev, node) &&
		    (!strcmp(mnt, "/") || !strcmp(mnt, "/mnt/iso") ||
		     !strcmp(mnt, "/boot")))
			boot = true;
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return boot;
}

/*
 * blkid, without blkid: the LABEL and TYPE of a filesystem, read from the
 * superblock. Three formats cover everything a stick is formatted with, and a
 * fourth would be a library.
 */
static void probe_fs(const char *node, char *label, size_t nlabel, char *type,
		     size_t ntype)
{
	unsigned char buf[4096];
	int fd;

	label[0] = type[0] = '\0';
	fd = open(node, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;

	/*
	 * LUKS FIRST, AND THAT ORDER IS THE POINT. A LUKS container holds
	 * whatever bytes were on the device before it, so a header written
	 * over an old ext4 still carries that superblock at 0x438 — probe for
	 * a filesystem first and an encrypted volume reads as the plaintext it
	 * used to be, which is a row offering to mount ciphertext.
	 *
	 * `LUKS\xba\xbe` at 0, the version big-endian at 6, and LUKS2 puts a
	 * 48-byte label at 24. Version 1 has no label there and gets none.
	 */
	if (pread(fd, buf, 64, 0) == 64 && !memcmp(buf, "LUKS\xba\xbe", 6)) {
		unsigned ver = ((unsigned)buf[6] << 8) | buf[7];

		snprintf(type, ntype, "crypto_LUKS");
		if (ver == 2)
			snprintf(label, nlabel, "%.48s", (char *)buf + 24);
		close(fd);
		return;
	}

	/* ext2/3/4: magic 0xEF53 at 0x438, label at 0x478. */
	if (pread(fd, buf, sizeof(buf), 1024) == (ssize_t)sizeof(buf)) {
		if (buf[0x38] == 0x53 && buf[0x39] == 0xEF) {
			snprintf(type, ntype, "ext4");
			snprintf(label, nlabel, "%.16s", (char *)buf + 0x78);
		}
	}
	/* FAT: "FAT32   " at 0x52, "FAT" at 0x36; label at 0x47 / 0x2b. */
	if (!type[0] && pread(fd, buf, 512, 0) == 512) {
		if (!memcmp(buf + 0x52, "FAT32", 5)) {
			snprintf(type, ntype, "vfat");
			snprintf(label, nlabel, "%.11s", (char *)buf + 0x47);
		} else if (!memcmp(buf + 0x36, "FAT", 3)) {
			snprintf(type, ntype, "vfat");
			snprintf(label, nlabel, "%.11s", (char *)buf + 0x2b);
		} else if (!memcmp(buf + 3, "NTFS    ", 8)) {
			snprintf(type, ntype, "ntfs3");
		} else if (!memcmp(buf + 3, "EXFAT   ", 8)) {
			snprintf(type, ntype, "exfat");
		}
	}
	/* ISO9660: "CD001" at 0x8001. */
	if (!type[0]) {
		unsigned char iso[8];
		if (pread(fd, iso, sizeof(iso), 0x8000) == (ssize_t)sizeof(iso) &&
		    !memcmp(iso + 1, "CD001", 5))
			snprintf(type, ntype, "iso9660");
	}
	/* btrfs: "_BHRfS_M" at 0x10040, label at 0x102b8. */
	if (!type[0] && pread(fd, buf, sizeof(buf), 0x10000) ==
				 (ssize_t)sizeof(buf)) {
		if (!memcmp(buf + 0x40, "_BHRfS_M", 8)) {
			snprintf(type, ntype, "btrfs");
			snprintf(label, nlabel, "%.32s", (char *)buf + 0x2b8);
		}
	}
	close(fd);

	/* A FAT label is space-padded and a blank one is not a label. */
	for (int i = (int)strlen(label) - 1; i >= 0; i--) {
		if (label[i] == ' ')
			label[i] = '\0';
		else
			break;
	}
}

/*
 * IS THIS WHOLE DISK THE MEDIUM THE SESSION IS RUNNING FROM?
 *
 * `is_boot_medium()` answers for one node and cannot see a sibling. A live USB
 * carries an iso9660 partition AND a vfat ESP, and only the first is refused
 * by that rule — so the second is offered, and destroying it destroys the
 * medium. Any disk with an iso9660 partition on it is the boot disk, whole.
 *
 * ONLY IN A LIVE SESSION. An installed system has a real root, and a data DVD
 * in the drive is an ordinary thing to be handed.
 */
static bool km_disk_is_boot(const char *disk)
{
	char path[512];
	int pn = 0;
	bool boot = false;
	char **parts;

	if (!disk || !*disk || !root_is_overlay())
		return false;
	snprintf(path, sizeof(path), "%s/block/%s", sysroot(), disk);
	parts = kb_listdir(path, &pn);
	if (!parts)
		return false;
	for (int k = 0; k < pn && !boot; k++) {
		char node[256], label[KM_NAME], fstype[32];

		if (strncmp(parts[k], disk, strlen(disk)))
			continue;
		snprintf(path, sizeof(path), "%s/block/%s/%s/partition",
			 sysroot(), disk, parts[k]);
		if (access(path, F_OK) != 0)
			continue;
		snprintf(node, sizeof(node), "%s/%s", devroot(), parts[k]);
		probe_fs(node, label, sizeof(label), fstype, sizeof(fstype));
		if (!strcmp(fstype, "iso9660"))
			boot = true;
	}
	kb_strv_free(parts);
	return boot;
}

static void scan(void)
{
	char path[512];
	int n = 0;
	char **disks;

	ndev = 0;
	snprintf(path, sizeof(path), "%s/block", sysroot());
	disks = kb_listdir(path, &n);
	if (!disks)
		return;

	for (int i = 0; i < n && ndev < KM_DEVS; i++) {
		const char *disk = disks[i];

		/* Not a loop, not a ram disk, not a device-mapper node: none of
		 * them is a thing somebody plugged in. */
		if (!strncmp(disk, "loop", 4) || !strncmp(disk, "ram", 3) ||
		    !strncmp(disk, "dm-", 3) || !strncmp(disk, "zram", 4))
			continue;
		if (!is_removable(disk))
			continue;

		/* The partitions of the disk, and the disk itself when it has
		 * none — a stick formatted without a partition table is a
		 * normal thing to be handed. */
		int pn = 0;
		snprintf(path, sizeof(path), "%s/block/%s", sysroot(), disk);
		char **parts = kb_listdir(path, &pn);
		int added = 0;

		for (int k = 0; k < pn && ndev < KM_DEVS; k++) {
			if (strncmp(parts[k], disk, strlen(disk)))
				continue;
			snprintf(path, sizeof(path), "%s/block/%s/%s/partition",
				 sysroot(), disk, parts[k]);
			if (access(path, F_OK) != 0)
				continue;

			struct kmdev *d = &devs[ndev];
			memset(d, 0, sizeof(*d));
			snprintf(d->kname, sizeof(d->kname), "%s", parts[k]);
			snprintf(d->disk, sizeof(d->disk), "%s", disk);
			snprintf(d->node, sizeof(d->node), "%s/%s", devroot(),
				 parts[k]);
			snprintf(path, sizeof(path), "%s/block/%s/%s/size",
				 sysroot(), disk, parts[k]);
			char *sz = read_trim(path);
			d->bytes = sz ? strtoull(sz, NULL, 10) * 512ULL : 0;
			free(sz);
			probe_fs(d->node, d->label, sizeof(d->label), d->fstype,
				 sizeof(d->fstype));
			find_mount(d->node, d->mnt, sizeof(d->mnt));
			if (d->fstype[0] && !is_boot_medium(d->node, d->fstype) &&
			    !in_fstab(d->node, d->label)) {
				ndev++;
				added++;
			}
		}
		kb_strv_free(parts);

		if (!added && ndev < KM_DEVS) {
			struct kmdev *d = &devs[ndev];
			memset(d, 0, sizeof(*d));
			snprintf(d->kname, sizeof(d->kname), "%s", disk);
			snprintf(d->disk, sizeof(d->disk), "%s", disk);
			snprintf(d->node, sizeof(d->node), "%s/%s", devroot(),
				 disk);
			snprintf(path, sizeof(path), "%s/block/%s/size",
				 sysroot(), disk);
			char *sz = read_trim(path);
			d->bytes = sz ? strtoull(sz, NULL, 10) * 512ULL : 0;
			free(sz);
			probe_fs(d->node, d->label, sizeof(d->label), d->fstype,
				 sizeof(d->fstype));
			find_mount(d->node, d->mnt, sizeof(d->mnt));
			if (d->fstype[0] && !is_boot_medium(d->node, d->fstype) &&
			    !in_fstab(d->node, d->label))
				ndev++;
		}
	}
	kb_strv_free(disks);
}

/* ── the allowlist ─────────────────────────────────────────────────────
 *
 * EVERY TOKEN IS CHECKED BEFORE IT MEANS ANYTHING, and the token COUNT is
 * fixed per verb. The dispatch this replaces read an index with `atoi(buf + 6)`
 * and threw the rest of the line away, so `mount 0 anything at all` was a
 * well-formed mount — harmless while the daemon only called `mount(2)`, and
 * not harmless at all now that a verb can reach `mkfs`.
 * ────────────────────────────────────────────────────────────────────── */

/* 1 to 3 digits and nothing else, and inside the list the daemon just built.
 * `atoi` on a client's string cannot fail, which is the problem: it answers 0
 * for every word that is not a number. */
static int km_index(const char *tok)
{
	size_t len = tok ? strlen(tok) : 0;
	int v = 0;

	if (len < 1 || len > 3)
		return -1;
	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char)tok[i]))
			return -1;
		v = v * 10 + (tok[i] - '0');
	}
	return v < ndev ? v : -1;
}

/* The byte count of a second frame: 1 to KM_SECRET_MAX, decimal, nothing else. */
static int km_count(const char *tok)
{
	size_t len = tok ? strlen(tok) : 0;
	int v = 0;

	if (len < 1 || len > 4)
		return -1;
	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char)tok[i]))
			return -1;
		v = v * 10 + (tok[i] - '0');
	}
	return (v >= 1 && v <= KM_SECRET_MAX) ? v : -1;
}

/*
 * THE ONE PLACE THIS DAEMON SPAWNS A CHILD, and before J.1 there was none —
 * it mounted with `mount(2)` and unmounted with `umount2()`. That is the real
 * change here, larger than the three verbs.
 *
 * ABSOLUTE PATHS ONLY. `kb_run_feed` execs through `execvp`, and a root daemon
 * that resolved a program name through an inherited PATH would run whatever
 * came first on it.
 *
 * IN FIXTURE MODE NOTHING RUNS. The argv is printed instead, which is what
 * lets the refusals be asserted without a disk to lose.
 */
static int km_exec(const KbArgv *a, const char *feed, size_t nfeed)
{
	if (km_fixture) {
		for (int i = 0; i < a->n && a->v[i]; i++)
			printf("%s%s", i ? " " : "exec ", a->v[i]);
		printf("\n");
		/* Flushed, because the harness reads this after killing the
		 * daemon and a buffered line dies with it. */
		fflush(stdout);
		return 0;
	}
	return feed ? kb_run_feed(a, feed, nfeed) : kb_run(a);
}

/*
 * THE DEVICE IS WHAT THE DAEMON SAYS IT IS, re-derived at the moment of use.
 *
 * Between the scan that built the row and the syscall that acts on it, the
 * path can become a symlink or a different device. O_NOFOLLOW defeats the
 * first; requiring a block device whose `st_rdev` matches the one `/sys`
 * recorded defeats the second. Skipped under the fixture, whose "devices" are
 * ordinary files.
 */
static bool km_node_is(const struct kmdev *d)
{
	char path[512], *txt;
	struct stat st;
	unsigned maj = 0, min = 0;
	int fd, ok = 0;

	if (km_fixture)
		return true;
	snprintf(path, sizeof(path), "%s/block/%s/%s/dev", sysroot(), d->disk,
		 d->kname);
	if (access(path, F_OK) != 0)
		snprintf(path, sizeof(path), "%s/block/%s/dev", sysroot(),
			 d->disk);
	txt = read_trim(path);
	if (!txt || sscanf(txt, "%u:%u", &maj, &min) != 2) {
		free(txt);
		return false;
	}
	free(txt);
	fd = open(d->node, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return false;
	if (fstat(fd, &st) == 0 && S_ISBLK(st.st_mode) &&
	    major(st.st_rdev) == maj && minor(st.st_rdev) == min)
		ok = 1;
	close(fd);
	return ok != 0;
}

/* ── mounting ──────────────────────────────────────────────────────────── */

/* [A-Za-z0-9._-] and nothing else. A label is whatever was written into a
 * superblock by somebody else's computer, and it becomes a PATH component. */
static void sanitise(const char *in, char *out, size_t n)
{
	size_t k = 0;

	for (const char *p = in; *p && k + 1 < n; p++) {
		unsigned char c = (unsigned char)*p;
		if (isalnum(c) || c == '.' || c == '_' || c == '-')
			out[k++] = (char)c;
	}
	out[k] = '\0';
	/* A name of dots is not a name. */
	if (!out[0] || !strcmp(out, ".") || !strcmp(out, ".."))
		out[0] = '\0';
}

static bool exec_allowed(void)
{
	char *s = kb_read_all(conf_path(), NULL);
	bool yes = false;

	if (!s)
		return false;
	yes = strstr(s, "exec = yes") != NULL ||
	      strstr(s, "exec=yes") != NULL;
	free(s);
	return yes;
}

/*
 * A DESTRUCTIVE VERB IS OPT-IN ON A SHIPPED IMAGE, the same argument `noexec`
 * already won. `format` writes a filesystem over whatever was there; a desktop
 * that offers that by default on every machine it is installed on is one where
 * a mis-click is unrecoverable.
 */
static bool format_allowed(void)
{
	char *s = kb_read_all(conf_path(), NULL);
	bool yes = false;

	if (!s)
		return false;
	yes = strstr(s, "format = yes") != NULL ||
	      strstr(s, "format=yes") != NULL;
	free(s);
	return yes;
}

/*
 * WHAT NO VERB MAY TOUCH. Checked once, here, so a verb added later cannot
 * forget one of them: the medium the session booted from, whole; anything
 * currently mounted; and anything whose node is no longer the device the scan
 * recorded.
 */
static int km_writable(int idx, char *out, size_t nout)
{
	struct kmdev *d;

	if (idx < 0 || idx >= ndev) {
		snprintf(out, nout, "no such device");
		return -1;
	}
	d = &devs[idx];
	if (d->mnt[0]) {
		snprintf(out, nout, "unmount it first");
		return -1;
	}
	if (km_disk_is_boot(d->disk)) {
		snprintf(out, nout, "that is the medium this session booted "
				    "from");
		return -1;
	}
	if (!km_node_is(d)) {
		snprintf(out, nout, "%s is not the device it was", d->node);
		return -1;
	}
	return 0;
}

static int do_mount(int idx, uid_t uid, char *out, size_t nout)
{
	struct passwd *pw = getpwuid(uid);
	/* `dir` is `parent` plus a separator plus a sanitised label, so it has
	 * to be able to hold both without gcc having to guess. */
	char base[KM_NAME], parent[128], dir[192];

	if (idx < 0 || idx >= ndev)
		return -1;
	struct kmdev *d = &devs[idx];
	if (d->mnt[0]) {
		snprintf(out, nout, "%s", d->mnt);
		return 0;		/* already there: not an error */
	}
	if (!fs_supported(d->fstype)) {
		snprintf(out, nout, "this kernel cannot mount %s", d->fstype);
		return -1;
	}

	sanitise(d->label[0] ? d->label : d->kname, base, sizeof(base));
	if (!base[0])
		snprintf(base, sizeof(base), "disk");

	snprintf(parent, sizeof(parent), "/media/%s",
		 pw && pw->pw_name ? pw->pw_name : "user");
	snprintf(dir, sizeof(dir), "%s/%s", parent, base);
	if (mkdir("/media", 0755) != 0 && errno != EEXIST)
		return -1;
	if (mkdir(parent, 0755) != 0 && errno != EEXIST)
		return -1;
	if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
		snprintf(out, nout, "cannot create %s: %s", dir,
			 strerror(errno));
		return -1;
	}

	unsigned long flags = MS_NOSUID | MS_NODEV;
	if (!exec_allowed())
		flags |= MS_NOEXEC;

	/*
	 * FAT and NTFS have no ownership of their own, so the mount has to be
	 * told whose it is — without this a stick mounts owned by root and the
	 * user who asked for it cannot write to it.
	 */
	char opts[128] = "";
	if (!strcmp(d->fstype, "vfat") || !strcmp(d->fstype, "exfat") ||
	    !strcmp(d->fstype, "ntfs3"))
		snprintf(opts, sizeof(opts), "uid=%u,gid=%u,fmask=0117,dmask=0007",
			 (unsigned)uid, pw ? (unsigned)pw->pw_gid : 0u);

	if (mount(d->node, dir, d->fstype, flags, opts[0] ? opts : NULL) != 0) {
		snprintf(out, nout, "mount %s: %s", d->node, strerror(errno));
		rmdir(dir);
		return -1;
	}
	/* A native filesystem keeps its own ownership; the MOUNTPOINT is still
	 * made the caller's so an empty stick is writable. */
	if (!opts[0] && pw)
		(void)!chown(dir, uid, pw->pw_gid);

	snprintf(d->mnt, sizeof(d->mnt), "%s", dir);
	snprintf(out, nout, "%s", dir);
	return 0;
}

static int do_unmount(int idx, char *out, size_t nout)
{
	if (idx < 0 || idx >= ndev)
		return -1;
	struct kmdev *d = &devs[idx];
	if (!d->mnt[0]) {
		snprintf(out, nout, "not mounted");
		return -1;
	}
	sync();
	if (umount2(d->mnt, 0) != 0) {
		/*
		 * EBUSY is the everyday answer and "busy" alone is useless.
		 * The daemon does not walk /proc to name the holder — that is
		 * the FRONT END's job and it can do it unprivileged — but it
		 * does say which mountpoint, so there is something to walk.
		 */
		snprintf(out, nout, "unmount %s: %s", d->mnt, strerror(errno));
		return -1;
	}
	/* Only the directory this daemon made, and only when it is empty:
	 * rmdir on a non-empty directory fails, which is the guard. */
	rmdir(d->mnt);
	snprintf(out, nout, "%s", d->mnt);
	d->mnt[0] = '\0';
	return 0;
}

/* ── the protocol ──────────────────────────────────────────────────────── */

/*
 * ── THE PRIVILEGED VERBS ────────────────────────────────────────────────
 *
 * Every one of them goes through km_writable() first, so the medium the
 * session booted from, anything mounted, and anything whose node has changed
 * under the daemon are refused in ONE place rather than three.
 */

/* Eject. Optical media eject their own node; a stick ejects the PARENT DISK,
 * because a start-stop on one partition of a stick means nothing to the
 * hardware and leaves the device powered. */
static int do_eject(int idx, char *out, size_t nout)
{
	struct kmdev *d;
	char node[300];
	KbArgv a = { 0 };

	if (km_writable(idx, out, nout) != 0)
		return -1;
	d = &devs[idx];
	if (!strcmp(d->fstype, "iso9660"))
		snprintf(node, sizeof(node), "%s", d->node);
	else
		snprintf(node, sizeof(node), "%s/%s", devroot(), d->disk);

	kb_argv_add(&a, "/usr/bin/eject");
	/* `--` because the node is a path this daemon built from a kernel name,
	 * and a leading dash in one would otherwise be read as an option. */
	kb_argv_add(&a, "--");
	kb_argv_add(&a, node);
	kb_argv_end(&a);
	if (km_exec(&a, NULL, 0) != 0) {
		snprintf(out, nout, "eject refused by the device");
		return -1;
	}
	snprintf(out, nout, "%s", d->kname);
	return 0;
}

/*
 * THE MAPPER NAME IS THE DAEMON'S, never the client's. `kdos-<kname>` is
 * derived from the row, so a client cannot ask for a mapping named anything
 * else — and `close` finds the same name from the same row without being told
 * it.
 */
static void km_mapname(const struct kmdev *d, char *out, size_t n)
{
	snprintf(out, n, "kdos-%s", d->kname);
}

static int do_unlock(int idx, const char *pass, size_t npass, char *out,
		     size_t nout)
{
	struct kmdev *d;
	char map[64], mnode[300];
	KbArgv a = { 0 };

	if (km_writable(idx, out, nout) != 0)
		return -1;
	d = &devs[idx];
	if (strcmp(d->fstype, "crypto_LUKS")) {
		snprintf(out, nout, "not an encrypted volume");
		return -1;
	}
	km_mapname(d, map, sizeof(map));
	snprintf(mnode, sizeof(mnode), "%s/mapper/%s", devroot(), map);
	/* Already open is not an error, the shape do_mount keeps: a person who
	 * pressed it twice asked for the state it is now in. */
	if (access(mnode, F_OK) == 0) {
		snprintf(out, nout, "%s", map);
		return 0;
	}

	kb_argv_add(&a, "/usr/sbin/cryptsetup");
	kb_argv_add(&a, "open");
	/* The passphrase arrives on the child's STDIN and never in argv:
	 * /proc/<pid>/cmdline is world-readable for the life of the process. */
	kb_argv_add(&a, "--key-file=-");
	kb_argv_add(&a, "--");
	kb_argv_add(&a, d->node);
	kb_argv_add(&a, map);
	kb_argv_end(&a);
	if (km_exec(&a, pass, npass) != 0) {
		/* One message for a wrong passphrase and for a header this
		 * build of cryptsetup will not open: telling the two apart is
		 * telling somebody which of their guesses was closer. */
		snprintf(out, nout, "could not unlock");
		return -1;
	}
	snprintf(out, nout, "%s", map);
	return 0;
}

static int do_close(int idx, char *out, size_t nout)
{
	struct kmdev *d;
	char map[64];
	KbArgv a = { 0 };

	if (idx < 0 || idx >= ndev) {
		snprintf(out, nout, "no such device");
		return -1;
	}
	d = &devs[idx];
	km_mapname(d, map, sizeof(map));

	kb_argv_add(&a, "/usr/sbin/cryptsetup");
	kb_argv_add(&a, "close");
	kb_argv_add(&a, "--");
	kb_argv_add(&a, map);
	kb_argv_end(&a);
	if (km_exec(&a, NULL, 0) != 0) {
		snprintf(out, nout, "could not close %s", map);
		return -1;
	}
	snprintf(out, nout, "%s", map);
	return 0;
}

/*
 * FORMAT. The confirmation is the device's own kernel name, typed by the
 * person and compared against the string THIS DAEMON put in the list — not a
 * flag, not a hash, not the word yes. A client cannot send a confirmation it
 * was not shown, and a surface cannot accidentally confirm on somebody's
 * behalf, because the only way to produce the bytes is to have read the row.
 */
static int do_format(int idx, const char *fstype, const char *confirm,
		     size_t nconfirm, char *out, size_t nout)
{
	static const struct {
		const char *name, *prog, *flag, *labelopt;
	} FS[] = {
		{ "ext4",  "/usr/sbin/mkfs.ext4",  "-F", "-L" },
		{ "btrfs", "/usr/bin/mkfs.btrfs",  "-f", "-L" },
		{ "vfat",  "/usr/sbin/mkfs.vfat",  "-I", "-n" },
		{ "exfat", "/usr/sbin/mkfs.exfat", NULL, "-n" },
	};
	struct kmdev *d;
	KbArgv a = { 0 };
	int f = -1;

	if (!format_allowed()) {
		snprintf(out, nout, "format is off; set `format = yes` in "
				    "/etc/kdos/mountd.conf");
		return -1;
	}
	for (int i = 0; i < (int)(sizeof(FS) / sizeof(FS[0])); i++)
		if (!strcmp(fstype, FS[i].name))
			f = i;
	if (f < 0) {
		snprintf(out, nout, "unknown filesystem");
		return -1;
	}
	if (km_writable(idx, out, nout) != 0)
		return -1;
	d = &devs[idx];

	/* An explicit length equality and memcmp, not a compare bounded by a
	 * length the client chose: a confirmation of "sd" must not match the
	 * row "sdb1". */
	if (nconfirm != strlen(d->kname) ||
	    memcmp(confirm, d->kname, nconfirm) != 0) {
		/* The precision is the field's own width: without it the
		 * compiler cannot bound the copy and warns, because the
		 * length comparison above puts kname beyond its range
		 * analysis. */
		snprintf(out, nout, "type %.31s to confirm", d->kname);
		return -1;
	}

	kb_argv_add(&a, FS[f].prog);
	if (FS[f].flag)
		kb_argv_add(&a, FS[f].flag);
	kb_argv_add(&a, FS[f].labelopt);
	kb_argv_add(&a, "KDOS");
	kb_argv_add(&a, d->node);
	kb_argv_end(&a);
	if (km_exec(&a, NULL, 0) != 0) {
		snprintf(out, nout, "mkfs failed");
		return -1;
	}
	snprintf(out, nout, "%s", d->kname);
	return 0;
}

static void reply_list(int c)
{
	char line[512];

	for (int i = 0; i < ndev; i++) {
		const struct kmdev *d = &devs[i];
		double gb = (double)d->bytes / (1024.0 * 1024.0 * 1024.0);
		int n = snprintf(line, sizeof(line), "%d\t%s\t%s\t%s\t%.1fG\t%s\n",
				 i, d->kname, d->label[0] ? d->label : "-",
				 d->fstype, gb, d->mnt[0] ? d->mnt : "-");
		(void)!write(c, line, (size_t)n);
	}
	(void)!write(c, "ok\n", 3);
}

static int serve(void)
{
	const char *path = sock_path();

	if (geteuid() != 0 && !strcmp(path, KM_SOCKET)) {
		fprintf(stderr, "kdos-mountd: must run as root\n");
		return 1;
	}

	int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (srv < 0) {
		fprintf(stderr, "kdos-mountd: socket: %s\n", strerror(errno));
		return 1;
	}
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	unlink(path);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-mountd: bind %s: %s\n", path,
			strerror(errno));
		close(srv);
		return 1;
	}
	/* 0666 with SO_PEERCRED as the real gate — kdos-powerd's rule, and the
	 * reason is the same: a mode that LOOKED like the authorisation invites
	 * somebody to weaken the credential check. */
	chmod(path, 0666);
	if (listen(srv, 8) < 0) {
		close(srv);
		return 1;
	}

	for (;;) {
		int c = accept(srv, NULL, NULL);
		if (c < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		struct ucred cred = {0};
		socklen_t len = sizeof(cred);
		if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0 ||
		    !uid_allowed(cred.uid)) {
			fprintf(stderr, "kdos-mountd: refused uid %u (not root "
					"and not in %s)\n",
				(unsigned)cred.uid, KM_GROUP);
			(void)!write(c, "err not permitted\n", 18);
			close(c);
			continue;
		}

		/*
		 * ── TWO FRAMES ─────────────────────────────────────────
		 *
		 * Frame 1 is one line: a verb and up to three tokens.
		 * Frame 2 exists only for `unlock` and `format` and is the
		 * exact byte count frame 1 declared — a passphrase or a typed
		 * device name. It is a SEPARATE FRAME so a secret is never a
		 * token: a tokeniser splits on spaces, and a passphrase may
		 * contain them.
		 *
		 * The first read may already hold some of frame 2, so what
		 * follows the newline is kept rather than discarded.
		 */
		char buf[KM_MAX + KM_SECRET_MAX + 2] = {0};
		ssize_t n = read(c, buf, sizeof(buf) - 1);
		if (n <= 0) {
			close(c);
			continue;
		}
		buf[n] = '\0';

		char *nl = memchr(buf, '\n', (size_t)n);
		size_t linelen = nl ? (size_t)(nl - buf) : (size_t)n;
		size_t have = nl ? (size_t)n - linelen - 1 : 0;
		char line[KM_MAX] = {0};

		if (linelen >= sizeof(line)) {
			(void)!write(c, "err too long\n", 13);
			close(c);
			continue;
		}
		memcpy(line, buf, linelen);
		line[strcspn(line, "\r")] = '\0';

		/* At most four tokens, and the count is fixed per verb below.
		 * A trailing token nobody named is a request this daemon does
		 * not understand, not one it silently ignores. */
		char *tok[5] = {0};
		int ntok = 0;
		for (char *sp = NULL, *t = strtok_r(line, " \t", &sp);
		     t && ntok < 5; t = strtok_r(NULL, " \t", &sp))
			tok[ntok++] = t;

		/* The list is re-read on EVERY request, not cached: a stick
		 * pulled out between two requests must not still be offered,
		 * and the scan is a handful of file reads. */
		scan();

		char msg[512] = "";
		const char *verb = ntok ? tok[0] : "";
		int idx;

		if (ntok == 1 && !strcmp(verb, "ping")) {
			(void)!write(c, "ok\n", 3);
		} else if (ntok == 1 && !strcmp(verb, "list")) {
			reply_list(c);
		} else if (ntok == 2 && !strcmp(verb, "mount")) {
			idx = km_index(tok[1]);
			if (idx >= 0 && do_mount(idx, cred.uid, msg,
						 sizeof(msg)) == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "no such device");
		} else if (ntok == 2 && !strcmp(verb, "unmount")) {
			idx = km_index(tok[1]);
			if (idx >= 0 && do_unmount(idx, msg, sizeof(msg)) == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "no such device");
		} else if (ntok == 2 && !strcmp(verb, "eject")) {
			idx = km_index(tok[1]);
			if (idx >= 0 && do_eject(idx, msg, sizeof(msg)) == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "no such device");
		} else if (ntok == 2 && !strcmp(verb, "close")) {
			idx = km_index(tok[1]);
			if (idx >= 0 && do_close(idx, msg, sizeof(msg)) == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "no such device");
		} else if ((ntok == 3 && !strcmp(verb, "unlock")) ||
			   (ntok == 4 && !strcmp(verb, "format"))) {
			int want = km_count(tok[ntok - 1]);

			idx = km_index(tok[1]);
			if (idx < 0 || want < 0) {
				(void)!write(c, "err bad request\n", 16);
				close(c);
				continue;
			}
			/*
			 * ONE BUFFER, AND EVERY EXIT WIPES IT. A passphrase
			 * that outlived the request would sit in a root
			 * daemon's heap for the life of the session.
			 */
			static char secret[KM_SECRET_MAX + 1];
			size_t need = (size_t)want;
			size_t got = have < need ? have : need;

			memcpy(secret, buf + linelen + 1, got);
			/* The room left is computed against the BUFFER as well
			 * as against the frame. Both bounds hold — km_count()
			 * already refused anything over KM_SECRET_MAX — but a
			 * read bounded only by a value the compiler cannot
			 * follow is one it must assume the worst about. */
			while (got < need && got < sizeof(secret) - 1) {
				size_t room = need - got;
				ssize_t r;

				if (room > sizeof(secret) - 1 - got)
					room = sizeof(secret) - 1 - got;
				r = read(c, secret + got, room);
				if (r <= 0)
					break;
				got += (size_t)r;
			}
			if (got != need) {
				explicit_bzero(secret, sizeof(secret));
				(void)!write(c, "err short frame\n", 16);
				close(c);
				continue;
			}
			int rc;

			if (ntok == 3)
				rc = do_unlock(idx, secret, got, msg,
					       sizeof(msg));
			else
				rc = do_format(idx, tok[2], secret, got, msg,
					       sizeof(msg));
			explicit_bzero(secret, sizeof(secret));
			if (rc == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "refused");
		} else {
			(void)!write(c, "err unknown command\n", 20);
		}
		close(c);
	}
	close(srv);
	unlink(path);
	return 1;
}

/* ── the client ────────────────────────────────────────────────────────── */

static int ask(const char *word)
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	char buf[4096];
	ssize_t n;

	if (fd < 0)
		return 2;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sock_path());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "kdos-mount: no kdos-mountd on %s (%s)\n",
			sock_path(), strerror(errno));
		close(fd);
		return 2;
	}
	dprintf(fd, "%s\n", word);
	while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
		buf[n] = '\0';
		fputs(buf, stdout);
	}
	close(fd);
	return 0;
}

static int usage(void)
{
	fprintf(stderr,
		"usage: kdos-mount list\n"
		"       kdos-mount mount <index>\n"
		"       kdos-mount unmount <index>\n"
		"       kdos-mount ping\n"
		"\nThe index is a row from `list`. There is no form that takes\n"
		"a device or a mountpoint: the daemon decides both.\n");
	return 2;
}

int main(int argc, char **argv)
{
	const char *self = strrchr(argv[0], '/');

	self = self ? self + 1 : argv[0];
	kb_set_progname(self);

	if (!strcmp(self, "kdos-mountd")) {
		if (argc > 2 && !strcmp(argv[1], "--fixture")) {
			/*
			 * What WOULD be offered, and nothing is mounted. The
			 * only way selection logic this consequential gets
			 * tested — the seam `kdos stutter --fixture` and
			 * `kdos-oomd --fixture` already use.
			 */
			km_fixture = 1;
			setenv("KDOS_MOUNTD_SYS", argv[2], 1);
			if (argc > 3)
				setenv("KDOS_MOUNTD_DEV", argv[3], 1);
			scan();
			for (int i = 0; i < ndev; i++)
				printf("%d\t%s\t%s\t%s\t%llu\n", i,
				       devs[i].kname,
				       devs[i].label[0] ? devs[i].label : "-",
				       devs[i].fstype, devs[i].bytes);
			printf("%d eligible\n", ndev);
			return 0;
		}
		/*
		 * THE SAME FIXTURE, SERVING. `--fixture` prints the list and
		 * returns, which cannot exercise a VERB — and the verbs are
		 * the half where a refusal matters. This serves on
		 * $KDOS_MOUNTD_SOCKET with the fixture's roots and with
		 * km_exec printing instead of running, so `format` can be
		 * driven at a device that must be refused without a disk to
		 * lose. It is as safe as `--fixture` for the same reason:
		 * nothing it decides to do is done.
		 */
		if (argc > 2 && !strcmp(argv[1], "--fixture-serve")) {
			km_fixture = 1;
			setenv("KDOS_MOUNTD_SYS", argv[2], 1);
			if (argc > 3)
				setenv("KDOS_MOUNTD_DEV", argv[3], 1);
			return serve();
		}
		if (argc > 1) {
			fprintf(stderr, "usage: kdos-mountd [--fixture SYS "
					"[DEV]] [--fixture-serve SYS [DEV]]\n");
			return 2;
		}
		signal(SIGPIPE, SIG_IGN);
		return serve();
	}

	if (argc < 2)
		return usage();
	if (!strcmp(argv[1], "list") || !strcmp(argv[1], "ping"))
		return ask(argv[1]);
	if ((!strcmp(argv[1], "mount") || !strcmp(argv[1], "unmount")) &&
	    argc > 2) {
		char word[64];
		/* The index is re-rendered as a NUMBER rather than passed
		 * through: whatever argv holds, what reaches the daemon is an
		 * integer. */
		snprintf(word, sizeof(word), "%s %d", argv[1], atoi(argv[2]));
		return ask(word);
	}
	return usage();
}

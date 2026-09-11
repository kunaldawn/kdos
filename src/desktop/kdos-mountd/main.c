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
#include <linux/netlink.h>
#include <poll.h>
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
/*
 * THE LONGEST REQUEST LINE. A block-device verb is a word and three short
 * tokens; `cifs` is what sets this number. A DNS name may be 253 bytes, a
 * share 80, a username 104 and an NT domain 255, so a legal corporate share
 * spells a line of about seven hundred — at 128 the daemon would have refused
 * as malformed the exact requests it exists to serve.
 */
#define KM_MAX 1024
/* The largest second frame: a passphrase, or a typed device name. Long enough
 * for a real passphrase and short enough that a client cannot make this daemon
 * hold anything. */
#define KM_SECRET_MAX 512
/*
 * The tokeniser's array. `cifs` is the longest verb at six tokens, and this
 * leaves headroom on purpose: a request of five still reaches the dispatch and
 * is refused there as an unknown command — which is the answer `mount 0 rm -rf
 * /` has always got and the one the suite asserts. A request that FILLS the
 * array is refused by count instead. Neither path truncates.
 */
#define KM_TOK 8
#define KM_DEVS 32
/* One subscriber per session, and a machine with four logged-in desktops is
 * not a thing this daemon has to be good at. The cap exists so a client that
 * reconnects in a loop cannot make the daemon hold file descriptors forever. */
#define KM_SUBS 4
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

/* Where a mount is made, overridable for the fixture like every other root.
 * Hardcoding `/media` made the mountpoint the one thing about a mount that
 * could not be asserted: a test would have had to write into the running
 * machine's own tree to see where a share had landed. */
static const char *mediaroot(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_MEDIA") : NULL;
	return p && *p ? p : "/media";
}

/*
 * WHERE HOTPLUG COMES FROM, and the fixture's version of it. A kernel uevent
 * arrives on a netlink socket that needs CAP_NET_ADMIN to bind and that a test
 * cannot write to at all — so under `--fixture` the seam names a FIFO instead,
 * carrying the same NUL-separated key=value blob the kernel sends. Gated like
 * every other root here: a variable inherited from an init environment names
 * nothing.
 */
static const char *uevent_path(void)
{
	const char *p = km_fixture ? getenv("KDOS_MOUNTD_UEVENT") : NULL;
	return p && *p ? p : NULL;
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

/*
 * THE MAPPER AN `unlock` OPENED, listed beside the container it came from.
 *
 * WITHOUT THIS AN UNLOCK IS A DEAD END: the container's row goes on saying
 * `crypto_LUKS`, nothing on the list can be mounted, and the filesystem inside
 * — the only reason anybody unlocked it — is reachable from no verb at all.
 *
 * FOUND BY THE NAME THIS DAEMON ITSELF CHOSE, not by walking `/sys/block/dm-*`
 * and its slaves. `km_mapname()` is the one place the name is decided, so a
 * mapper this daemon did not open is not this daemon's to offer — which is
 * exactly the right answer for somebody's own `cryptsetup open` of a root
 * volume.
 *
 * IT CARRIES THE CONTAINER'S `disk`, so every destructive verb is still
 * refused by the physical drive: a format aimed at a mapper on the boot medium
 * must fail for the same reason as one aimed at its ESP.
 */
static void add_mapper(const struct kmdev *src)
{
	char map[64], node[512];

	if (ndev >= KM_DEVS || strcmp(src->fstype, "crypto_LUKS"))
		return;
	km_mapname(src, map, sizeof(map));
	snprintf(node, sizeof(node), "%s/mapper/%s", devroot(), map);

	/*
	 * A NAME THAT WOULD NOT FIT IS NOT OFFERED AT ALL. Every verb
	 * re-derives its device from these two fields, so a truncated one
	 * addresses something that is not the row it is written on — and a
	 * truncation a copy cannot report is exactly the shape of the bug
	 * that dropped a stick with no error anywhere.
	 */
	if (strlen(map) >= sizeof(devs[0].kname) ||
	    strlen(node) >= sizeof(devs[0].node))
		return;
	if (access(node, F_OK) != 0)
		return;		/* locked, which is the usual state */

	struct kmdev *d = &devs[ndev];

	memset(d, 0, sizeof(*d));
	memcpy(d->kname, map, strlen(map) + 1);
	memcpy(d->node, node, strlen(node) + 1);
	snprintf(d->disk, sizeof(d->disk), "%s", src->disk);
	d->bytes = src->bytes;
	probe_fs(d->node, d->label, sizeof(d->label), d->fstype,
		 sizeof(d->fstype));
	find_mount(d->node, d->mnt, sizeof(d->mnt));
	if (d->fstype[0])
		ndev++;
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
				add_mapper(&devs[ndev - 1]);
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
			    !in_fstab(d->node, d->label)) {
				ndev++;
				add_mapper(&devs[ndev - 1]);
			}
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
static int km_row(const char *tok, int n)
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
	return v < n ? v : -1;
}

/* The device list's rows. A share's rows are a different list and are checked
 * against that one — an index is only ever true of the list it came with. */
static int km_index(const char *tok)
{
	return km_row(tok, ndev);
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
 * ── the four fields of a share ─────────────────────────────────────────
 *
 * `mount.cifs` ASSEMBLES ITS OPTION STRING WITH BARE CONCATENATION AND
 * ESCAPES NOTHING BUT THE PASSWORD. It builds the UNC and `user=<username>`
 * by appending, and only the password has its commas doubled. So a comma
 * anywhere in the server, the share, the username or the domain is a NEW
 * MOUNT OPTION injected into the kernel's cifs parser, and a `/` or a `\` in
 * a server silently re-aims the mount, because the helper's own
 * `parse_unc()` splits on exactly those.
 *
 * The escaping therefore cannot live in the option string. Each field is
 * checked against a character allowlist of its own before it means anything,
 * and what is not on the list is a refusal rather than a quoted character:
 * quoting is a second implementation of the helper's parser, and two parsers
 * of one string eventually disagree.
 */
static bool km_chars(const char *s, const char *extra, size_t lo, size_t hi)
{
	size_t len = s ? strlen(s) : 0;

	if (len < lo || len > hi)
		return false;
	for (size_t i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)s[i];

		if (isalnum(ch) || strchr(extra, ch))
			continue;
		return false;
	}
	return true;
}

/*
 * A DNS NAME OR AN IP ADDRESS, and nothing else this image can resolve.
 * musl reads `/etc/hosts` and `/etc/resolv.conf`; `nsswitch.conf` is inert and
 * there is no winbind and no mDNS, so a workgroup name would reach the helper
 * and fail inside it with a message nobody can act on. An IPv6 literal is
 * refused with everything else: it cannot be spelled in a UNC, and the `ip=`
 * option that would carry one is a second way to say where a share is.
 */
static bool km_host(const char *s)
{
	size_t len = s ? strlen(s) : 0;

	if (!km_chars(s, ".-", 1, 253))
		return false;
	/* A label may not begin or end a name with a separator, and `..` is a
	 * name with an empty label in it. */
	if (s[0] == '.' || s[0] == '-' || s[len - 1] == '.' ||
	    s[len - 1] == '-' || strstr(s, ".."))
		return false;
	return true;
}

static bool km_share(const char *s)
{
	return km_chars(s, "._-$", 1, 80);
}

static bool km_user(const char *s)
{
	return km_chars(s, "._@-", 1, 104);
}

/* `-` is the whole of "no domain": a token cannot be empty, because the
 * request line is split on spaces. */
static bool km_domain(const char *s)
{
	if (s && !strcmp(s, "-"))
		return true;
	return km_chars(s, ".-", 1, 255);
}

/*
 * THE SECOND FRAME, READ BY BYTE COUNT ALONE.
 *
 * A frame read needs the count, the socket and the bytes the first read
 * already pulled in past the newline — and nothing about the verb. `cifs`
 * names a server rather than a device row, so a reader that took an index
 * with the count could not serve it at all.
 *
 * Every exit wipes the buffer: a passphrase that outlived its request would
 * sit in a root daemon's heap for the life of the session.
 */
static int km_frame(int c, const char *buf, size_t linelen, size_t have,
		    int want, char *secret, size_t cap)
{
	size_t need = (size_t)want;
	size_t got = have < need ? have : need;

	if (want < 0 || need >= cap)
		return -1;
	memcpy(secret, buf + linelen + 1, got);
	/* The room left is computed against the BUFFER as well as against the
	 * frame. Both bounds hold — km_count() already refused anything over
	 * KM_SECRET_MAX — but a read bounded only by a value the compiler
	 * cannot follow is one it must assume the worst about. */
	while (got < need && got < cap - 1) {
		size_t room = need - got;
		ssize_t r;

		if (room > cap - 1 - got)
			room = cap - 1 - got;
		r = read(c, secret + got, room);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	if (got != need)
		return -1;
	secret[got] = '\0';
	return (int)got;
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
 * THE SAME CHILD, WITH AN ENVIRONMENT AND A REASON.
 *
 * A mount helper needs both: `$PASSWD_FD` is the only way to hand it a
 * password that is neither in argv nor in a file, and its refusal is on
 * stderr, which `km_exec` sends to /dev/null. A surface that reported a status
 * and no reason would say the same thing for a wrong password as for a server
 * that is not there.
 *
 * IN FIXTURE MODE THE ENVIRONMENT IS PRINTED WITH THE ARGV. Printing only the
 * argv would make the secure route indistinguishable from the insecure one:
 * the check that matters about this verb is that the password travels on a
 * descriptor, and an argv-only dump cannot tell a `PASSWD_FD` run from a
 * `pass=` one.
 */
static int km_exec_env(const KbArgv *a, const char *const *env, int nenv,
		       const char *feed, size_t nfeed, char *err, size_t ncap)
{
	if (err && ncap)
		err[0] = '\0';
	if (km_fixture) {
		for (int i = 0; i < nenv && env && env[i]; i++)
			printf("env %s\n", env[i]);
		for (int i = 0; i < a->n && a->v[i]; i++)
			printf("%s%s", i ? " " : "exec ", a->v[i]);
		printf("\n");
		/* The byte count and never the bytes: a fixture that echoed a
		 * password would put one in a test log. */
		printf("stdin %zu bytes\n", nfeed);
		fflush(stdout);
		return 0;
	}
	return kb_run_feed_env(a, env, nenv, feed, nfeed, err, ncap);
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
	char base[KM_NAME], parent[192], dir[256];

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

	/* A TRUNCATED MOUNTPOINT IS A MOUNT SOMEWHERE ELSE, so a path that
	 * does not fit is a refusal rather than a shortened name. */
	if (snprintf(parent, sizeof(parent), "%s/%s", mediaroot(),
		     pw && pw->pw_name ? pw->pw_name : "user") >=
		    (int)sizeof(parent) ||
	    snprintf(dir, sizeof(dir), "%s/%s", parent, base) >=
		    (int)sizeof(dir)) {
		snprintf(out, nout, "that name makes a path longer than a "
				    "mountpoint can be");
		return -1;
	}
	if (mkdir(mediaroot(), 0755) != 0 && errno != EEXIST)
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

/*
 * ── a share on another machine ─────────────────────────────────────────
 *
 * THE HELPER RUNS, THE DAEMON DOES NOT MOUNT. `mount(2)` cannot raise a cifs
 * session on its own: the SMB dialect, the authentication and the tree connect
 * all happen in `mount.cifs` before the syscall it eventually makes. So this
 * is the second verb that spawns a child, and it goes through `km_exec`'s
 * successor for the same reason `unlock` does — the secret must not be in
 * argv.
 *
 * `PASSWD_FD=0` AND THE PASSWORD ON STDIN. `mount.cifs` will take a password
 * from `$PASSWD`, from a file named by `$PASSWD_FILE`, from a descriptor named
 * by `$PASSWD_FD`, or from `pass=` in the option string. Only the descriptor
 * keeps it out of both the process table and the filesystem: an option string
 * is argv, an environment value is `/proc/<pid>/environ`, and a file is a file
 * somebody has to delete.
 *
 * THE MOUNT IS THE CALLER'S. `uid=`, `gid=`, `file_mode=` and `dir_mode=` are
 * given because a cifs server that speaks no unix extensions reports every
 * file as owned by root, and a share nobody but root can read is a share that
 * did not mount as far as the person who asked is concerned.
 */
static int do_cifs(const char *server, const char *share, const char *user,
		   const char *domain, const char *pass, size_t npass,
		   uid_t uid, char *out, size_t nout)
{
	struct passwd *pw = getpwuid(uid);
	char parent[192], dir[352], unc[352], opts[512];
	char err[512] = "";
	const char *env[1] = { "PASSWD_FD=0" };
	KbArgv a = { 0 };
	int rc;

	if (!km_host(server) || !km_share(share) || !km_user(user) ||
	    !km_domain(domain)) {
		snprintf(out, nout, "a field carries a character a share name "
				    "cannot");
		return -1;
	}
	/*
	 * THE MODULE IS LOADED BEFORE THE QUESTION IS ASKED. `cifs` is a
	 * module on this image and nothing else here loads it, and
	 * /proc/filesystems lists only what is already in the kernel — so the
	 * support check would refuse every first connection on a machine that
	 * can do this perfectly well. Under the fixture nothing is loaded and
	 * nothing is checked: the assertion is about the request this daemon
	 * makes, and gating it on the kernel the suite happens to run under
	 * would make it a different test on every machine.
	 */
	if (!km_fixture) {
		if (!fs_supported("cifs")) {
			KbArgv m = { 0 };

			kb_argv_add(&m, "/sbin/modprobe");
			kb_argv_add(&m, "cifs");
			kb_argv_end(&m);
			km_exec(&m, NULL, 0);
		}
		if (!fs_supported("cifs")) {
			snprintf(out, nout, "this kernel cannot mount cifs");
			return -1;
		}
	}

	snprintf(unc, sizeof(unc), "//%s/%s", server, share);
	/* A TRUNCATED MOUNTPOINT IS A MOUNT SOMEWHERE ELSE. */
	if (snprintf(parent, sizeof(parent), "%s/%s", mediaroot(),
		     pw && pw->pw_name ? pw->pw_name : "user") >=
		    (int)sizeof(parent) ||
	    snprintf(dir, sizeof(dir), "%s/%s-%s", parent, server, share) >=
		    (int)sizeof(dir)) {
		snprintf(out, nout, "that share's name makes a path longer "
				    "than a mountpoint can be");
		return -1;
	}

	/* ALREADY THERE IS NOT AN ERROR, the same answer `mount` gives for a
	 * stick that is already mounted: the caller wanted the share
	 * available and it is. */
	{
		char at[256];

		find_mount(unc, at, sizeof(at));
		if (at[0]) {
			snprintf(out, nout, "%s", at);
			return 0;
		}
	}

	if (mkdir(mediaroot(), 0755) != 0 && errno != EEXIST)
		return -1;
	if (mkdir(parent, 0755) != 0 && errno != EEXIST)
		return -1;
	if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
		snprintf(out, nout, "cannot create %s: %s", dir,
			 strerror(errno));
		return -1;
	}

	snprintf(opts, sizeof(opts),
		 "user=%s%s%s,uid=%u,gid=%u,file_mode=0600,dir_mode=0700,"
		 "nosuid,nodev%s",
		 user, strcmp(domain, "-") ? ",domain=" : "",
		 strcmp(domain, "-") ? domain : "",
		 (unsigned)uid, pw ? (unsigned)pw->pw_gid : 0u,
		 exec_allowed() ? "" : ",noexec");

	kb_argv_add(&a, "/sbin/mount.cifs");
	kb_argv_add(&a, unc);
	kb_argv_add(&a, dir);
	kb_argv_add(&a, "-o");
	kb_argv_add(&a, opts);
	kb_argv_end(&a);

	rc = km_exec_env(&a, env, 1, pass, npass, err, sizeof(err));
	if (rc != 0) {
		/* The helper's own words, first line only: it prints a usage
		 * block after some failures and a surface has one row. */
		err[strcspn(err, "\r\n")] = '\0';
		snprintf(out, nout, "%s", err[0] ? err : "the share refused");
		rmdir(dir);
		return -1;
	}
	snprintf(out, nout, "%s", dir);
	return 0;
}

/*
 * WHAT IS CONNECTED IS WHAT /proc/mounts SAYS IS CONNECTED. No list is held
 * between requests, for the reason the device list is not: a server that went
 * away, or a share a second session mounted, must not be answered for out of
 * this daemon's memory.
 */
static int shares(char unc[][352], char at[][256], int max)
{
	char *data = kb_read_all(mounts_path(), NULL);
	int n = 0;

	if (!data)
		return 0;
	for (char *p = data; *p && n < max;) {
		char *nl = strchr(p, '\n');
		char dev[352] = "", mnt[256] = "", type[32] = "";

		if (nl)
			*nl = '\0';
		if (sscanf(p, "%351s %255s %31s", dev, mnt, type) == 3 &&
		    (!strcmp(type, "cifs") || !strcmp(type, "smb3"))) {
			snprintf(unc[n], 352, "%s", dev);
			snprintf(at[n], 256, "%s", mnt);
			n++;
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	free(data);
	return n;
}

static void reply_shares(int c)
{
	char unc[KM_DEVS][352], at[KM_DEVS][256];
	int n = shares(unc, at, KM_DEVS);
	char line[640];

	for (int i = 0; i < n; i++) {
		int len = snprintf(line, sizeof(line), "%d\t%s\t%s\n", i,
				   unc[i], at[i]);

		(void)!write(c, line, (size_t)len);
	}
	(void)!write(c, "ok\n", 3);
}

/*
 * THE ROW NUMBER IS ONLY TRUE OF THE LIST IT CAME WITH, which is why the list
 * is rebuilt here rather than remembered: between the `shares` that drew a row
 * and the `disconnect` that acts on it, a share may have gone and the row
 * below it moved up.
 */
static int do_disconnect(int idx, char *out, size_t nout)
{
	char unc[KM_DEVS][352], at[KM_DEVS][256];
	int n = shares(unc, at, KM_DEVS);

	if (idx < 0 || idx >= n) {
		snprintf(out, nout, "no such share");
		return -1;
	}
	/* IN FIXTURE MODE NOTHING IS UNMOUNTED, for km_exec's reason: the
	 * mountpoints in a recorded /proc/mounts belong to the machine the
	 * suite happens to run on. */
	if (km_fixture) {
		printf("umount %s\n", at[idx]);
		fflush(stdout);
		snprintf(out, nout, "%s", unc[idx]);
		return 0;
	}
	if (umount2(at[idx], 0) != 0) {
		snprintf(out, nout, "%s: %s", at[idx], strerror(errno));
		return -1;
	}
	/* The mountpoint this daemon made goes with it; one it did not make
	 * is not empty and rmdir refuses, which is the answer wanted. */
	rmdir(at[idx]);
	snprintf(out, nout, "%s", unc[idx]);
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
 * SMART, WHICH IS A READ AND STILL BELONGS HERE.
 *
 * `smartctl` needs the raw block device, which nothing in a session may open —
 * so the alternative to a verb is a setuid binary or a sudo rule, and both are
 * a wider hole than one privileged daemon answering one question. It reports
 * the DISK's health rather than the partition's: SMART is a property of the
 * drive, and a row per partition would print the same answer four times.
 *
 * ITS OUTPUT IS CAPTURED AND FILTERED, never forwarded whole. `smartctl -a` is
 * two hundred lines of vendor attributes; what a person opening a disks window
 * wants is whether the drive says it is failing, and the model and serial that
 * say WHICH drive that is.
 */
static int do_smart(int idx, char *out, size_t nout)
{
	struct kmdev *d;
	char node[256], cap[4096];
	KbArgv a = { 0 };

	if (idx < 0 || idx >= ndev) {
		snprintf(out, nout, "no such device");
		return -1;
	}
	d = &devs[idx];
	snprintf(node, sizeof(node), "%s/%s", devroot(), d->disk);

	kb_argv_add(&a, "/usr/sbin/smartctl");
	kb_argv_add(&a, "-H");
	kb_argv_add(&a, "-i");
	kb_argv_add(&a, "--");
	kb_argv_add(&a, node);
	kb_argv_end(&a);

	if (km_fixture) {
		/* The fixture proves the argv, like every other verb: there is
		 * no drive behind it to answer. */
		km_exec(&a, NULL, 0);
		snprintf(out, nout, "PASSED");
		return 0;
	}
	/*
	 * THE EXIT STATUS IS A BITFIELD AND NOT A FAILURE. `smartctl` sets
	 * bit 0 for a command-line error, bit 1 for a device it could not open
	 * and bits 3-7 for a drive that is unwell — so a non-zero status is
	 * frequently the answer rather than the absence of one. Only an empty
	 * capture means nothing was learnt.
	 */
	kb_run_capture(&a, cap, sizeof(cap));
	if (!cap[0]) {
		snprintf(out, nout, "smartctl said nothing about %s", d->disk);
		return -1;
	}

	/* Model, serial and the health line, in that order, tab-separated —
	 * three fields a surface draws rather than a page it must parse. */
	char model[96] = "-", serial[64] = "-", health[64] = "-";
	char *save = NULL;

	for (char *ln = strtok_r(cap, "\n", &save); ln;
	     ln = strtok_r(NULL, "\n", &save)) {
		const char *v = strchr(ln, ':');

		if (!v)
			continue;
		v++;
		while (*v == ' ' || *v == '\t')
			v++;
		if (!strncmp(ln, "Device Model", 12) ||
		    !strncmp(ln, "Model Number", 12))
			snprintf(model, sizeof(model), "%s", v);
		else if (!strncmp(ln, "Serial Number", 13))
			snprintf(serial, sizeof(serial), "%s", v);
		else if (strstr(ln, "overall-health") ||
			 strstr(ln, "SMART Health Status"))
			snprintf(health, sizeof(health), "%s", v);
	}
	snprintf(out, nout, "%s\t%s\t%s", model, serial, health);
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

/* ── hotplug ───────────────────────────────────────────────────────────── */

/*
 * THE KERNEL'S OWN BROADCAST, not udev's. `NETLINK_KOBJECT_UEVENT` is what
 * udev itself listens to; taking it directly means this daemon learns about a
 * stick with no dependency on a rule file, on udev running, or on the two
 * agreeing about what a block device is. It needs no capability beyond the
 * root this daemon already has, and binding group 1 is a subscribe, not a
 * privilege — nothing can be sent from here.
 */
static int uevent_open(void)
{
	const char *fx = uevent_path();
	struct sockaddr_nl sa = { .nl_family = AF_NETLINK, .nl_groups = 1 };
	int fd;

	if (fx) {
		/* O_RDWR AND NOT O_RDONLY. A reader-only open of a FIFO blocks
		 * until a writer arrives, which would hang the daemon before
		 * it ever reached poll(); holding a writer of our own also
		 * stops the poll from spinning on EOF between test writes. */
		return open(fx, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	}
	fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
		    NETLINK_KOBJECT_UEVENT);
	if (fd < 0)
		return -1;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/*
 * IS THIS EVENT ONE THE LIST WOULD MOVE FOR? A uevent is a NUL-separated run
 * of `key=value`, and the daemon cares about exactly one subsystem. `change`
 * is in the set because that is what a drive reports when a disc is inserted
 * into a tray that was already there — dropping it would make optical media
 * the one case hotplug missed.
 */
static bool uevent_block(const char *buf, size_t n)
{
	bool block = false, act = false;

	for (size_t i = 0; i < n;) {
		const char *k = buf + i;
		size_t len = strnlen(k, n - i);

		if (!strcmp(k, "SUBSYSTEM=block"))
			block = true;
		else if (!strcmp(k, "ACTION=add") ||
			 !strcmp(k, "ACTION=remove") ||
			 !strcmp(k, "ACTION=change"))
			act = true;
		i += len + 1;
	}
	return block && act;
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

	/*
	 * THE ONE LONG-LIVED CONNECTION. Every other verb is answered and the
	 * socket closed, which is what lets a bare blocking accept() serve the
	 * whole daemon; `subscribe` holds its connection open, so a bare
	 * accept() would leave every other client waiting behind it forever.
	 * The shape is kdos-oomd's: one poll over the listener, the event
	 * source and each subscriber.
	 */
	int uev = uevent_open();
	int subs[KM_SUBS];
	int nsubs = 0;

	if (uev < 0)
		fprintf(stderr, "kdos-mountd: no uevent source; "
				"`subscribe` will report nothing\n");

	for (;;) {
		struct pollfd fds[2 + KM_SUBS];
		int nfds = 0, li, ui = -1, sbase;

		li = nfds;
		fds[nfds].fd = srv;
		fds[nfds].events = POLLIN;
		nfds++;
		if (uev >= 0) {
			ui = nfds;
			fds[nfds].fd = uev;
			fds[nfds].events = POLLIN;
			nfds++;
		}
		sbase = nfds;
		for (int i = 0; i < nsubs; i++) {
			/* Nothing is expected FROM a subscriber; this watches
			 * for the hangup that says the session went away. */
			fds[nfds].fd = subs[i];
			fds[nfds].events = 0;
			nfds++;
		}

		if (poll(fds, (nfds_t)nfds, -1) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		/* A subscriber that went away is dropped before anything is
		 * written to it, so a hung-up session cannot cost a SIGPIPE. */
		for (int i = nsubs - 1; i >= 0; i--) {
			if (!(fds[sbase + i].revents &
			      (POLLHUP | POLLERR | POLLNVAL)))
				continue;
			close(subs[i]);
			subs[i] = subs[--nsubs];
		}

		if (ui >= 0 && (fds[ui].revents & POLLIN)) {
			char ev[2048];
			ssize_t n = read(uev, ev, sizeof(ev));

			/*
			 * `changed` AND NOT A DEVICE NAME. An index is only
			 * meaningful against the list the daemon published a
			 * moment ago, and scan() rebuilds that on every
			 * request — so the event says the list moved and the
			 * client asks again. That keeps the rule the whole
			 * protocol is built on: the client never names a
			 * device, it names a row.
			 */
			if (n > 0 && uevent_block(ev, (size_t)n)) {
				for (int i = nsubs - 1; i >= 0; i--) {
					if (write(subs[i], "changed\n", 8) == 8)
						continue;
					close(subs[i]);
					subs[i] = subs[--nsubs];
				}
			}
		}

		if (!(fds[li].revents & POLLIN))
			continue;

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
		/*
		 * A LINE THAT FILLS THE ARRAY IS REFUSED RATHER THAN
		 * TRUNCATED.
		 *
		 * Stopping the tokeniser at the array's size and dispatching
		 * anyway is exactly the defect the allowlist was built to
		 * kill — `mount 0 rm -rf /` parsed as `mount 0`, because the
		 * tail was thrown away rather than objected to. It was dormant
		 * only because no verb reached the array's size, and the next
		 * verb added is what would have woken it.
		 */
		char *tok[KM_TOK] = {0};
		int ntok = 0;
		for (char *sp = NULL, *t = strtok_r(line, " \t", &sp);
		     t && ntok < KM_TOK; t = strtok_r(NULL, " \t", &sp))
			tok[ntok++] = t;
		if (ntok >= KM_TOK) {
			(void)!write(c, "err too many arguments\n", 23);
			close(c);
			continue;
		}

		/* The list is re-read on EVERY request, not cached: a stick
		 * pulled out between two requests must not still be offered,
		 * and the scan is a handful of file reads. */
		scan();

		char msg[512] = "";
		const char *verb = ntok ? tok[0] : "";
		int idx;

		if (ntok == 1 && !strcmp(verb, "subscribe")) {
			/*
			 * THE ONE VERB THAT KEEPS ITS SOCKET. Every other
			 * request is answered and closed; this one is handed
			 * to the poll set above and written to whenever the
			 * device list moves. It carries no state — the daemon
			 * remembers nothing about a subscriber except the file
			 * descriptor — so the rule that a connection holds no
			 * session survives; what changes is that one of them
			 * outlives its answer.
			 *
			 * `ok` FIRST, so a client can tell a subscription that
			 * was accepted from one that was refused for the cap
			 * without waiting for an event that may be hours away.
			 */
			if (nsubs >= KM_SUBS) {
				(void)!write(c, "err too many subscribers\n",
					     25);
				close(c);
				continue;
			}
			(void)!write(c, "ok\n", 3);
			subs[nsubs++] = c;
			continue;	/* NOT closed */
		} else if (ntok == 1 && !strcmp(verb, "ping")) {
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
		} else if (ntok == 2 && !strcmp(verb, "smart")) {
			idx = km_index(tok[1]);
			if (idx >= 0 && do_smart(idx, msg, sizeof(msg)) == 0)
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
		} else if (ntok == 1 && !strcmp(verb, "shares")) {
			reply_shares(c);
		} else if (ntok == 2 && !strcmp(verb, "disconnect")) {
			char unc[KM_DEVS][352], at[KM_DEVS][256];
			int n = shares(unc, at, KM_DEVS);

			/* NOT km_index: that one is bounded by the DEVICE
			 * list, and a share is not a device. */
			idx = km_row(tok[1], n);
			if (idx >= 0 &&
			    do_disconnect(idx, msg, sizeof(msg)) == 0)
				dprintf(c, "ok %s\n", msg);
			else
				dprintf(c, "err %s\n",
					msg[0] ? msg : "no such share");
		} else if ((ntok == 3 && !strcmp(verb, "unlock")) ||
			   (ntok == 4 && !strcmp(verb, "format")) ||
			   (ntok == 6 && !strcmp(verb, "cifs"))) {
			int want = km_count(tok[ntok - 1]);

			/*
			 * THE INDEX IS THE BLOCK-DEVICE VERBS' ALONE. `cifs`
			 * names a server rather than a row, so what the two
			 * kinds share is the second frame and nothing else.
			 */
			idx = strcmp(verb, "cifs") ? km_index(tok[1]) : 0;
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
			int got = km_frame(c, buf, linelen, have, want, secret,
					   sizeof(secret));

			if (got < 0) {
				explicit_bzero(secret, sizeof(secret));
				(void)!write(c, "err short frame\n", 16);
				close(c);
				continue;
			}
			int rc;

			if (ntok == 3)
				rc = do_unlock(idx, secret, (size_t)got, msg,
					       sizeof(msg));
			else if (ntok == 4)
				rc = do_format(idx, tok[2], secret,
					       (size_t)got, msg, sizeof(msg));
			else
				rc = do_cifs(tok[1], tok[2], tok[3], tok[4],
					     secret, (size_t)got, cred.uid,
					     msg, sizeof(msg));
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
		/* FLUSHED PER READ, because `subscribe` never ends. stdout to
		 * a pipe is block-buffered, so without this a subscriber
		 * reading this command's output would see nothing until four
		 * kilobytes of events had accumulated — which for a stick
		 * being plugged in is never. */
		fflush(stdout);
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
		"       kdos-mount smart <index>\n"
		"       kdos-mount ping\n"
		"       kdos-mount subscribe\n"
		"\nThe index is a row from `list`. There is no form that takes\n"
		"a device or a mountpoint: the daemon decides both.\n"
		"\n`subscribe` writes `changed` whenever the list moves and\n"
		"never exits; it names no device, because a row number is only\n"
		"true of the list it came with. Ask again with `list`.\n");
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
	if (!strcmp(argv[1], "list") || !strcmp(argv[1], "ping") ||
	    !strcmp(argv[1], "subscribe"))
		return ask(argv[1]);
	if ((!strcmp(argv[1], "mount") || !strcmp(argv[1], "unmount") ||
	     !strcmp(argv[1], "smart")) && argc > 2) {
		char word[64];
		/* The index is re-rendered as a NUMBER rather than passed
		 * through: whatever argv holds, what reaches the daemon is an
		 * integer. */
		snprintf(word, sizeof(word), "%s %d", argv[1], atoi(argv[2]));
		return ask(word);
	}
	return usage();
}

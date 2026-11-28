/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall-core — the installer with no screen
 *
 * What the installer can see about the machine, what the person decided, and
 * how the work is carried out. Every front end drives this one backend: the
 * terminal wizard (src/system/kdos-installer) links it beside libktui, and the
 * graphical installer (src/desktop/kdos-installer-gui) runs the same binary
 * headless with `--events json` and reads the stream. A second backend would be
 * a second opinion about what an install does, and the two would come apart on
 * the first edit made to only one of them.
 *
 * IT LINKS libkbase AND libkcolor AND NOTHING ELSE. No libktui: nothing here
 * draws, and a front end that is not a terminal must not pull the terminal
 * toolkit in to get an installer. libkcolor stays because the boot menu, the
 * text consoles and the login screen are painted in the chosen accent before
 * any session exists, and their colours come out of the one table.
 * ---------------------------------
 */

#ifndef KINSTALL_CORE_H
#define KINSTALL_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "kbase.h"
#include "kcolor.h"	/* the scheme table, and which of its rows is the default */

#define KI_VERSION "5.0"

/* ────────────────────────────────────────────────────────────────────────
 * Probe
 * ──────────────────────────────────────────────────────────────────────── */

#define MAX_DISKS 32
#define MAX_PARTS 32

typedef struct {
	char name[32];		/* sda1                                    */
	char path[64];		/* /dev/sda1                               */
	unsigned long long start, sectors;
	char fstype[16];
	char label[40];
	char uuid[40];
	int num;		/* the kernel's partition number, 1-based  */
	int is_esp;
	/* GPT type 21686148-6449-6E6F-744E-656564454649: the raw space
	 * Limine's BIOS stage is written into on a GPT disk. */
	int is_bios_boot;
	int mounted;
	char mountpoint[64];
	/* Another block device sits on this one — an open LUKS container, an
	 * active volume group, an array. mkfs refuses it, and a reuse plan
	 * that picked it would fail after the point of no return. */
	int held;
	/* An LVM2_member partition's volume group, read as a whole-disk
	 * physical volume's is ("" otherwise), so a front end names the
	 * group an erase destroys. */
	char vg[64];
	/* The node /dev/<name> answers for another device than sysfs's
	 * partition: Ventoy's hook makes its boot partition's node the
	 * ISO's dm device. "ventoy" for Ventoy's mapping, the dm name (or
	 * kernel name) of the device it points at otherwise, "" for a node
	 * that is its partition. A remapped partition is never read through
	 * its node — that would read the other device — and is never an
	 * ESP. */
	char remap[32];
} Part;

typedef struct {
	char name[32];
	char path[64];
	char model[48];
	char tran[16];		/* sata, nvme, usb, virtio                 */
	unsigned long long sectors;
	int sector_size;
	int rotational;
	int removable;
	int readonly;
	int is_boot_media;	/* the stick we are running from           */
	int is_boot_copy;	/* another disk holding that medium's id   */
	char table[8];		/* gpt, dos, -                             */
	Part part[MAX_PARTS];
	int nparts;
	/* The signature on the whole device, with or without partitions: a
	 * physical volume pvcreate wrote there, a filesystem, a LUKS
	 * container, the ISO 9660 image of a written stick. Its type as
	 * blkid names it, its label, for a physical volume its group where
	 * that can be read ("" otherwise), and for an ISO 9660 image its size
	 * (0 when unknown), so a front end never calls such a disk empty. */
	char fstype[16];
	char label[40];
	char vg[64];
	unsigned long long fs_bytes;
	/* The ISO 9660 image is what the disk holds: its partitions are all
	 * the image's hybrid windows (see ki_disk_signature), so a front end
	 * names the image instead of them. 0 for a disk repartitioned over a
	 * written stick, whose stale descriptor names nothing an erase
	 * destroys. */
	int iso_image;
} Disk;

/*
 * WHERE IN THE COPY ITS COST LIES. rsync's `to-chk` counts every entry of
 * its list, in its own order, and the entries do not cost alike: a regular
 * file is read, written and closed, while a directory, a symbolic link or a
 * later name of a hard-linked file is a call or two on the target. The live
 * tree keeps some twenty-five thousand links in its icon themes past the
 * middle of its list, so a time left that charges every entry a file's cost
 * reads the rest of the copy as up to half again as long as it is there.
 *
 * A profile holds the cost done after each 1/KI_COPY_PROF of the entries, as
 * a fraction of the whole, a regular file's first name counting 1 and any
 * other entry KI_COPY_OTHER_W, and `per_entry` the units of an average
 * entry. KI_COPY_OTHER_W is the ratio of the two costs fitted to the
 * progress of the live medium's install onto a virtual disk: about 0.1 ms
 * an entry against 0.35 ms a file. ki_copy_note() reads the files done and
 * left through the profile; with `ok` 0, every entry counts as a file.
 */
#define KI_COPY_PROF 128
#define KI_COPY_OTHER_W 0.3

typedef struct {
	int ok;
	double per_entry;
	float at[KI_COPY_PROF + 1];
} KiCopyProfile;

/* The kinds of entry a profile is built from, one byte each in rsync's
 * order: a regular file's first name or any other entry, and whether it is
 * inside the box store. */
enum { KI_ENT_OTHER = 0, KI_ENT_FILE = 1, KI_ENT_STORE = 2 };

/* The profile of the `n` entries of `kind`, leaving out the box store's
 * when `store` is 0. */
void ki_copy_profile(KiCopyProfile *p, const unsigned char *kind, size_t n,
		     int store);

typedef struct {
	int uefi;
	/*
	 * HOW WIDE THAT FIRMWARE IS, in bits, and 0 where it did not say.
	 *
	 * A 64-bit CPU does not imply a 64-bit firmware: the early Atom
	 * tablets run this kernel and this userland and load only a 32-bit EFI
	 * binary. Firmware finds the right one by itself on removable media —
	 * it reads `EFI/BOOT/BOOTIA32.EFI` and never looks at BOOTX64 — but an
	 * NVRAM entry names one path, so the installer has to know which to
	 * write. Read from `/sys/firmware/efi/fw_platform_size`.
	 */
	int fw_bits;
	int secure_boot;
	/*
	 * WHETHER THERE IS A NETWORK: a default route, IPv4 or IPv6, on an
	 * interface that is up with a carrier (ki_probe_network()). It says
	 * nothing about whether the binhost answers, only that a fetch can
	 * try; with none a front end says the native groups are installed
	 * after the first boot rather than promising a download.
	 */
	int network;
	unsigned long long mem_kb;
	char cpu[64];
	int cores;		/* logical CPUs: threads, not cores        */
	int live;		/* booted from the squashfs overlay        */
	unsigned long long payload_kb;	/* what an install will copy       */
	unsigned long long appbox_kb;	/* of which, the alien app store   */
	/* The bytes of every name of a hard-linked file after its first, which
	 * rsync -H counts in its percentage and never copies; payload_kb counts
	 * each name's blocks. */
	unsigned long long link_extra_bytes;
	unsigned long long link_extra_appbox_bytes;	/* of which, the store */
	/* Where the copy's cost lies, with the box store and without it; the
	 * second is slot B's too, whose -x copy never enters /home. */
	KiCopyProfile copy_prof, copy_prof_sys;
} SysInfo;

/*
 * A LOGICAL VOLUME, as the kernel sees it once its group is active: a dm
 * device whose uuid starts `LVM-`. Read from /sys/block/dm-N and never from
 * `lvs`, for the reason probe.c gives for not parsing lsblk.
 *
 * Only volumes a filesystem can go on are kept. A thin pool's or a cache's
 * visible wrapper carries a suffix after the uuid, and its sub-volumes
 * (`_tdata`, `_cmeta`, `_rimage_0`, …) are internal to LVM; offering either
 * as a root is offering to format LVM's own metadata. A thin volume and a
 * cached volume are ordinary volumes here, and boot: the initramfs carries
 * thin_check and cache_check.
 */
#define MAX_LVS 64

typedef struct {
	char vg[64];
	char lv[64];
	char path[160];		/* /dev/<vg>/<lv>                          */
	char dm[16];		/* dm-3                                    */
	unsigned long long sectors;
	char fstype[16];
	char label[40];
	char uuid[40];
	int mounted;
	char mountpoint[64];
	int held;
} Lv;

extern Disk ki_disk[MAX_DISKS];
extern int ki_ndisk;
extern SysInfo ki_sys;
extern Lv ki_lv[MAX_LVS];
extern int ki_nlv;

void probe_system(void);
/* ki_sys.network, read again: probe_system() does it once, and a page that
 * says whether the groups download asks as it opens, since a cable plugged
 * in after the wizard started changes the answer. $KDOS_NET_ROOT, for a
 * test, is a directory read in place of / for proc/net/route,
 * proc/net/ipv6_route and sys/class/net. */
void ki_probe_network(void);
/* The size of the tree an install copies, into ki_sys.payload_kb and
 * appbox_kb, its extra hard-link names into link_extra_bytes and
 * link_extra_appbox_bytes, and where its cost lies into copy_prof and
 * copy_prof_sys: a walk of the whole root filesystem in rsync's order, the
 * one slow probe, and so called on its own. */
void probe_payload(void);
/* The same walk of an installed tree at `root` (slot A, for slot B's copy),
 * on its own filesystem and with nothing left out: its size, its extra
 * hard-link names and its copy profile. -1, the figures zero, when `root`
 * could not be walked. */
int probe_payload_at(const char *root, unsigned long long *kb,
		     unsigned long long *link_bytes, KiCopyProfile *prof);
/* Also activates every volume group, once per process and only as root, then
 * lists the logical volumes into ki_lv. */
void probe_disks(void);
Lv *lv_by_path(const char *path);
/* A dm name, `vg-lv` with each `-` inside a name doubled, split into its
 * two names. Non-zero for a name that is not a logical volume's. */
int ki_dm_split(const char *name, char *vg, size_t vcap, char *lv, size_t lcap);
/* Whether a block device (a kernel name: sda2, dm-3) sits, through any stack
 * of containers and volume groups, on the disk named `disk` (sda). */
int ki_dev_on_disk(const char *dev, const char *disk);
/* Whether volume group `vg` has a physical volume anywhere but on `disk`,
 * whether or not any of its volumes is active. Runs `lvm pvs`. Active
 * volumes are also read from /sys/block/dm-*, which needs no privilege, so
 * an unprivileged plan refuses the same clash. */
int ki_vg_off_disk(const char *vg, const char *disk);

/* ────────────────────────────────────────────────────────────────────────
 * The applications this medium knows how to build.
 *
 * THE CATALOGUE IS READ, NOT A PACK INDEX. Nothing is baked onto the medium;
 * what ships is `/usr/share/kdos/appstore/catalogue`, and an application is
 * built by podman on the installed machine. The installer's job is to record
 * WHICH.
 *
 * IT LINKS catalogue.c DIRECTLY rather than running `kdos-appbox`. catalogue.c
 * uses `kb_*` alone, so it costs no library — which is what lets kinstall live
 * in 10_bootstrap and exist on every tree from the first bootable image — and
 * a live installer cannot assume anything is on `$PATH` in the target.
 *
 * THE UNIT IS A GROUP. A row for every application in the catalogue is not a
 * thing to page through during an install; seven named bundles is. An answer
 * file names groups too: ki_apps_enter() matches group ids only, and an
 * application id there selects nothing.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * How the ticked set will reach the installed machine, SHOWN ON THE PAGE. A
 * person must not discover at first boot that nothing was installed.
 *
 * RECORDED, NEVER BUILT DURING THE INSTALL. `kdos-appbox` has no target root:
 * an import or a build run from the installer would land in the live
 * session's pack store and container storage and end with the live medium.
 * The selection is written to the target's /var/lib/kdos/apps-pending, and
 * the first login offers to build it.
 */
enum {
	APPS_NONE = 0,	/* nothing ticked                                  */
	APPS_PENDING,	/* recorded, and the first session offers them     */
};

#define MAX_APPGROUPS 32

typedef struct {
	char id[64];
	char desc[128];
	int  napp;		/* members that are applications              */
	unsigned long long bytes;	/* an ESTIMATE, and labelled one     */
	int  chosen;
} KiGroup;

extern KiGroup ki_group[MAX_APPGROUPS];
extern int ki_ngroup;
extern int ki_apps_present;	/* a catalogue was found and parsed         */

void probe_apps(void);
/* Bytes the chosen groups cost, as an estimate. What every summary and
 * `--dump plan` report. */
unsigned long long ki_apps_bytes(void);
/* APPS_PENDING when any group is ticked, else APPS_NONE. */
int  ki_apps_route(void);
/* Read the catalogue and apply an answer file's `apps =`: `essential` where
 * it named nothing, `essential` where none of its names is a group, and no
 * group at all for the word `none`. Called by
 * the wizard's Applications page and by every path that plans an install
 * WITHOUT walking the wizard — `--dump`, `--unattended`, `--events`. */
void ki_apps_enter(void);
/* cfg.apps rebuilt from the ticks: one space-separated line, the form the
 * answer file records, or `none`. */
void ki_apps_collect(void);

/* ────────────────────────────────────────────────────────────────────────
 * The native groups: sets of ports the release's binhost holds prebuilt.
 *
 * READ FROM THE LIVE SYSTEM'S /ports/groups ($KDOS_GROUPS for a test), the
 * file `kdos update groups` reads. Unlike the catalogue's groups these are
 * INSTALLED DURING THE INSTALL when the network answers: `kdos update fetch
 * --binhost-only` takes the medium's own release's binhost index, and
 * `kdos update install --root <target> <id>` puts each group's packages into
 * the new system. Offline, the ids are written to the target's
 * /var/lib/kdos/groups-pending, and `kdos update install --pending` installs
 * them later. None is chosen by default, each one being a download, but for
 * the language group a Chinese, Japanese or Korean keyboard or time zone
 * points at (ki_lang_why()), which starts ticked unless the answer file
 * named `groups =`.
 * ──────────────────────────────────────────────────────────────────────── */
#define MAX_NATIVE 64

typedef struct {
	char id[64];
	char title[128];
	/* What the group installs, counted by port: a member that is a meta
	 * port (a recipe with no `source =`) counts as the ports it depends
	 * on, so a group of one Gear set counts that set's applications. */
	int  nports;
	int  chosen;
	/* What in the answers points at this group, "the Japanese
	 * keyboard", or "" for nothing. */
	char why[64];
} KiNative;

extern KiNative ki_native[MAX_NATIVE];
extern int ki_nnative;
/* 1 when `kdos update` would find a binhost for the groups: update.conf's
 * `binhost =`, $KDOS_UPDATE_BINHOST, or a release named in the TREE stamp
 * beside the groups file. Read by probe_groups(). */
extern int ki_binhost_named;

void probe_groups(void);
/* Read the groups file and apply an answer file's `groups =`: the ids it
 * names, nothing for `none` or an empty line, an unknown id ignored. With no
 * `groups =` line, the language groups the answers point at are ticked, and
 * a later call applies only what changed in that answer since the last. */
void ki_groups_enter(void);
/* Why the keyboard (cfg.xkb_layout, cfg.keymap) or the time zone
 * (cfg.tz_label) points at language group `group`, as "the Japanese
 * keyboard" or "the time zone Asia/Tokyo"; "" when nothing does. */
void ki_lang_why(const char *group, char *out, size_t n);
/* cfg.groups rebuilt from the ticks, or `none`. */
void ki_groups_collect(void);
/* How many native groups are ticked. */
int  ki_groups_chosen(void);

void probe_part(const char *path, Part *p);
/* The facts of partition p (p->path its node) for the probe, given the
 * node's device number `node_mm` and sysfs's `sys_mm` ("8:2"): where they
 * agree, sniffed from the node, with udev's record run/udev/data/b<sys_mm>
 * under `root` ("" on a live system) for an exFAT label or a type no
 * signature matches; where they differ, p->remap names what the node points
 * at, read through sys/dev/block/<node_mm> and sys/class/block/<kname>/dm/name
 * under `root`, and the filesystem is read from `disk`, the whole disk, at
 * the partition's own offset p->start * 512. */
void ki_part_facts(Part *p, const char *node_mm, const char *sys_mm,
		   const char *root, const char *disk);
/* The medium id in the ISO 9660 image on the whole of `path` (its Volume Set
 * Identifier, 16 hex digits) into `out`: 1, or 0 when it carries none. A
 * `path` that cannot be opened is read from its udev record `rec` (its
 * ID_FS_PUBLISHER_ID); "" or NULL for none. */
int  ki_medium_id(const char *path, const char *rec, char *out, size_t cap);
/* Marks is_boot_copy on every disk, not is_boot_media, whose image carries
 * the id `cmdline`'s kdos.medium= names; none when it names none. `root`
 * stands for / under sysfs and udev's records ("" on a running system). */
void ki_mark_boot_copies(const char *cmdline, const char *root);
/* The table type of `d` into d->table ("gpt", "dos" or "-"), and the ESP
 * and BIOS boot partitions among the d->part[] already listed, matched by
 * start: GPT type GUIDs, or MBR primary entries typed 0xef; a remapped
 * partition is neither. A disk it cannot open keeps udev's table type. */
void ki_disk_table(Disk *d);
/* The whole-device signature of a disk into d->fstype and d->label, a
 * physical volume's group into d->vg (from the active volumes' `slaves` in
 * /sys/block, which needs no privilege, then `lvm pvs` as root), and an
 * ISO 9660 image's size into d->fs_bytes: its descriptor's, or where the
 * last partition ends when the device cannot be read. d->iso_image is set
 * when every partition is a hybrid window of that image: it holds nothing
 * but FAT or ISO 9660 (or no filesystem), and it ends inside the
 * descriptor's size, or, with the descriptor unread, some partition starts
 * inside the first MiB, where an image's windows do and no partitioning
 * tool puts one. probe_disks() calls it for every disk it lists, after
 * the partitions. */
void ki_disk_signature(Disk *d);
/* What an erase of `d` destroys, as the Summary names it: its partitions
 * as `<type> "<label>" <size>` joined by ", " (an LVM physical volume as
 * "LVM physical volume (volume group <vg>)", a remapped node's partition
 * as "Ventoy's boot partition <size>" or "mapped to <remap> <size>"), or
 * the whole-device
 * signature of a disk with none; a disk whose iso_image is set as
 * `ISO 9660 image "<label>" on the whole disk, <size>`, its hybrid
 * windows unnamed; "" for an empty disk. At most `max`
 * partitions are named (every one for max <= 0), and those past it or past
 * `cap` are counted in a closing ", and <n> more partitions"; an entry is
 * never cut. */
void ki_disk_contents(const Disk *d, int max, char *out, size_t cap);
/* The model udev's data file `data` (/run/udev/data/b<major>:<minor>)
 * records for a disk: E:ID_MODEL_ENC with its \xNN escapes decoded and
 * its blanks trimmed. It replaces `model`, which holds the sysfs model on
 * entry, only when it is longer and starts with it: sysfs's SCSI model is
 * the 16 bytes of the INQUIRY reply, cut mid-word for a longer ATA name.
 * Returns 1 when it replaced `model`. */
int ki_udev_model(const char *data, char *model, size_t cap);
Disk *disk_by_path(const char *path);
/* A disk's size in bytes. /sys/block reports `size` in 512-byte units
 * whatever the drive's logical sector is, so multiplying by sector_size
 * would make a 4Kn disk eight times larger than it is. */
unsigned long long ki_disk_bytes(const Disk *d);

/* ────────────────────────────────────────────────────────────────────────
 * Configuration — everything a front end collects, nothing written until
 * the install runs.
 * ──────────────────────────────────────────────────────────────────────── */

enum { PLAN_WIPE = 0, PLAN_REUSE, PLAN_MANUAL };
enum { SWAP_NONE = 0, SWAP_FILE, SWAP_PART };

typedef struct {
	char keymap[64];
	/* The answer named a console keymap. When it did not and it named an
	 * XKB layout, the console keymap is the one nearest that layout. */
	int keymap_set;
	/* The graphical session's layout, as XKB spells it, or "" for the one
	 * nearest the console keymap (kdos-settingsd derives it at run time).
	 * Letters, digits and `_+,:.-` only: the value lands in a file that is
	 * also valid shell. */
	char xkb_layout[64];
	char xkb_variant[64];

	/*
	 * SDDM LOGS THE ACCOUNT IN WITHOUT ASKING, and only on an encrypted
	 * root with one account (the A15 default): the passphrase typed at boot
	 * already proved who is at the machine. Anywhere else the login screen
	 * is shown, and an answer asking for autologin there is refused before
	 * anything is written. `autologin_lock` starts that session locked,
	 * which is the default; 0 is "no password after the disk passphrase".
	 */
	int autologin;
	int autologin_lock;
	char tz[80];
	char tz_label[64];

	char disk[64];
	int plan;
	char part_esp[64];
	/* A partition, or on the reuse plan a logical volume by its
	 * /dev/<vg>/<lv> path — which is why this is wider than the ESP's. */
	char part_root[160];
	int format_esp;
	char fstype[16];
	/* The erase plan on LVM: the root partition becomes the one physical
	 * volume of group KI_VG. Where the disk holds them, the group carries
	 * both A/B root slots, the swap and the shared home; see ki_ab(). */
	int lvm;
	int swap;
	long swap_mb;
	/* LUKS2 on the root partition. The passphrase never reaches argv, never
	 * reaches a dump, and never reaches the answer file — see conf.c. */
	int luks;
	char luks_pass[128];
	char luks_pass2[128];

	char hostname[64];
	/*
	 * NO ACCOUNT IS MADE: the first person to switch the machine on makes
	 * it in plasma-setup, which a boot step arms because
	 * /etc/plasma-setup-done is absent. The live account is removed from
	 * the copy, and the account fields below are not read.
	 */
	int oem;
	char username[33];
	char fullname[64];
	char userpass[128];
	char userpass2[128];
	int user_wheel;
	char rootpass[128];
	char rootpass2[128];
	int root_locked;

	char theme[16];
	/* The panel layout the first login lays out: kdos, dock or tiling. */
	char desk[16];
	int with_appbox;
	/* Which groups the answer file named, space separated. Empty means
	 * "whatever the page chose", which starts as `essential`; `none` is
	 * no group. Only group ids match; a line naming no group falls back to
	 * `essential`. */
	char apps[1024];
	/* Native group ids from /ports/groups, space-separated, or `none`;
	 * empty is none. See ki_groups_enter(). */
	char groups[1024];
	/* The answer file had a `groups =` line, so no language group is
	 * ticked for the person. */
	int groups_set;
	unsigned svc_off;	/* bitmask over ki_services                */

	int reboot_after;
	int dry_run;
} Config;

extern Config cfg;

typedef struct {
	const char *name;	/* matches /etc/init.d/NN_<name>.sh         */
	const char *label;
	const char *note;
	int def_on;
} Service;

extern const Service ki_services[];
extern int ki_nservices;

/*
 * The root filesystems the installer can create. One table, because every
 * consumer has to agree: the menu offers it, mkfs creates it, fstab describes
 * it and the swapfile step has to know what a swapfile means there.
 *
 * `passno` is 0 for everything but ext4 on purpose — there is no fsck.btrfs
 * worth running and no fsck.xfs at all, so a non-zero pass is an instruction
 * to run a checker that is not installed.
 *
 * `swapfile` is the trap. btrfs needs a NOCOW, uncompressed file, which is
 * what `btrfs filesystem mkswapfile` makes; swapon refuses a fallocate'd one
 * with EINVAL. xfs and f2fs get a file written out in full, which leaves no
 * unwritten extent for any kernel's swapon to judge; the KDOS kernel accepts
 * a fallocate'd file on both, so dd there is the kernel-independent choice.
 * ext4 takes the fast path.
 */
enum { KI_SWAPFILE_FALLOCATE = 0, KI_SWAPFILE_DD, KI_SWAPFILE_BTRFS };

typedef struct {
	const char *name;	/* ext4                                    */
	const char *mkfs;	/* mkfs.ext4                               */
	const char *force;	/* -F or -f: overwrite an existing fs      */
	const char *opts;	/* fstab mount options                     */
	int passno;		/* fstab fs_passno                         */
	int swapfile;		/* how a swap FILE has to be made here     */
	const char *note;
} Filesystem;

extern const Filesystem ki_filesystems[];
extern int ki_nfilesystems;
const Filesystem *ki_fs(const char *name);

/* The three panel layouts a first login can lay out, by the names
 * `kdosrc [Desk] Layout` and `kdos desk` take. ki_desks[0] is the default. */
extern const char *const ki_desks[];
extern const int ki_ndesks;
int ki_desk_ok(const char *name);

/*
 * THE ERASE PLAN'S VOLUME GROUP, and its volumes are named for the A/B slots.
 *
 * WHERE THE DISK HOLDS THEM, THE GROUP IS LAID OUT FOR UPDATES: slot A's root
 * KI_LV_A and slot B's KI_LV_B, each ki_slot_bytes(); the swap volume when
 * swap was asked for; and KI_LV_HOME, the rest, mounted at /home by both
 * slots. An update is written into the slot not running and the home is the
 * one thing both see, so a person's files survive every switch. One group
 * means one LUKS container and one passphrase for all of it.
 *
 * A DISK TOO SMALL FOR TWO SLOTS gets root_a alone, all of the group, with the
 * home on it, and no second slot: ki_ab() is that decision, and the Layout
 * page, every summary and the Format step all ask it, so what is shown is
 * what is created.
 */
#define KI_VG "kdos"
#define KI_LV_A "root_a"
#define KI_LV_B "root_b"
#define KI_LV_SWAP "swap"
#define KI_LV_HOME "home"

/*
 * WHAT BOTH SLOTS SHARE lives on the home volume: the homes themselves, and
 * the pack store, bound from here onto /var/lib/kdos/packs by fstab. A slot
 * is replaced whole by an update (a slot image, or packages into the other
 * root), and packs and boxes are never part of one; kept in a slot they
 * would be gone after the switch. Everything else a machine keeps in its
 * root — /etc, the state under /var — `kdos update` carries into the slot
 * it arms.
 */
#define KI_SHARED_PACKS "/home/.kdos/packs"

/*
 * THE ERASE PLAN'S LAYOUT, by partition number, so do_partition, the
 * Layout page, the Summary and `--dump plan` name the same partitions: the
 * ESP first, then a 1 MiB BIOS boot partition, then swap when it is a
 * partition, then the root. The ESP stays partition 1 and the root stays
 * the last, which is what a person reading `lsblk` expects of a disk.
 *
 * The BIOS boot partition is what lets one disk start on both firmwares.
 * On a GPT disk Limine's BIOS stage cannot live in the gap after the boot
 * sector, because the GPT itself is there; `limine bios-install` writes it
 * into this partition, raw, and refuses a GPT disk that has none. It needs
 * 32 KiB; 1 MiB keeps every later partition on a 1 MiB boundary.
 */
#define KI_ESP_MB 512
#define KI_BIOS_BOOT_MB 1
#define KI_PART_ESP 1
#define KI_PART_BIOS_BOOT 2
#define KI_GUID_BIOS_BOOT "21686148-6449-6E6F-744E-656564454649"
/* Partition n of a disk's node: sda -> sda1, nvme0n1 -> nvme0n1p1. One rule
 * for the partitioner and every page that previews it, so the two cannot
 * disagree. */
void ki_part_name(const char *disk, int n, char *out, size_t cap);
/* 3, or 0 when swap is not a partition. On the A/B layout the swap is a
 * volume in the group, never a partition. */
int ki_wipe_part_swap(void);
int ki_wipe_part_root(void);	/* the last partition: 4 or 3          */

/*
 * WHERE LIMINE'S BIOS STAGE GOES on the plan's disk: the number of its BIOS
 * boot partition (the erase plan's own, or one a reused disk already has),
 * 0 for the gap after a dos partition table's boot sector, or -1 for a GPT
 * disk with no BIOS boot partition — a disk that starts on UEFI only.
 */
int ki_bios_boot_part(void);

/* Bytes this install copies onto the root, as the Disk page states it. */
unsigned long long ki_need_bytes(void);
/* The headroom a whole disk must have beyond ki_need_bytes(): the ESP, the
 * BIOS boot partition and room for the copy's own metadata. */
#define KI_DISK_MARGIN (768ULL << 20)
/* The room the root must keep after the copy, in MiB: ext4's 5% reserve,
 * the journal, the logs and the first logins all take from it. A root that
 * holds the install and nothing more is full at the first login, and every
 * write in the session then fails with ENOSPC. */
#define KI_ROOT_FREE_MB 2048ULL
/* The smallest swap any front end offers, in MiB, and the step of the
 * graphical size field: a swap size the core advises is a multiple of it,
 * and a disk with less room than this beside the install is told to have
 * none, since the graphical field cannot hold a smaller size. */
#define KI_SWAP_MIN_MB 256ULL
/* The least size of a reused root: ki_need_bytes() and KI_ROOT_FREE_MB. */
unsigned long long ki_root_need_bytes(void);
/* What a whole disk must hold for one root: ki_root_need_bytes() and
 * KI_DISK_MARGIN. ki_disk_check(), the swap rule beside one root, the probe
 * dump and the Disk pages all read this one figure, so a disk is never drawn
 * as fitting that the install then refuses. */
unsigned long long ki_disk_need_bytes(void);
/* What a root keeps free once the install and a swap file in it are taken,
 * in bytes, for these answers: the one root of the erase plan or the reused
 * root. -1 on the A/B layout, whose slot is sized for updates and whose
 * home is a volume of its own, and when the root is not one the probe saw. */
long long ki_root_free_bytes(void);
/* Below this a front end warns on its Summary that the root will be too
 * small for application groups or boxed apps: the Plasma groups alone run
 * to gigabytes. */
#define KI_ROOT_LOW_BYTES (4096ULL << 20)
/* The erase plan's root partition, in bytes, for the disk and swap chosen. */
unsigned long long ki_wipe_root_bytes(void);
/* 1 when these answers lay the disk out for A/B updates: the erase plan, on
 * LVM, on a disk that holds two slots, the swap and a home of its own. */
int ki_ab(void);
/* One root slot on the A/B layout, in MiB: the install, half as much again
 * for an update to grow into, never under KI_SLOT_SHARE percent of the room
 * past the swap, nor under KI_SLOT_MIN_MB. */
#define KI_SLOT_MIN_MB 8192
/* Each slot's least share of the room, in percent: two slots take 60, the
 * home the rest. */
#define KI_SLOT_SHARE 30
/* The least a shared home is made at. Below it two slots do not fit. */
#define KI_HOME_MIN_MB 4096
unsigned long long ki_slot_mb(void);
/* What the erase plan's partition must hold for two slots, the swap volume
 * and the least home, in MiB, for these answers: what ki_ab() compares the
 * disk against. */
unsigned long long ki_ab_need_mb(void);
/* The swap volume on the A/B layout, in MiB; 0 when no swap was asked. */
unsigned long long ki_swap_lv_mb(void);

/* The keyboard the graphical session starts with, when the answers named
 * one: kxkbrc's lists, which the installer writes as /etc/xdg/kxkbrc. */
int ki_xkb_word_ok(const char *s);
/* A kbd keymap name: letters, digits and _ . + -, not starting with . or -,
 * at most 63 bytes. What the installer writes to EFI/kdos/keymap. */
int ki_keymap_name_ok(const char *s);
/* The console keymap nearest an XKB layout, out of the keymaps under
 * `share` (the live root's /usr/share), or "us" when none is installed.
 * `<layout>-latin1` is tried before `<layout>`, which kbd gives the 7-bit
 * national map. */
void ki_xkb_to_vt(const char *share, const char *layout, const char *variant,
		  char *out, size_t cap);

void conf_defaults(void);
/* "-" reads the answers from standard input, which is how a privileged
 * install is handed passwords without a file on disk holding them. */
int conf_load(const char *path);
int conf_save(const char *path);

/* A user name and a host name as useradd and hostname1 take them. */
int ki_name_ok(const char *s);
int ki_host_ok(const char *s);
/* A full name passwd's GECOS field can hold: no ':' and no control
 * character. */
int ki_fullname_ok(const char *s);
/* The accounts the image already carries, which a person may not take:
 * renaming the live account onto one would leave two lines of one name in
 * /etc/passwd. NULL-terminated; `--dump look` lists it for the window. */
extern const char *const ki_reserved_names[];
int ki_name_reserved(const char *s);

/*
 * EVERYTHING AN UNATTENDED INSTALL WOULD FAIL ON, asked before it starts. A
 * front end that is not the wizard walked no page validators, so the answers
 * it hands over are checked here, by the same rules, and refused with the
 * first problem's text. 0 when the answers can be installed.
 */
int ki_answers_check(char *err, size_t n);

/*
 * WHETHER A DISK CAN TAKE THE INSTALL AT ALL: not the medium this runs from
 * or a copy of it, not read-only, and at least ki_disk_need_bytes(). 0 when it can; otherwise the reason, which the Disk pages
 * show beside the disk.
 */
int ki_disk_check(const Disk *d, char *err, size_t n);

/*
 * THE TARGET AS THE PROBE SAW IT, checked against the answers: the disk by
 * ki_disk_check(), and on the reuse plan the ESP and the root. Neither may be
 * the BIOS boot partition or the other; an ESP kept unformatted must already
 * be FAT; the root may not be an LVM physical volume, a device something
 * holds open, or smaller than the install and KI_ROOT_FREE_MB; and a machine
 * started in legacy BIOS mode needs a disk it can start from. Needs probe_disks() and, for the
 * sizes, probe_payload(). On the erase plan without two slots, the swap
 * partition or file must fit beside the install: ki_disk_check()'s rule plus
 * the swap and, on LVM or LUKS, KI_VOLUME_META_MB of volume metadata; on the
 * reuse plan a swap file counts against the root. Asked after
 * ki_answers_check() by the wizard's
 * pages, `--dump plan`'s `refused`, `--events` and the Prepare step, so
 * every route into an install refuses the same targets.
 */
int ki_target_check(char *err, size_t n);
/* The swap part of ki_target_check() alone, for the chosen disk: non-zero,
 * with the refusal, when the erase plan's swap does not fit beside the
 * install. A front end shows it where the swap is chosen. */
int ki_swap_check(char *err, size_t n);

/* ────────────────────────────────────────────────────────────────────────
 * Install engine
 *
 * The work runs in a forked child that speaks a one-line protocol back over
 * a pipe, so the sequential shape of an install stays sequential code and
 * a front end stays a single-threaded loop.
 * ──────────────────────────────────────────────────────────────────────── */

/* What a copy's progress lines have said so far, zeroed when a copy
 * starts: the last file counts (`to-chk=left/total`), the samples the time
 * left is fitted on — seconds, bytes and files done, the files in a
 * profile's units, thinned to at most KI_COPY_SAMPLES spread over the whole
 * copy — the time left last shown with the seconds and the progress it was
 * shown at, whether a figure is showing (`counting`; none while the
 * estimate settles), when the bytes or files last advanced (`moved_el`),
 * the last line's estimate (`est`, which a tick counts down toward), the
 * estimates of the last KI_COPY_HIST slots of KI_COPY_HIST_S seconds, the
 * lowest and highest of each (`h_*`, `hn` slots filled in all), and the
 * fraction done last given. */
#define KI_COPY_SAMPLES 64
#define KI_COPY_HIST 32
#define KI_COPY_HIST_S 0.1

typedef struct {
	long left, total;
	int n;
	double step;
	double el[KI_COPY_SAMPLES], bytes[KI_COPY_SAMPLES],
		files[KI_COPY_SAMPLES];
	int shown_ok, counting;
	double shown, shown_el, shown_bytes, shown_files, moved_el;
	double est;
	int hn;
	double h_el[KI_COPY_HIST], h_lo[KI_COPY_HIST], h_hi[KI_COPY_HIST];
	double frac;
} KiCopyFiles;

/* The time left shown (install.c, THE TIME SHOWN). The selftest replays
 * the eight recorded installs in testing/fixtures/kinstall, each copy plain
 * and slowed two, three and five times, and one12's root copy re-timed to
 * two GUI installs on 12 GiB, through `harness copy` with its ticks, and
 * holds every frame to its bounds. Each value below is named with the
 * range, varied alone, on which every replay passes but e2e10-30's slot B,
 * and sits inside it away from its edges; past the range a replay fails as
 * said. e2e10-30's slot B passes on no value: its estimate stands at 1.3
 * to 1.4 times the real time from 13 s to 17 s of its 28, through a
 * stretch that copies slower than the rest, and the figure reads up to
 * 1.33 times the real time there, a bound of its own in the selftest.
 *
 * Until the estimate settles no figure is shown. It has settled once the
 * note's fraction, the seconds so far over those and the estimate, is
 * KI_COPY_START (0.04 to 0.12, every replay the same) and the estimates
 * that stood over the last KI_COPY_SETTLE_S seconds (1.25 to 3; at 1
 * fresh12's root copy slowed five times shows its first fits, low, and
 * stands at 0:00:01) lie within KI_COPY_SPREAD above the lowest of them
 * (0.1 to 1; at 0.075 the re-timed slow start of one12 is first shown at
 * its forced settle, at up to 1.9 times the real time), or once the
 * fraction is KI_COPY_SETTLED (0.12 to 0.2; at 0.11 fresh30's slot B
 * slowed five times is shown its first fits, low, and stands at 0:00:01),
 * which is before a fifth of the copy while the estimate is at most 1.31
 * times the real time left and past it for a higher one.
 * The estimate the countdown is set against is the highest of those that
 * stood over the last KI_COPY_EST_S seconds (0 to 0.2; at 0.3 fresh30's
 * slot B reads over the high bound). Above it the countdown falls to it,
 * at least as fast as the clock and at most KI_COPY_FAST times it (2 to
 * 30; at 1.5 the re-timed slow start reads over the high bound). Below it
 * the countdown runs at the clock while within KI_COPY_NEAR of it (0 to
 * 0.11), slower the further below, and at KI_COPY_SLOW of the clock from
 * KI_COPY_FAR below it on (0.1 to 0.2); at a NEAR of 0.12 or a FAR of 0.25
 * two30's slot B slowed five times, whose estimate is low through its
 * first half and then jumps, reads under the low floor. KI_COPY_SLOW is
 * above 1/(2 - KI_COPY_TICK_S), so no whole second stands for two seconds
 * while the copy moves, and passes up to 0.6 (at 0.62 two30's slot B
 * slowed five times reads under the low floor).
 * A tick comes each KI_COPY_TICK_S with no line, and a copy whose bytes and
 * files have not advanced for KI_COPY_QUIET_S, longer than the second rsync
 * leaves between the running lines of a large file, stands. */
#define KI_COPY_START 0.08
#define KI_COPY_SETTLE_S 2.0
#define KI_COPY_SPREAD 0.3
#define KI_COPY_SETTLED 0.16
#define KI_COPY_FAST 8.0
#define KI_COPY_EST_S 0.1
#define KI_COPY_NEAR 0.06
#define KI_COPY_FAR 0.16
#define KI_COPY_SLOW 0.58
#define KI_COPY_TICK_S 0.2
#define KI_COPY_QUIET_S 1.5
/* The span of samples, in seconds, the first fit needs: behind a listing
 * of several seconds a fifth of a short copy is past before two seconds of
 * samples are, and until the first fit no time left is shown. */
#define KI_COPY_SPAN_S 1.0
/* A sample KI_COPY_HALF_S seconds older than the newest weighs half as much
 * in the fit (10 to 30): at 7.5 s two30's slot B slowed five times reads
 * under the low floor and stands at 0:00:01. */
#define KI_COPY_HALF_S 20.0
/* The floors of the seconds a file and a byte cost, which hold the cost the
 * samples cannot yet tell apart from the other: below each cost on the live
 * medium's copy onto a virtual disk, about 0.35 ms a file and 4 ns a byte,
 * so that a faster machine is seldom shown more time than it takes. */
#define KI_COPY_FILE_S 100e-6
#define KI_COPY_BYTE_S 2e-9

/*
 * One rsync --info=progress2 line, `el` seconds into a copy of `total`
 * bytes (0 when not measured), of which `links` are names of hard-linked
 * files after their first, which rsync counts and never copies, as the Copy
 * step's note: "364 MiB of 4.1 GiB, 25 MiB/s, about 0:03:15 left", or
 * "…, estimating time left" while no figure is shown, and its fraction done
 * into *frac: of the time once there is an estimate (the time shown, or
 * the estimate while none is), of the bytes it copies before that, and
 * never less than the last one. `prof`
 * says where in the list of entries the cost lies (NULL: every entry a
 * file). `files` carries the file counts, the samples the time left is fitted on and the
 * time last shown from line to line; NULL reads the counts from this line
 * alone and fits nothing. 0 for a line that is not a progress line.
 */
int ki_copy_note(const char *line, double el, unsigned long long total,
		 unsigned long long links, const KiCopyProfile *prof,
		 KiCopyFiles *files, double *frac, char *note, size_t n);
/* The Copy step's note `el` seconds into the same copy when no line has
 * come for KI_COPY_TICK_S: the time left counted down from the last line's,
 * which the next line goes on from. 0, and nothing written, while no
 * figure is shown (THE TIME SHOWN in install.c) and once no progress has
 * come for KI_COPY_QUIET_S, when the time shown stands. */
int ki_copy_tick(double el, unsigned long long total,
		 unsigned long long links, KiCopyFiles *files, double *frac,
		 char *note, size_t n);

#define MAX_STEPS 16
#define LOG_LINES 4096
#define LOG_COLS 200

enum { ST_PENDING = 0, ST_RUNNING, ST_DONE, ST_FAIL, ST_SKIP };

typedef struct {
	const char *title;
	const char *detail;
	int state;
	double t0, t1;
	double frac;		/* -1 when indeterminate                   */
	/* The time it is expected to take, in tenths of a second: see THE
	 * STEPS' WEIGHTS in install.c. */
	int weight;
	char note[96];
} StepUi;

typedef struct {
	StepUi step[MAX_STEPS];
	int nsteps;
	int cur;
	int running;
	int failed;
	int done;
	char failmsg[256];
	/* Every warning the child sent: something the installed system will
	 * lack while the install carries on. Every front end lists them, and
	 * each is in the log with a `warning:` prefix. */
	char warn[8][LOG_COLS];
	int nwarn;
	pid_t pid;
	int fd;
	/* When the install first started (a retry keeps it), and when it
	 * finished or stopped: the terminal installer reports t1 - t0 once
	 * `done` or `failed` is set, so the running clock and the closing
	 * figure are one number. */
	double t0, t1;
	char (*log)[LOG_COLS];
	int nlog, logtop;
	int logfd;
} Install;

extern Install inst;

void install_plan(void);	/* fill the step table from cfg            */
void install_start(int from_step);
void install_pump(void);
void install_abort(void);
int install_child_main(int wfd, int from_step);	/* runs in the child       */
void install_log(const char *line);
const char *ki_state_name(int st);
/* <src> to <dst>, 0644, as the Bootloader step places a file on the ESP: 0,
 * or -1 when either cannot be opened, a read fails, a write is short or
 * fails, or the close fails (a full vfat reports it there). Each outcome is a
 * log line. A dry run copies nothing and returns 0. */
int ki_copy_file(const char *src, const char *dst);
/* 1 with the entry number in num when an efibootmgr line is a stale KDOS
 * entry (install.c, ki_nvram_stale). */
int ki_nvram_stale(const char *line, const char *byuuid, const char *esp,
		   char num[5]);

/* Each protocol line the child sends, before the step table takes it in: the
 * events stream's tap on the run. NULL leaves the table the only reader. */
extern void (*ki_line_hook)(const char *line);

/*
 * THE INSTALL WITH NO SCREEN: `--unattended --events json`. The plan, then
 * one JSON object per line on stdout for every protocol line the child sends,
 * then the exit status: 0 for a finished install, 1 for a failed one, 2 for
 * answers refused before anything was written. See events.c for the shape.
 */
int ki_events_run(void);

/* ────────────────────────────────────────────────────────────────────────
 * The files that make the installed machine a KDOS Plasma desktop
 *
 * Each writer takes the root it writes under — the target's mount point in an
 * install, a fixture directory in a test — and returns 0, or -1 with the
 * reason in `err`. They honour cfg.dry_run, naming each file and writing none.
 * ──────────────────────────────────────────────────────────────────────── */

/* Where a writer says what it wrote; NULL says nothing. */
extern void (*ki_log_hook)(const char *line);

/* A root command as the new account can run it, for the Packs and Firmware
 * warnings and the groups-pending header: "`sudo <cmd>`" for an
 * administrator, and for an account outside `wheel` (Administrator unticked),
 * which sudo refuses, "`su -c '<cmd>'` (root's password)" — that install has
 * a root password, since ki_answers_check refuses a locked root beside a
 * non-administrator. The string lives until the next call. */
const char *ki_root_cmd(const char *cmd);

/* /etc/sddm.conf.d/kde_settings.conf's [Autologin], the A15 lock, and
 * /etc/plasma-setup-done — present unless the install is an OEM one. */
int ki_write_login(const char *root, char *err, size_t n);
/* /etc/xdg/kdeglobals' global theme, /etc/xdg/kdosrc's desk, /etc/kdos/accent,
 * /etc/vtrgb and the login screen's set for the chosen accent. */
int ki_write_look(const char *root, char *err, size_t n);
/* The command that writes the chosen accent's home files into <root><home>:
 * `env -i` with that home's HOME and XDG directories and the target's
 * XDG_CONFIG_DIRS, then `<kdos> theme --account <accent>`. Nothing in it
 * reaches the live session or the live machine's accent. */
void ki_theme_argv(KbArgv *a, const char *kdos, const char *root,
		   const char *home);
/* Every entry under <path> to uid:gid, links themselves and never what they
 * name, on <path>'s own filesystem: 0, or -1 with errno. */
int ki_hand_tree(const char *path, uid_t uid, gid_t gid);
/* <user>'s numbers out of <root>/etc/passwd; 1000:1000, the live account's,
 * when it names none. */
void ki_account_ids(const char *root, const char *user, uid_t *uid,
		    gid_t *gid);
/* The account's half of the accent: the ki_theme_argv command through <run>
 * into <root>/etc/skel, and, unless the install is an OEM one, into
 * <root>/home/<username>, whose .config, .cache and .local are then handed to
 * the account by number. Each failure is a ki_log_hook line; the result is
 * how many of the runs failed. */
int ki_seed_accent(const char *root, const char *kdos,
		   int (*run)(const KbArgv *a));
/* /etc/keymap, and with an XKB layout /etc/kdos/xkb.conf and /etc/xdg/kxkbrc. */
int ki_write_keyboard(const char *root, char *err, size_t n);

/* ────────────────────────────────────────────────────────────────────────
 * Headless dumps — the machine as probed, the install as planned
 *
 * `what` is "probe", "plan", "apps" or "look"; `json` picks the rendering, not
 * a second pass. Every one runs before any front end takes the screen and
 * writes nothing to disk.
 * ──────────────────────────────────────────────────────────────────────── */

int ki_dump(const char *what, int json);
/* The text and the JSON `--dump probe` writes, for the machine already in
 * ki_sys and the disks and volumes already in ki_disk and ki_lv. */
void ki_dump_probe_text(void);
void ki_dump_probe_json(void);
/* WHAT THE COPY WRITES, in bytes, the total its progress line counts to:
 * the walk's payload less every name of a file after its first, which
 * rsync -H makes a link at no copy cost. The system's part (`store` 0) is
 * the whole copy without the box store, and the copy into slot B; the
 * store's (`store` 1) is what copying the store adds. -1 when the payload
 * was handed on (ki_dump_payload_set) rather than walked. */
long long ki_copy_bytes(int store);
/* The payload and box store a front end measured as root, in KiB, for every
 * later dump to take instead of its own walk (`--payload-kb`, `--appbox-kb`):
 * a walk as another user reads less of the tree. */
void ki_dump_payload_set(unsigned long long payload_kb,
			 unsigned long long appbox_kb);

#endif /* KINSTALL_CORE_H */

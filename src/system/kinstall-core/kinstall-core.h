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
	char table[8];		/* gpt, dos, -                             */
	Part part[MAX_PARTS];
	int nparts;
} Disk;

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
	unsigned long long mem_kb;
	char cpu[64];
	int cores;
	int live;		/* booted from the squashfs overlay        */
	unsigned long long payload_kb;	/* what an install will copy       */
	unsigned long long appbox_kb;	/* of which, the alien app store   */
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
/* The size of the tree an install copies, into ki_sys.payload_kb and
 * appbox_kb: a walk of the whole root filesystem, the one slow probe, and so
 * called on its own. */
void probe_payload(void);
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
 * whether or not any of its volumes is active. Runs `lvm pvs`. */
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

void probe_part(const char *path, Part *p);
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
 * BIOS boot partition and room for the copy's own metadata. Every front end
 * and the install itself read this one figure, so a disk is never drawn as
 * fitting that the install then refuses. */
#define KI_DISK_MARGIN (768ULL << 20)
/* The erase plan's root partition, in bytes, for the disk and swap chosen. */
unsigned long long ki_wipe_root_bytes(void);
/* 1 when these answers lay the disk out for A/B updates: the erase plan, on
 * LVM, on a disk that holds two slots, the swap and a home of its own. */
int ki_ab(void);
/* One root slot on the A/B layout, in MiB: the install, half as much again
 * for an update to grow into, never under KI_SLOT_MIN_MB. */
#define KI_SLOT_MIN_MB 8192
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
/* The console keymap nearest an XKB layout, out of the keymaps under
 * `share` (the live root's /usr/share), or "us" when none is installed. */
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
 * WHETHER A DISK CAN TAKE THE INSTALL AT ALL: not the medium this runs from,
 * not read-only, and at least ki_need_bytes() + KI_DISK_MARGIN. 0 when it
 * can; otherwise the reason, which the Disk pages show beside the disk.
 */
int ki_disk_check(const Disk *d, char *err, size_t n);

/*
 * THE TARGET AS THE PROBE SAW IT, checked against the answers: the disk by
 * ki_disk_check(), and on the reuse plan the ESP and the root. Neither may be
 * the BIOS boot partition or the other; an ESP kept unformatted must already
 * be FAT; the root may not be an LVM physical volume, a device something
 * holds open, or smaller than the install; and a machine started in legacy
 * BIOS mode needs a disk it can start from. Needs probe_disks() and, for the
 * sizes, probe_payload(). Asked after ki_answers_check() by the wizard's
 * pages, `--dump plan`'s `refused`, `--events` and the Prepare step, so
 * every route into an install refuses the same targets.
 */
int ki_target_check(char *err, size_t n);

/* ────────────────────────────────────────────────────────────────────────
 * Install engine
 *
 * The work runs in a forked child that speaks a one-line protocol back over
 * a pipe, so the sequential shape of an install stays sequential code and
 * a front end stays a single-threaded loop.
 * ──────────────────────────────────────────────────────────────────────── */

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
	double t0;
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

/* /etc/sddm.conf.d/kde_settings.conf's [Autologin], the A15 lock, and
 * /etc/plasma-setup-done — present unless the install is an OEM one. */
int ki_write_login(const char *root, char *err, size_t n);
/* /etc/xdg/kdeglobals' global theme, /etc/xdg/kdosrc's desk, /etc/kdos/accent,
 * /etc/vtrgb and the login screen's set for the chosen accent. */
int ki_write_look(const char *root, char *err, size_t n);
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

#endif /* KINSTALL_CORE_H */

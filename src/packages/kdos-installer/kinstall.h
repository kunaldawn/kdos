/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer
 *
 * The terminal, the cell buffer, the input layer and every widget live in
 * libktui now; the string, file and time helpers live in libkbase. What is
 * left here is the installer and nothing else: what it can see about the
 * machine, what the user decided, and how the work is carried out.
 * ---------------------------------
 */

#ifndef KINSTALL_H
#define KINSTALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "kbase.h"
#include "kcolor.h"	/* the scheme table, and which of its rows is the default */
#include "ktui.h"

#define KI_VERSION "4.0"

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
	int is_esp;
	int mounted;
	char mountpoint[64];
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

extern Disk ki_disk[MAX_DISKS];
extern int ki_ndisk;
extern SysInfo ki_sys;

void probe_system(void);
void probe_disks(void);

/* ────────────────────────────────────────────────────────────────────────
 * The applications this medium knows how to build.
 *
 * THE CATALOGUE IS READ, NOT A PACK INDEX. Nothing is baked onto the medium;
 * what ships is `/usr/share/kdos/appstore/catalogue`, and an application is
 * built by podman on the installed machine. The installer's job is to record
 * WHICH, and to install them itself only when it can.
 *
 * IT LINKS catalogue.c DIRECTLY rather than running `kdos-appbox`. kinstall
 * links libkbase, libktui and libkcolor and nothing else — which is what lets
 * it live in phase 1 and exist on every tree from the first bootable image —
 * and catalogue.c uses `kb_*` alone, so it costs no library. A live installer
 * also cannot assume anything is on `$PATH` in the target.
 *
 * THE UNIT IS A GROUP. A list of 182 applications is not a thing to page
 * through during an install; seven named bundles is. An application chosen by
 * id in an answer file still works — `cat_expand` takes either.
 * ──────────────────────────────────────────────────────────────────────── */

/* How the ticked set will actually be installed, decided at plan time and
 * SHOWN ON THE PAGE. A person must not discover at first boot that nothing
 * was installed. */
enum {
	APPS_NONE = 0,	/* nothing ticked                                  */
	APPS_IMPORT,	/* an exported set on a mounted device — offline   */
	APPS_NETWORK,	/* built during the install                        */
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
/* The archive the Applications page found, or "" — what makes APPS_IMPORT
 * possible. One archive is found at probe time and the page offers no way to
 * choose another: a picker would need a filesystem walk the paint path cannot
 * afford. */
extern char ki_apps_archive[512];

void probe_apps(void);
/* Bytes the chosen groups cost, as an estimate. What the Summary and
 * `--dump plan` report. */
unsigned long long ki_apps_bytes(void);
/* Which of the four routes this install will take, given what is ticked, what
 * is on the mounted devices and whether a network answers. */
int  ki_apps_route(void);
/* The Applications page's `enter`: read the catalogue and apply an answer
 * file's `apps =`. Called by the page, and by every path that plans an install
 * WITHOUT walking the wizard — `--dump plan` and `--unattended`. */
void ki_apps_enter(void);

void probe_part(const char *path, Part *p);
Disk *disk_by_path(const char *path);

/* ────────────────────────────────────────────────────────────────────────
 * Configuration — everything the wizard collects, nothing written until
 * the install page runs.
 * ──────────────────────────────────────────────────────────────────────── */

enum { PLAN_WIPE = 0, PLAN_REUSE, PLAN_MANUAL };
enum { SWAP_NONE = 0, SWAP_FILE, SWAP_PART };

typedef struct {
	char keymap[64];

	/* Whether tty1 logs the account in without asking. Default OFF for an
	 * install and on for the live medium: a machine with one account and no
	 * password has nothing to ask, and one somebody installed does. */
	int autologin;
	char tz[80];
	char tz_label[64];

	char disk[64];
	int plan;
	char part_esp[64];
	char part_root[64];
	int format_esp;
	char fstype[16];
	int swap;
	long swap_mb;
	/* LUKS2 on the root partition. The passphrase never reaches argv, never
	 * reaches a dump, and never reaches the answer file — see conf.c. */
	int luks;
	char luks_pass[128];
	char luks_pass2[128];

	char hostname[64];
	char username[33];
	char fullname[64];
	char userpass[128];
	char userpass2[128];
	int user_wheel;
	char rootpass[128];
	char rootpass2[128];
	int root_locked;

	char theme[16];
	int with_appbox;
	/* Which groups or applications the answer file named, space separated.
	 * Empty means "whatever the page chose", which starts as `essential`.
	 * A group id or an application id both work: cat_expand takes either. */
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
 * `swapfile` is the trap. `fallocate` produces unwritten extents, and swapon
 * refuses those on xfs ("swapfile has holes"); btrfs needs a NOCOW,
 * uncompressed file, which is what `btrfs filesystem mkswapfile` makes. Only
 * ext4 is happy with the fast path.
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

void conf_defaults(void);
int conf_load(const char *path);
int conf_save(const char *path);

/* ────────────────────────────────────────────────────────────────────────
 * Install engine
 *
 * The work runs in a forked child that speaks a one-line protocol back over
 * a pipe, so the sequential shape of an install stays sequential code and
 * the UI stays a single-threaded poll loop.
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

/* ────────────────────────────────────────────────────────────────────────
 * Headless dumps — the machine as probed, the install as planned
 *
 * `what` is "probe" or "plan"; `json` picks the rendering, not a second pass.
 * Both run before the terminal is taken over and write nothing to disk.
 * ──────────────────────────────────────────────────────────────────────── */

int ki_dump(const char *what, int json);

/* ────────────────────────────────────────────────────────────────────────
 * Pages
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct Page {
	const char *id;
	const char *title;
	const char *subtitle;
	void (*enter)(void);
	void (*draw)(KRect body);
	/* Return 0 to allow Next, or fill err and return non-zero. */
	int (*validate)(char *err, size_t n);
	int (*event)(KtuiEvent *ev);
	int hide_nav;
} Page;

extern Page *ki_pages[];
extern int ki_npages;
extern int ki_page;

int page_index(const char *id);
void page_goto(int n);
void nav_next(void);
void nav_back(void);

extern int ki_quit;
extern int ki_help;

#endif /* KINSTALL_H */

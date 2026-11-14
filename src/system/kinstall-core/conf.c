/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kinstall-core — the answers, and the layout they imply
 * ---------------------------------
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kinstall-core.h"

Config cfg;

/* Only the services a person would actually want to decide about. udev,
 * dbus, elogind and the rest are not choices, they are the system. */
const Service ki_services[] = {
	{ "networkmanager", "NetworkManager", "wired, wifi and VPN", 1 },
	{ "bluetooth", "Bluetooth", "bluez daemon", 1 },
	{ "alsa", "ALSA state", "restore mixer levels at boot", 1 },
	{ "cups", "CUPS", "printing daemon", 0 },
	{ "sshd", "OpenSSH server", "accepts remote logins", 0 },
};

int ki_nservices = (int)(sizeof(ki_services) / sizeof(ki_services[0]));

/*
 * ext4 first because it is the default and the one the kernel has built in.
 * btrfs and xfs are built in too; f2fs is a MODULE, which is why
 * 090_initramfs.sh carries it — a root the initramfs cannot mount is a machine
 * that installs fine and never boots again.
 */
const Filesystem ki_filesystems[] = {
	{ "ext4",  "mkfs.ext4",  "-F", "defaults,noatime", 1,
	  KI_SWAPFILE_FALLOCATE,
	  "the default: journalled, boring, and what the kernel has built in" },
	{ "btrfs", "mkfs.btrfs", "-f", "defaults,noatime,compress=zstd:3", 0,
	  KI_SWAPFILE_BTRFS,
	  "snapshots and transparent zstd compression" },
	{ "xfs",   "mkfs.xfs",   "-f", "defaults,noatime", 0,
	  KI_SWAPFILE_DD,
	  "large files and parallel IO; it cannot be shrunk" },
	/* fs_passno 0: there is no fsck.f2fs worth running at boot — f2fs.fsck
	 * exists but is a repair tool, not a boot-time check. The swapfile is
	 * dd'd for the same reason xfs's is: a file written out in full has no
	 * unwritten extent for any kernel's `swapon -a` to refuse at the NEXT
	 * boot, where a refusal leaves no swap and nothing saying why. */
	{ "f2fs",  "mkfs.f2fs",  "-f", "defaults,noatime", 0,
	  KI_SWAPFILE_DD,
	  "log-structured, for flash: a stick, an SD card or a cheap eMMC" },
};

int ki_nfilesystems = (int)(sizeof(ki_filesystems) / sizeof(ki_filesystems[0]));

const Filesystem *ki_fs(const char *name)
{
	for (int i = 0; i < ki_nfilesystems; i++)
		if (!strcmp(ki_filesystems[i].name, name))
			return &ki_filesystems[i];
	return &ki_filesystems[0];
}

/* The templates `org.kdos.desk.<name>` that kdos-plasma-look installs, in the
 * order every front end offers them. */
const char *const ki_desks[] = { "kdos", "dock", "tiling" };
const int ki_ndesks = (int)(sizeof(ki_desks) / sizeof(ki_desks[0]));

int ki_desk_ok(const char *name)
{
	for (int i = 0; i < ki_ndesks; i++)
		if (!strcmp(ki_desks[i], name))
			return 1;
	return 0;
}

unsigned long long ki_need_bytes(void)
{
	return (unsigned long long)(cfg.with_appbox
					    ? ki_sys.payload_kb
					    : ki_sys.payload_kb - ki_sys.appbox_kb) *
	       1024;
}

void ki_part_name(const char *disk, int n, char *out, size_t cap)
{
	size_t l = strlen(disk);
	int digit = l && isdigit((unsigned char)disk[l - 1]);

	snprintf(out, cap, "%s%s%d", disk, digit ? "p" : "", n);
}

int ki_wipe_part_swap(void)
{
	return cfg.swap == SWAP_PART && cfg.swap_mb > 0 && !ki_ab() ? 3 : 0;
}

int ki_wipe_part_root(void)
{
	return ki_wipe_part_swap() ? 4 : 3;
}

int ki_bios_boot_part(void)
{
	Disk *d;

	if (cfg.plan == PLAN_WIPE)
		return KI_PART_BIOS_BOOT;
	d = disk_by_path(cfg.disk);
	if (!d)
		return -1;
	for (int i = 0; i < d->nparts; i++)
		if (d->part[i].is_bios_boot && d->part[i].num > 0)
			return d->part[i].num;
	return !strcmp(d->table, "dos") ? 0 : -1;
}

/* The whole disk less the ESP and the BIOS boot partition: what the root
 * partition holds when nothing else is carved out of the disk. */
static unsigned long long wipe_rest_bytes(void)
{
	Disk *d = disk_by_path(cfg.disk);
	unsigned long long total;
	unsigned long long used = (unsigned long long)(KI_ESP_MB + KI_BIOS_BOOT_MB)
				  << 20;

	if (!d)
		return 0;
	total = ki_disk_bytes(d);
	return total > used ? total - used : 0;
}

/* The layout do_partition writes: the ESP, the BIOS boot partition, the swap
 * partition when one was asked for, and the rest. */
unsigned long long ki_wipe_root_bytes(void)
{
	unsigned long long rest = wipe_rest_bytes();

	if (ki_wipe_part_swap()) {
		unsigned long long swp = (unsigned long long)cfg.swap_mb << 20;
		return rest > swp ? rest - swp : 0;
	}
	return rest;
}

/*
 * A SLOT IS THE SYSTEM AND ROOM FOR ITS NEXT VERSION. An update is written
 * whole into the slot that is not running, so a slot made exactly the size
 * of today's install refuses the first update that grows. Half as much again
 * is the room; KI_SLOT_MIN_MB is the floor, for a payload that was not
 * measured or is small. The box store and every other file under /home live
 * on the shared home, not in a slot, so they are not counted here. Rounded up
 * to a whole GiB, which is how lvcreate's sizes read back.
 */
unsigned long long ki_slot_mb(void)
{
	unsigned long long kb = ki_sys.payload_kb > ki_sys.appbox_kb
					? ki_sys.payload_kb - ki_sys.appbox_kb
					: 0;
	unsigned long long mb = (kb + kb / 2) / 1024;

	if (mb < KI_SLOT_MIN_MB)
		mb = KI_SLOT_MIN_MB;
	return (mb + 1023) / 1024 * 1024;
}

unsigned long long ki_swap_lv_mb(void)
{
	return cfg.swap != SWAP_NONE && cfg.swap_mb > 0
		       ? (unsigned long long)cfg.swap_mb
		       : 0;
}

/*
 * TWO SLOTS, THE SWAP AND A HOME, OR ONE ROOT. The LUKS header and LVM's
 * metadata come out of the partition first, and 32 MiB covers both. The home
 * must hold the box store the copy carries into it as well as the floor.
 */
unsigned long long ki_ab_need_mb(void)
{
	unsigned long long home = KI_HOME_MIN_MB;

	if (cfg.with_appbox)
		home += ki_sys.appbox_kb / 1024;
	return 32 + 2 * ki_slot_mb() + ki_swap_lv_mb() + home;
}

int ki_ab(void)
{
	if (cfg.plan != PLAN_WIPE || !cfg.lvm)
		return 0;
	return (wipe_rest_bytes() >> 20) >= ki_ab_need_mb();
}

/* ──────────────────────────────────────────────────────────────────────── */

int ki_xkb_word_ok(const char *s)
{
	if (!*s)
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && !strchr("_+,:.-", *c))
			return 0;
	return 1;
}

/* A kbd keymap by name, compressed or not, anywhere under `dir`. kbd sorts
 * its maps into a tree of directories by keyboard type and layout family. */
static int keymap_in(const char *dir, const char *name, int depth)
{
	char **ents;
	int found = 0;
	size_t nl = strlen(name);

	if (depth > 4)
		return 0;
	ents = kb_listdir(dir, NULL);
	for (char **e = ents; e && *e && !found; e++) {
		char p[1024];

		if (!strncmp(*e, name, nl) && !strncmp(*e + nl, ".map", 4) &&
		    (!(*e)[nl + 4] || (*e)[nl + 4] == '.')) {
			found = 1;
			break;
		}
		snprintf(p, sizeof(p), "%s/%s", dir, *e);
		if (kb_is_dir(p) && !kb_is_link(p))
			found = keymap_in(p, name, depth + 1);
	}
	kb_strv_free(ents);
	return found;
}

static int keymap_installed(const char *share, const char *name)
{
	char p[512];

	snprintf(p, sizeof(p), "%s/keymaps", share);
	if (keymap_in(p, name, 0))
		return 1;
	snprintf(p, sizeof(p), "%s/kbd/keymaps", share);
	return keymap_in(p, name, 0);
}

/*
 * THE CONSOLE MAP NEAREST AN XKB LAYOUT, for the text consoles and the
 * recovery login: the layout's own variant first where kbd has a map of that
 * shape, then the layouts kbd names differently, then `<layout>` and
 * `<layout>-latin1`, which is how kbd names most of the rest. Each candidate
 * must be installed; a layout with no console map at all leaves `us`, which
 * every kbd carries, rather than a name loadkeys cannot load.
 */
void ki_xkb_to_vt(const char *share, const char *layout, const char *variant,
		  char *out, size_t cap)
{
	static const struct {
		const char *layout, *variant, *vt;
	} map[] = {
		{ "us", "dvorak", "dvorak" },
		{ "us", "colemak", "en-latin9" },
		{ "us", "intl", "us-acentos" },
		{ "de", "nodeadkeys", "de-latin1-nodeadkeys" },
		{ "de", "neo", "neo" },
		{ "fr", "bepo", "fr-bepo" },
		{ "gb", "", "uk" },
		{ "ee", "", "et" },
		{ "ca", "", "cf" },
		{ "latam", "", "la-latin1" },
		{ "se", "", "sv-latin1" },
		{ "ch", "fr", "fr_CH" },
		{ "ch", "", "sg" },
		{ "si", "", "slovene" },
		{ "hr", "", "croat" },
		{ "rs", "latin", "sr-latin" },
		{ "rs", "", "sr-cy" },
		{ "kz", "", "kazakh" },
		{ "kg", "", "kyrgyz" },
		{ "ir", "", "fa" },
	};
	char cand[96];

	kb_strlcpy(out, "us", cap);
	if (!layout || !*layout || !ki_xkb_word_ok(layout))
		return;
	if (!variant)
		variant = "";
	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (strcmp(map[i].layout, layout))
			continue;
		if (map[i].variant[0] && strcmp(map[i].variant, variant))
			continue;
		if (keymap_installed(share, map[i].vt)) {
			kb_strlcpy(out, map[i].vt, cap);
			return;
		}
	}
	if (keymap_installed(share, layout)) {
		kb_strlcpy(out, layout, cap);
		return;
	}
	snprintf(cand, sizeof(cand), "%s-latin1", layout);
	if (keymap_installed(share, cand))
		kb_strlcpy(out, cand, cap);
}

/* ──────────────────────────────────────────────────────────────────────── */

void conf_defaults(void)
{
	memset(&cfg, 0, sizeof(cfg));
	kb_strlcpy(cfg.keymap, "us", sizeof(cfg.keymap));
	/* The colon form, not a rules string: musl reads the zone FILE when
	 * `TZ` starts with a colon, so this cannot name different rules from
	 * the `/etc/localtime` the same install writes. */
	kb_strlcpy(cfg.tz, ":/etc/localtime", sizeof(cfg.tz));
	kb_strlcpy(cfg.tz_label, "UTC", sizeof(cfg.tz_label));
	kb_strlcpy(cfg.fstype, "ext4", sizeof(cfg.fstype));
	kb_strlcpy(cfg.hostname, "kdos", sizeof(cfg.hostname));
	kb_strlcpy(cfg.username, "kdos", sizeof(cfg.username));
	kb_strlcpy(cfg.fullname, "KDOS User", sizeof(cfg.fullname));
	kb_strlcpy(cfg.theme, KCOL_DEFAULT_NAME, sizeof(cfg.theme));
	kb_strlcpy(cfg.desk, ki_desks[0], sizeof(cfg.desk));
	cfg.plan = PLAN_WIPE;
	cfg.format_esp = 1;
	/* THE A/B LAYOUT IS THE DEFAULT. Updates are written into the slot not
	 * running, and only the erase plan on LVM makes the second slot; a
	 * default without it is a machine whose first update has nowhere to
	 * go. */
	cfg.lvm = 1;
	cfg.swap = SWAP_FILE;
	cfg.swap_mb = 4096;
	cfg.autologin_lock = 1;
	cfg.user_wheel = 1;
	cfg.root_locked = 1;
	cfg.with_appbox = 1;
	cfg.reboot_after = 1;

	for (int i = 0; i < ki_nservices; i++)
		if (!ki_services[i].def_on)
			cfg.svc_off |= 1u << i;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int yes(const char *v)
{
	return !strcmp(v, "yes") || !strcmp(v, "1") || !strcmp(v, "true");
}

/* A `disk =` answer longer than cfg.disk holds. Cut short it names another
 * path, which the install would report as gone away, or worse find; the
 * answers check refuses it by name instead. */
static int disk_cut;

/* -1 for a key this installer does not know. */
static int set_kv(const char *k, const char *v)
{
	if (!strcmp(k, "keymap")) {
		kb_strlcpy(cfg.keymap, v, sizeof(cfg.keymap));
		cfg.keymap_set = 1;
	} else if (!strcmp(k, "xkb_layout"))
		kb_strlcpy(cfg.xkb_layout, v, sizeof(cfg.xkb_layout));
	else if (!strcmp(k, "xkb_variant"))
		kb_strlcpy(cfg.xkb_variant, v, sizeof(cfg.xkb_variant));
	else if (!strcmp(k, "autologin"))
		cfg.autologin = yes(v);
	else if (!strcmp(k, "autologin_lock"))
		cfg.autologin_lock = yes(v);
	else if (!strcmp(k, "timezone"))
		kb_strlcpy(cfg.tz, v, sizeof(cfg.tz));
	else if (!strcmp(k, "timezone_label"))
		kb_strlcpy(cfg.tz_label, v, sizeof(cfg.tz_label));
	else if (!strcmp(k, "disk")) {
		kb_strlcpy(cfg.disk, v, sizeof(cfg.disk));
		disk_cut = strlen(v) >= sizeof(cfg.disk);
	}
	else if (!strcmp(k, "plan"))
		cfg.plan = !strcmp(v, "reuse") ? PLAN_REUSE
			 : !strcmp(v, "manual") ? PLAN_MANUAL : PLAN_WIPE;
	else if (!strcmp(k, "esp"))
		kb_strlcpy(cfg.part_esp, v, sizeof(cfg.part_esp));
	else if (!strcmp(k, "root"))
		kb_strlcpy(cfg.part_root, v, sizeof(cfg.part_root));
	else if (!strcmp(k, "format_esp"))
		cfg.format_esp = atoi(v);
	else if (!strcmp(k, "fstype"))
		/* ki_fs falls back to ext4, so an answer file naming a
		 * filesystem this build cannot create installs a working
		 * machine rather than failing at the mkfs. */
		kb_strlcpy(cfg.fstype, ki_fs(v)->name, sizeof(cfg.fstype));
	else if (!strcmp(k, "swap"))
		cfg.swap = !strcmp(v, "file") ? SWAP_FILE
			 : !strcmp(v, "partition") ? SWAP_PART : SWAP_NONE;
	else if (!strcmp(k, "swap_mb"))
		cfg.swap_mb = atol(v);
	else if (!strcmp(k, "luks"))
		cfg.luks = atoi(v);
	else if (!strcmp(k, "lvm"))
		cfg.lvm = atoi(v);
	else if (!strcmp(k, "luks_passphrase")) {
		/* Accepted for an unattended install, exactly like `password=`,
		 * and written back no more than that one is. A passphrase on the
		 * install medium is the operator's decision to make, not ours to
		 * make for them by refusing. The confirmation is the same value:
		 * an answer file has nobody to mistype it twice. */
		kb_strlcpy(cfg.luks_pass, v, sizeof(cfg.luks_pass));
		kb_strlcpy(cfg.luks_pass2, v, sizeof(cfg.luks_pass2));
	} else if (!strcmp(k, "hostname"))
		kb_strlcpy(cfg.hostname, v, sizeof(cfg.hostname));
	else if (!strcmp(k, "oem"))
		cfg.oem = yes(v);
	else if (!strcmp(k, "username"))
		kb_strlcpy(cfg.username, v, sizeof(cfg.username));
	else if (!strcmp(k, "fullname"))
		kb_strlcpy(cfg.fullname, v, sizeof(cfg.fullname));
	else if (!strcmp(k, "password")) {
		kb_strlcpy(cfg.userpass, v, sizeof(cfg.userpass));
		kb_strlcpy(cfg.userpass2, v, sizeof(cfg.userpass2));
	} else if (!strcmp(k, "admin"))
		cfg.user_wheel = yes(v);
	else if (!strcmp(k, "root_password")) {
		kb_strlcpy(cfg.rootpass, v, sizeof(cfg.rootpass));
		kb_strlcpy(cfg.rootpass2, v, sizeof(cfg.rootpass2));
	} else if (!strcmp(k, "root_locked"))
		cfg.root_locked = atoi(v);
	else if (!strcmp(k, "theme"))
		kb_strlcpy(cfg.theme, v, sizeof(cfg.theme));
	else if (!strcmp(k, "desk"))
		kb_strlcpy(cfg.desk, v, sizeof(cfg.desk));
	else if (!strcmp(k, "alien_apps"))
		cfg.with_appbox = atoi(v);
	/* Space-separated catalogue group ids, or `none`. A line none of whose
	 * ids names a group falls back to `essential` — see ki_apps_enter();
	 * refusing would leave a machine with no operating system over a
	 * misspelling. */
	else if (!strcmp(k, "apps"))
		kb_strlcpy(cfg.apps, v, sizeof(cfg.apps));
	/* Native group ids, or `none`; an id no group has is ignored, since
	 * a group is something extra and the system installs without it. */
	else if (!strcmp(k, "groups")) {
		kb_strlcpy(cfg.groups, v, sizeof(cfg.groups));
		cfg.groups_set = 1;
	}
	else if (!strcmp(k, "reboot"))
		cfg.reboot_after = atoi(v);
	else if (!strcmp(k, "services")) {
		cfg.svc_off = 0;
		for (int i = 0; i < ki_nservices; i++) {
			const char *p = strstr(v, ki_services[i].name);
			if (!p)
				cfg.svc_off |= 1u << i;
		}
	} else
		return -1;
	return 0;
}

/* Standard input, whole, into `buf`. An answer longer than the buffer is
 * refused rather than read in part: a cut line is a key with half a value. */
static int read_stdin(char *buf, size_t cap)
{
	size_t n = 0;

	for (;;) {
		ssize_t r;

		if (n + 1 >= cap) {
			errno = EFBIG;
			return -1;
		}
		r = read(0, buf + n, cap - n - 1);
		if (r < 0 && errno == EINTR)
			continue;
		if (r < 0)
			return -1;
		if (r == 0)
			break;
		n += (size_t)r;
	}
	buf[n] = 0;
	return (int)n;
}

int conf_load(const char *path)
{
	char buf[8192];
	int rc = !strcmp(path, "-") ? read_stdin(buf, sizeof(buf))
				    : kb_read_file(path, buf, sizeof(buf));

	if (rc < 0)
		return -1;

	char *save = NULL;
	for (char *line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		while (*line == ' ' || *line == '\t')
			line++;
		if (!*line || *line == '#')
			continue;
		char *eq = strchr(line, '=');
		if (!eq)
			continue;
		*eq = 0;
		char *k = line, *v = eq + 1;
		size_t n = strlen(k);
		while (n && (k[n - 1] == ' ' || k[n - 1] == '\t'))
			k[--n] = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		n = strlen(v);
		while (n && (v[n - 1] == ' ' || v[n - 1] == '\r'))
			v[--n] = 0;
		/* Named, not refused: the key still does nothing, but an answer
		 * file whose key was misspelt or renamed otherwise installs the
		 * default for it with no sign that the line was never read. */
		if (set_kv(k, v) < 0)
			fprintf(stderr, "kinstall: %s: unknown key '%s' ignored\n",
				!strcmp(path, "-") ? "standard input" : path, k);
	}
	/* The secrets leave the stack copy: cfg holds the one copy crypt()
	 * and cryptsetup need. */
	memset(buf, 0, sizeof(buf));
	return 0;
}

int conf_save(const char *path)
{
	char buf[4096], svc[256] = "";
	for (int i = 0; i < ki_nservices; i++) {
		if (cfg.svc_off & (1u << i))
			continue;
		if (svc[0])
			strncat(svc, " ", sizeof(svc) - strlen(svc) - 1);
		strncat(svc, ki_services[i].name, sizeof(svc) - strlen(svc) - 1);
	}

	snprintf(buf, sizeof(buf),
		 "# KDOS installer answer file\n"
		 "# kinstall --config <this file> [--unattended]\n"
		 "#\n"
		 "# Passwords are deliberately NOT written here — including the\n"
		 "# LUKS passphrase. Set them with password= / root_password= /\n"
		 "# luks_passphrase= yourself if you accept a plaintext credential\n"
		 "# on the medium this file lives on.\n"
		 "\n"
		 "keymap         = %s\n"
		 "xkb_layout     = %s\n"
		 "xkb_variant    = %s\n"
		 "timezone       = %s\n"
		 "timezone_label = %s\n"
		 "\n"
		 "disk           = %s\n"
		 "plan           = %s\n"
		 "esp            = %s\n"
		 "root           = %s\n"
		 "format_esp     = %d\n"
		 "fstype         = %s\n"
		 "swap           = %s\n"
		 "swap_mb        = %ld\n"
		 "luks           = %d\n"
		 "lvm            = %d\n"
		 "\n"
		 "hostname       = %s\n"
		 "oem            = %d\n"
		 "username       = %s\n"
		 "fullname       = %s\n"
		 "admin          = %d\n"
		 "root_locked    = %d\n"
		 "autologin      = %s\n"
		 "autologin_lock = %d\n"
		 "\n"
		 "theme          = %s\n"
		 "desk           = %s\n"
		 "alien_apps     = %d\n"
		 "apps           = %s\n"
		 "groups         = %s\n"
		 "services       = %s\n"
		 "reboot         = %d\n",
		 cfg.keymap, cfg.xkb_layout, cfg.xkb_variant, cfg.tz,
		 cfg.tz_label,
		 cfg.disk,
		 cfg.plan == PLAN_REUSE ? "reuse"
					: cfg.plan == PLAN_MANUAL ? "manual" : "wipe",
		 cfg.part_esp, cfg.part_root, cfg.format_esp, cfg.fstype,
		 cfg.swap == SWAP_FILE ? "file"
				       : cfg.swap == SWAP_PART ? "partition" : "none",
		 cfg.swap_mb, cfg.luks, cfg.lvm,
		 cfg.hostname, cfg.oem, cfg.username, cfg.fullname,
		 cfg.user_wheel, cfg.root_locked,
		 cfg.autologin ? "yes" : "no", cfg.autologin_lock,
		 cfg.theme, cfg.desk, cfg.with_appbox, cfg.apps,
		 cfg.groups[0] ? cfg.groups : "none", svc,
		 cfg.reboot_after);

	return kb_write_file(path, buf);
}

/* ──────────────────────────────────────────────────────────────────────── */

int ki_name_ok(const char *s)
{
	if (!*s || isdigit((unsigned char)*s) || strlen(s) > 32)
		return 0;
	for (const char *c = s; *c; c++)
		if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) &&
		    *c != '_' && *c != '-')
			return 0;
	return 1;
}

int ki_host_ok(const char *s)
{
	if (!*s || *s == '-' || strlen(s) > 63)
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '-')
			return 0;
	return 1;
}

/* The full name lands in passwd's fifth field, so a ':' would split the line
 * into one field too many and a control character would end it early; either
 * leaves an /etc/passwd that logs nobody in. */
int ki_fullname_ok(const char *s)
{
	for (const unsigned char *c = (const unsigned char *)s; *c; c++)
		if (*c == ':' || *c < 0x20 || *c == 0x7f)
			return 0;
	return 1;
}

/*
 * THE ZONE IS A PATH AND THE TZ STRING IS SHELL. The label names a file under
 * /usr/share/zoneinfo, so it is a zone name's characters and never `..`; the
 * TZ string is written between single quotes into a profile script every
 * login shell sources, root's included, so a quote or a line break in it
 * would be a command.
 */
static int tz_ok(const char *tz, const char *label)
{
	for (const unsigned char *c = (const unsigned char *)tz; *c; c++)
		if (*c == '\'' || *c < 0x20 || *c == 0x7f)
			return 0;
	if (label[0] == '/' || strstr(label, ".."))
		return 0;
	for (const char *c = label; *c; c++)
		if (!isalnum((unsigned char)*c) && !strchr("/_+-", *c))
			return 0;
	return 1;
}

const char *const ki_reserved_names[] = {
	"root", "bin", "daemon", "sys", "adm", "nobody", "messagebus",
	"sddm", "polkitd", "sshd", "plasma-setup", NULL,
};

int ki_name_reserved(const char *s)
{
	for (int i = 0; ki_reserved_names[i]; i++)
		if (!strcmp(s, ki_reserved_names[i]))
			return 1;
	return 0;
}

int ki_answers_check(char *err, size_t n)
{
	if (!cfg.disk[0]) {
		snprintf(err, n, "no target disk was named");
		return 1;
	}
	if (disk_cut) {
		snprintf(err, n, "the target disk's path is longer than %zu "
				 "bytes", sizeof(cfg.disk) - 1);
		return 1;
	}
	if (!kcol_find(cfg.theme)) {
		snprintf(err, n, "'%s' is not an accent", cfg.theme);
		return 1;
	}
	if (!ki_desk_ok(cfg.desk)) {
		snprintf(err, n, "'%s' is not a desk (kdos, dock or tiling)",
			 cfg.desk);
		return 1;
	}
	if ((cfg.xkb_layout[0] && !ki_xkb_word_ok(cfg.xkb_layout)) ||
	    (cfg.xkb_variant[0] && !ki_xkb_word_ok(cfg.xkb_variant))) {
		snprintf(err, n, "the keyboard layout holds a character XKB "
				 "does not name layouts with");
		return 1;
	}
	if (!ki_host_ok(cfg.hostname)) {
		snprintf(err, n, "hostname must be letters, digits and dashes");
		return 1;
	}
	if (!tz_ok(cfg.tz, cfg.tz_label)) {
		snprintf(err, n, "'%s' is not a time zone", cfg.tz_label);
		return 1;
	}
	if (cfg.luks && !cfg.luks_pass[0]) {
		snprintf(err, n, "the encrypted root has no passphrase");
		return 1;
	}
	if (cfg.luks && strcmp(cfg.luks_pass, cfg.luks_pass2)) {
		snprintf(err, n, "the two passphrases do not match");
		return 1;
	}
	if (cfg.plan != PLAN_WIPE && (!cfg.part_root[0] || !cfg.part_esp[0])) {
		snprintf(err, n, "the reuse plan names no %s",
			 cfg.part_root[0] ? "ESP" : "root");
		return 1;
	}
	if (cfg.swap != SWAP_NONE && cfg.swap_mb <= 0) {
		snprintf(err, n, "swap size must be greater than zero");
		return 1;
	}
	/* ROOT BEFORE THE ACCOUNT, OEM OR NOT: an unlocked root with no
	 * password would be hashed from the empty string, and the installed
	 * machine would open a root login to anybody at its console. */
	if (!cfg.root_locked && !cfg.rootpass[0]) {
		snprintf(err, n, "set a root password, or lock the account");
		return 1;
	}
	if (!cfg.root_locked && strcmp(cfg.rootpass, cfg.rootpass2)) {
		snprintf(err, n, "the two root passwords do not match");
		return 1;
	}
	if (cfg.oem) {
		if (cfg.autologin) {
			snprintf(err, n, "an OEM install has no account to "
					 "log in automatically");
			return 1;
		}
		return 0;
	}
	if (!ki_name_ok(cfg.username) || ki_name_reserved(cfg.username)) {
		snprintf(err, n, "'%s' cannot be a user name", cfg.username);
		return 1;
	}
	if (!ki_fullname_ok(cfg.fullname)) {
		snprintf(err, n, "a full name cannot hold ':' or a control "
				 "character");
		return 1;
	}
	if (!cfg.userpass[0]) {
		snprintf(err, n, "set a password for %s", cfg.username);
		return 1;
	}
	if (strcmp(cfg.userpass, cfg.userpass2)) {
		snprintf(err, n, "the two passwords do not match");
		return 1;
	}
	if (!cfg.user_wheel && cfg.root_locked) {
		snprintf(err, n, "root is locked and %s is not an administrator "
				 "— nobody could ever gain privileges",
			 cfg.username);
		return 1;
	}
	/* AUTOLOGIN ONLY WHERE THE BOOT ALREADY ASKED WHO IT IS. The disk's
	 * passphrase is that question on an encrypted root; anywhere else an
	 * automatic login is a machine that opens for whoever switches it
	 * on, and the login screen is shown instead. */
	if (cfg.autologin && !cfg.luks) {
		snprintf(err, n, "automatic login is offered only on an "
				 "encrypted root");
		return 1;
	}
	return 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int ki_disk_check(const Disk *d, char *err, size_t n)
{
	unsigned long long need = ki_need_bytes() + KI_DISK_MARGIN;

	if (d->is_boot_media) {
		snprintf(err, n, "%s is the medium this runs from", d->path);
		return 1;
	}
	if (d->readonly) {
		snprintf(err, n, "%s is read-only", d->path);
		return 1;
	}
	if (ki_disk_bytes(d) < need) {
		snprintf(err, n, "%s holds %s; the install needs at least %s",
			 d->path, kb_human_size(ki_disk_bytes(d)),
			 kb_human_size(need));
		return 1;
	}
	return 0;
}

/* A partition of any disk the probe saw, by its node. */
static const Part *part_by_path(const char *path)
{
	for (int i = 0; i < ki_ndisk; i++)
		for (int k = 0; k < ki_disk[i].nparts; k++)
			if (!strcmp(ki_disk[i].part[k].path, path))
				return &ki_disk[i].part[k];
	return NULL;
}

/*
 * THE REUSE PLAN'S TWO CHOICES. Each rule is one the install would otherwise
 * break after the point of no return: mkfs on the BIOS boot partition leaves
 * a disk that no longer starts on legacy BIOS, mkfs on a physical volume or
 * on a held device fails (or, on a PV it did not fail on, destroys a volume
 * group), and a root smaller than the install fails part way through the
 * copy.
 */
static int reuse_check(char *err, size_t n)
{
	const Part *esp, *rp;
	const Lv *l;
	unsigned long long sectors;
	const char *fstype;
	int held;

	if (!strcmp(cfg.part_esp, cfg.part_root)) {
		snprintf(err, n, "the ESP and root cannot be the same partition");
		return 1;
	}
	esp = part_by_path(cfg.part_esp);
	if (!esp) {
		snprintf(err, n, "%s is not a partition on this machine",
			 cfg.part_esp);
		return 1;
	}
	if (esp->is_bios_boot) {
		snprintf(err, n, "%s is the BIOS boot partition — pick the EFI "
				 "system partition", esp->path);
		return 1;
	}
	if (esp->held) {
		snprintf(err, n, "%s is in use by an open container or volume "
				 "group — it cannot be the ESP", esp->path);
		return 1;
	}
	if (!cfg.format_esp && strcmp(esp->fstype, "vfat")) {
		snprintf(err, n, "%s holds no FAT filesystem — format it, or "
				 "pick the EFI system partition", esp->path);
		return 1;
	}

	l = lv_by_path(cfg.part_root);
	rp = l ? NULL : part_by_path(cfg.part_root);
	if (l) {
		sectors = l->sectors;
		fstype = l->fstype;
		held = l->held;
	} else if (rp) {
		if (rp->is_bios_boot) {
			snprintf(err, n, "%s is the BIOS boot partition — pick "
					 "another root", rp->path);
			return 1;
		}
		if (rp->is_esp) {
			snprintf(err, n, "%s is an EFI system partition — pick "
					 "another root", rp->path);
			return 1;
		}
		sectors = rp->sectors;
		fstype = rp->fstype;
		held = rp->held;
	} else {
		snprintf(err, n, "%s is not a partition or logical volume on "
				 "this machine", cfg.part_root);
		return 1;
	}
	if (!strcmp(fstype, "LVM2_member")) {
		snprintf(err, n, "%s is an LVM physical volume — pick one of its "
				 "logical volumes as the root", cfg.part_root);
		return 1;
	}
	if (held) {
		snprintf(err, n, "%s is in use by an open container or volume "
				 "group — close it first", cfg.part_root);
		return 1;
	}
	/* Partition and volume sizes are in /sys's 512-byte units. */
	if (sectors * 512ULL < ki_need_bytes()) {
		snprintf(err, n, "%s holds %s, smaller than the %s this install "
				 "needs",
			 cfg.part_root, kb_human_size(sectors * 512ULL),
			 kb_human_size(ki_need_bytes()));
		return 1;
	}
	/* A disk that could not start on this machine is refused before the
	 * root is formatted. On UEFI it is a note: the disk still starts. */
	if (!ki_sys.uefi && ki_bios_boot_part() < 0) {
		snprintf(err, n, "this machine starts in legacy BIOS mode and %s "
				 "has no BIOS boot partition — add a 1 MiB one, or "
				 "erase the disk",
			 cfg.disk);
		return 1;
	}
	return 0;
}

int ki_target_check(char *err, size_t n)
{
	Disk *d = disk_by_path(cfg.disk);

	if (!d) {
		snprintf(err, n, "%s is not a disk on this machine", cfg.disk);
		return 1;
	}
	if (ki_disk_check(d, err, n))
		return 1;
	if (cfg.luks && !kb_have_prog("cryptsetup")) {
		snprintf(err, n, "this image has no cryptsetup — it cannot "
				 "create an encrypted root");
		return 1;
	}
	if (cfg.plan == PLAN_REUSE)
		return reuse_check(err, n);
	if (cfg.plan == PLAN_WIPE && cfg.lvm) {
		if (!kb_have_prog("lvm")) {
			snprintf(err, n, "this image has no lvm — it cannot put "
					 "the root on LVM");
			return 1;
		}
		/* THE GROUP NAME IS FIXED, so one of that name with a physical
		 * volume off the target disk stops vgcreate after this disk
		 * was erased. One wholly on the target is erased with it. */
		if (ki_vg_off_disk(KI_VG, d->name)) {
			snprintf(err, n, "a volume group named " KI_VG " is "
					 "already on another disk — rename it "
					 "with vgrename, or leave LVM off");
			return 1;
		}
	}
	return 0;
}

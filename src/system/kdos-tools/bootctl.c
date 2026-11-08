/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-bootctl — two root slots, and a boot that can change its mind
 *
 *   kdos-bootctl status [--json]       what the state file says, and which
 *                                      version each slot holds
 *   kdos-bootctl list [--json[=short|pretty]]
 *                                      the boot entries, one per slot, in the
 *                                      shape `bootctl list --json` prints
 *   kdos-bootctl set-slot a <uuid> [<luks-uuid>]
 *                                      record where a slot lives, and which
 *                                      container it lives inside
 *   kdos-bootctl crypt <fs-uuid>       the container UUID that filesystem is
 *                                      inside, for the initramfs
 *   kdos-bootctl try <slot> [n]        boot that slot once through UEFI
 *                                      BootNext, or n times led by the menu
 *                                      where there is no BootNext
 *   kdos-bootctl cancel                forget a trial that has not booted
 *   kdos-bootctl deploy <root> [<slot>]
 *                                      put that root's kernel and initramfs
 *                                      in its slot's directory on the ESP
 *   kdos-bootctl select [<slot>]       DECIDE and count down (the initramfs);
 *                                      the slot is the menu entry's kdos_slot=
 *   kdos-bootctl mark-good             this boot worked (kdos-sysupdated, once
 *                                      rcS and the session have both said so)
 *   kdos-bootctl theme <accent>        repaint the boot menu and the text
 *                                      consoles in that scheme
 *   kdos-bootctl theme --print <name>  the theme block, to stdout
 *   kdos-bootctl splash phosphor|modern
 *                                      the boot splash's motion, as
 *                                      kdos.splash= on every KDOS entry
 *   kdos-bootctl palette [<name>]      that scheme's setvtrgb table, to
 *                                      stdout — the default scheme's is what
 *                                      `fs/etc/vtrgb` has to be
 *
 * The shape is RAUC's state machine and none of its dependencies: a file with
 * `active`, `try` and `attempts` in it, one decision, and one place that
 * decrements. What it replaces is the thing Limine does not have — **boot
 * counting**. systemd-boot counts by renaming files with `+N-M` suffixes;
 * Limine has nothing of the sort, so the counting is ours, and it belongs in the
 * INITRAMFS rather than in `rcS`: a kernel that boots into a wedged userland
 * must still be caught, and `rcS` in that userland never runs to say so.
 *
 * THE STATE FILE LIVES ON THE ESP, WHICH IS FAT AND HAS NO JOURNAL. A torn write
 * here bricks the machine — not "fails to update", bricks: the initramfs cannot
 * tell which slot to boot. So every write is temp file, `fsync` the file,
 * `fsync` the DIRECTORY, then `rename`. The directory fsync is the step people
 * leave out, and without it the rename can be lost while the data survives.
 *
 * A file that cannot be parsed is treated as ABSENT, never as partial: half a
 * state file that looked complete is exactly how a machine ends up booting a
 * slot that was never installed. Absent means "boot the root the kernel command
 * line already names", which is what a machine with no A/B setup does anyway.
 *
 * A SLOT KNOWS ITS OWN CONTAINER, AND THAT IS WHAT JOINS A/B TO ENCRYPTION.
 * `select` yields a FILESYSTEM identifier; on an encrypted machine that
 * filesystem is inside a LUKS container, and the container the kernel command
 * line names is one fixed `cryptdevice=`. Two slots inside two containers
 * cannot both be named there, so the second slot's container is recorded HERE
 * — per slot, beside the filesystem it holds — and the initramfs asks for it
 * after it has chosen. Without that, selecting slot B unlocks slot A's
 * container and then looks for B's filesystem inside it, and finds nothing.
 *
 * `crypt_a`/`crypt_b` EMPTY IS THE UNENCRYPTED MACHINE and is the common
 * case. The initramfs falls back to the command line's `cryptdevice=` when a
 * slot names none, so a single-container machine needs no per-slot entry.
 *
 * EACH SLOT BOOTS ITS OWN KERNEL. A slot's kernel and initramfs live in
 * EFI/kdos/<slot>/ on the ESP, put there by `deploy` from that root's /boot, and
 * the kernel's modules live only in that root — so a slot booted on the other
 * slot's kernel has no modules at all. Limine picks the kernel before anything
 * of ours runs, which makes the MENU part of the state: every change of state
 * regenerates the /KDOS entries so the first one boots the slot an unattended
 * boot must reach, and the other slot gets one entry of its own. The initramfs then
 * hands `select` the slot its kernel belongs to, and a mismatch can only mean
 * the other entry was picked by hand.
 *
 * ON UEFI THE CANDIDATE NEVER LEADS THE MENU. A kernel that dies before its
 * initramfs runs cannot count an attempt, so a menu led by it boots it again
 * after every reset. `try` asks the firmware instead, through BootNext, for
 * one boot of a Limine copy whose own menu defaults to the candidate; every
 * other boot starts the confirmed slot, and the candidate leads only once
 * `mark-good` has made it the active slot. A BIOS boot has no BootNext and
 * keeps the menu-led trial: `n` counted boots, and a kernel that dies early is
 * left by picking the confirmed slot's entry by hand. See the section on
 * BootNext below.
 *
 * A FLAT EFI/kdos/vmlinuz IS A SLOT WITH NO DIRECTORY OF ITS OWN. It boots
 * either root, which is what an ESP holds before any `deploy`; a slot without
 * its own directory boots it, a menu with no per-slot directory at all is left
 * exactly as it is, and the flat pair is deleted once no entry names it.
 *
 * A SLOT'S VERSION IS RECORDED BESIDE ITS KERNEL. `deploy` copies the root's
 * identity (/etc/kdos-release, else os-release) into EFI/kdos/<slot>/release,
 * because the other slot is not mounted when a session asks what it holds:
 * the System Versions page, the logout screen's "will switch to KDOS 0.2.13
 * (slot B)" and the rollback notice all read it through `status --json` and
 * `list --json`. A slot with no such file is named by version only while it is
 * the one running, read from the live root.
 *
 * CONFIRMATION COMES AFTER THE SESSION, SO A TRIAL CAN BE ARMED BEFORE IT.
 * `mark-good` runs when kdos-sysupdated holds both bless marks, a minute or
 * more into the boot, and an update applied inside that minute has already
 * called `try`. `try` therefore records the boot that armed it (`armed`, the
 * kernel's boot id), and `mark-good` in that same boot leaves the trial
 * exactly as it is: rolling it back, promoting it unbooted or removing the
 * BootNext it just wrote would each throw the update away or skip its test.
 * ---------------------------------
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kdos-tools.h"

#define BOOTSTATE_DEFAULT "/boot/efi/EFI/kdos/bootstate"
#define TRY_ATTEMPTS 3

typedef struct {
	char slot[2][80];	/* a, b — the root UUID of each, "" if unset */
	/* The LUKS container each slot's filesystem is inside, "" for a slot
	 * that is not encrypted. See the note at the top of this file. */
	char crypt[2][80];
	int active;		/* 0 = a, 1 = b                              */
	int trying;		/* -1 when not trying, else the slot index   */
	int attempts;
	/* The candidate is booted once through UEFI BootNext, and the menu's
	 * default stays on the active slot. 0 is the menu-led trial. */
	int bootnext;
	/* The slot whose trial ended without being confirmed, -1 for none: the
	 * rollback notice a session shows once. Cleared by the next `try` and
	 * by a confirmation. */
	int failed;
	/* The boot id of the boot that ran `try`, "" when it could not be read.
	 * See CONFIRMATION COMES AFTER THE SESSION at the top of this file. */
	char armed[40];
} BootState;

static const char *slot_name(int i)
{
	return i == 1 ? "b" : "a";
}

static int slot_index(const char *s)
{
	if (!s)
		return -1;
	if (!strcmp(s, "a") || !strcmp(s, "A"))
		return 0;
	if (!strcmp(s, "b") || !strcmp(s, "B"))
		return 1;
	return -1;
}

static const char *state_path(void)
{
	/* Overridable so the state machine can be exercised against a fixture
	 * with no ESP and no root. */
	const char *e = getenv("KDOS_BOOTSTATE");
	return (e && *e) ? e : BOOTSTATE_DEFAULT;
}

/* ── reading ───────────────────────────────────────────────────────────── */

static int state_load(BootState *st)
{
	memset(st, 0, sizeof(*st));
	st->trying = -1;
	st->failed = -1;
	st->active = 0;

	size_t len = 0;
	char *data = kb_read_all(state_path(), &len);
	if (!data)
		return -1;

	int saw_active = 0;
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		char buf[256];
		size_t n = nl ? (size_t)(nl - line) : strlen(line);

		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		memcpy(buf, line, n);
		buf[n] = 0;
		line = nl ? nl + 1 : NULL;

		char *eq = strchr(buf, '=');
		if (!buf[0] || buf[0] == '#' || !eq)
			continue;
		*eq = 0;
		char *k = buf, *v = eq + 1;
		while (*k == ' ' || *k == '\t')
			k++;
		for (char *e = k + strlen(k); e > k && (e[-1] == ' ' || e[-1] == '\t');)
			*--e = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		v[strcspn(v, " \t\r")] = 0;

		if (!strcmp(k, "slot_a"))
			kb_strlcpy(st->slot[0], v, sizeof(st->slot[0]));
		else if (!strcmp(k, "slot_b"))
			kb_strlcpy(st->slot[1], v, sizeof(st->slot[1]));
		/* An UNKNOWN key is skipped by the loop's own default, and a
		 * state file with no crypt_a/crypt_b parses as a machine with
		 * no containers. */
		else if (!strcmp(k, "crypt_a"))
			kb_strlcpy(st->crypt[0], v, sizeof(st->crypt[0]));
		else if (!strcmp(k, "crypt_b"))
			kb_strlcpy(st->crypt[1], v, sizeof(st->crypt[1]));
		else if (!strcmp(k, "active")) {
			int i = slot_index(v);
			if (i < 0) {
				free(data);
				return -1;	/* unparsable: treat as absent */
			}
			st->active = i;
			saw_active = 1;
		} else if (!strcmp(k, "try")) {
			st->trying = v[0] ? slot_index(v) : -1;
		} else if (!strcmp(k, "attempts")) {
			st->attempts = atoi(v);
		} else if (!strcmp(k, "bootnext")) {
			st->bootnext = !strcmp(v, "yes");
		} else if (!strcmp(k, "failed")) {
			st->failed = v[0] ? slot_index(v) : -1;
		} else if (!strcmp(k, "armed")) {
			kb_strlcpy(st->armed, v, sizeof(st->armed));
		}
	}
	free(data);

	/* The one field without which nothing else means anything. */
	if (!saw_active)
		return -1;
	if (st->trying >= 0 && !st->slot[st->trying][0])
		st->trying = -1;	/* trying a slot that has no root */
	if (st->trying < 0) {
		st->bootnext = 0;
		st->armed[0] = 0;
	}
	return 0;
}

/* The kernel's id for this boot, which `try` records and `mark-good` compares.
 * KDOS_BOOT_ID stands in for it; a state file other than the real one reads
 * no kernel id without it, so a fixture is never judged by the boot of the
 * machine running it. 0 with the id in `out`, -1 when there is none. */
static int boot_id(char *out, size_t n)
{
	const char *e = getenv("KDOS_BOOT_ID");
	const char *s = getenv("KDOS_BOOTSTATE");

	out[0] = 0;
	if (e && *e) {
		kb_strlcpy(out, e, n);
	} else {
		if (s && *s && strcmp(s, BOOTSTATE_DEFAULT))
			return -1;
		if (kb_read_file("/proc/sys/kernel/random/boot_id", out, n) <= 0)
			return -1;
	}
	out[strcspn(out, " \t\r\n")] = 0;
	return out[0] ? 0 : -1;
}

static int running_slot(void);

/* ── writing, the only part that can brick a machine ───────────────────── */

static int state_save(const BootState *st)
{
	const char *path = state_path();
	char tmp[1024], dir[1024];

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	else
		kb_strlcpy(dir, ".", sizeof(dir));

	KbBuf b = {0};
	kb_buf_printf(&b,
		      "# KDOS boot state. Written by kdos-bootctl; the initramfs\n"
		      "# reads it before it mounts a root filesystem.\n"
		      "slot_a   = %s\n"
		      "slot_b   = %s\n"
		      "crypt_a  = %s\n"
		      "crypt_b  = %s\n"
		      "active   = %s\n"
		      "try      = %s\n"
		      "attempts = %d\n"
		      "bootnext = %s\n"
		      "armed    = %s\n"
		      "failed   = %s\n",
		      st->slot[0], st->slot[1], st->crypt[0], st->crypt[1],
		      slot_name(st->active),
		      st->trying >= 0 ? slot_name(st->trying) : "",
		      st->attempts,
		      st->trying >= 0 && st->bootnext ? "yes" : "",
		      st->trying >= 0 ? st->armed : "",
		      st->failed >= 0 ? slot_name(st->failed) : "");

	int rc = -1;
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		goto out;
	if (write(fd, b.p, b.n) != (ssize_t)b.n) {
		close(fd);
		unlink(tmp);
		goto out;
	}
	/* The data, then the directory entry. FAT has no journal: without the
	 * second fsync the rename can be lost while the bytes survive, and the
	 * machine comes up reading the OLD state — which is the good failure —
	 * or, with the fsyncs in the other order, a zero-length file, which is
	 * the one that bricks it. */
	if (fsync(fd) != 0) {
		close(fd);
		unlink(tmp);
		goto out;
	}
	close(fd);

	if (rename(tmp, path) != 0) {
		unlink(tmp);
		goto out;
	}
	int dfd = open(dir, O_RDONLY | O_DIRECTORY);
	if (dfd >= 0) {
		fsync(dfd);
		close(dfd);
	}
	rc = 0;
out:
	kb_buf_free(&b);
	return rc;
}

/* ── the bootloader's colours ──────────────────────────────────────────── */

#define LIMINE_CONF_DEFAULT "/boot/efi/limine.conf"

static const char *limine_path(void)
{
	/* Overridable for the same reason `KDOS_BOOTSTATE` is: the restamp has
	 * to be exercisable against a fixture on a machine with no ESP. */
	const char *e = getenv("KDOS_LIMINE_CONF");
	return (e && *e) ? e : LIMINE_CONF_DEFAULT;
}

/* setvtrgb's table, read by kdos-getty onto every VT before it clears the
 * screen. Overridable for the same reason the two above are. */
#define VTRGB_DEFAULT "/etc/vtrgb"

static const char *vtrgb_path(void)
{
	const char *e = getenv("KDOS_VTRGB");
	return (e && *e) ? e : VTRGB_DEFAULT;
}

/*
 * THE KEYS THE THEME OWNS, and nothing else in the file is this function's to
 * touch. `timeout` and every entry are written by the installer, and the
 * /KDOS entries and `default_entry` are regenerated by menu_sync from the
 * boot state; a restamp that rewrote the file from a template would discard
 * an A/B slot somebody is mid-rollback on.
 *
 * `wallpaper` and `term_font` are NOT here, and they are the only two that are
 * not: they name paths on the ESP, and which artwork and which face are
 * installed is not a question an accent answers. `wallpaper_style` and
 * `term_font_scale` ARE here, because they are layout — a restamp that moved
 * only the colours would leave a machine installed earlier drawing its menu at
 * `2x2` over a `centered` backdrop, which is the unreadable arrangement in
 * every scheme.
 */
static const char *const THEME_KEYS[] = {
	"interface_branding",
	"interface_branding_colour",	"interface_branding_color",
	"interface_help_hidden",
	"interface_help_colour",	"interface_help_color",
	"interface_help_colour_bright",	"interface_help_color_bright",
	"backdrop",
	"term_background",		"term_foreground",
	"term_background_bright",	"term_foreground_bright",
	"term_palette",			"term_palette_bright",
	"term_margin",			"term_margin_gradient",
	"wallpaper_style",		"term_font_scale",
	NULL
};

/* The key of a `key: value` line, or -1 for a blank, a comment, an entry
 * heading or an indented entry setting. Limine's entries are indented and its
 * globals are not, so leading whitespace alone separates the two. */
static int theme_key_line(const char *line, size_t n)
{
	size_t k = 0;

	if (!n || line[0] == '#' || line[0] == '/' || line[0] == ' ' ||
	    line[0] == '\t')
		return -1;
	while (k < n && line[k] != ':')
		k++;
	if (k == n)
		return -1;
	for (int i = 0; THEME_KEYS[i]; i++)
		if (strlen(THEME_KEYS[i]) == k &&
		    !strncmp(line, THEME_KEYS[i], k))
			return i;
	return -1;
}

/*
 * Rewrite the theme block of a limine.conf in place.
 *
 * The new block lands where the FIRST owned line was, so a file keeps the
 * shape whoever wrote it gave it; every other owned line is dropped. A file
 * with no owned line at all takes the block immediately before its first
 * entry, which is the only position both firmwares' parsers accept it in — a
 * global written after an entry heading belongs to that entry.
 */
static int limine_restamp(const char *path, const KcolScheme *sc)
{
	char theme[1024];
	size_t len = 0;
	char *data = kb_read_all(path, &len);

	if (!data)
		return -1;
	if (kcol_limine_conf(sc, theme, sizeof(theme)) >= (int)sizeof(theme)) {
		free(data);
		return -1;
	}

	KbBuf out = {0};
	int placed = 0;
	for (char *line = data; line && *line;) {
		char *nl = strchr(line, '\n');
		size_t n = nl ? (size_t)(nl - line) : strlen(line);
		size_t t = n;

		while (t && (line[t - 1] == '\r' || line[t - 1] == ' '))
			t--;

		if (theme_key_line(line, t) >= 0) {
			if (!placed) {
				kb_buf_str(&out, theme);
				placed = 1;
			}
		} else {
			/* An entry heading, and the block has nowhere else to
			 * go: everything after this belongs to an entry. */
			if (!placed && t && line[0] == '/') {
				/* And a blank line: a global butted straight
				 * against an entry heading parses, but reads
				 * as part of the entry to anyone editing it. */
				kb_buf_str(&out, theme);
				kb_buf_str(&out, "\n");
				placed = 1;
			}
			kb_buf_add(&out, line, n);
			kb_buf_str(&out, "\n");
		}
		line = nl ? nl + 1 : NULL;
	}
	if (!placed)
		kb_buf_str(&out, theme);
	free(data);

	/* The ESP is FAT and has no journal, so this is the same
	 * temp/fsync/rename/fsync-the-directory the boot state gets: a
	 * zero-length limine.conf is a machine that shows no menu. */
	int rc = kb_write_file_atomic(path, out.p);
	kb_buf_free(&out);
	return rc;
}

static int cmd_theme(int argc, char **argv)
{
	int print = argc > 2 && !strcmp(argv[2], "--print");
	const char *name = print ? (argc > 3 ? argv[3] : NULL)
				 : (argc > 2 ? argv[2] : NULL);
	const KcolScheme *sc;

	/*
	 * THE NAME IS ONE OF EIGHT COMPILED-IN STRINGS. That is the whole of
	 * the validation and the whole of the safety argument for reaching
	 * this from an unprivileged session: there is no path here to aim, and
	 * a name that is not a scheme names nothing at all.
	 */
	sc = name ? kcol_find(name) : kcol_default();
	if (!sc) {
		fprintf(stderr, "bootctl: no accent named '%s'\n", name);
		return 2;
	}

	if (print) {
		char theme[1024];
		if (kcol_limine_conf(sc, theme, sizeof(theme)) >=
		    (int)sizeof(theme)) {
			fprintf(stderr, "bootctl: theme block does not fit\n");
			return 1;
		}
		fputs(theme, stdout);
		return 0;
	}

	/*
	 * AND THE TEXT CONSOLE, WHICH IS THE OTHER SURFACE A SESSION DOES NOT
	 * OWN. The boot menu and the text consoles — the boot's messages
	 * before the greeter, the recovery login on tty2 — are the two places
	 * an accent has to reach through a file rather than through a running
	 * program, they are both root's to write, and they are wanted
	 * together: a menu retinted while the console after it keeps the old
	 * accent is a boot that changes colour halfway through.
	 *
	 * BEFORE THE ESP TEST AND NOT AFTER IT. The live medium has no
	 * writable limine.conf and returns success below; the console there
	 * is as retintable as anywhere else, and a return placed first would
	 * make `kdos theme` a no-op on the ISO.
	 */
	{
		char vt[512];

		if (kcol_vtrgb(sc, vt, sizeof(vt)) >= (int)sizeof(vt)) {
			fprintf(stderr, "bootctl: palette does not fit\n");
			return 1;
		}
		if (kb_write_file_atomic(vtrgb_path(), vt) != 0)
			fprintf(stderr, "bootctl: cannot rewrite %s: %s — "
				"text consoles keep the old accent\n",
				vtrgb_path(), strerror(errno));
		else
			printf("text consoles are now %s at the next "
			       "login prompt\n", sc->name);
	}

	/*
	 * A MACHINE WITH NO WRITABLE ESP IS NOT A FAILURE. The live medium is
	 * read-only and a machine installed without one has no limine.conf at
	 * all; refusing here would make `kdos theme` fail on the ISO, where
	 * every other surface retints perfectly. Reported and exit 0.
	 */
	const char *path = limine_path();
	if (!kb_path_exists(path)) {
		printf("no bootloader configuration at %s — boot menu "
		       "unchanged\n", path);
		return 0;
	}
	if (limine_restamp(path, sc) != 0) {
		fprintf(stderr, "bootctl: cannot rewrite %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	printf("boot menu is now %s\n", sc->name);
	return 0;
}

/* ── what each slot holds ───────────────────────────────────────────────── */

/* A slot's identity: the version a person is told, the image it came from,
 * the tree its packages were built from and when that tree was made. Every
 * field may be empty, the time 0. */
typedef struct {
	char version[64];
	char build[64];
	char date[32];
	char tree[64];
	long long tree_time;
} SlotIdent;

/*
 * One `KEY=value` line of an os-release-shaped file, the quotes taken off;
 * "" when the key is absent. Only printable ASCII other than a quote or a
 * backslash survives: a value is written back onto the ESP and into JSON, and
 * a file that put a newline or a quote in it must not be able to add a line
 * to either.
 */
static void osr_value(const char *data, const char *key, char *out, size_t n)
{
	size_t kl = strlen(key);

	out[0] = 0;
	for (const char *l = data; l && *l;) {
		const char *nl = strchr(l, '\n');
		size_t len = nl ? (size_t)(nl - l) : strlen(l);
		if (len > kl && !strncmp(l, key, kl) && l[kl] == '=') {
			const char *v = l + kl + 1, *end = l + len;
			char q = 0;
			if (v < end && (*v == '"' || *v == '\''))
				q = *v++;
			size_t o = 0;
			for (; v < end && o + 1 < n; v++) {
				unsigned char ch = (unsigned char)*v;
				if (q && *v == q)
					break;
				if (ch < 0x20 || ch > 0x7e || ch == '"' ||
				    ch == '\'' || ch == '\\')
					continue;
				out[o++] = *v;
			}
			while (o && (out[o - 1] == ' ' || out[o - 1] == '\r'))
				o--;
			out[o] = 0;
			return;
		}
		l = nl ? nl + 1 : NULL;
	}
}

/*
 * The identity of the root mounted at `root`. /etc/kdos-release is the
 * record: the image phase writes it and `kdos update` rewrites it in every
 * slot it fills. A root without it is named from os-release — VERSION_ID, and
 * `+BUILD_ID` when the image was stamped with its commit — which is the same
 * rule kdos-sysupdated's GetVersion keeps, so the two never name one system
 * differently. The tree falls back to the commit and the time /ports/TREE
 * records.
 */
static void ident_from_root(const char *root, SlotIdent *id)
{
	char p[PATH_MAX], t[24];
	char *d;

	memset(id, 0, sizeof(*id));
	snprintf(p, sizeof(p), "%s/etc/kdos-release", root);
	if ((d = kb_read_all(p, NULL))) {
		osr_value(d, "VERSION", id->version, sizeof(id->version));
		osr_value(d, "BUILD_ID", id->build, sizeof(id->build));
		osr_value(d, "BUILD_DATE", id->date, sizeof(id->date));
		osr_value(d, "TREE", id->tree, sizeof(id->tree));
		osr_value(d, "TREE_TIME", t, sizeof(t));
		id->tree_time = atoll(t);
		free(d);
	}
	snprintf(p, sizeof(p), "%s/etc/os-release", root);
	if ((d = kb_read_all(p, NULL))) {
		char vid[24], bid[32];
		osr_value(d, "VERSION_ID", vid, sizeof(vid));
		osr_value(d, "BUILD_ID", bid, sizeof(bid));
		if (!id->version[0] && vid[0])
			snprintf(id->version, sizeof(id->version), "%s%s%s",
				 vid, bid[0] ? "+" : "", bid);
		if (!id->build[0])
			kb_strlcpy(id->build, bid, sizeof(id->build));
		if (!id->date[0])
			osr_value(d, "KDOS_BUILD_DATE", id->date,
				  sizeof(id->date));
		free(d);
	}
	/* /ports/TREE describes the tree in place, which `kdos update fetch`
	 * replaces before the root is updated from it: its time is this
	 * root's only when its commit is the one the root records. */
	if (!id->tree[0] || !id->tree_time) {
		char commit[64] = "";
		long long when = 0;
		snprintf(p, sizeof(p), "%s/ports/TREE", root);
		if ((d = kb_read_all(p, NULL))) {
			for (const char *l = d; l && *l;) {
				if (!strncmp(l, "commit ", 7)) {
					size_t k = strcspn(l + 7, " \t\r\n");
					if (k >= sizeof(commit))
						k = sizeof(commit) - 1;
					memcpy(commit, l + 7, k);
					commit[k] = 0;
				} else if (!strncmp(l, "time ", 5)) {
					when = atoll(l + 5);
				}
				l = strchr(l, '\n');
				l = l ? l + 1 : NULL;
			}
			free(d);
		}
		if (!id->tree[0]) {
			kb_strlcpy(id->tree, commit, sizeof(id->tree));
			id->tree_time = when;
		} else if (!id->tree_time && commit[0] &&
			   !strncmp(commit, id->tree, strlen(id->tree))) {
			id->tree_time = when;
		}
	}
}

/* The version of the root mounted at `root`, by the rule above, for `kdos
 * update` (update.c): what it records a filled slot under and what it calls
 * the running system must be the names this file shows. */
void bootctl_root_version(const char *root, char *out, size_t n);
void bootctl_root_version(const char *root, char *out, size_t n)
{
	SlotIdent id;
	ident_from_root(root, &id);
	kb_strlcpy(out, id.version, n);
}

/* The root this boot runs, for its own identity: KDOS_SYSROOT stands in for
 * it, and a fixture state file reads no live root without it. */
static const char *sysroot(void)
{
	const char *e = getenv("KDOS_SYSROOT");
	const char *s = getenv("KDOS_BOOTSTATE");

	if (e && *e)
		return e;
	if (s && *s && strcmp(s, BOOTSTATE_DEFAULT))
		return NULL;
	return "/";
}

/* ── one kernel per slot ───────────────────────────────────────────────── */

#define STATE_SUFFIX "/EFI/kdos/bootstate"

/* The mount point of the ESP the state file is on: the state path minus its
 * fixed tail. -1 for a state file that is not on an ESP, which is every
 * fixture the state machine is exercised against — and which is why no verb
 * that only moves the state ever touches a menu there. */
static int esp_root(char *out, size_t n)
{
	const char *p = state_path();
	size_t l = strlen(p), s = strlen(STATE_SUFFIX);

	if (l < s || strcmp(p + l - s, STATE_SUFFIX) || l - s + 1 > n)
		return -1;
	memcpy(out, p, l - s);
	out[l - s] = 0;
	return 0;
}

static void menu_path(const char *esp, char *out, size_t n)
{
	const char *e = getenv("KDOS_LIMINE_CONF");

	if (e && *e)
		kb_strlcpy(out, e, n);
	else
		snprintf(out, n, "%s/limine.conf", esp);
}

/* Both files of a kernel, in `<esp>/<dir>/`. A directory holding one without
 * the other is not a kernel: a kernel beside no initramfs, or beside another
 * kernel's, mounts no root. */
static int has_kernel(const char *esp, const char *dir)
{
	char k[PATH_MAX], i[PATH_MAX];

	snprintf(k, sizeof(k), "%s/%s/vmlinuz", esp, dir);
	snprintf(i, sizeof(i), "%s/%s/initramfs.cpio.gz", esp, dir);
	return kb_path_exists(k) && kb_path_exists(i);
}

/* The ESP directory slot i boots from, relative to the ESP: its own, else the
 * flat pair, else NULL. `own` says which. */
static const char *slot_boot_dir(const char *esp, int i, char *buf, size_t n,
				 int *own)
{
	snprintf(buf, n, "EFI/kdos/%s", slot_name(i));
	*own = has_kernel(esp, buf);
	if (*own)
		return buf;
	if (has_kernel(esp, "EFI/kdos")) {
		kb_strlcpy(buf, "EFI/kdos", n);
		return buf;
	}
	return NULL;
}

/* The slot the menu's first entry boots, which is the one Limine starts when
 * nobody picks. A trial through BootNext never moves it off the active slot:
 * the candidate is reached through EFI/kdos/trial/ for one boot, and every
 * other boot — the reset after a kernel that died before its initramfs among
 * them — lands on the confirmed slot. A menu-led trial leads with the
 * candidate while it has attempts left; its last attempt spent, the active
 * slot's entry leads again, so the rollback happens without a second reboot. */
static int next_slot(const BootState *st)
{
	if (st->bootnext)
		return st->active;
	return st->trying >= 0 && st->attempts > 0 ? st->trying : st->active;
}

/* A BootNext trial whose one boot has not been spent: the only state in which
 * EFI/kdos/trial/ exists. */
static int trial_pending(const BootState *st)
{
	return st->bootnext && st->trying >= 0 && st->attempts > 0;
}

/* The version a bzImage carries: the boot protocol's kernel_version field at
 * 0x20E points, relative to 0x200, at a string that starts with it. Empty for
 * anything that is not a bzImage. */
static void kernel_version(const char *path, char *out, size_t n)
{
	unsigned char h[0x210];
	char s[128];
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	out[0] = 0;
	if (fd < 0)
		return;
	if (pread(fd, h, sizeof(h), 0) == (ssize_t)sizeof(h) &&
	    !memcmp(h + 0x202, "HdrS", 4)) {
		unsigned off = h[0x20E] | (unsigned)h[0x20F] << 8;
		ssize_t r = off ? pread(fd, s, sizeof(s) - 1, off + 0x200) : -1;
		if (r > 0) {
			s[r] = 0;
			s[strcspn(s, " \n")] = 0;
			kb_strlcpy(out, s, n);
		}
	}
	close(fd);
}

/* One line of a limine.conf: its bounds, without the newline or trailing
 * blanks. */
typedef struct {
	const char *p;
	size_t n;	/* trimmed length */
	size_t full;	/* through the newline */
} Line;

static int line_next(const char **cur, Line *l)
{
	const char *s = *cur;

	if (!s || !*s)
		return 0;
	const char *nl = strchr(s, '\n');
	size_t len = nl ? (size_t)(nl - s) : strlen(s);
	l->p = s;
	l->full = nl ? len + 1 : len;
	while (len && (s[len - 1] == '\r' || s[len - 1] == ' ' ||
		       s[len - 1] == '\t'))
		len--;
	l->n = len;
	*cur = s + l->full;
	return 1;
}

static int is_indented(const Line *l)
{
	return l->n && (l->p[0] == ' ' || l->p[0] == '\t');
}

static int is_ours(const Line *l)
{
	return l->n >= 5 && !strncmp(l->p, "/KDOS", 5);
}

/*
 * THE WORDS A QUIET BOOT ADDS, on each entry that boots to the desktop: no
 * kernel messages and no console cursor between the boot menu and the login
 * screen. The verbose and single-user entries leave all three out. kinstall
 * writes the first menu with the same set (its KI_QUIET), and the selftest
 * compares the two menus, so a word added here and not there fails it.
 */
#define BC_QUIET "quiet loglevel=3 vt.global_cursor_default=0"

/*
 * The kernel command line every /KDOS entry is built from: the first /KDOS
 * entry's, less the words the generator owns — which slot, which root, and
 * the verbosity each entry sets for itself (BC_QUIET, `loglevel=`, `single`).
 * Everything else carries over, so `bootstate=`, the command line's
 * `cryptdevice=` and anything added by hand survive every regeneration.
 */
static int menu_template(const char *conf, KbBuf *base)
{
	const char *cur = conf;
	Line l;
	int in = 0;

	while (line_next(&cur, &l)) {
		if (!is_indented(&l)) {
			if (in && l.n && l.p[0] == '/')
				return -1;	/* an entry with no cmdline */
			in = in || (l.n && is_ours(&l));
			continue;
		}
		if (!in)
			continue;
		const char *v = l.p;
		const char *end = l.p + l.n;
		while (v < end && (*v == ' ' || *v == '\t'))
			v++;
		if ((size_t)(end - v) < 8 || strncmp(v, "cmdline:", 8))
			continue;
		v += 8;
		while (v < end) {
			while (v < end && (*v == ' ' || *v == '\t'))
				v++;
			const char *w = v;
			while (v < end && *v != ' ' && *v != '\t')
				v++;
			size_t wn = (size_t)(v - w);
			if (!wn || (wn == 5 && !strncmp(w, "quiet", 5)) ||
			    (wn == 6 && !strncmp(w, "single", 6)) ||
			    (wn >= 9 && !strncmp(w, "loglevel=", 9)) ||
			    (wn >= 25 &&
			     !strncmp(w, "vt.global_cursor_default=", 25)) ||
			    (wn >= 10 && !strncmp(w, "kdos_slot=", 10)))
				continue;
			/* root= is kept as a marker, so the slot's root lands
			 * where the command line had it. */
			if (base->n)
				kb_buf_str(base, " ");
			if (wn >= 5 && !strncmp(w, "root=", 5))
				kb_buf_str(base, "root=");
			else
				kb_buf_add(base, w, wn);
		}
		return 0;
	}
	return -1;
}

static void menu_entry(KbBuf *b, const char *title, const char *comment,
		       const char *dir, int slot, const char *uuid,
		       const char *base, const char *extra)
{
	kb_buf_printf(b,
		      "/%s\n"
		      "    comment: %s\n"
		      "    protocol: linux\n"
		      "    path: boot():/%s/vmlinuz\n"
		      "    module_path: boot():/%s/initramfs.cpio.gz\n"
		      "    cmdline: kdos_slot=%s",
		      title, comment, dir, dir, slot_name(slot));
	int rooted = 0;
	for (const char *w = base; *w;) {
		size_t wn = strcspn(w, " ");
		kb_buf_str(b, " ");
		if (wn == 5 && !strncmp(w, "root=", 5)) {
			kb_buf_printf(b, "root=UUID=%s", uuid);
			rooted = 1;
		} else
			kb_buf_add(b, w, wn);
		w += wn;
		while (*w == ' ')
			w++;
	}
	if (!rooted)
		kb_buf_printf(b, " root=UUID=%s", uuid);
	kb_buf_printf(b, " %s\n", extra);
}

/* Where a BootNext trial boots from: a copy of the ESP's own Limine binary
 * and a limine.conf beside it. Limine reads the config beside its EFI binary
 * before any other, so this directory's copy is the one the trial boot sees,
 * and every other boot — which starts EFI/BOOT/ — sees the ESP root's. */
#define TRIAL_DIR "EFI/kdos/trial"

static void trial_clear(const char *esp)
{
	static const char *const files[] = {
		"limine.conf", "BOOTX64.EFI", "BOOTIA32.EFI", NULL
	};
	char f[PATH_MAX];

	for (int k = 0; files[k]; k++) {
		snprintf(f, sizeof(f), "%s/" TRIAL_DIR "/%s", esp, files[k]);
		unlink(f);
	}
	snprintf(f, sizeof(f), "%s/" TRIAL_DIR, esp);
	rmdir(f);
}

/*
 * The file around a new /KDOS block, with `default_entry` at `dflt`. An entry
 * is its heading and the indented lines under it; the blank lines after one of
 * ours go with it, and one is put back before whatever follows. default_entry
 * keeps its line; a file without one gets it before the first entry, where a
 * global still reads as a global.
 */
static void menu_rebuild(const char *conf, const char *block, int dflt_at,
			 KbBuf *out)
{
	const char *cur = conf;
	Line l;
	int placed = 0, skipping = 0, dflt = 0;

	while (line_next(&cur, &l)) {
		if (skipping && (is_indented(&l) || !l.n))
			continue;
		if (skipping && !is_ours(&l)) {
			skipping = 0;
			kb_buf_str(out, "\n");
		}
		if (!is_indented(&l) && l.n >= 14 &&
		    !strncmp(l.p, "default_entry:", 14)) {
			if (!dflt)
				kb_buf_printf(out, "default_entry: %d\n",
					      dflt_at);
			dflt = 1;
			continue;
		}
		if (l.n && l.p[0] == '/' && !dflt) {
			kb_buf_printf(out, "default_entry: %d\n\n", dflt_at);
			dflt = 1;
		}
		if (is_ours(&l)) {
			if (!placed)
				kb_buf_str(out, block);
			placed = 1;
			skipping = 1;
			continue;
		}
		kb_buf_add(out, l.p, l.full);
	}
	kb_buf_str(out, "");
}

/* Write `text` to `path` unless it already holds exactly that. */
static int write_if_changed(const char *path, const char *text)
{
	char *old = kb_read_all(path, NULL);
	int same = old && !strcmp(old, text);

	free(old);
	return same ? 0 : kb_write_file_atomic(path, text);
}

/* ── the splash's motion ───────────────────────────────────────────────── */

/*
 * `kdos.splash=<motion>` on the command line of every /KDOS entry of one
 * limine.conf: dropped wherever it stands, then added at the end of the line
 * for `phosphor`; Modern is its absence. kdos-splash reads the LAST such word
 * of /proc/cmdline, so one per line keeps the file and the boot in step.
 * menu_template carries the word into every regenerated entry, which is what
 * keeps it across `try`, `deploy` and the slot entries.
 */
static int splash_rewrite(const char *path, int phosphor)
{
	char *data = kb_read_all(path, NULL);
	const char *cur = data;
	KbBuf out = {0};
	Line l;
	int in = 0;

	if (!data)
		return -1;
	while (line_next(&cur, &l)) {
		if (!is_indented(&l)) {
			in = l.n && is_ours(&l);
			kb_buf_add(&out, l.p, l.full);
			continue;
		}
		const char *v = l.p, *end = l.p + l.n;
		while (v < end && (*v == ' ' || *v == '\t'))
			v++;
		if (!in || (size_t)(end - v) < 8 || strncmp(v, "cmdline:", 8)) {
			kb_buf_add(&out, l.p, l.full);
			continue;
		}
		v += 8;
		kb_buf_add(&out, l.p, (size_t)(v - l.p));
		while (v < end) {
			while (v < end && (*v == ' ' || *v == '\t'))
				v++;
			const char *w = v;
			while (v < end && *v != ' ' && *v != '\t')
				v++;
			size_t wn = (size_t)(v - w);
			if (!wn || (wn >= 12 && !strncmp(w, "kdos.splash=", 12)))
				continue;
			kb_buf_str(&out, " ");
			kb_buf_add(&out, w, wn);
		}
		if (phosphor)
			kb_buf_str(&out, " kdos.splash=phosphor");
		if (l.full > l.n)
			kb_buf_str(&out, "\n");
	}
	kb_buf_str(&out, "");
	int rc = write_if_changed(path, out.p);
	kb_buf_free(&out);
	free(data);
	return rc;
}

/*
 * `kdos-bootctl splash phosphor|modern`: the boot splash's motion, machine
 * wide, because the splash runs before any user exists. The ESP's menu and a
 * pending trial's copy beside the trial binary both carry it, or a trial boot
 * would play the other motion. 1 with one line on stderr when the menu cannot
 * be written (the live medium has none to write), which kdos-powerd reports
 * as "boot menu unchanged".
 */
static int cmd_splash(int argc, char **argv)
{
	const char *m = argc > 2 ? argv[2] : "";
	int phosphor = !strcmp(m, "phosphor");

	if (!phosphor && strcmp(m, "modern")) {
		fprintf(stderr, "usage: kdos-bootctl splash phosphor|modern\n");
		return 2;
	}
	const char *path = limine_path();
	if (!kb_path_exists(path)) {
		fprintf(stderr, "bootctl: no bootloader configuration at %s — "
			"the boot splash is unchanged\n", path);
		return 1;
	}
	if (splash_rewrite(path, phosphor) != 0) {
		fprintf(stderr, "bootctl: cannot rewrite %s: %s\n", path,
			strerror(errno));
		return 1;
	}
	char dir[PATH_MAX], trial[PATH_MAX + 64];
	kb_strlcpy(dir, path, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		snprintf(trial, sizeof(trial), "%s/" TRIAL_DIR "/limine.conf",
			 dir);
		if (kb_path_exists(trial) && splash_rewrite(trial, phosphor)) {
			fprintf(stderr, "bootctl: cannot rewrite %s: %s\n",
				trial, strerror(errno));
			return 1;
		}
	}
	printf("boot splash is now %s\n", phosphor ? "phosphor" : "modern");
	return 0;
}

/*
 * Make the /KDOS entries of limine.conf match the state: the next slot's three
 * entries first, then one for the other slot when it has a root and a kernel.
 * Every other line of the file — the theme, the timeout, memtest86+ — is left
 * where it is. `default_entry` is pointed at the first /KDOS entry.
 *
 * A pending BootNext trial also gets EFI/kdos/trial/limine.conf: the same
 * file with `default_entry` on the candidate's entry, which is the fourth of
 * the block. That entry carries `panic=10` unless the command line already
 * sets one, so a candidate kernel that panics resets the machine by itself
 * and the reset boots the confirmed slot. Any other state removes the
 * directory.
 *
 * Nothing is written when nothing changed, when the file has no /KDOS entry to
 * learn a command line from, when no slot has a directory of its own, or when
 * the next slot has no kernel at all: a menu that boots nothing is worse than
 * one that boots the previous kernel.
 */
static int menu_sync(const BootState *st)
{
	char esp[1024], conf_path[PATH_MAX];
	char dbuf[2][64];
	const char *dir[2];
	int own[2];

	if (esp_root(esp, sizeof(esp)) != 0)
		return 0;
	if (!trial_pending(st))
		trial_clear(esp);
	menu_path(esp, conf_path, sizeof(conf_path));
	for (int i = 0; i < 2; i++)
		dir[i] = st->slot[i][0] ? slot_boot_dir(esp, i, dbuf[i],
							sizeof(dbuf[i]), &own[i])
					: NULL;
	int next = next_slot(st), other = !next;
	if (!dir[next] || !((dir[0] && own[0]) || (dir[1] && own[1])))
		return 0;

	char *conf = kb_read_all(conf_path, NULL);
	if (!conf)
		return 0;
	KbBuf base = {0};
	if (menu_template(conf, &base) != 0) {
		free(conf);
		kb_buf_free(&base);
		return 0;
	}
	kb_buf_str(&base, "");

	int led = !st->bootnext && st->trying >= 0 && st->attempts > 0;
	int trial = trial_pending(st) && dir[other];
	char c[160];
	KbBuf block = {0};
	snprintf(c, sizeof(c), led ? "Try the update in slot %s"
				   : "Start this machine (slot %s)",
		 slot_name(next));
	menu_entry(&block, "KDOS", c, dir[next], next, st->slot[next],
		   base.p, BC_QUIET);
	kb_buf_str(&block, "\n");
	menu_entry(&block, "KDOS (verbose)",
		   "Every kernel message on the console", dir[next], next,
		   st->slot[next], base.p, "loglevel=7");
	kb_buf_str(&block, "\n");
	menu_entry(&block, "KDOS (single user)",
		   "A root shell for root's password, no desktop", dir[next],
		   next, st->slot[next], base.p, "loglevel=7 single");
	if (dir[other]) {
		char t[32];
		int panics = !strncmp(base.p, "panic=", 6) ||
			     strstr(base.p, " panic=");
		snprintf(t, sizeof(t), "KDOS (slot %s)", slot_name(other));
		snprintf(c, sizeof(c), "%s",
			 led ? "The confirmed root: picking it abandons the "
			       "update"
			 : st->bootnext && st->trying == other
			     ? "The update on trial: one boot, then the "
			       "confirmed root again"
			     : "The other root, for one boot: nothing is "
			       "confirmed");
		kb_buf_str(&block, "\n");
		menu_entry(&block, t, c, dir[other], other, st->slot[other],
			   base.p, trial && !panics ? BC_QUIET " panic=10"
						    : BC_QUIET);
	}
	kb_buf_free(&base);

	/* Which entry the block starts at: Limine counts every entry heading,
	 * and default_entry is 1-based. */
	const char *cur = conf;
	Line l;
	int first = 1;
	while (line_next(&cur, &l) && !(l.n && is_ours(&l)))
		if (l.n && l.p[0] == '/')
			first++;

	KbBuf out = {0};
	menu_rebuild(conf, block.p, first, &out);

	int rc = 0;
	if (strcmp(out.p, conf) != 0)
		rc = kb_write_file_atomic(conf_path, out.p);
	if (rc == 0 && trial) {
		char tdir[PATH_MAX], tconf[PATH_MAX];
		KbBuf tout = {0};
		snprintf(tdir, sizeof(tdir), "%s/" TRIAL_DIR, esp);
		snprintf(tconf, sizeof(tconf), "%s/" TRIAL_DIR "/limine.conf",
			 esp);
		menu_rebuild(conf, block.p, first + 3, &tout);
		rc = kb_mkdir_p(tdir) == 0 ? write_if_changed(tconf, tout.p)
					   : -1;
		kb_buf_free(&tout);
	}
	kb_buf_free(&block);
	/* The flat pair goes once nothing names it; until then it is the
	 * kernel a slot without a directory of its own boots. */
	if (rc == 0 && !strstr(out.p, "boot():/EFI/kdos/vmlinuz")) {
		char f[PATH_MAX];
		snprintf(f, sizeof(f), "%s/EFI/kdos/vmlinuz", esp);
		unlink(f);
		snprintf(f, sizeof(f), "%s/EFI/kdos/initramfs.cpio.gz", esp);
		unlink(f);
	}
	kb_buf_free(&out);
	free(conf);
	return rc;
}

/* Copy a file onto the ESP whole: written beside the target, flushed, renamed
 * over it, and the directory flushed. FAT has no journal, so an interruption
 * leaves the old file and never half of the new one. */
static int copy_atomic(const char *src, const char *dst)
{
	char tmp[PATH_MAX], dir[PATH_MAX], buf[1 << 16];
	int in, out, rc = -1;
	ssize_t r;

	snprintf(tmp, sizeof(tmp), "%s.new", dst);
	in = open(src, O_RDONLY | O_CLOEXEC);
	if (in < 0)
		return -1;
	out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (out < 0) {
		close(in);
		return -1;
	}
	while ((r = read(in, buf, sizeof(buf))) > 0)
		for (ssize_t off = 0; off < r;) {
			ssize_t w = write(out, buf + off, (size_t)(r - off));
			if (w <= 0) {
				if (w < 0 && errno == EINTR)
					continue;
				r = -1;
				goto done;
			}
			off += w;
		}
done:
	close(in);
	if (r == 0 && fsync(out) == 0 && close(out) == 0) {
		out = -1;
		if (rename(tmp, dst) == 0)
			rc = 0;
	}
	if (out >= 0)
		close(out);
	if (rc != 0) {
		unlink(tmp);
		return -1;
	}
	kb_strlcpy(dir, dst, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash) {
		*slash = 0;
		int dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (dfd >= 0) {
			fsync(dfd);
			close(dfd);
		}
	}
	return 0;
}

/* The source the mount table names for the filesystem mounted at `path`, the
 * last mount there winning. -1 when nothing is mounted there. */
static int mount_source(const char *path, char *src, size_t srclen)
{
	char want[PATH_MAX];

	src[0] = 0;
	if (!realpath(path, want))
		return -1;
	char *mi = kb_read_all("/proc/self/mountinfo", NULL);
	if (!mi)
		return -1;
	for (char *l = mi; l && *l;) {
		char *nl = strchr(l, '\n');
		if (nl)
			*nl = 0;
		/* id parent maj:min root MOUNTPOINT ... - fstype SOURCE ... */
		char mp[PATH_MAX], fs[64], dev[PATH_MAX];
		char *sep = strstr(l, " - ");
		if (sep && sscanf(l, "%*s %*s %*s %*s %4095s", mp) == 1 &&
		    sscanf(sep + 3, "%63s %4095s", fs, dev) == 2 &&
		    !strcmp(mp, want))
			kb_strlcpy(src, dev, srclen);	/* the last wins */
		l = nl ? nl + 1 : NULL;
	}
	free(mi);
	return src[0] ? 0 : -1;
}

/*
 * Which slot the filesystem mounted at `root` is: the device the mount table
 * names for it, against each slot's /dev/disk/by-uuid link, both resolved.
 * The mount table and not st_dev, because a btrfs root reports an anonymous
 * device number that is no block device at all. -1 when neither matches.
 */
static int slot_of_root(const BootState *st, const char *root)
{
	char src[PATH_MAX], dev[PATH_MAX];

	if (mount_source(root, src, sizeof(src)) != 0 || !realpath(src, dev))
		return -1;
	for (int i = 0; i < 2; i++) {
		char by[PATH_MAX], real[PATH_MAX];
		if (!st->slot[i][0])
			continue;
		snprintf(by, sizeof(by), "/dev/disk/by-uuid/%s", st->slot[i]);
		if (realpath(by, real) && !strcmp(real, dev))
			return i;
	}
	return -1;
}

/* A newc header's hex field of eight digits; -1 for anything else. */
static long newc_field(const unsigned char *h)
{
	long v = 0;

	for (int k = 0; k < 8; k++) {
		int c = h[k];
		int d = c >= '0' && c <= '9'   ? c - '0'
			: c >= 'a' && c <= 'f' ? c - 'a' + 10
			: c >= 'A' && c <= 'F' ? c - 'A' + 10
					       : -1;
		if (d < 0)
			return -1;
		v = v * 16 + d;
	}
	return v;
}

/* Read exactly n bytes of a stream, or discard them when buf is NULL. */
static int read_full(int fd, unsigned char *buf, size_t n)
{
	unsigned char sink[4096];

	while (n) {
		size_t want = buf ? n : (n < sizeof(sink) ? n : sizeof(sink));
		ssize_t r = read(fd, buf ? buf : sink, want);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0)
			return -1;
		if (buf)
			buf += r;
		n -= (size_t)r;
	}
	return 0;
}

/*
 * Whether an initramfs carries modules for kernel `ver`: 1 when it holds
 * lib/modules/<ver>/, 0 when it holds a module tree for other kernels only,
 * -1 when it holds none or cannot be read. The microcode at the front is
 * uncompressed newc and is walked in place; the rest — the image's archive
 * and any the `linux` postinstall appended — is read through `gzip -dc`,
 * archive after archive.
 */
static int initramfs_carries(const char *path, const char *ver)
{
	unsigned char h[110];
	char name[512], want[256];
	off_t off = 0;
	int fd = open(path, O_RDONLY | O_CLOEXEC), trees = 0, found = 0;

	if (fd < 0)
		return -1;
	snprintf(want, sizeof(want), "lib/modules/%s/", ver);
	while (pread(fd, h, sizeof(h), off) == (ssize_t)sizeof(h) &&
	       !memcmp(h, "070701", 6)) {
		long fsz = newc_field(h + 54), nsz = newc_field(h + 94);
		if (fsz < 0 || nsz < 1 || nsz >= (long)sizeof(name) ||
		    pread(fd, name, (size_t)nsz, off + 110) != (ssize_t)nsz)
			break;
		name[nsz - 1] = 0;
		off = (off + 110 + nsz + 3) / 4 * 4;
		off = (off + fsz + 3) / 4 * 4;
		if (strcmp(name, "TRAILER!!!"))
			continue;
		unsigned char z[4];
		while (pread(fd, z, 4, off) == 4 && !memcmp(z, "\0\0\0\0", 4))
			off += 4;
	}
	unsigned char magic[2];
	if (pread(fd, magic, 2, off) != 2 || magic[0] != 0x1f ||
	    magic[1] != 0x8b || lseek(fd, off, SEEK_SET) != off) {
		close(fd);
		return -1;
	}

	int p[2];
	if (pipe(p) != 0) {
		close(fd);
		return -1;
	}
	pid_t pid = fork();
	if (pid == 0) {
		dup2(fd, 0);
		dup2(p[1], 1);
		/* gzip must hold no copy of the pipe but its stdout. The reader
		 * stops at the first matching entry and closes its end; a read
		 * end left open in gzip keeps the pipe alive, gzip blocks on a
		 * full pipe instead of dying of EPIPE, and the waitpid below
		 * never returns on any initramfs larger than the pipe buffer. */
		close(p[0]);
		if (p[1] != 1)
			close(p[1]);
		int nul = open("/dev/null", O_WRONLY);
		if (nul >= 0)
			dup2(nul, 2);
		execlp("gzip", "gzip", "-dc", (char *)NULL);
		_exit(127);
	}
	close(fd);
	close(p[1]);
	if (pid < 0) {
		close(p[0]);
		return -1;
	}
	int complete = 0;
	unsigned long long pos = 0;
	for (;;) {
		/* Archives follow one another, each padded with zeros. */
		if (read_full(p[0], h, 4) != 0) {
			complete = 1;
			break;
		}
		pos += 4;
		if (!memcmp(h, "\0\0\0\0", 4))
			continue;
		if (read_full(p[0], h + 4, sizeof(h) - 4) != 0 ||
		    memcmp(h, "070701", 6))
			break;
		pos += sizeof(h) - 4;
		long fsz = newc_field(h + 54), nsz = newc_field(h + 94);
		if (fsz < 0 || nsz < 1 || nsz >= (long)sizeof(name) ||
		    read_full(p[0], (unsigned char *)name, (size_t)nsz) != 0)
			break;
		pos += (unsigned long long)nsz;
		name[nsz - 1] = 0;
		size_t pad = (size_t)((4 - pos % 4) % 4);
		if (read_full(p[0], NULL, pad) != 0)
			break;
		pos += pad;
		pad = (size_t)((4 - (unsigned long long)fsz % 4) % 4);
		if (read_full(p[0], NULL, (size_t)fsz + pad) != 0)
			break;
		pos += (unsigned long long)fsz + pad;
		const char *n = name;
		if (!strncmp(n, "./", 2))
			n += 2;
		if (!strncmp(n, "lib/modules/", 12) && strchr(n + 12, '/')) {
			trees = 1;
			if (!strncmp(n, want, strlen(want))) {
				found = 1;
				break;
			}
		}
	}
	close(p[0]);
	int status = 0;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	if (found)
		return 1;
	if (!complete || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	return trees ? 0 : -1;
}

/*
 * The ESP directory a machine with no boot state boots its kernel from: the
 * one the first /KDOS entry's `path:` names, else the flat EFI/kdos. kinstall
 * writes slot A's directory into the menu whether or not it could write a
 * boot state, and a kernel put anywhere else is a kernel nothing boots.
 */
static void menu_kernel_dir(const char *esp, char *out, size_t n)
{
	char conf_path[PATH_MAX];
	char *conf;
	const char *cur;
	Line l;
	int in = 0;

	kb_strlcpy(out, "EFI/kdos", n);
	menu_path(esp, conf_path, sizeof(conf_path));
	if (!(conf = kb_read_all(conf_path, NULL)))
		return;
	cur = conf;
	while (line_next(&cur, &l)) {
		if (!is_indented(&l)) {
			if (in && l.n)
				break;
			in = l.n && is_ours(&l);
			continue;
		}
		if (!in)
			continue;
		const char *v = l.p, *end = l.p + l.n;
		while (v < end && (*v == ' ' || *v == '\t'))
			v++;
		static const char key[] = "path: boot():/";
		static const char tail[] = "/vmlinuz";
		size_t kl = sizeof(key) - 1, tl = sizeof(tail) - 1;
		if ((size_t)(end - v) <= kl + tl || strncmp(v, key, kl) ||
		    strncmp(end - tl, tail, tl))
			continue;
		size_t dl = (size_t)(end - v) - kl - tl;
		const char *d = v + kl;
		if (dl < n && dl >= 8 && !strncmp(d, "EFI/kdos", 8) &&
		    (dl == 8 || d[8] == '/') && !memchr(d, '.', dl)) {
			memcpy(out, d, dl);
			out[dl] = 0;
		}
		break;
	}
	free(conf);
}

/* EFI/kdos/<slot>/release, the identity `deploy` recorded for that slot. */
static void release_path(const char *esp, int i, char *out, size_t n)
{
	snprintf(out, n, "%s/EFI/kdos/%s/release", esp, slot_name(i));
}

static int ident_from_esp(const char *esp, int i, SlotIdent *id)
{
	char p[PATH_MAX];
	char *d;

	memset(id, 0, sizeof(*id));
	release_path(esp, i, p, sizeof(p));
	if (!(d = kb_read_all(p, NULL)))
		return -1;
	char t[24];
	osr_value(d, "VERSION", id->version, sizeof(id->version));
	osr_value(d, "BUILD_ID", id->build, sizeof(id->build));
	osr_value(d, "BUILD_DATE", id->date, sizeof(id->date));
	osr_value(d, "TREE", id->tree, sizeof(id->tree));
	osr_value(d, "TREE_TIME", t, sizeof(t));
	id->tree_time = atoll(t);
	free(d);
	return 0;
}

/* Written whole through the same temp, fsync and rename the boot state gets,
 * and only when it differs: the ESP is FAT, and a half-written release file
 * names a version nothing installed. */
static int ident_write_esp(const char *esp, int i, const SlotIdent *id)
{
	char p[PATH_MAX];
	KbBuf b = {0};

	release_path(esp, i, p, sizeof(p));
	kb_buf_printf(&b,
		      "# What slot %s holds. Written by kdos-bootctl deploy from\n"
		      "# that root's /etc/kdos-release.\n"
		      "VERSION=\"%s\"\n"
		      "BUILD_ID=\"%s\"\n"
		      "BUILD_DATE=\"%s\"\n"
		      "TREE=\"%s\"\n"
		      "TREE_TIME=\"%lld\"\n",
		      slot_name(i), id->version, id->build, id->date, id->tree,
		      id->tree_time);
	int rc = write_if_changed(p, b.p);
	kb_buf_free(&b);
	return rc;
}

/* The slot this boot runs: its menu entry's kdos_slot=, else, on the machine
 * itself, the slot whose filesystem is mounted at /. -1 when neither says. */
static int this_slot(const BootState *st)
{
	int r = running_slot();
	const char *root = sysroot();

	if (r < 0 && root && !strcmp(root, "/") && !getenv("KDOS_CMDLINE"))
		r = slot_of_root(st, "/");
	return r;
}

/* What slot i holds: the running slot's own root first, since a slot updated
 * in place has moved past what its last `deploy` recorded, then the record
 * on the ESP. */
static void slot_ident(const BootState *st, int i, SlotIdent *id)
{
	char esp[1024];
	const char *root = sysroot();

	memset(id, 0, sizeof(*id));
	if (!st->slot[i][0])
		return;
	if (i == this_slot(st) && root) {
		ident_from_root(root, id);
		if (id->version[0])
			return;
	}
	if (esp_root(esp, sizeof(esp)) == 0)
		ident_from_esp(esp, i, id);
}

/* The version the slot this boot does NOT run holds, for `kdos update notes`
 * (update.c): "" when nothing records one, which is every machine with no
 * boot state or no second slot. A user cannot open the state file on the
 * ESP, so the welcome tour's What's New page, which runs as one, reads the
 * other slot's version from the published slot.json instead. */
static const char *slot_pub_path(int *forced);
void bootctl_other_version(char *out, size_t n);
void bootctl_other_version(char *out, size_t n)
{
	BootState st;
	SlotIdent id;

	out[0] = 0;
	if (state_load(&st) != 0) {
		int forced;
		const char *p = slot_pub_path(&forced);
		char *d = p ? kb_read_all(p, NULL) : NULL;
		const char *k = d ? strstr(d, "\"previous_version\": \"") : NULL;
		if (k) {
			k += strlen("\"previous_version\": \"");
			size_t len = strcspn(k, "\"\\\n");
			if (len < n) {
				memcpy(out, k, len);
				out[len] = 0;
			}
		}
		free(d);
		return;
	}
	int r = this_slot(&st);
	if (r < 0)
		return;
	slot_ident(&st, !r, &id);
	kb_strlcpy(out, id.version, n);
}

/* "KDOS 0.2.13 (slot B)", the sentence a person reads on the logout screen
 * and in the System Versions page. */
static void slot_title(int i, const SlotIdent *id, char *out, size_t n)
{
	if (id->version[0])
		snprintf(out, n, "KDOS %s (slot %c)", id->version,
			 i == 1 ? 'B' : 'A');
	else
		snprintf(out, n, "KDOS (slot %c)", i == 1 ? 'B' : 'A');
}

/*
 * `deploy` puts a root's kernel on the ESP, in the directory of the slot that
 * root is. The initramfs is the one that goes with that kernel: the `linux`
 * postinstall's /boot/initramfs-kdos.cpio.gz where it wrote one, else the
 * image's own /boot/initramfs.cpio.gz, which is the one built for the image's
 * kernel. An initramfs holding modules for other kernels and none for this
 * one is refused, and the slot keeps what it booted: that init could not load
 * vfat to read the boot state, so it would count no attempt and never roll
 * back, and a root on xfs, f2fs, LUKS or md would not mount at all.
 *
 * The initramfs goes first and the kernel last, so the directory never holds
 * a new kernel beside an initramfs that lacks its modules; then the menu is
 * regenerated, which is what makes the directory bootable at all.
 *
 * A machine with no boot state has one root and no slots; its kernel goes
 * where the menu's first /KDOS entry boots it from.
 *
 * A slot's identity goes into EFI/kdos/<slot>/release with its kernel, after
 * the kernel and before the menu: a release file is only ever beside the
 * kernel it describes. A record that cannot be written is reported and does
 * not fail the deploy; the slot then boots and is named "KDOS (slot B)".
 */
static int cmd_deploy(BootState *st, int have, int argc, char **argv)
{
	char esp[1024], kern[PATH_MAX], rd[PATH_MAX], dir[2048];
	const char *root = argc > 2 ? argv[2] : NULL;
	struct stat ks, is;

	if (!root) {
		fprintf(stderr, "usage: kdos-bootctl deploy <root> [<a|b>]\n");
		return 2;
	}
	if (esp_root(esp, sizeof(esp)) != 0) {
		fprintf(stderr, "bootctl: %s is not on an ESP\n", state_path());
		return 1;
	}
	snprintf(kern, sizeof(kern), "%s/boot/vmlinuz-kdos", root);
	snprintf(rd, sizeof(rd), "%s/boot/initramfs-kdos.cpio.gz", root);
	if (!kb_path_exists(rd))
		snprintf(rd, sizeof(rd), "%s/boot/initramfs.cpio.gz", root);
	if (stat(kern, &ks) != 0 || !S_ISREG(ks.st_mode) ||
	    stat(rd, &is) != 0 || !S_ISREG(is.st_mode)) {
		fprintf(stderr, "bootctl: %s has no kernel and initramfs in "
				"/boot\n", root);
		return 1;
	}
	char kver[128];
	kernel_version(kern, kver, sizeof(kver));
	if (kver[0] && initramfs_carries(rd, kver) == 0) {
		fprintf(stderr, "bootctl: %s carries no modules for %s — "
				"nothing was written\n", rd, kver);
		return 1;
	}

	int i = -1;
	if (have) {
		i = argc > 3 ? slot_index(argv[3]) : slot_of_root(st, root);
		if (i < 0) {
			fprintf(stderr, "bootctl: %s is not a slot's root — "
					"name the slot\n", root);
			return argc > 3 ? 2 : 1;
		}
		if (!st->slot[i][0]) {
			fprintf(stderr, "bootctl: slot %s has no root "
					"filesystem\n", slot_name(i));
			return 1;
		}
		snprintf(dir, sizeof(dir), "%s/EFI/kdos/%s", esp, slot_name(i));
	} else {
		char rel[256];
		menu_kernel_dir(esp, rel, sizeof(rel));
		snprintf(dir, sizeof(dir), "%s/%s", esp, rel);
	}
	if (kb_mkdir_p(dir) != 0) {
		fprintf(stderr, "bootctl: cannot create %s: %s\n", dir,
			strerror(errno));
		return 1;
	}

	/* Both new files exist beside the old ones until each rename, so the
	 * whole of both has to fit; a megabyte over that for the directory
	 * entries and the FAT itself. Refused before a byte is written: an
	 * ESP that fills halfway through leaves a .new behind and the slot on
	 * its old kernel, which is the outcome, but not a way to learn it. */
	struct statvfs vf;
	unsigned long long need = (unsigned long long)ks.st_size +
				  (unsigned long long)is.st_size + (1ULL << 20);
	if (statvfs(dir, &vf) == 0) {
		unsigned long long avail =
			(unsigned long long)vf.f_bavail * vf.f_frsize;
		if (avail < need) {
			fprintf(stderr, "bootctl: the ESP has %llu MiB free and "
					"this kernel needs %llu MiB — nothing "
					"was written\n", avail >> 20,
				need >> 20);
			return 1;
		}
	}

	char dst[PATH_MAX];
	snprintf(dst, sizeof(dst), "%s/initramfs.cpio.gz", dir);
	if (copy_atomic(rd, dst) != 0) {
		fprintf(stderr, "bootctl: cannot write %s: %s\n", dst,
			strerror(errno));
		return 1;
	}
	snprintf(dst, sizeof(dst), "%s/vmlinuz", dir);
	if (copy_atomic(kern, dst) != 0) {
		fprintf(stderr, "bootctl: cannot write %s: %s\n", dst,
			strerror(errno));
		return 1;
	}
	if (have) {
		SlotIdent id;
		ident_from_root(root, &id);
		if (ident_write_esp(esp, i, &id) != 0)
			fprintf(stderr, "bootctl: cannot record what slot %s "
					"holds on the ESP: %s\n", slot_name(i),
				strerror(errno));
	}
	if (have && menu_sync(st) != 0) {
		fprintf(stderr, "bootctl: the kernel is in %s but the boot menu "
				"could not be rewritten\n", dir);
		return 1;
	}

	if (have)
		printf("slot %s boots %s from EFI/kdos/%s\n", slot_name(i),
		       kver[0] ? kver : "its kernel", slot_name(i));
	else
		printf("this machine boots %s from %s\n",
		       kver[0] ? kver : "its kernel", dir + strlen(esp) + 1);
	return 0;
}

/* ── UEFI BootNext: one boot of the candidate ──────────────────────────── */

/*
 * A CANDIDATE KERNEL THAT DIES BEFORE ITS INITRAMFS COUNTS NOTHING, so on
 * UEFI the counting is not what brings the machine back. `try` leaves the
 * menu's default on the confirmed slot and asks the FIRMWARE for one boot of
 * the candidate: a Boot#### load option that starts a copy of Limine in
 * EFI/kdos/trial/, whose limine.conf defaults to the candidate's entry, and
 * BootNext naming that option. The firmware deletes BootNext before it starts
 * the loader, so whatever happens to that boot — a panic, a hang and the reset
 * button, a userland that never confirms — the boot after it starts
 * EFI/BOOT/ and the confirmed slot. The candidate becomes the default only at
 * `mark-good`.
 *
 * WRITTEN THROUGH efivarfs AND NOT efibootmgr: the option is a fixed shape
 * built from the ESP's own partition entry, and nothing in it comes from a
 * name anybody typed. A state file pointed elsewhere with KDOS_BOOTSTATE
 * touches no firmware variable unless KDOS_EFIVARS names a directory as well,
 * so a fixture never reaches the NVRAM of the machine running it.
 */

#define EFI_GLOBAL_GUID "8be4df61-93ca-11d2-aa0d-00e098032b8c"
#define TRIAL_DESC "KDOS update trial"
/* NON_VOLATILE | BOOTSERVICE_ACCESS | RUNTIME_ACCESS, the attributes every
 * Boot#### and BootNext carry. */
#define EFI_VAR_ATTRS 0x7u

/* The efivarfs directory, or NULL where no firmware variable may be written. */
static const char *efivars_dir(void)
{
	const char *e = getenv("KDOS_EFIVARS");

	if (e && *e)
		return e;
	e = getenv("KDOS_BOOTSTATE");
	if (e && *e && strcmp(e, BOOTSTATE_DEFAULT))
		return NULL;
	return "/sys/firmware/efi/efivars";
}

/* The loader this firmware can execute: fw_platform_size sits beside the
 * efivars directory, and a kernel that does not publish it is read as 64. */
static const char *efi_loader(const char *vars)
{
	char p[PATH_MAX], v[8] = "";

	snprintf(p, sizeof(p), "%s/../fw_platform_size", vars);
	kb_read_file(p, v, sizeof(v));
	return atoi(v) == 32 ? "BOOTIA32.EFI" : "BOOTX64.EFI";
}

static void le16(unsigned char *p, unsigned v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

static void le32(unsigned char *p, uint32_t v)
{
	for (int k = 0; k < 4; k++)
		p[k] = (v >> (8 * k)) & 0xff;
}

static void le64(unsigned char *p, uint64_t v)
{
	for (int k = 0; k < 8; k++)
		p[k] = (v >> (8 * k)) & 0xff;
}

static uint64_t get_le(const unsigned char *p, int n)
{
	uint64_t v = 0;

	while (n--)
		v = v << 8 | p[n];
	return v;
}

/* What an EFI hard-drive device path node says about a partition. */
typedef struct {
	uint32_t number;	/* 1-based */
	uint64_t start, size;	/* in the disk's logical blocks */
	unsigned char sig[16];	/* GPT: the partition GUID as stored */
	unsigned char mbr_type;	/* 1 MBR, 2 GPT */
	unsigned char sig_type;	/* 1 MBR disk signature, 2 GUID */
} EfiPart;

/*
 * The partition entry of `number` on `disk`, read from the table itself: the
 * GPT header at LBA 1 of a 512- or a 4096-byte-sector disk, else a primary
 * MBR entry. Everything the firmware matches a hard-drive node against comes
 * from here, in the disk's own block units.
 */
static int efi_part_read(const char *disk, uint32_t number, EfiPart *ep)
{
	unsigned char s0[512], h[92], e[128];
	int fd = open(disk, O_RDONLY | O_CLOEXEC), rc = -1;

	memset(ep, 0, sizeof(*ep));
	ep->number = number;
	if (fd < 0)
		return -1;
	if (!number || pread(fd, s0, sizeof(s0), 0) != (ssize_t)sizeof(s0))
		goto out;
	for (unsigned lbs = 512; lbs <= 4096; lbs *= 8) {
		if (pread(fd, h, sizeof(h), lbs) != (ssize_t)sizeof(h) ||
		    memcmp(h, "EFI PART", 8))
			continue;
		uint64_t at = get_le(h + 72, 8);
		uint32_t count = (uint32_t)get_le(h + 80, 4);
		uint32_t esz = (uint32_t)get_le(h + 84, 4);
		if (number > count || esz < sizeof(e) || esz > 4096 ||
		    pread(fd, e, sizeof(e), (off_t)(at * lbs +
				    (uint64_t)(number - 1) * esz)) !=
			    (ssize_t)sizeof(e))
			goto out;
		static const unsigned char zero[16];
		uint64_t first = get_le(e + 32, 8), last = get_le(e + 40, 8);
		if (!memcmp(e, zero, 16) || last < first)
			goto out;
		memcpy(ep->sig, e + 16, 16);
		ep->start = first;
		ep->size = last - first + 1;
		ep->mbr_type = 2;
		ep->sig_type = 2;
		rc = 0;
		goto out;
	}
	if (s0[510] == 0x55 && s0[511] == 0xAA && number <= 4) {
		const unsigned char *pe = s0 + 446 + 16 * (number - 1);
		if (!pe[4] || pe[4] == 0xEE)
			goto out;
		memcpy(ep->sig, s0 + 440, 4);
		ep->start = get_le(pe + 8, 4);
		ep->size = get_le(pe + 12, 4);
		ep->mbr_type = 1;
		ep->sig_type = 1;
		rc = 0;
	}
out:
	close(fd);
	return rc;
}

/*
 * The disk and partition number the ESP at `esp` is on: the mount table's
 * source, its /sys/class/block entry's `partition`, and the directory above
 * that entry, which is the whole disk. KDOS_ESP_DISK=<image>:<n> stands in
 * for all three.
 */
static int esp_disk(const char *esp, char *disk, size_t n, uint32_t *number)
{
	const char *e = getenv("KDOS_ESP_DISK");
	char src[PATH_MAX], dev[PATH_MAX], p[PATH_MAX], name[NAME_MAX + 1];
	char v[16] = "";

	if (e && *e) {
		const char *c = strrchr(e, ':');
		if (!c || (size_t)(c - e) >= n)
			return -1;
		memcpy(disk, e, (size_t)(c - e));
		disk[c - e] = 0;
		*number = (uint32_t)strtoul(c + 1, NULL, 10);
		return *number ? 0 : -1;
	}
	if (mount_source(esp, src, sizeof(src)) != 0 || !realpath(src, dev))
		return -1;
	const char *base = strrchr(dev, '/');
	kb_strlcpy(name, base ? base + 1 : dev, sizeof(name));
	snprintf(p, sizeof(p), "/sys/class/block/%s/partition", name);
	if (kb_read_file(p, v, sizeof(v)) <= 0 || !(*number = (uint32_t)atoi(v)))
		return -1;
	snprintf(p, sizeof(p), "/sys/class/block/%s", name);
	if (!realpath(p, src))
		return -1;
	char *slash = strrchr(src, '/');
	if (!slash)
		return -1;
	*slash = 0;
	slash = strrchr(src, '/');
	kb_strlcpy(name, slash ? slash + 1 : src, sizeof(name));
	snprintf(disk, n, "/dev/%s", name);
	return 0;
}

/*
 * An EFI_LOAD_OPTION: attributes, the device path's length, the description
 * in UCS-2, then a hard-drive node, a file-path node and the end node. `path`
 * is relative to the ESP and written with backslashes. Returns the length,
 * and writes the option only when it fits in `cap`.
 */
static size_t load_option(unsigned char *out, size_t cap, const char *desc,
			  const EfiPart *ep, const char *path)
{
	size_t dl = strlen(desc), pl = strlen(path);
	size_t fnode = 4 + 2 * (1 + pl + 1);
	size_t fpl = 42 + fnode + 4;
	size_t need = 4 + 2 + 2 * (dl + 1) + fpl;

	if (!out || cap < need || fpl > 0xffff)
		return need;
	unsigned char *p = out;
	le32(p, 1);			/* LOAD_OPTION_ACTIVE */
	le16(p + 4, (unsigned)fpl);
	p += 6;
	for (size_t k = 0; k <= dl; k++, p += 2)
		le16(p, k < dl ? (unsigned char)desc[k] : 0);
	/* MEDIA_DEVICE_PATH / MEDIA_HARDDRIVE_DP */
	p[0] = 4;
	p[1] = 1;
	le16(p + 2, 42);
	le32(p + 4, ep->number);
	le64(p + 8, ep->start);
	le64(p + 16, ep->size);
	memcpy(p + 24, ep->sig, 16);
	p[40] = ep->mbr_type;
	p[41] = ep->sig_type;
	p += 42;
	/* MEDIA_DEVICE_PATH / MEDIA_FILEPATH_DP */
	p[0] = 4;
	p[1] = 4;
	le16(p + 2, (unsigned)fnode);
	p += 4;
	le16(p, '\\');
	p += 2;
	for (size_t k = 0; k <= pl; k++, p += 2)
		le16(p, k == pl ? 0 : path[k] == '/' ? '\\'
						    : (unsigned char)path[k]);
	/* END_DEVICE_PATH / END_ENTIRE */
	p[0] = 0x7f;
	p[1] = 0xff;
	le16(p + 2, 4);
	return need;
}

static void efivar_path(const char *vars, const char *name, char *out,
			size_t n)
{
	snprintf(out, n, "%s/%s-" EFI_GLOBAL_GUID, vars, name);
}

/*
 * One variable, replaced whole: efivarfs takes the attributes and the data in
 * a single write, and a write to an existing variable appends nothing only
 * because the old one is deleted first — which is also what gives a fixture
 * directory the same bytes.
 */
static int efivar_write(const char *vars, const char *name,
			const unsigned char *data, size_t len)
{
	char path[PATH_MAX];
	unsigned char *buf = malloc(len + 4);
	int rc = -1;

	if (!buf)
		return -1;
	efivar_path(vars, name, path, sizeof(path));
	unlink(path);
	le32(buf, EFI_VAR_ATTRS);
	memcpy(buf + 4, data, len);
	int fd = open(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
	if (fd >= 0) {
		if (write(fd, buf, len + 4) == (ssize_t)(len + 4))
			rc = 0;
		if (close(fd) != 0)
			rc = -1;
	}
	free(buf);
	return rc;
}

/* A variable's data without its attributes; NULL when it does not exist. */
static unsigned char *efivar_read(const char *vars, const char *name,
				  size_t *len)
{
	char path[PATH_MAX];
	size_t n = 0;

	efivar_path(vars, name, path, sizeof(path));
	unsigned char *d = (unsigned char *)kb_read_all(path, &n);
	if (!d || n < 4) {
		free(d);
		return NULL;
	}
	memmove(d, d + 4, n - 4);
	*len = n - 4;
	return d;
}

/* Whether the load option `d` carries our description. */
static int is_trial_option(const unsigned char *d, size_t n)
{
	size_t dl = strlen(TRIAL_DESC);

	if (n < 6 + 2 * (dl + 1))
		return 0;
	for (size_t k = 0; k <= dl; k++)
		if (get_le(d + 6 + 2 * k, 2) !=
		    (k < dl ? (unsigned char)TRIAL_DESC[k] : 0u))
			return 0;
	return 1;
}

/* The number of our Boot#### option; with `pick`, the lowest number nothing
 * uses when there is none. -1 when there is none to give. */
static int trial_option(const char *vars, int pick)
{
	static unsigned char used[0x10000 / 8];
	DIR *d = opendir(vars);
	struct dirent *de;
	int found = -1;

	if (!d)
		return -1;
	memset(used, 0, sizeof(used));
	while ((de = readdir(d))) {
		unsigned num;
		char tail[64], name[16];
		if (sscanf(de->d_name, "Boot%4X-%63s", &num, tail) != 2 ||
		    strlen(de->d_name) != 9 + strlen(EFI_GLOBAL_GUID) ||
		    strcmp(tail, EFI_GLOBAL_GUID))
			continue;
		used[num / 8] |= 1u << (num % 8);
		snprintf(name, sizeof(name), "Boot%04X", num);
		size_t n = 0;
		unsigned char *data = efivar_read(vars, name, &n);
		if (data && found < 0 && is_trial_option(data, n))
			found = (int)num;
		free(data);
	}
	closedir(d);
	if (found >= 0 || !pick)
		return found;
	for (unsigned k = 0; k < 0x10000; k++)
		if (!(used[k / 8] & (1u << (k % 8))))
			return (int)k;
	return -1;
}

/*
 * Arm the one-shot boot: the loader copied beside the trial limine.conf that
 * menu_sync wrote, the Boot#### option naming it, then BootNext. Anything
 * missing is reported and returns -1, and the caller falls back to the
 * menu-led trial.
 */
static int trial_arm(const char *esp)
{
	const char *vars = efivars_dir();
	char disk[PATH_MAX], src[PATH_MAX], dst[PATH_MAX], rel[64], name[16];
	uint32_t number;
	EfiPart ep;

	if (!vars || !kb_path_exists(vars))
		return -1;
	const char *loader = efi_loader(vars);
	snprintf(dst, sizeof(dst), "%s/" TRIAL_DIR "/limine.conf", esp);
	if (!kb_path_exists(dst)) {
		fprintf(stderr, "bootctl: no trial menu could be written\n");
		return -1;
	}
	snprintf(src, sizeof(src), "%s/EFI/BOOT/%s", esp, loader);
	snprintf(dst, sizeof(dst), "%s/" TRIAL_DIR "/%s", esp, loader);
	if (copy_atomic(src, dst) != 0) {
		fprintf(stderr, "bootctl: cannot copy %s into " TRIAL_DIR
				": %s\n", src, strerror(errno));
		return -1;
	}
	if (esp_disk(esp, disk, sizeof(disk), &number) != 0 ||
	    efi_part_read(disk, number, &ep) != 0) {
		fprintf(stderr, "bootctl: cannot read the ESP's partition "
				"entry\n");
		return -1;
	}
	int num = trial_option(vars, 1);
	if (num < 0) {
		fprintf(stderr, "bootctl: no free Boot#### number\n");
		return -1;
	}
	snprintf(rel, sizeof(rel), TRIAL_DIR "/%s", loader);
	size_t len = load_option(NULL, 0, TRIAL_DESC, &ep, rel);
	unsigned char *opt = malloc(len);
	unsigned char next[2];
	int rc = -1;
	if (opt) {
		load_option(opt, len, TRIAL_DESC, &ep, rel);
		snprintf(name, sizeof(name), "Boot%04X", (unsigned)num);
		le16(next, (unsigned)num);
		if (efivar_write(vars, name, opt, len) == 0 &&
		    efivar_write(vars, "BootNext", next, 2) == 0)
			rc = num;
		else
			fprintf(stderr, "bootctl: cannot write %s: %s\n",
				name, strerror(errno));
		free(opt);
	}
	return rc;
}

/* Our Boot#### option and a BootNext naming it, removed once no trial is
 * pending. Silent where there are no firmware variables. */
static void trial_disarm(void)
{
	const char *vars = efivars_dir();
	char path[PATH_MAX], name[16];

	if (!vars)
		return;
	int num = trial_option(vars, 0);
	if (num < 0)
		return;
	size_t n = 0;
	unsigned char *next = efivar_read(vars, "BootNext", &n);
	if (next && n == 2 && get_le(next, 2) == (uint64_t)num) {
		efivar_path(vars, "BootNext", path, sizeof(path));
		unlink(path);
	}
	free(next);
	snprintf(name, sizeof(name), "Boot%04X", (unsigned)num);
	efivar_path(vars, name, path, sizeof(path));
	unlink(path);
}

/* ── the commands ──────────────────────────────────────────────────────── */

/*
 * What a slot is, in the words the System Versions page shows: `empty` (no
 * root recorded), `armed` (the next boot tries it), `trial` (running, not
 * yet confirmed), `running` (the confirmed slot, running), `confirmed` (the
 * confirmed slot while the other one is on trial), `failed` (its last trial
 * ended unconfirmed) or `inactive` (the other root, holding what it held).
 */
static const char *slot_state(const BootState *st, int i, int run)
{
	if (!st->slot[i][0])
		return "empty";
	if (i == st->trying)
		return i == run ? "trial" : "armed";
	if (i == st->active)
		return i == run || run < 0 ? "running" : "confirmed";
	if (i == st->failed)
		return "failed";
	return "inactive";
}

/* A JSON string, or null for an empty one. */
static void json_str_or_null(KbBuf *b, const char *s)
{
	if (s && *s)
		kb_json_str(b, s);
	else
		kb_buf_str(b, "null");
}

static void json_slot(KbBuf *b, int i, int ident)
{
	if (ident < 0) {
		kb_buf_str(b, "null");
		return;
	}
	kb_buf_printf(b, "\"%s\"", slot_name(i));
}

/* What each slot holds, its title and the kernel version its ESP directory
 * boots ("" when the ESP holds none it would boot). */
static void slots_describe(const BootState *st, SlotIdent id[2],
			   char title[2][160], char kver[2][128])
{
	char esp[1024];
	int on_esp = esp_root(esp, sizeof(esp)) == 0;

	for (int i = 0; i < 2; i++) {
		char d[64], f[PATH_MAX];
		int own;
		kver[i][0] = 0;
		slot_ident(st, i, &id[i]);
		slot_title(i, &id[i], title[i], sizeof(title[i]));
		if (on_esp && st->slot[i][0] &&
		    slot_boot_dir(esp, i, d, sizeof(d), &own)) {
			snprintf(f, sizeof(f), "%s/%s/vmlinuz", esp, d);
			kernel_version(f, kver[i], sizeof(kver[i]));
		}
	}
}

/*
 * ONE LINE, AND EACH `"key": value` WITH THE SPACE. `kdos update` reads
 * `configured`, `active`, `trying`, `slot_a`, `slot_b`, `crypt_a`, `crypt_b`
 * and `running` with strstr, so their spelling and that space are an
 * interface; the rest is for readers that parse JSON properly — the System
 * Versions page, the notices and kdos-sysupdated.
 */
static void status_json(const BootState *st, KbBuf *b)
{
	int run = this_slot(st);
	SlotIdent id[2];
	char title[2][160], kver[2][128];

	slots_describe(st, id, title, kver);
	kb_buf_printf(b,
		      "{\"configured\": true, \"active\": \"%s\", "
		      "\"trying\": %s%s%s, \"attempts\": %d, "
		      "\"bootnext\": %s, "
		      "\"slot_a\": \"%s\", \"slot_b\": \"%s\", "
		      "\"crypt_a\": \"%s\", \"crypt_b\": \"%s\", ",
		      slot_name(st->active),
		      st->trying >= 0 ? "\"" : "null",
		      st->trying >= 0 ? slot_name(st->trying) : "",
		      st->trying >= 0 ? "\"" : "", st->attempts,
		      st->bootnext ? "true" : "false",
		      st->slot[0], st->slot[1], st->crypt[0], st->crypt[1]);
	kb_buf_str(b, "\"running\": ");
	json_slot(b, run, run);
	kb_buf_str(b, ", \"next\": ");
	json_slot(b, next_slot(st), 0);
	kb_buf_str(b, ", \"failed\": ");
	json_slot(b, st->failed, st->failed);
	kb_buf_str(b, ", \"entry\": ");
	if (st->trying >= 0)
		kb_buf_printf(b, "\"kdos-%s\"", slot_name(st->trying));
	else
		kb_buf_str(b, "null");
	kb_buf_str(b, ", \"slots\": {");
	for (int i = 0; i < 2; i++) {
		kb_buf_printf(b, "%s\"%s\": {\"state\": \"%s\", \"title\": ",
			      i ? ", " : "", slot_name(i),
			      slot_state(st, i, run));
		kb_json_str(b, title[i]);
		kb_buf_str(b, ", \"version\": ");
		json_str_or_null(b, id[i].version);
		kb_buf_str(b, ", \"build\": ");
		json_str_or_null(b, id[i].build);
		kb_buf_str(b, ", \"date\": ");
		json_str_or_null(b, id[i].date);
		kb_buf_str(b, ", \"tree\": ");
		json_str_or_null(b, id[i].tree);
		kb_buf_printf(b, ", \"tree_time\": %lld, \"kernel\": ",
			      id[i].tree_time);
		json_str_or_null(b, kver[i]);
		kb_buf_str(b, "}");
	}
	kb_buf_str(b, "}}\n");
}

static void list_json(const BootState *st, int pretty, KbBuf *b);

/*
 * THE COPIES A SESSION CAN READ. The ESP is root's alone (mounted
 * umask=0077), so the state file is closed to everything that runs as a
 * user: the logout screen asking what the restart will start, the welcome
 * tour's System page, the update tray, About this System. Every run as root
 * against this machine's own state therefore writes `status --json`, `list
 * --json=short` and the flat slot.json to /run, world-readable, and a
 * `--json` read that cannot open the state file prints those copies
 * instead. kdos-sysupdated runs `status
 * --json` at start, every half minute and after each change it makes, and
 * `try`, `cancel`, `mark-good`, `set-slot` and `deploy` write them as they
 * change the state, so a copy trails the state by half a minute at most.
 *
 * KDOS_BOOTSTATE_PUB, KDOS_BOOT_ENTRIES and KDOS_SLOT_PUB stand in for the
 * three paths; a fixture state file publishes nothing without them, and only
 * root writes the real ones.
 */
static const char *pub_path(const char *var, const char *def, int *forced)
{
	const char *e = getenv(var);
	const char *s = getenv("KDOS_BOOTSTATE");

	*forced = e && *e;
	if (*forced)
		return e;
	if (s && *s && strcmp(s, BOOTSTATE_DEFAULT))
		return NULL;
	return def;
}

static const char *pub_status_path(int *forced)
{
	return pub_path("KDOS_BOOTSTATE_PUB", "/run/kdos/bootstate.json", forced);
}

static const char *boot_entries_path(int *forced)
{
	return pub_path("KDOS_BOOT_ENTRIES", "/run/kdos/boot-entries.json",
			forced);
}

static const char *slot_pub_path(int *forced)
{
	return pub_path("KDOS_SLOT_PUB", "/run/kdos/slot.json", forced);
}

/*
 * /run/kdos/slot.json, flat for a shell reader (kcm-kdos-health's
 * about-slot, About this System's "System slot" row): the slot this boot
 * runs, its version, its state — confirmed, trial, booted (picked for one
 * boot, nothing confirmed) or single (no second root) — and the other
 * slot's letter and version, "" where there is none. Nothing when this boot
 * is no slot the state knows; the reader then falls back to kdos_slot= on
 * the command line.
 */
static int slot_json(const BootState *st, KbBuf *b)
{
	int run = this_slot(st);
	SlotIdent id[2];

	if (run < 0 || !st->slot[run][0])
		return -1;
	slot_ident(st, run, &id[0]);
	slot_ident(st, !run, &id[1]);
	const char *state = !st->slot[!run][0] ? "single"
			    : run == st->trying ? "trial"
			    : run == st->active ? "confirmed" : "booted";
	kb_buf_printf(b, "{\"slot\": \"%s\", \"version\": ", slot_name(run));
	kb_json_str(b, id[0].version);
	kb_buf_printf(b, ", \"state\": \"%s\", \"previous_slot\": \"%s\", "
			 "\"previous_version\": ", state,
		      st->slot[!run][0] ? slot_name(!run) : "");
	kb_json_str(b, st->slot[!run][0] ? id[1].version : "");
	kb_buf_str(b, "}\n");
	return 0;
}

/* A file written whole and only when it differs, its directory made first:
 * a copy rewritten every half minute with the same bytes is churn. */
static int write_run_file(const char *p, const char *data)
{
	char dir[PATH_MAX];

	kb_strlcpy(dir, p, sizeof(dir));
	char *slash = strrchr(dir, '/');
	if (slash && slash != dir) {
		*slash = 0;
		kb_mkdir_p(dir);
	}
	return write_if_changed(p, data);
}

static void publish(const BootState *st)
{
	int forced;
	const char *p;
	KbBuf b = {0};

	if ((p = pub_status_path(&forced)) && (forced || geteuid() == 0)) {
		status_json(st, &b);
		if (write_run_file(p, b.p) != 0)
			fprintf(stderr, "bootctl: cannot write %s: %s\n", p,
				strerror(errno));
		kb_buf_free(&b);
	}
	if ((p = slot_pub_path(&forced)) && (forced || geteuid() == 0)) {
		if (slot_json(st, &b) == 0) {
			if (write_run_file(p, b.p) != 0)
				fprintf(stderr, "bootctl: cannot write %s: %s\n",
					p, strerror(errno));
		} else {
			unlink(p);
		}
		kb_buf_free(&b);
	}
	if ((p = boot_entries_path(&forced)) && (forced || geteuid() == 0)) {
		list_json(st, 0, &b);
		if (write_run_file(p, b.p) != 0)
			fprintf(stderr, "bootctl: cannot write %s: %s — the "
					"logout screen will not name the slot\n",
				p, strerror(errno));
		kb_buf_free(&b);
	}
}

/* The state file is there and this process may not read it: a user, on an
 * ESP mounted for root alone. */
static int state_closed(void)
{
	struct stat sb;
	if (stat(state_path(), &sb) != 0)
		return errno == EACCES;
	return access(state_path(), R_OK) != 0;
}

/* A published copy, printed as it was written when it is the shape `want`
 * opens with; 0 when one was printed. */
static int print_copy(const char *p, char want)
{
	char *d = p ? kb_read_all(p, NULL) : NULL;
	int ok = d && d[0] == want && strchr(d, '\n');

	if (ok)
		fputs(d, stdout);
	free(d);
	return ok ? 0 : -1;
}

static int cmd_status(const BootState *st, int have, int json)
{
	if (!have) {
		int forced, closed = state_closed();
		if (json && closed &&
		    print_copy(pub_status_path(&forced), '{') == 0)
			return 0;
		if (json)
			printf("{\"configured\": false}\n");
		else if (closed)
			printf("the boot state at %s is readable by root alone; "
			       "`kdos-bootctl status --json` prints the copy "
			       "published in /run/kdos\n", state_path());
		else
			printf("no boot state at %s — this machine has one "
			       "root and no rollback\n", state_path());
		return 1;
	}
	if (json) {
		KbBuf b = {0};
		status_json(st, &b);
		fputs(b.p, stdout);
		kb_buf_free(&b);
		return 0;
	}
	int run = this_slot(st);
	SlotIdent id[2];
	char title[2][160], kver[2][128], esp[1024];
	int on_esp = esp_root(esp, sizeof(esp)) == 0;
	slots_describe(st, id, title, kver);
	for (int i = 0; i < 2; i++)
		if (st->slot[i][0])
			printf("slot %s   %s, %s\n", slot_name(i), title[i],
			       slot_state(st, i, run));
	if (st->failed >= 0)
		printf("failed   %s's last trial ended without being "
		       "confirmed\n", slot_name(st->failed));
	printf("active   %s  %s\n", slot_name(st->active),
	       st->slot[st->active][0] ? st->slot[st->active] : "(no root)");
	if (st->crypt[st->active][0])
		printf("         inside LUKS %s\n", st->crypt[st->active]);
	printf("other    %s  %s\n", slot_name(!st->active),
	       st->slot[!st->active][0] ? st->slot[!st->active] : "(no root)");
	if (st->crypt[!st->active][0])
		printf("         inside LUKS %s\n", st->crypt[!st->active]);
	/* The kernel each slot boots, read out of the bzImage on the ESP. */
	if (on_esp)
		for (int i = 0; i < 2; i++) {
			char d[64], f[PATH_MAX], ver[128];
			int own;
			if (!st->slot[i][0])
				continue;
			if (!slot_boot_dir(esp, i, d, sizeof(d), &own)) {
				printf("kernel   %s  none on the ESP\n",
				       slot_name(i));
				continue;
			}
			snprintf(f, sizeof(f), "%s/%s/vmlinuz", esp, d);
			kernel_version(f, ver, sizeof(ver));
			printf("kernel   %s  %s from %s%s\n", slot_name(i),
			       ver[0] ? ver : "(no version)", d,
			       own ? "" : ", the pair either slot boots");
		}
	if (trial_pending(st))
		printf("trying   %s once, through UEFI BootNext — any boot after "
		       "that one starts %s\n", slot_name(st->trying),
		       slot_name(st->active));
	else if (st->trying >= 0 && st->bootnext)
		printf("trying   %s, booted once and not confirmed — the next "
		       "boot starts %s\n", slot_name(st->trying),
		       slot_name(st->active));
	else if (st->trying >= 0)
		printf("trying   %s, %d attempt(s) left\n",
		       slot_name(st->trying), st->attempts);
	else
		printf("trying   nothing — this slot is confirmed good\n");
	return 0;
}

/*
 * `list --json=short`: the boot entries in the shape `bootctl list --json`
 * prints, for the logout screen. plasma-workspace's logout greeter reads login1's
 * RebootToBootLoaderEntry — `kdos-<slot>` once `try` has run — and shows the
 * `showTitle` of the entry whose `id` equals it; KDOS carries a patch that
 * makes it run this command instead of `bootctl`, so no program of that name
 * is installed. One entry per slot with a root, the one the next boot starts
 * first.
 *
 * ALWAYS VALID JSON AND EXIT 0 UNDER --json, `[]` for a machine with no boot
 * state: the greeter treats any other exit as "no entries" and a parse
 * failure the same way, and a caller has no other way to tell them apart.
 * The greeter runs as the user, so what it reads is the copy root published
 * (THE COPIES A SESSION CAN READ, above).
 */
static void list_json(const BootState *st, int pretty, KbBuf *b)
{
	int run = this_slot(st), next = next_slot(st);
	int order[2] = { next, !next };
	int n = 0;

	kb_buf_str(b, pretty ? "[\n" : "[");
	for (int k = 0; k < 2; k++) {
		int i = order[k];
		SlotIdent id;
		char title[160], eid[16];

		if (!st->slot[i][0])
			continue;
		slot_ident(st, i, &id);
		slot_title(i, &id, title, sizeof(title));
		snprintf(eid, sizeof(eid), "kdos-%s", slot_name(i));
		if (n++)
			kb_buf_str(b, pretty ? ",\n" : ",");
		kb_buf_str(b, pretty ? "  {\n    \"type\": " : "{\"type\":");
		kb_json_str(b, "kdos-slot");
		kb_buf_str(b, pretty ? ",\n    \"id\": " : ",\"id\":");
		kb_json_str(b, eid);
		kb_buf_str(b, pretty ? ",\n    \"title\": " : ",\"title\":");
		kb_json_str(b, title);
		kb_buf_str(b, pretty ? ",\n    \"showTitle\": " : ",\"showTitle\":");
		kb_json_str(b, title);
		kb_buf_str(b, pretty ? ",\n    \"version\": " : ",\"version\":");
		json_str_or_null(b, id.version);
		kb_buf_printf(b, pretty ? ",\n    \"isDefault\": %s,\n"
					  "    \"isSelected\": %s\n  }"
					: ",\"isDefault\":%s,\"isSelected\":%s}",
			      i == next ? "true" : "false",
			      i == run ? "true" : "false");
	}
	kb_buf_str(b, pretty ? (n ? "\n]\n" : "]\n") : "]\n");
}

static int cmd_list(const BootState *st, int have, int argc, char **argv)
{
	const char *mode = NULL;

	for (int k = 2; k < argc; k++) {
		if (!strcmp(argv[k], "--json"))
			mode = "short";
		else if (!strncmp(argv[k], "--json=", 7))
			mode = argv[k] + 7;
		else {
			fprintf(stderr, "usage: kdos-bootctl list "
					"[--json[=short|pretty|off]]\n");
			return 2;
		}
	}
	if (mode && !strcmp(mode, "off"))
		mode = NULL;
	if (mode && strcmp(mode, "short") && strcmp(mode, "pretty")) {
		fprintf(stderr, "bootctl: --json takes short, pretty or off\n");
		return 2;
	}
	int pretty = mode && !strcmp(mode, "pretty");

	if (!have) {
		int forced, closed = state_closed();
		if (mode) {
			/* Root's copy, printed short as it was written (the
			 * greeter parses either), else no entry at all. */
			if (!closed ||
			    print_copy(boot_entries_path(&forced), '[') != 0)
				printf("[]\n");
			return 0;
		}
		if (closed)
			printf("the boot state at %s is readable by root alone; "
			       "`kdos-bootctl list --json` prints the copy "
			       "published in /run/kdos\n", state_path());
		else
			printf("no boot state at %s — this machine has one root "
			       "and no other entry\n", state_path());
		return 1;
	}

	if (!mode) {
		int run = this_slot(st), next = next_slot(st);
		int order[2] = { next, !next };
		for (int k = 0; k < 2; k++) {
			int i = order[k];
			SlotIdent id;
			char title[160];
			if (!st->slot[i][0])
				continue;
			slot_ident(st, i, &id);
			slot_title(i, &id, title, sizeof(title));
			printf("kdos-%s  %s%s%s\n", slot_name(i), title,
			       i == next ? "  (default)" : "",
			       i == run ? "  (running)" : "");
		}
		return 0;
	}
	KbBuf b = {0};
	list_json(st, pretty, &b);
	fputs(b.p, stdout);
	kb_buf_free(&b);
	return 0;
}

/*
 * The one-shot entry elogind reports as login1's RebootToBootLoaderEntry when
 * it runs with SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY=1: the logout screen reads
 * it to say which version the restart starts. KDOS_REBOOT_ENTRY stands in for
 * the path; a fixture state file writes none without it. It describes this
 * boot's next one, so it lives on /run and is gone after the restart.
 */
static const char *reboot_entry_path(void)
{
	const char *e = getenv("KDOS_REBOOT_ENTRY");
	const char *s = getenv("KDOS_BOOTSTATE");

	if (e && *e)
		return e;
	if (s && *s && strcmp(s, BOOTSTATE_DEFAULT))
		return NULL;
	return "/run/systemd/reboot-to-boot-loader-entry";
}

/* The entry goes with the trial it named: a cancelled trial leaves the
 * logout screen naming no other version. */
static void reboot_entry_clear(int slot)
{
	const char *p = reboot_entry_path();
	char line[16], want[16];

	if (!p || kb_read_file(p, line, sizeof(line)) <= 0)
		return;
	line[strcspn(line, "\r\n")] = 0;
	snprintf(want, sizeof(want), "kdos-%s", slot_name(slot));
	if (!strcmp(line, want))
		unlink(p);
}

/* The published list first, then the entry that sends the greeter to it, so
 * a greeter that reads the entry always finds the list naming it. */
static void reboot_entry_set(const BootState *st, int slot)
{
	const char *p = reboot_entry_path();
	char line[16];

	publish(st);
	if (!p)
		return;
	snprintf(line, sizeof(line), "kdos-%s\n", slot_name(slot));
	if (write_run_file(p, line) != 0)
		fprintf(stderr, "bootctl: cannot write %s: %s — the logout "
				"screen will not name the slot\n", p,
			strerror(errno));
}

/*
 * `select` is the initramfs's whole job here: decide, count down, and print the
 * UUID to boot. Everything it can do is one of three things —
 *
 *   trying and attempts left   spend one and boot the candidate
 *   trying and out of attempts ROLL BACK: forget the candidate, boot active
 *   not trying                 boot active
 *
 * — and the counting happens BEFORE the boot, not after, because a boot that
 * hangs must still consume its attempt. That is the whole reason this is not in
 * `rcS`.
 */
static int cmd_select(BootState *st, int have, int entry)
{
	if (!have)
		return 1;	/* nothing to say; the caller keeps root= */

	/*
	 * A BOOTNEXT TRIAL HAS ONE BOOT, and the firmware has already spent
	 * the request for it. The candidate's own entry spends the state's
	 * copy too; any other entry is the confirmed slot's — the default the
	 * firmware fell back to after a candidate that died, or never started
	 * the trial loader at all — and rolls the candidate back. A candidate
	 * picked by hand after its boot was spent is one more boot of it,
	 * which `mark-good` confirms as usual. Every trial that ends here
	 * without its boot is recorded in `failed`, for the rollback notice.
	 */
	if (st->trying >= 0 && st->bootnext) {
		int boot = st->active;
		if (entry == st->trying) {
			boot = st->trying;
			if (st->attempts > 0) {
				st->attempts = 0;
				if (state_save(st) != 0)
					fprintf(stderr, "bootctl: WARNING: could "
							"not write the boot "
							"state\n");
			} else
				fprintf(stderr, "bootctl: slot %s picked from "
						"the menu — one boot, nothing "
						"counted\n",
					slot_name(entry));
		} else {
			fprintf(stderr, "bootctl: slot %s did not confirm — "
					"rolling back to %s\n",
				slot_name(st->trying), slot_name(st->active));
			st->failed = st->trying;
			st->trying = -1;
			st->attempts = 0;
			st->bootnext = 0;
			if (state_save(st) != 0)
				fprintf(stderr, "bootctl: WARNING: could not "
						"write the boot state\n");
			else if (menu_sync(st) != 0)
				fprintf(stderr, "bootctl: WARNING: could not "
						"rewrite the boot menu\n");
		}
		if (!st->slot[boot][0])
			return 1;
		printf("%s\n", st->slot[boot]);
		return 0;
	}

	/*
	 * THE ENTRY'S SLOT IS NOT THE ONE THE MENU LEADS WITH, so somebody
	 * picked the other entry by hand, and the kernel running this is that
	 * slot's: it boots that slot and no other. Picking the confirmed slot
	 * while a candidate is on trial abandons the candidate — that entry is
	 * the way back from a kernel that dies before this initramfs can count
	 * anything. Any other hand-picked boot is one boot of the other root:
	 * nothing spent and nothing promoted, except that a candidate picked
	 * after its last attempt is still the candidate, and `mark-good`
	 * confirms it. An abandoned candidate is recorded in `failed`, as any
	 * trial is that ends unconfirmed.
	 */
	if (entry >= 0 && entry != next_slot(st)) {
		if (st->trying >= 0 && entry == st->active) {
			fprintf(stderr, "bootctl: slot %s picked from the menu "
					"— abandoning the update in slot %s\n",
				slot_name(entry), slot_name(st->trying));
			st->failed = st->trying;
			st->trying = -1;
			st->attempts = 0;
			if (state_save(st) != 0)
				fprintf(stderr, "bootctl: WARNING: could not "
						"write the boot state\n");
			else
				menu_sync(st);
		} else
			fprintf(stderr, "bootctl: slot %s picked from the menu "
					"— one boot, nothing counted\n",
				slot_name(entry));
		if (!st->slot[entry][0])
			return 1;
		printf("%s\n", st->slot[entry]);
		return 0;
	}

	int boot = st->active;
	if (st->trying >= 0) {
		if (st->attempts > 0) {
			st->attempts--;
			boot = st->trying;
			if (st->attempts == 0)
				fprintf(stderr, "bootctl: last attempt at slot "
						"%s\n", slot_name(st->trying));
		} else {
			/* The state keeps only the attempts left, not how many
			 * `try` granted, so the message names no count. */
			fprintf(stderr, "bootctl: slot %s spent its attempts "
					"without being marked good — rolling "
					"back to %s\n",
				slot_name(st->trying), slot_name(st->active));
			st->failed = st->trying;
			st->trying = -1;
			st->attempts = 0;
		}
		/* Written before anything boots. A machine that loses power
		 * here comes up with the old state and simply tries again,
		 * which is the failure worth having. */
		if (state_save(st) != 0)
			fprintf(stderr, "bootctl: WARNING: could not write the "
					"boot state — this boot will not "
					"count\n");
		/* The last attempt moves the menu to the active slot's kernel,
		 * so a candidate that fails it rolls back at the next boot
		 * without a reboot in between. */
		else if (menu_sync(st) != 0)
			fprintf(stderr, "bootctl: WARNING: could not rewrite "
					"the boot menu\n");
	}

	if (!st->slot[boot][0])
		return 1;
	printf("%s\n", st->slot[boot]);
	return 0;
}

/*
 * The slot of the kernel running this, from the kdos_slot= its menu entry
 * carries; -1 when the command line names none. KDOS_CMDLINE stands in for
 * /proc/cmdline; a state file other than the real one reads no command line without it, so a
 * fixture run on a KDOS machine is not judged by that machine's own slot.
 */
static int running_slot(void)
{
	const char *e = getenv("KDOS_CMDLINE"), *s = getenv("KDOS_BOOTSTATE");
	char buf[4096] = "";

	if (!(e && *e) && s && *s && strcmp(s, BOOTSTATE_DEFAULT))
		return -1;
	if (kb_read_file(e && *e ? e : "/proc/cmdline", buf, sizeof(buf)) <= 0)
		return -1;
	for (char *w = strtok(buf, " \t\n"); w; w = strtok(NULL, " \t\n"))
		if (!strncmp(w, "kdos_slot=", 10))
			return slot_index(w + 10);
	return -1;
}

/*
 * `mark-good` is the other half. kdos-sysupdated runs it once both bless marks
 * exist under /run/kdos/bless/: `userspace`, which the end of rcS writes after
 * every service that would tell you the userland is broken has started, and
 * `graphical`, which a Plasma session or the login greeter earns by staying
 * up for a minute (a machine whose inittab starts no SDDM needs the first
 * only). Confirming earlier would confirm a system that has not yet failed
 * rather than one that has succeeded.
 */
static int cmd_mark_good(BootState *st, int have)
{
	if (!have)
		return 0;	/* nothing to confirm is not a failure */
	/*
	 * A TRIAL ARMED IN THIS BOOT IS LEFT ALONE, before anything below can
	 * touch it. This boot runs the confirmed slot — `try` refuses the
	 * active one — so there is nothing here to confirm, and each step
	 * after this would undo the update: the disarm removes the BootNext
	 * `try` just wrote, the kernel test rolls a candidate back that has
	 * not had its boot, and the promotion confirms one that never ran.
	 */
	char now[40];
	if (st->trying >= 0 && st->armed[0] && boot_id(now, sizeof(now)) == 0 &&
	    !strcmp(now, st->armed)) {
		printf("slot %s is armed for the next boot; nothing in this "
		       "boot to confirm\n", slot_name(st->trying));
		return 0;
	}
	/* Every boot that gets here has had its BootNext, so the option that
	 * served it goes whatever the state says. */
	trial_disarm();
	/*
	 * A CANDIDATE IS CONFIRMED ONLY ON ITS OWN KERNEL. `select` in the
	 * initramfs rolls back a boot whose kdos_slot= names another slot;
	 * this repeats the test for a boot it did not count (an initramfs
	 * with no kdos-bootctl, a state it could not write), because
	 * confirming that boot would make a kernel that never ran the menu's
	 * lead. It rolls back.
	 */
	int run = running_slot();
	if (st->trying >= 0 && run >= 0 && run != st->trying) {
		fprintf(stderr, "bootctl: slot %s's kernel did not boot — rolling "
				"back to %s\n",
			slot_name(st->trying), slot_name(st->active));
		st->failed = st->trying;
		st->trying = -1;
		st->attempts = 0;
		st->bootnext = 0;
		if (state_save(st) != 0) {
			fprintf(stderr, "bootctl: cannot write the boot state\n");
			return 1;
		}
	}
	/* The menu is brought into line on every boot, confirmed or not:
	 * `select` rewrites it only when its own decision moves the next boot,
	 * and leaves the rollback above, a state it could not write and a boot
	 * it did not run for to this. Written only when it differs. */
	if (st->trying < 0) {
		if (menu_sync(st) != 0)
			fprintf(stderr, "bootctl: cannot rewrite the boot "
					"menu\n");
		return 0;
	}

	st->active = st->trying;
	st->trying = -1;
	st->attempts = 0;
	st->bootnext = 0;
	st->failed = -1;
	if (state_save(st) != 0) {
		fprintf(stderr, "bootctl: cannot write the boot state\n");
		return 1;
	}
	if (menu_sync(st) != 0)
		fprintf(stderr, "bootctl: cannot rewrite the boot menu\n");
	printf("slot %s confirmed good\n", slot_name(st->active));
	return 0;
}

static int bootctl_run(int argc, char **argv);

/*
 * Every command that reads or changes this machine's own boot state leaves
 * the copies a session reads current (publish, THE COPIES A SESSION CAN
 * READ). `select` is not one: it runs in the initramfs, whose /run the booted
 * system does not keep.
 */
int bootctl_main(int argc, char **argv)
{
	static const char *const touches[] = { "status", "list", "try", "cancel",
					       "mark-good", "set-slot", "deploy",
					       NULL };
	const char *cmd = argc > 1 ? argv[1] : "status";
	int rc = bootctl_run(argc, argv);

	for (int i = 0; touches[i]; i++)
		if (!strcmp(cmd, touches[i])) {
			BootState st;
			if (state_load(&st) == 0)
				publish(&st);
			break;
		}
	return rc;
}

static int bootctl_run(int argc, char **argv)
{
	BootState st;
	int have = state_load(&st) == 0;
	const char *cmd = argc > 1 ? argv[1] : "status";

	if (!strcmp(cmd, "status"))
		return cmd_status(&st, have, argc > 2 && !strcmp(argv[2], "--json"));
	if (!strcmp(cmd, "list"))
		return cmd_list(&st, have, argc, argv);
	if (!strcmp(cmd, "select")) {
		/* An entry slot that is not a slot name is ignored rather than
		 * refused: this runs where refusing means no root at all. */
		return cmd_select(&st, have,
				  argc > 2 ? slot_index(argv[2]) : -1);
	}
	if (!strcmp(cmd, "deploy"))
		return cmd_deploy(&st, have, argc, argv);
	if (!strcmp(cmd, "mark-good"))
		return cmd_mark_good(&st, have);
	/* No boot state needed and none consulted: the colours of the menu are
	 * not the A/B machine's business, and a machine with no bootstate must
	 * still be able to repaint its bootloader. */
	if (!strcmp(cmd, "theme"))
		return cmd_theme(argc, argv);
	if (!strcmp(cmd, "splash"))
		return cmd_splash(argc, argv);
	/*
	 * THE CONSOLE PALETTE ON ITS OWN, WRITTEN NOWHERE. `fs/etc/vtrgb` is
	 * generated and committed, so the tree carries a second copy of the
	 * default scheme; this is what the selftest diffs it against, and a
	 * copy nothing compares is the copy that goes stale.
	 */
	if (!strcmp(cmd, "palette")) {
		char vt[512];
		const KcolScheme *sc = argc > 2 ? kcol_find(argv[2])
						: kcol_default();

		if (!sc) {
			fprintf(stderr, "bootctl: no accent named '%s'\n",
				argv[2]);
			return 2;
		}
		if (kcol_vtrgb(sc, vt, sizeof(vt)) >= (int)sizeof(vt)) {
			fprintf(stderr, "bootctl: palette does not fit\n");
			return 1;
		}
		fputs(vt, stdout);
		return 0;
	}

	/*
	 * WHICH CONTAINER THAT FILESYSTEM IS INSIDE, asked by the initramfs
	 * after `select` has chosen.
	 *
	 * KEYED BY THE FILESYSTEM UUID AND NOT BY A SLOT NAME, which is what
	 * makes it a single call with no state between the two. `select` has
	 * already spent an attempt and may have rolled back; asking "which
	 * slot did that turn out to be" would be a second decision, made
	 * separately, that could disagree with the first.
	 *
	 * SILENT AND 1 WHERE THERE IS NO CONTAINER, so the caller's fallback
	 * to the command line's own `cryptdevice=` is the empty answer rather
	 * than a special case.
	 */
	if (!strcmp(cmd, "crypt")) {
		if (argc < 3) {
			fprintf(stderr, "usage: kdos-bootctl crypt <fs-uuid>\n");
			return 2;
		}
		if (!have)
			return 1;
		for (int i = 0; i < 2; i++)
			if (st.slot[i][0] && !strcmp(st.slot[i], argv[2])) {
				if (!st.crypt[i][0])
					return 1;
				printf("%s\n", st.crypt[i]);
				return 0;
			}
		return 1;
	}

	if (!strcmp(cmd, "set-slot")) {
		if (argc < 4) {
			fprintf(stderr, "usage: kdos-bootctl set-slot <a|b> "
					"<uuid> [<luks-uuid>]\n");
			return 2;
		}
		int i = slot_index(argv[2]);
		if (i < 0) {
			fprintf(stderr, "bootctl: no slot named '%s'\n", argv[2]);
			return 2;
		}
		if (!have) {
			/* First write on a machine that had no state: the slot
			 * being described is the one running, so it is active
			 * and confirmed. */
			memset(&st, 0, sizeof(st));
			st.trying = -1;
			st.failed = -1;
			st.active = i;
		}
		kb_strlcpy(st.slot[i], argv[3], sizeof(st.slot[i]));
		/*
		 * THE CONTAINER IS REWRITTEN EVERY TIME, including to empty
		 * when none is given. A slot described without one is a slot
		 * that is not encrypted, and keeping the previous value would
		 * have an updater that reinstalled a plain filesystem over an
		 * encrypted slot leave the initramfs unlocking a container
		 * that is no longer in the way.
		 */
		kb_strlcpy(st.crypt[i], argc > 4 ? argv[4] : "",
			   sizeof(st.crypt[i]));
		if (state_save(&st) != 0)
			return 1;
		return menu_sync(&st) == 0 ? 0 : 1;
	}

	if (!strcmp(cmd, "try")) {
		if (argc < 3) {
			fprintf(stderr, "usage: kdos-bootctl try <a|b> [n]\n");
			return 2;
		}
		if (!have) {
			fprintf(stderr, "bootctl: no boot state to update\n");
			return 1;
		}
		int i = slot_index(argv[2]);
		if (i < 0) {
			fprintf(stderr, "bootctl: no slot named '%s'\n", argv[2]);
			return 2;
		}
		if (!st.slot[i][0]) {
			/* Refused rather than recorded: a `try` pointing at a
			 * slot with no root is a state file that will send the
			 * next boot nowhere. */
			fprintf(stderr, "bootctl: slot %s has no root "
					"filesystem\n", slot_name(i));
			return 1;
		}
		if (i == st.active) {
			fprintf(stderr, "bootctl: slot %s is already active\n",
				slot_name(i));
			return 1;
		}
		/*
		 * A SLOT WITH NO KERNEL IS REFUSED ONCE THE ESP HAS PER-SLOT
		 * DIRECTORIES. The menu cannot lead with a slot that has no
		 * kernel, so it would keep booting the active slot's entry,
		 * and `select` would read that as the confirmed slot picked by
		 * hand and abandon the candidate on its first boot.
		 */
		char esp[1024], d[64];
		int own, own_other, on_esp = esp_root(esp, sizeof(esp)) == 0;
		if (on_esp) {
			int kernel = slot_boot_dir(esp, i, d, sizeof(d),
						   &own) != NULL;
			slot_boot_dir(esp, !i, d, sizeof(d), &own_other);
			if (!kernel && own_other) {
				fprintf(stderr, "bootctl: slot %s has no kernel "
						"on the ESP — run `kdos-bootctl "
						"deploy <its root> %s` first\n",
					slot_name(i), slot_name(i));
				return 1;
			}
		}
		/*
		 * UEFI BOOTNEXT FIRST, THE MENU-LED TRIAL WHERE IT CANNOT BE
		 * HAD. The state and the trial limine.conf are written before
		 * BootNext, so the firmware is never asked for a boot whose
		 * menu does not exist yet; a machine with no firmware
		 * variables — a BIOS boot — or one whose firmware refuses the
		 * write gets `n` counted boots led by the menu instead, and
		 * recovering from a kernel that dies before its initramfs is
		 * then a matter of picking the confirmed slot's entry.
		 */
		int n = argc > 3 ? atoi(argv[3]) : TRY_ATTEMPTS;
		if (n < 1)
			n = 1;
		const char *vars = efivars_dir();
		st.trying = i;
		st.bootnext = on_esp && vars && kb_path_exists(vars);
		st.attempts = st.bootnext ? 1 : n;
		st.failed = -1;
		boot_id(st.armed, sizeof(st.armed));
		trial_disarm();
		if (state_save(&st) != 0)
			return 1;
		if (menu_sync(&st) != 0) {
			fprintf(stderr, "bootctl: the boot menu could not be "
					"rewritten — slot %s will not boot\n",
				slot_name(i));
			return 1;
		}
		if (st.bootnext && trial_arm(esp) < 0) {
			fprintf(stderr, "bootctl: no UEFI BootNext — slot %s is "
					"led by the menu instead\n",
				slot_name(i));
			trial_disarm();
			st.bootnext = 0;
			st.attempts = n;
			if (state_save(&st) != 0)
				return 1;
			if (menu_sync(&st) != 0) {
				fprintf(stderr, "bootctl: the boot menu could "
						"not be rewritten — slot %s "
						"will not boot\n",
					slot_name(i));
				return 1;
			}
		}
		reboot_entry_set(&st, i);
		if (st.bootnext)
			printf("will boot slot %s once, through UEFI BootNext; "
			       "any boot after that one that has not confirmed "
			       "it rolls back to %s\n", slot_name(i),
			       slot_name(st.active));
		else
			printf("will boot slot %s up to %d time(s), then roll "
			       "back to %s; a kernel that dies before its "
			       "initramfs is left by picking /KDOS (slot %s)\n",
			       slot_name(i), st.attempts,
			       slot_name(st.active), slot_name(st.active));
		return 0;
	}

	/*
	 * `cancel`: forget a trial that has not had its boot, so the next boot
	 * starts the confirmed slot. A trial this boot is RUNNING is not
	 * cancelled: it ends by its blessing or by its rollback, and the boot
	 * it was given is already spent.
	 *
	 * THE FIRMWARE'S ONE-SHOT GOES FIRST, then the state: a machine that
	 * loses power between the two comes up on the confirmed slot, whose
	 * `select` rolls the stale trial back, and never on the slot that was
	 * cancelled.
	 */
	if (!strcmp(cmd, "cancel")) {
		if (!have) {
			fprintf(stderr, "bootctl: no boot state to update\n");
			return 1;
		}
		if (st.trying < 0) {
			printf("no trial is armed\n");
			return 0;
		}
		if (this_slot(&st) == st.trying) {
			fprintf(stderr, "bootctl: slot %s is running on trial; it is "
					"confirmed or rolled back, not cancelled\n",
				slot_name(st.trying));
			return 1;
		}
		int was = st.trying;
		trial_disarm();
		st.trying = -1;
		st.attempts = 0;
		st.bootnext = 0;
		st.armed[0] = 0;
		if (state_save(&st) != 0) {
			fprintf(stderr, "bootctl: cannot write the boot state\n");
			return 1;
		}
		if (menu_sync(&st) != 0)
			fprintf(stderr, "bootctl: cannot rewrite the boot menu\n");
		reboot_entry_clear(was);
		printf("slot %s is no longer tried; the next boot starts %s\n",
		       slot_name(was), slot_name(st.active));
		return 0;
	}

	fprintf(stderr,
		"usage: kdos-bootctl {status [--json]|select [<a|b>]|"
		"mark-good|\n"
		"                     cancel|\n"
		"                     list [--json[=short|pretty|off]]|\n"
		"                     crypt <fs-uuid>|\n"
		"                     set-slot <a|b> <uuid> [<luks-uuid>]|\n"
		"                     try <a|b> [n]|\n"
		"                     deploy <root> [<a|b>]|\n"
		"                     palette [<accent>]|\n"
		"                     splash phosphor|modern|\n"
		"                     theme [--print] <accent>}\n");
	return 2;
}

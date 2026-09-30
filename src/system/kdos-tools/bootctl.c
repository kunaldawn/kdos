/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-bootctl — two root slots, and a boot that can change its mind
 *
 *   kdos-bootctl status                what the state file says
 *   kdos-bootctl set-slot a <uuid> [<luks-uuid>]
 *                                      record where a slot lives, and which
 *                                      container it lives inside
 *   kdos-bootctl crypt <fs-uuid>       the container UUID that filesystem is
 *                                      inside, for the initramfs
 *   kdos-bootctl try <slot> [n]        boot that slot once through UEFI
 *                                      BootNext, or n times led by the menu
 *                                      where there is no BootNext
 *   kdos-bootctl deploy <root> [<slot>]
 *                                      put that root's kernel and initramfs
 *                                      in its slot's directory on the ESP
 *   kdos-bootctl select [<slot>]       DECIDE and count down (the initramfs);
 *                                      the slot is the menu entry's kdos_slot=
 *   kdos-bootctl mark-good             this boot worked (the end of rcS)
 *   kdos-bootctl theme <accent>        repaint the boot menu and the text
 *                                      consoles in that scheme
 *   kdos-bootctl theme --print <name>  the theme block, to stdout
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
 * slot names none, so a machine installed before slots carried containers
 * boots exactly as it did.
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
	/* Overridable so the state machine can be exercised without an ESP —
	 * the same reason `kdos stutter` can be pointed at a fixture /proc. */
	const char *e = getenv("KDOS_BOOTSTATE");
	return (e && *e) ? e : BOOTSTATE_DEFAULT;
}

/* ── reading ───────────────────────────────────────────────────────────── */

static int state_load(BootState *st)
{
	memset(st, 0, sizeof(*st));
	st->trying = -1;
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
		/* An UNKNOWN key is skipped by the loop's own default, so a
		 * state file written before these two existed parses as a
		 * machine with no containers — which is what it is. */
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
		}
	}
	free(data);

	/* The one field without which nothing else means anything. */
	if (!saw_active)
		return -1;
	if (st->trying >= 0 && !st->slot[st->trying][0])
		st->trying = -1;	/* trying a slot that has no root */
	if (st->trying < 0)
		st->bootnext = 0;
	return 0;
}

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
		      "bootnext = %s\n",
		      st->slot[0], st->slot[1], st->crypt[0], st->crypt[1],
		      slot_name(st->active),
		      st->trying >= 0 ? slot_name(st->trying) : "",
		      st->attempts,
		      st->trying >= 0 && st->bootnext ? "yes" : "");

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
	 * THE NAME IS ONE OF SEVEN COMPILED-IN STRINGS. That is the whole of
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
	 * OWN. The boot menu and tty1 are the two places an accent has to
	 * reach through a file rather than through a running program, they
	 * are both root's to write, and they are wanted together: a menu
	 * retinted while the login prompt underneath it keeps the old accent
	 * is a boot that changes colour halfway through.
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
 * The kernel command line every /KDOS entry is built from: the first /KDOS
 * entry's, less the words the generator owns — which slot, which root, and
 * the verbosity each entry sets for itself. Everything else carries over, so
 * `bootstate=`, the command line's `cryptdevice=` and anything added by hand
 * survive every regeneration.
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
		   base.p, "quiet loglevel=3");
	kb_buf_str(&block, "\n");
	menu_entry(&block, "KDOS (verbose)",
		   "Every kernel message on the console", dir[next], next,
		   st->slot[next], base.p, "loglevel=7");
	kb_buf_str(&block, "\n");
	menu_entry(&block, "KDOS (single user)", "A root shell, no session",
		   dir[next], next, st->slot[next], base.p,
		   "loglevel=7 single");
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
			   base.p, trial && !panics ? "quiet loglevel=3 panic=10"
						    : "quiet loglevel=3");
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

static int cmd_status(const BootState *st, int have, int json)
{
	if (!have) {
		if (json)
			printf("{\"configured\": false}\n");
		else
			printf("no boot state at %s — this machine has one "
			       "root and no rollback\n", state_path());
		return 1;
	}
	if (json) {
		printf("{\"configured\": true, \"active\": \"%s\", "
		       "\"trying\": %s%s%s, \"attempts\": %d, "
		       "\"bootnext\": %s, "
		       "\"slot_a\": \"%s\", \"slot_b\": \"%s\", "
		       "\"crypt_a\": \"%s\", \"crypt_b\": \"%s\"}\n",
		       slot_name(st->active),
		       st->trying >= 0 ? "\"" : "null",
		       st->trying >= 0 ? slot_name(st->trying) : "",
		       st->trying >= 0 ? "\"" : "", st->attempts,
		       st->bootnext ? "true" : "false",
		       st->slot[0], st->slot[1], st->crypt[0], st->crypt[1]);
		return 0;
	}
	printf("active   %s  %s\n", slot_name(st->active),
	       st->slot[st->active][0] ? st->slot[st->active] : "(no root)");
	if (st->crypt[st->active][0])
		printf("         inside LUKS %s\n", st->crypt[st->active]);
	printf("other    %s  %s\n", slot_name(!st->active),
	       st->slot[!st->active][0] ? st->slot[!st->active] : "(no root)");
	if (st->crypt[!st->active][0])
		printf("         inside LUKS %s\n", st->crypt[!st->active]);
	/* The kernel each slot boots, read out of the bzImage on the ESP. */
	char esp[1024];
	if (esp_root(esp, sizeof(esp)) == 0)
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
	 * which rcS confirms as usual.
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
	 * after its last attempt is still the candidate, and rcS confirms it.
	 */
	if (entry >= 0 && entry != next_slot(st)) {
		if (st->trying >= 0 && entry == st->active) {
			fprintf(stderr, "bootctl: slot %s picked from the menu "
					"— abandoning the update in slot %s\n",
				slot_name(entry), slot_name(st->trying));
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
			fprintf(stderr, "bootctl: slot %s failed %d times — "
					"rolling back to %s\n",
				slot_name(st->trying), TRY_ATTEMPTS,
				slot_name(st->active));
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
 * `mark-good` is the other half, and it runs at the END of rcS — after the
 * services that would tell you the userland is broken have started. Confirming
 * earlier would confirm a system that has not yet failed rather than one that
 * has succeeded.
 */
/*
 * The slot of the kernel running this, from the kdos_slot= its menu entry
 * carries; -1 when the command line names none, as an entry written before
 * menus carried it does. KDOS_CMDLINE stands in for /proc/cmdline; a state
 * file other than the real one reads no command line without it, so a
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

static int cmd_mark_good(BootState *st, int have)
{
	if (!have)
		return 0;	/* nothing to confirm is not a failure */
	/* Every boot that gets here has had its BootNext, so the option that
	 * served it goes whatever the state says. */
	trial_disarm();
	/*
	 * A CANDIDATE IS CONFIRMED ONLY ON ITS OWN KERNEL. An init that
	 * ignores kdos_slot= boots the candidate's root under the confirmed
	 * slot's kernel after the candidate's kernel died; confirming that
	 * boot would make the dead kernel the menu's lead. It rolls back.
	 */
	int run = running_slot();
	if (st->trying >= 0 && run >= 0 && run != st->trying) {
		fprintf(stderr, "bootctl: slot %s's kernel did not boot — rolling "
				"back to %s\n",
			slot_name(st->trying), slot_name(st->active));
		st->trying = -1;
		st->attempts = 0;
		st->bootnext = 0;
		if (state_save(st) != 0) {
			fprintf(stderr, "bootctl: cannot write the boot state\n");
			return 1;
		}
	}
	/* The menu is brought into line on every boot, confirmed or not: an
	 * initramfs whose init predates the menu's kdos_slot= counts and rolls
	 * back without touching the menu, and this is the first code of ours
	 * that runs after it. Written only when it differs. */
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
	if (state_save(st) != 0) {
		fprintf(stderr, "bootctl: cannot write the boot state\n");
		return 1;
	}
	if (menu_sync(st) != 0)
		fprintf(stderr, "bootctl: cannot rewrite the boot menu\n");
	printf("slot %s confirmed good\n", slot_name(st->active));
	return 0;
}

int bootctl_main(int argc, char **argv)
{
	BootState st;
	int have = state_load(&st) == 0;
	const char *cmd = argc > 1 ? argv[1] : "status";

	if (!strcmp(cmd, "status"))
		return cmd_status(&st, have, argc > 2 && !strcmp(argv[2], "--json"));
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

	fprintf(stderr,
		"usage: kdos-bootctl {status [--json]|select [<a|b>]|"
		"mark-good|\n"
		"                     crypt <fs-uuid>|\n"
		"                     set-slot <a|b> <uuid> [<luks-uuid>]|\n"
		"                     try <a|b> [n]|\n"
		"                     deploy <root> [<a|b>]|\n"
		"                     palette [<accent>]|\n"
		"                     theme [--print] <accent>}\n");
	return 2;
}

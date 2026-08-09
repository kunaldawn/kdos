/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — the wizard
 *
 * Nothing on any page writes to a disk. Every decision lands in `cfg`, the
 * summary shows all of them at once, and the install step is the single
 * point of no return. That ordering is the whole reason this is a rewrite
 * and not a port: the old installer partitioned the disk in the middle of
 * the questionnaire, so backing out of question six was not a thing.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "kinstall.h"

/* ──────────────────────────────────────────────────────────────────────── */

/* nvme0n1 -> nvme0n1p1, sda -> sda1. Same rule the installer uses when it
 * writes the table, so the preview and the result cannot disagree. */
static void pname(const char *disk, int n, char *out, size_t cap)
{
	size_t l = strlen(disk);
	int digit = l && isdigit((unsigned char)disk[l - 1]);
	snprintf(out, cap, "%s%s%d", disk, digit ? "p" : "", n);
}

/* ════════════════════════════════════════════════════════════════════════
 * 1 · WELCOME
 * ════════════════════════════════════════════════════════════════════════ */

static const char *KDOS_MARK[] = {
	"██╗  ██╗██████╗  ██████╗ ███████╗",
	"██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝",
	"█████╔╝ ██║  ██║██║   ██║███████╗",
	"██╔═██╗ ██║  ██║██║   ██║╚════██║",
	"██║  ██╗██████╔╝╚██████╔╝███████║",
	"╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝",
};

static void welcome_draw(KRect b)
{
	int y = b.y;

	/* The wordmark is the one place the double-line glyphs are used, and
	 * they are exactly the six that were patched into the console font —
	 * on a terminal that cannot draw them the ASCII table takes over. */
	if (b.h >= 22 && b.w > 40 && (ktui_caps & KT_CAP_UTF8)) {
		for (int i = 0; i < 6; i++)
			ktui_draw_text(b.x + (b.w - 33) / 2, y + i, b.w, KDOS_MARK[i],
				  KT_ACCENT, KT_BG, 0);
		y += 7;
	} else {
		ktui_draw_text(b.x + (b.w - 14) / 2, y, b.w, "K D O S", KT_ACCENT,
			  KT_BG, 0);
		y += 2;
	}

	const char *tag = "I use KDOS btw.";
	ktui_draw_text(b.x + (b.w - (int)strlen(tag)) / 2, y, b.w, tag, KT_MID, KT_BG, 0);
	y += 2;

	ktui_section(b.x, y, b.w, "THIS MACHINE");
	y++;

	char v[128];
	snprintf(v, sizeof(v), "%s  (%d thread%s)", ki_sys.cpu, ki_sys.cores,
		 ki_sys.cores == 1 ? "" : "s");
	ktui_kv(b.x, y++, b.w, "processor", v, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "memory",
	      kb_human_size((unsigned long long)ki_sys.mem_kb * 1024), KT_TEXT);
	snprintf(v, sizeof(v), "%d disk%s visible", ki_ndisk,
		 ki_ndisk == 1 ? "" : "s");
	ktui_kv(b.x, y++, b.w, "storage", v, ki_ndisk ? KT_TEXT : KT_ERR);
	ktui_kv(b.x, y++, b.w, "install size",
	      kb_human_size((unsigned long long)ki_sys.payload_kb * 1024), KT_TEXT);

	snprintf(v, sizeof(v), "%s%s", ki_sys.uefi ? "UEFI" : "legacy BIOS",
		 ki_sys.secure_boot ? "  (Secure Boot enabled)" : "");
	ktui_kv(b.x, y++, b.w, "firmware", v, ki_sys.uefi ? KT_TEXT : KT_ERR);
	y++;

	ktui_section(b.x, y, b.w, "PREFLIGHT");
	y++;

	struct {
		int ok;
		const char *msg;
	} checks[] = {
		{ geteuid() == 0, "running as root" },
		{ ki_sys.uefi, "booted in UEFI mode" },
		{ ki_ndisk > 0, "at least one writable disk" },
		{ kb_have_prog("rsync"), "rsync present" },
		{ kb_have_prog("mkfs.ext4") && kb_have_prog("mkfs.vfat"),
		  "mkfs.ext4 and mkfs.vfat present" },
		{ kb_path_exists("/usr/share/refind"), "rEFInd payload present" },
	};

	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
		ktui_draw_text(b.x, y, 3, checks[i].ok ? ktui_glyph[KT_G_SQUARE] : "!",
			  checks[i].ok ? KT_ACCENT : KT_ERR, KT_BG, 0);
		ktui_draw_text(b.x + 2, y, b.w - 2, checks[i].msg,
			  checks[i].ok ? KT_TEXT : KT_ERR, KT_BG, 0);
		y++;
	}

	if (!ki_sys.uefi) {
		y++;
		ktui_para(b.x, y, b.w,
		     "KDOS ships a UEFI bootloader only. This machine booted in "
		     "legacy mode, so an installed system would not start. Boot "
		     "the medium in UEFI mode and run the installer again.",
		     KT_WARN);
	}
}

static int welcome_validate(char *err, size_t n)
{
	if (geteuid() != 0) {
		snprintf(err, n, "kinstall must run as root — try: sudo kinstall");
		return 1;
	}
	if (!ki_ndisk) {
		snprintf(err, n, "no disks found — nothing can be installed to");
		return 1;
	}
	if (!ki_sys.uefi) {
		snprintf(err, n,
			 "not booted via UEFI — the installed system would not boot");
		return 1;
	}
	return 0;
}

static Page page_welcome = {
	"welcome", "Welcome", "what this machine looks like",
	NULL, welcome_draw, welcome_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 2 · KEYBOARD
 * ════════════════════════════════════════════════════════════════════════ */

#define MAX_KEYMAPS 1024

static char keymaps[MAX_KEYMAPS][32];
static int nkeymaps;
static int kmfilter_idx[MAX_KEYMAPS];
static int nkmfilter;
static char kmfilter[32];
static char kmtest[64];
static KtuiList kmlist;
static int km_loaded;

static void keymap_scan_dir(const char *dir, int depth)
{
	if (depth > 3 || nkeymaps >= MAX_KEYMAPS)
		return;
	DIR *d = opendir(dir);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && nkeymaps < MAX_KEYMAPS) {
		if (e->d_name[0] == '.')
			continue;
		char p[512];
		snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);

		const char *dot = strstr(e->d_name, ".map");
		if (dot) {
			char name[32];
			size_t n = (size_t)(dot - e->d_name);
			if (n >= sizeof(name))
				n = sizeof(name) - 1;
			memcpy(name, e->d_name, n);
			name[n] = 0;
			for (int i = 0; i < nkeymaps; i++)
				if (!strcmp(keymaps[i], name))
					goto next;
			kb_strlcpy(keymaps[nkeymaps++], name, sizeof(keymaps[0]));
			continue;
		}
		keymap_scan_dir(p, depth + 1);
next:
		continue;
	}
	closedir(d);
}

static void keymap_load(void)
{
	if (km_loaded)
		return;
	km_loaded = 1;
	keymap_scan_dir("/usr/share/keymaps", 0);
	keymap_scan_dir("/usr/share/kbd/keymaps", 0);

	for (int i = 1; i < nkeymaps; i++) {
		char t[32];
		kb_strlcpy(t, keymaps[i], sizeof(t));
		int k = i - 1;
		while (k >= 0 && strcmp(keymaps[k], t) > 0) {
			kb_strlcpy(keymaps[k + 1], keymaps[k], sizeof(t));
			k--;
		}
		kb_strlcpy(keymaps[k + 1], t, sizeof(t));
	}
	if (!nkeymaps)
		kb_strlcpy(keymaps[nkeymaps++], "us", sizeof(keymaps[0]));
}

static void keymap_filter(void)
{
	nkmfilter = 0;
	for (int i = 0; i < nkeymaps; i++) {
		if (kmfilter[0]) {
			const char *h = keymaps[i], *n = kmfilter;
			int hit = 0;
			for (; *h; h++) {
				size_t l = strlen(n);
				if (!strncasecmp(h, n, l)) {
					hit = 1;
					break;
				}
			}
			if (!hit)
				continue;
		}
		kmfilter_idx[nkmfilter++] = i;
	}
}

static void keymap_apply(const char *name)
{
	if (!kb_have_prog("loadkeys"))
		return;
	pid_t pid = fork();
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		dup2(null, 1);
		dup2(null, 2);
		execlp("loadkeys", "loadkeys", name, NULL);
		_exit(127);
	}
	if (pid > 0)
		waitpid(pid, NULL, 0);
}

static void keymap_row(int idx, int x, int y, int w, int selected, int focus,
		       void *u)
{
	(void)focus;
	(void)u;
	const char *name = keymaps[kmfilter_idx[idx]];
	int cur = !strcmp(name, cfg.keymap);
	int fg = selected ? KT_BG : cur ? KT_ACCENT : KT_TEXT;
	int bg = selected ? KT_ACCENT : KT_BG;
	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ", fg, bg, 0);
	ktui_draw_text(x + 3, y, w - 3, name, fg, bg, 0);
}

static void keyboard_enter(void)
{
	keymap_load();
	keymap_filter();
	for (int i = 0; i < nkmfilter; i++)
		if (!strcmp(keymaps[kmfilter_idx[i]], cfg.keymap))
			kmlist.sel = i;
}

static void keyboard_draw(KRect b)
{
	int y = b.y;
	int listw = b.w / 2 > 40 ? 40 : b.w / 2;
	if (listw < 20)
		listw = b.w;

	ktui_section(b.x, y, b.w, "CONSOLE KEYMAP");
	y++;
	y += ktui_para(b.x, y, b.w,
		  "The layout the text console uses. It is applied immediately "
		  "so you can try it in the box below, and written to "
		  "/etc/keymap on the installed system.", KT_MID);
	y++;

	ktui_draw_text(b.x, y, 8, "filter", KT_MID, KT_BG, 0);
	if (ktui_input(krect(b.x + 8, y, listw - 8, 1), kmfilter, sizeof(kmfilter),
		     0, "type to narrow"))
		keymap_filter();
	y += 2;

	int lh = b.y + b.h - y - 1;
	if (lh < 3)
		lh = 3;

	int lid = ktui_id();
	if (ktui_list(krect(b.x, y, listw, lh), &kmlist, nkmfilter, keymap_row, NULL,
		    lid)) {
		if (kmlist.sel < nkmfilter) {
			kb_strlcpy(cfg.keymap, keymaps[kmfilter_idx[kmlist.sel]],
				 sizeof(cfg.keymap));
			keymap_apply(cfg.keymap);
		}
	}
	/* Moving the selection is enough to try it — nobody wants to press
	 * Enter on every candidate to find out where the slashes went. */
	if (ktui_focused(lid) && kmlist.sel < nkmfilter &&
	    strcmp(cfg.keymap, keymaps[kmfilter_idx[kmlist.sel]])) {
		kb_strlcpy(cfg.keymap, keymaps[kmfilter_idx[kmlist.sel]],
			 sizeof(cfg.keymap));
		keymap_apply(cfg.keymap);
	}

	if (b.w > listw + 24) {
		int rx = b.x + listw + 3;
		int rw = b.x + b.w - rx;
		int ry = y;
		ktui_section(rx, ry, rw, "SELECTED");
		ry += 2;
		ktui_draw_text(rx, ry, rw, cfg.keymap, KT_ACCENT, KT_BG, 0);
		ry += 2;
		ktui_note(rx, ry++, rw, "try it here:");
		ktui_input(krect(rx, ry, rw, 1), kmtest, sizeof(kmtest), 0,
			 "\\ | @ \" # ~");
		ry += 2;
		if (!kb_have_prog("loadkeys"))
			ktui_para(rx, ry, rw,
			     "loadkeys is not installed, so the choice cannot be "
			     "previewed — it will still be written to the target.",
			     KT_WARN);
	}
}

static Page page_keyboard = {
	"keyboard", "Keyboard", "console keymap",
	keyboard_enter, keyboard_draw, NULL, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 3 · TIME
 * ════════════════════════════════════════════════════════════════════════ */

/* KDOS ships no tzdata — there is no /usr/share/zoneinfo to walk. musl reads
 * a POSIX TZ string straight out of the environment, DST rules and all, so
 * that is what the installer writes. Each row carries both: the label is what
 * a person recognises, the string is what libc actually needs. */
static const struct {
	const char *label;
	const char *tz;
} zones[] = {
	{ "UTC", "UTC0" },
	{ "Atlantic/Reykjavik", "GMT0" },
	{ "Europe/London", "GMT0BST,M3.5.0/1,M10.5.0" },
	{ "Europe/Dublin", "GMT0IST,M3.5.0/1,M10.5.0" },
	{ "Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0" },
	{ "Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Prague", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Budapest", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3" },
	{ "Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
	{ "Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
	{ "Europe/Bucharest", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
	{ "Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
	{ "Europe/Istanbul", "<+03>-3" },
	{ "Europe/Moscow", "MSK-3" },
	{ "Africa/Lagos", "WAT-1" },
	{ "Africa/Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24" },
	{ "Africa/Johannesburg", "SAST-2" },
	{ "Africa/Nairobi", "EAT-3" },
	{ "Asia/Jerusalem", "IST-2IDT,M3.4.4/26,M10.5.0" },
	{ "Asia/Dubai", "<+04>-4" },
	{ "Asia/Karachi", "PKT-5" },
	{ "Asia/Tashkent", "<+05>-5" },
	{ "Asia/Kolkata", "IST-5:30" },
	{ "Asia/Kathmandu", "<+0545>-5:45" },
	{ "Asia/Dhaka", "<+06>-6" },
	{ "Asia/Bangkok", "<+07>-7" },
	{ "Asia/Jakarta", "WIB-7" },
	{ "Asia/Shanghai", "CST-8" },
	{ "Asia/Hong_Kong", "HKT-8" },
	{ "Asia/Singapore", "<+08>-8" },
	{ "Asia/Taipei", "CST-8" },
	{ "Asia/Seoul", "KST-9" },
	{ "Asia/Tokyo", "JST-9" },
	{ "Australia/Perth", "AWST-8" },
	{ "Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
	{ "Australia/Brisbane", "AEST-10" },
	{ "Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
	{ "Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3" },
	{ "Pacific/Honolulu", "HST10" },
	{ "America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0" },
	{ "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0" },
	{ "America/Phoenix", "MST7" },
	{ "America/Denver", "MST7MDT,M3.2.0,M11.1.0" },
	{ "America/Chicago", "CST6CDT,M3.2.0,M11.1.0" },
	{ "America/Mexico_City", "CST6" },
	{ "America/New_York", "EST5EDT,M3.2.0,M11.1.0" },
	{ "America/Toronto", "EST5EDT,M3.2.0,M11.1.0" },
	{ "America/Halifax", "AST4ADT,M3.2.0,M11.1.0" },
	{ "America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0" },
	{ "America/Bogota", "<-05>5" },
	{ "America/Lima", "<-05>5" },
	{ "America/Santiago", "<-04>4<-03>,M9.1.6/24,M4.1.6/24" },
	{ "America/Sao_Paulo", "<-03>3" },
	{ "America/Argentina/Buenos_Aires", "<-03>3" },
};

static const int nzones = (int)(sizeof(zones) / sizeof(zones[0]));
static int tzidx[128], ntzidx;
static char tzfilter[32];
static KtuiList tzlist;

static void tz_filter(void)
{
	ntzidx = 0;
	for (int i = 0; i < nzones; i++) {
		if (tzfilter[0]) {
			int hit = 0;
			for (const char *h = zones[i].label; *h; h++)
				if (!strncasecmp(h, tzfilter, strlen(tzfilter))) {
					hit = 1;
					break;
				}
			if (!hit)
				continue;
		}
		if (ntzidx < (int)(sizeof(tzidx) / sizeof(tzidx[0])))
			tzidx[ntzidx++] = i;
	}
}

static void tz_row(int idx, int x, int y, int w, int selected, int focus, void *u)
{
	(void)focus;
	(void)u;
	int z = tzidx[idx];
	int cur = !strcmp(zones[z].label, cfg.tz_label);
	int fg = selected ? KT_BG : cur ? KT_ACCENT : KT_TEXT;
	int bg = selected ? KT_ACCENT : KT_BG;
	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ", fg, bg, 0);
	ktui_draw_text(x + 3, y, w - 3, zones[z].label, fg, bg, 0);
}

static void time_enter(void)
{
	tz_filter();
	for (int i = 0; i < ntzidx; i++)
		if (!strcmp(zones[tzidx[i]].label, cfg.tz_label))
			tzlist.sel = i;
}

static void time_draw(KRect b)
{
	int y = b.y;
	int listw = b.w / 2 > 40 ? 40 : b.w / 2;
	if (listw < 24)
		listw = b.w;

	ktui_section(b.x, y, b.w, "TIME ZONE");
	y++;
	y += ktui_para(b.x, y, b.w,
		  "KDOS carries no tzdata, so the zone is written as a POSIX TZ "
		  "string that musl reads directly — DST rules included.", KT_MID);
	y++;

	ktui_draw_text(b.x, y, 8, "filter", KT_MID, KT_BG, 0);
	if (ktui_input(krect(b.x + 8, y, listw - 8, 1), tzfilter, sizeof(tzfilter), 0,
		     "type to narrow"))
		tz_filter();
	y += 2;

	int lh = b.y + b.h - y - 1;
	if (lh < 3)
		lh = 3;
	int lid = ktui_id();
	ktui_list(krect(b.x, y, listw, lh), &tzlist, ntzidx, tz_row, NULL, lid);
	if (tzlist.sel < ntzidx) {
		int z = tzidx[tzlist.sel];
		if (ktui_focused(lid)) {
			kb_strlcpy(cfg.tz_label, zones[z].label, sizeof(cfg.tz_label));
			kb_strlcpy(cfg.tz, zones[z].tz, sizeof(cfg.tz));
		}
	}

	if (b.w > listw + 24) {
		int rx = b.x + listw + 3;
		int rw = b.x + b.w - rx;
		int ry = y;
		ktui_section(rx, ry, rw, "SELECTED");
		ry += 2;
		ktui_draw_text(rx, ry++, rw, cfg.tz_label, KT_ACCENT, KT_BG, 0);
		ry++;
		ktui_kv(rx, ry++, rw, "TZ string", cfg.tz, KT_TEXT);
		ry++;

		setenv("TZ", cfg.tz, 1);
		tzset();
		time_t t = time(NULL);
		struct tm tm;
		localtime_r(&t, &tm);
		char when[64];
		strftime(when, sizeof(when), "%a %d %b  %H:%M:%S", &tm);
		ktui_kv(rx, ry++, rw, "local time", when, KT_WARN);
		ry++;
		ktui_para(rx, ry, rw,
		     "The hardware clock is left exactly as it is; only the "
		     "environment is set.", KT_DIM);
	}
}

static Page page_time = {
	"time", "Time", "zone and clock",
	time_enter, time_draw, NULL, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 4 · DISK
 * ════════════════════════════════════════════════════════════════════════ */

static KtuiList disklist;

static void disk_row(int idx, int x, int y, int w, int selected, int focus,
		     void *u)
{
	(void)focus;
	(void)u;
	Disk *d = &ki_disk[idx];
	int cur = !strcmp(d->path, cfg.disk);
	int warn = d->is_boot_media || d->readonly;
	int fg = selected ? KT_BG : warn ? KT_WARN : cur ? KT_ACCENT : KT_TEXT;
	int bg = selected ? KT_ACCENT : KT_BG;

	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ", fg, bg, 0);
	ktui_draw_textf(x + 3, y, w - 3, fg, bg, 0, "%-10s %8s  %-6s %s",
		   d->name,
		   kb_human_size(d->sectors * (unsigned long long)d->sector_size),
		   d->tran, d->is_boot_media ? "[boot medium]" : d->model);
}

static void disk_enter(void)
{
	probe_disks();
	for (int i = 0; i < ki_ndisk; i++)
		if (!strcmp(ki_disk[i].path, cfg.disk))
			disklist.sel = i;
	if (!cfg.disk[0]) {
		/* Default to the first fixed disk that is not the stick we
		 * booted from — the common case, without pre-selecting the
		 * one choice that would destroy the installer mid-run. */
		for (int i = 0; i < ki_ndisk; i++) {
			if (ki_disk[i].is_boot_media || ki_disk[i].readonly)
				continue;
			kb_strlcpy(cfg.disk, ki_disk[i].path, sizeof(cfg.disk));
			disklist.sel = i;
			break;
		}
	}
}

/* A proportional bar of the existing table. Eight colours means the parts are
 * told apart by letter, not by hue — which is also what makes it readable on
 * a monochrome laptop panel. */
static void draw_parttable(KRect r, Disk *d)
{
	if (!d->nparts) {
		ktui_draw_hline(r.x, r.y, r.w, KT_G_SHADE, KT_DIM, KT_BG);
		ktui_draw_text(r.x + 2, r.y, r.w - 4, " unpartitioned ", KT_DIM, KT_BG, 0);
		return;
	}
	for (int i = 0; i < r.w; i++)
		ktui_draw_text(r.x + i, r.y, 1, ktui_glyph[KT_G_SHADE], KT_DIM, KT_BG, 0);

	/* Every partition gets at least one cell, and the cursor walks forward
	 * so a 512M ESP next to a 1T root still shows up instead of being
	 * rounded away and then painted over by its neighbour. */
	int cursor = 0;
	for (int i = 0; i < d->nparts; i++) {
		Part *p = &d->part[i];
		int x0 = (int)((double)p->start / (double)d->sectors * r.w);
		int wl = (int)((double)p->sectors / (double)d->sectors * r.w);
		if (x0 < cursor)
			x0 = cursor;
		if (wl < 1)
			wl = 1;
		if (x0 >= r.w)
			break;
		if (x0 + wl > r.w)
			wl = r.w - x0;
		cursor = x0 + wl;
		int c = p->is_esp ? KT_WARN : KT_MID;
		for (int k = 0; k < wl; k++)
			ktui_draw_text(r.x + x0 + k, r.y, 1, ktui_glyph[KT_G_FULL], c,
				  KT_BG, 0);
		if (wl > 3) {
			char tag[16];
			snprintf(tag, sizeof(tag), "%d", i + 1);
			ktui_draw_text(r.x + x0 + 1, r.y, wl - 1, tag, KT_BG, c,
				  KT_A_REVERSE);
		}
	}
}

static void disk_draw(KRect b)
{
	int y = b.y;

	ktui_section(b.x, y, b.w, "TARGET DISK");
	y++;

	int lh = ki_ndisk > 6 ? 6 : (ki_ndisk ? ki_ndisk : 1);
	int lid = ktui_id();
	ktui_list(krect(b.x, y, b.w, lh), &disklist, ki_ndisk, disk_row, NULL, lid);
	if (ki_ndisk && disklist.sel < ki_ndisk && ktui_focused(lid))
		kb_strlcpy(cfg.disk, ki_disk[disklist.sel].path, sizeof(cfg.disk));
	y += lh + 1;

	Disk *d = ki_ndisk ? &ki_disk[disklist.sel] : NULL;
	if (!d)
		return;

	ktui_section(b.x, y, b.w, "CURRENT LAYOUT");
	y++;
	draw_parttable(krect(b.x, y, b.w, 1), d);
	y += 2;

	unsigned long long need =
		(unsigned long long)(cfg.with_appbox ? ki_sys.payload_kb
						     : ki_sys.payload_kb -
							       ki_sys.appbox_kb) *
		1024;
	unsigned long long have = d->sectors * (unsigned long long)d->sector_size;

	if (d->nparts) {
		ktui_draw_textf(b.x, y++, b.w, KT_MID, KT_BG, 0,
			   "  %-12s %-10s %-9s %-9s %s", "PART", "SIZE", "TYPE",
			   "LABEL", "MOUNT");
		for (int i = 0; i < d->nparts; i++) {
			Part *p = &d->part[i];
			ktui_draw_textf(b.x, y++, b.w, p->is_esp ? KT_WARN : KT_TEXT,
				   KT_BG, 0, "  %-12s %-10s %-9s %-9s %s",
				   p->name,
				   kb_human_size(p->sectors * 512ULL),
				   p->is_esp ? "ESP"
					     : p->fstype[0] ? p->fstype : "-",
				   p->label[0] ? p->label : "-",
				   p->mounted ? p->mountpoint : "");
		}
	} else {
		ktui_note(b.x + 2, y++, b.w, "no partition table");
	}
	y++;

	{
		ktui_kv(b.x, y++, b.w, "capacity", kb_human_size(have), KT_TEXT);
		char v[64];
		snprintf(v, sizeof(v), "%s", kb_human_size(need));
		ktui_kv(b.x, y++, b.w, "install needs", v,
		      have > need + (512ULL << 20) ? KT_TEXT : KT_ERR);
	}

	if (d->is_boot_media)
		ktui_para(b.x, y, b.w,
		     "This is the medium KDOS is running from. Installing onto it "
		     "will pull the filesystem out from under the installer.",
		     KT_ERR);
	else if (d->removable)
		ktui_note(b.x, y, b.w, "removable device");
}

static int disk_validate(char *err, size_t n)
{
	Disk *d = cfg.disk[0] ? disk_by_path(cfg.disk) : NULL;
	if (!d) {
		snprintf(err, n, "select a disk to install onto");
		return 1;
	}
	if (d->is_boot_media) {
		snprintf(err, n, "%s is the running boot medium — pick another",
			 d->name);
		return 1;
	}
	if (d->readonly) {
		snprintf(err, n, "%s is read-only", d->name);
		return 1;
	}
	unsigned long long need =
		(unsigned long long)(cfg.with_appbox
					     ? ki_sys.payload_kb
					     : ki_sys.payload_kb - ki_sys.appbox_kb) *
		1024;
	unsigned long long have = d->sectors * (unsigned long long)d->sector_size;
	if (have < need + (768ULL << 20)) {
		snprintf(err, n, "%s holds %s; the install needs at least %s",
			 d->name, kb_human_size(have),
			 kb_human_size(need + (768ULL << 20)));
		return 1;
	}
	return 0;
}

static Page page_disk = {
	"disk", "Disk", "where KDOS goes",
	disk_enter, disk_draw, disk_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 5 · LAYOUT
 * ════════════════════════════════════════════════════════════════════════ */

static KtuiList esplist, rootlist;
static char swapbuf[16];

static void part_row_generic(int idx, int x, int y, int w, int selected,
			     int focus, void *u)
{
	(void)focus;
	Disk *d = (Disk *)u;
	Part *p = &d->part[idx];
	int fg = selected ? KT_BG : KT_TEXT;
	int bg = selected ? KT_ACCENT : KT_BG;
	ktui_draw_textf(x + 1, y, w - 1, fg, bg, 0, "%-12s %8s  %-8s %s", p->name,
		   kb_human_size(p->sectors * 512ULL),
		   p->is_esp ? "ESP" : p->fstype[0] ? p->fstype : "-",
		   p->label);
}

static void layout_enter(void)
{
	snprintf(swapbuf, sizeof(swapbuf), "%ld", cfg.swap_mb);
	Disk *d = disk_by_path(cfg.disk);
	if (!d)
		return;
	if (!cfg.part_esp[0])
		for (int i = 0; i < d->nparts; i++)
			if (d->part[i].is_esp) {
				kb_strlcpy(cfg.part_esp, d->part[i].path,
					 sizeof(cfg.part_esp));
				esplist.sel = i;
				break;
			}
}

static void layout_draw(KRect b)
{
	Disk *d = disk_by_path(cfg.disk);
	int y = b.y;

	ktui_section(b.x, y, b.w, "PARTITIONING");
	y++;

	ktui_radio(b.x, y++, b.w, "Erase the whole disk and lay it out for KDOS",
		 &cfg.plan, PLAN_WIPE);
	ktui_note(b.x + 4, y++, b.w, "GPT: 512M EFI system partition, then root");
	ktui_radio(b.x, y++, b.w, "Use partitions that already exist",
		 &cfg.plan, PLAN_REUSE);
	ktui_note(b.x + 4, y++, b.w,
	     "keeps every other partition — the dual-boot path");
	ktui_radio(b.x, y++, b.w, "Partition it myself first (cfdisk)",
		 &cfg.plan, PLAN_MANUAL);
	y++;

	if (cfg.plan == PLAN_MANUAL) {
		if (ktui_button(krect(b.x + 4, y, 26, 1), "Open cfdisk", 1, 0)) {
			char *a[] = { "cfdisk", cfg.disk, NULL };
			ktui_run_console(a);
			probe_disks();
			cfg.plan = PLAN_REUSE;
		}
		y += 2;
		ktui_para(b.x, y, b.w,
		     "cfdisk takes over the screen. Make an EFI System partition "
		     "and a Linux partition, write the table, quit — the "
		     "installer comes back and switches to picking them.", KT_MID);
		return;
	}

	if (cfg.plan == PLAN_REUSE) {
		if (!d || !d->nparts) {
			ktui_para(b.x, y, b.w,
			     "This disk has no partitions to reuse. Choose "
			     "another mode.", KT_ERR);
			return;
		}
		int half = (b.w - 3) / 2;
		int lh = d->nparts > 6 ? 6 : d->nparts;

		ktui_section(b.x, y, half, "EFI SYSTEM PARTITION");
		ktui_section(b.x + half + 3, y, half, "ROOT PARTITION");
		y++;

		int e = ktui_id();
		ktui_list(krect(b.x, y, half, lh), &esplist, d->nparts,
			part_row_generic, d, e);
		if (ktui_focused(e) && esplist.sel < d->nparts)
			kb_strlcpy(cfg.part_esp, d->part[esplist.sel].path,
				 sizeof(cfg.part_esp));

		int r = ktui_id();
		ktui_list(krect(b.x + half + 3, y, half, lh), &rootlist, d->nparts,
			part_row_generic, d, r);
		if (ktui_focused(r) && rootlist.sel < d->nparts)
			kb_strlcpy(cfg.part_root, d->part[rootlist.sel].path,
				 sizeof(cfg.part_root));
		y += lh + 1;

		ktui_check(b.x, y++, b.w, "Reformat the ESP as FAT32", &cfg.format_esp);
		ktui_note(b.x + 4, y++, b.w,
		     "leave this off when another OS boots from the same ESP");
		y++;
		ktui_kv(b.x, y++, b.w, "ESP", cfg.part_esp[0] ? cfg.part_esp : "-",
		      KT_TEXT);
		ktui_kv(b.x, y++, b.w, "root",
		      cfg.part_root[0] ? cfg.part_root : "-", KT_ACCENT);
		ktui_note(b.x, y++, b.w, "the root partition is reformatted, always");
		return;
	}

	/* PLAN_WIPE */
	ktui_section(b.x, y, b.w, "FILESYSTEM AND SWAP");
	y++;
	ktui_kv(b.x, y++, b.w, "root filesystem", "ext4", KT_TEXT);
	ktui_note(b.x + 17, y++, b.w,
	     "the only mkfs KDOS ships; btrfs and xfs are not ported yet");
	y++;

	ktui_radio(b.x, y++, b.w / 2, "No swap", &cfg.swap, SWAP_NONE);
	ktui_radio(b.x, y++, b.w / 2, "Swap file on the root filesystem",
		 &cfg.swap, SWAP_FILE);
	ktui_radio(b.x, y++, b.w / 2, "Dedicated swap partition", &cfg.swap,
		 SWAP_PART);

	if (cfg.swap != SWAP_NONE) {
		y++;
		ktui_draw_text(b.x, y, 16, "size (MiB)", KT_MID, KT_BG, 0);
		if (ktui_input(krect(b.x + 17, y, 12, 1), swapbuf, sizeof(swapbuf), 0,
			     "4096")) {
			cfg.swap_mb = atol(swapbuf);
			if (cfg.swap_mb < 0)
				cfg.swap_mb = 0;
		}
		char hint[80];
		snprintf(hint, sizeof(hint), "RAM is %s",
			 kb_human_size((unsigned long long)ki_sys.mem_kb * 1024));
		ktui_note(b.x + 31, y, b.w, hint);
		y += 2;
	} else {
		y++;
	}

	if (!d)
		return;

	ktui_section(b.x, y, b.w, "WHAT WILL BE WRITTEN");
	y++;

	unsigned long long total = d->sectors * (unsigned long long)d->sector_size;
	unsigned long long esp = 512ULL << 20;
	unsigned long long swp = cfg.swap == SWAP_PART
					 ? (unsigned long long)cfg.swap_mb << 20
					 : 0;
	unsigned long long root = total > esp + swp ? total - esp - swp : 0;

	int bx = b.x, bwid = b.w;
	int wesp = (int)((double)esp / total * bwid);
	int wswp = (int)((double)swp / total * bwid);
	if (esp && wesp < 2)
		wesp = 2;
	if (swp && wswp < 2)
		wswp = 2;
	int wroot = bwid - wesp - wswp;

	for (int i = 0; i < wesp; i++)
		ktui_draw_text(bx + i, y, 1, ktui_glyph[KT_G_FULL], KT_WARN, KT_BG, 0);
	for (int i = 0; i < wswp; i++)
		ktui_draw_text(bx + wesp + i, y, 1, ktui_glyph[KT_G_FULL], KT_MID, KT_BG, 0);
	for (int i = 0; i < wroot; i++)
		ktui_draw_text(bx + wesp + wswp + i, y, 1, ktui_glyph[KT_G_FULL], KT_ACCENT,
			  KT_BG, 0);
	y += 2;

	char nm[96];
	int n = 1;
	pname(d->path, n++, nm, sizeof(nm));
	ktui_draw_textf(b.x, y++, b.w, KT_WARN, KT_BG, 0, "  %-14s %-9s %s", nm,
		   kb_human_size(esp), "EFI System, FAT32, mounted at /boot/efi");
	if (swp) {
		pname(d->path, n++, nm, sizeof(nm));
		ktui_draw_textf(b.x, y++, b.w, KT_MID, KT_BG, 0, "  %-14s %-9s %s", nm,
			   kb_human_size(swp), "Linux swap");
	}
	pname(d->path, n++, nm, sizeof(nm));
	ktui_draw_textf(b.x, y++, b.w, KT_ACCENT, KT_BG, 0, "  %-14s %-9s %s", nm,
		   kb_human_size(root), "ext4, mounted at /");
	y++;

	ktui_para(b.x, y, b.w,
	     "Everything currently on this disk is destroyed. Nothing happens "
	     "until you confirm on the summary page.", KT_ERR);
}

static int layout_validate(char *err, size_t n)
{
	if (cfg.plan == PLAN_MANUAL) {
		snprintf(err, n,
			 "run cfdisk, or switch to one of the other two modes");
		return 1;
	}
	if (cfg.plan == PLAN_REUSE) {
		if (!cfg.part_root[0]) {
			snprintf(err, n, "pick the partition to use as root");
			return 1;
		}
		if (!cfg.part_esp[0]) {
			snprintf(err, n, "pick the EFI system partition");
			return 1;
		}
		if (!strcmp(cfg.part_esp, cfg.part_root)) {
			snprintf(err, n, "the ESP and root cannot be the same "
					 "partition");
			return 1;
		}
		Disk *d = disk_by_path(cfg.disk);
		if (d)
			for (int i = 0; i < d->nparts; i++)
				if (!strcmp(d->part[i].path, cfg.part_root) &&
				    d->part[i].sectors * 512ULL <
					    (unsigned long long)ki_sys.payload_kb *
						    1024) {
					snprintf(err, n,
						 "%s is smaller than the %s this "
						 "install needs",
						 cfg.part_root,
						 kb_human_size((unsigned long long)
									ki_sys.payload_kb *
								1024));
					return 1;
				}
	}
	if (cfg.swap != SWAP_NONE && cfg.swap_mb <= 0) {
		snprintf(err, n, "swap size must be greater than zero");
		return 1;
	}
	return 0;
}

static Page page_layout = {
	"layout", "Layout", "partitions, filesystem, swap",
	layout_enter, layout_draw, layout_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 6 · ACCOUNTS
 * ════════════════════════════════════════════════════════════════════════ */

static int name_ok(const char *s)
{
	if (!*s || isdigit((unsigned char)*s))
		return 0;
	for (const char *c = s; *c; c++)
		if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) &&
		    *c != '_' && *c != '-')
			return 0;
	return 1;
}

static int host_ok(const char *s)
{
	if (!*s)
		return 0;
	for (const char *c = s; *c; c++)
		if (!isalnum((unsigned char)*c) && *c != '-')
			return 0;
	return 1;
}

static void accounts_draw(KRect b)
{
	int y = b.y;
	int fw = b.w > 44 ? 34 : b.w - 18;

	ktui_section(b.x, y, b.w, "MACHINE");
	y++;
	ktui_draw_text(b.x, y, 16, "hostname", KT_MID, KT_BG, 0);
	ktui_input(krect(b.x + 17, y, fw, 1), cfg.hostname, sizeof(cfg.hostname), 0,
		 "kdos");
	if (!host_ok(cfg.hostname))
		ktui_draw_text(b.x + 18 + fw, y, b.w, "letters, digits and -", KT_ERR,
			  KT_BG, 0);
	y += 2;

	ktui_section(b.x, y, b.w, "YOU");
	y++;
	ktui_draw_text(b.x, y, 16, "full name", KT_MID, KT_BG, 0);
	ktui_input(krect(b.x + 17, y, fw, 1), cfg.fullname, sizeof(cfg.fullname), 0,
		 "KDOS User");
	y++;
	ktui_draw_text(b.x, y, 16, "username", KT_MID, KT_BG, 0);
	ktui_input(krect(b.x + 17, y, fw, 1), cfg.username, sizeof(cfg.username), 0,
		 "kdos");
	if (!name_ok(cfg.username))
		ktui_draw_text(b.x + 18 + fw, y, b.w, "lowercase, no leading digit",
			  KT_ERR, KT_BG, 0);
	y++;
	ktui_draw_text(b.x, y, 16, "password", KT_MID, KT_BG, 0);
	ktui_input(krect(b.x + 17, y, fw, 1), cfg.userpass, sizeof(cfg.userpass), 1,
		 "required");
	y++;
	ktui_draw_text(b.x, y, 16, "confirm", KT_MID, KT_BG, 0);
	ktui_input(krect(b.x + 17, y, fw, 1), cfg.userpass2, sizeof(cfg.userpass2), 1,
		 "");
	if (cfg.userpass2[0] && strcmp(cfg.userpass, cfg.userpass2))
		ktui_draw_text(b.x + 18 + fw, y, b.w, "does not match", KT_ERR, KT_BG,
			  0);
	y++;
	ktui_pw_meter(b.x + 17, y, b.w - 17, cfg.userpass);
	y += 2;

	ktui_check(b.x, y++, b.w, "Administrator (member of wheel, may use sudo)",
		 &cfg.user_wheel);
	y++;

	ktui_section(b.x, y, b.w, "ROOT");
	y++;
	ktui_check(b.x, y++, b.w, "Lock the root account (log in as yourself, use sudo)",
		 &cfg.root_locked);

	if (!cfg.root_locked) {
		y++;
		ktui_draw_text(b.x, y, 16, "root password", KT_MID, KT_BG, 0);
		ktui_input(krect(b.x + 17, y, fw, 1), cfg.rootpass,
			 sizeof(cfg.rootpass), 1, "required");
		y++;
		ktui_draw_text(b.x, y, 16, "confirm", KT_MID, KT_BG, 0);
		ktui_input(krect(b.x + 17, y, fw, 1), cfg.rootpass2,
			 sizeof(cfg.rootpass2), 1, "");
		if (cfg.rootpass2[0] && strcmp(cfg.rootpass, cfg.rootpass2))
			ktui_draw_text(b.x + 18 + fw, y, b.w, "does not match", KT_ERR,
				  KT_BG, 0);
		y += 2;
	} else {
		y++;
		ktui_note(b.x + 4, y++, b.w,
		     "the live image's root password is not carried over");
		y++;
	}

	ktui_para(b.x, y, b.w,
		     "The live system logs in as kdos/kdos automatically. Both "
		     "the name and the password are replaced here, and tty1's "
		     "autologin follows the rename.", KT_DIM);
}

static int accounts_validate(char *err, size_t n)
{
	if (!host_ok(cfg.hostname)) {
		snprintf(err, n, "hostname must be letters, digits and dashes");
		return 1;
	}
	if (!name_ok(cfg.username)) {
		snprintf(err, n,
			 "username must be lowercase letters, digits, - or _");
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
	if (!cfg.root_locked) {
		if (!cfg.rootpass[0]) {
			snprintf(err, n, "set a root password, or lock the account");
			return 1;
		}
		if (strcmp(cfg.rootpass, cfg.rootpass2)) {
			snprintf(err, n, "the two root passwords do not match");
			return 1;
		}
	}
	if (!cfg.user_wheel && cfg.root_locked) {
		snprintf(err, n,
			 "root is locked and %s is not an administrator — nobody "
			 "could ever gain privileges",
			 cfg.username);
		return 1;
	}
	return 0;
}

static Page page_accounts = {
	"accounts", "Accounts", "hostname, user, passwords",
	NULL, accounts_draw, accounts_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 7 · SYSTEM
 * ════════════════════════════════════════════════════════════════════════ */

static void system_draw(KRect b)
{
	int y = b.y;

	ktui_section(b.x, y, b.w, "ACCENT");
	y++;
	ktui_note(b.x, y++, b.w,
	     "applied to COSMIC, GTK, the icons, the cursors, foot, btop and "
	     "this installer");
	y++;

	int col = b.w / 4;
	if (col < 14)
		col = 14;
	for (int i = 0; i < ktui_ntheme; i++) {
		int themed = !strcmp(cfg.theme, ktui_themes[i].name);
		int v = themed;
		int x = b.x + (i % 4) * col;
		int yy = y + (i / 4) * 2;
		if (ktui_radio(x, yy, col - 2, ktui_themes[i].label, &v, 1)) {
			kb_strlcpy(cfg.theme, ktui_themes[i].name, sizeof(cfg.theme));
			/* Repaint the installer in the accent being chosen —
			 * on a VT that means reloading the console palette,
			 * which is why the theme lives behind an ioctl and not
			 * behind setvtrgb. */
			ktui_theme_set(cfg.theme);
			ktui_term_repalette();
			ktui_draw_invalidate();
		}
		/* A swatch, so the name is not the only cue. */
		for (int k = 0; k < 6; k++)
			ktui_draw_text(x + 4 + k, yy + 1, 1, ktui_glyph[KT_G_FULL],
				  k < 2 ? KT_ACCENT : k < 4 ? KT_MID : KT_WARN,
				  KT_BG, 0);
	}
	y += ((ktui_ntheme + 3) / 4) * 2 + 1;

	ktui_section(b.x, y, b.w, "ALIEN APPS");
	y++;
	char lbl[128];
	snprintf(lbl, sizeof(lbl), "Install the alien app library (%s)",
		 kb_human_size((unsigned long long)ki_sys.appbox_kb * 1024));
	ktui_check(b.x, y++, b.w, lbl, &cfg.with_appbox);
	y += ktui_para(b.x + 4, y, b.w - 4,
		  "The pre-baked Debian container behind every GUI app — "
		  "browser, office, GIMP, the lot. Leaving it out installs a "
		  "much smaller system that can still fetch it later.", KT_DIM);
	y++;

	ktui_section(b.x, y, b.w, "SERVICES AT BOOT");
	y++;
	for (int i = 0; i < ki_nservices; i++) {
		int on = !(cfg.svc_off & (1u << i));
		char l[128];
		snprintf(l, sizeof(l), "%-18s %s", ki_services[i].label,
			 ki_services[i].note);
		if (ktui_check(b.x, y, b.w, l, &on)) {
			if (on)
				cfg.svc_off &= ~(1u << i);
			else
				cfg.svc_off |= 1u << i;
		}
		y++;
	}
	y++;
	ktui_note(b.x, y, b.w,
		     "anything here can be flipped later by touching or removing "
		     "/etc/service.disabled/<name>");
}

static Page page_system = {
	"system", "System", "accent, alien apps, services",
	NULL, system_draw, NULL, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 8 · SUMMARY
 * ════════════════════════════════════════════════════════════════════════ */

static void summary_draw(KRect b)
{
	int y = b.y;
	Disk *d = disk_by_path(cfg.disk);
	char v[192];

	ktui_section(b.x, y, b.w, "REVIEW");
	y++;

	ktui_kv(b.x, y++, b.w, "keymap", cfg.keymap, KT_TEXT);
	snprintf(v, sizeof(v), "%s  (%s)", cfg.tz_label, cfg.tz);
	ktui_kv(b.x, y++, b.w, "time zone", v, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "hostname", cfg.hostname, KT_TEXT);
	snprintf(v, sizeof(v), "%s (%s)%s", cfg.username, cfg.fullname,
		 cfg.user_wheel ? ", administrator" : "");
	ktui_kv(b.x, y++, b.w, "user", v, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "root account",
	      cfg.root_locked ? "locked" : "password set",
	      cfg.root_locked ? KT_TEXT : KT_WARN);
	ktui_kv(b.x, y++, b.w, "accent", cfg.theme, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "alien apps",
	      cfg.with_appbox ? "installed" : "left out",
	      cfg.with_appbox ? KT_TEXT : KT_WARN);

	char svc[160] = "";
	for (int i = 0; i < ki_nservices; i++) {
		if (cfg.svc_off & (1u << i))
			continue;
		if (svc[0])
			strncat(svc, ", ", sizeof(svc) - strlen(svc) - 1);
		strncat(svc, ki_services[i].label, sizeof(svc) - strlen(svc) - 1);
	}
	ktui_kv(b.x, y++, b.w, "services", svc[0] ? svc : "none", KT_TEXT);
	y++;

	ktui_section(b.x, y, b.w, "DISK");
	y++;
	if (d) {
		snprintf(v, sizeof(v), "%s  %s  %s", d->path,
			 kb_human_size(d->sectors * (unsigned long long)d->sector_size),
			 d->model);
		ktui_kv(b.x, y++, b.w, "target", v, KT_TEXT);
	}
	ktui_kv(b.x, y++, b.w, "mode",
	      cfg.plan == PLAN_WIPE ? "erase the disk and repartition"
				    : "use existing partitions",
	      cfg.plan == PLAN_WIPE ? KT_ERR : KT_TEXT);
	if (cfg.plan == PLAN_REUSE) {
		ktui_kv(b.x, y++, b.w, "ESP", cfg.part_esp, KT_TEXT);
		ktui_kv(b.x, y++, b.w, "root (reformatted)", cfg.part_root, KT_ERR);
		ktui_kv(b.x, y++, b.w, "format ESP", cfg.format_esp ? "yes" : "no",
		      KT_TEXT);
	}
	snprintf(v, sizeof(v), "%s",
		 cfg.swap == SWAP_NONE ? "none"
				       : cfg.swap == SWAP_FILE ? "swap file"
							       : "swap partition");
	if (cfg.swap != SWAP_NONE)
		snprintf(v + strlen(v), sizeof(v) - strlen(v), ", %ld MiB",
			 cfg.swap_mb);
	ktui_kv(b.x, y++, b.w, "swap", v, KT_TEXT);
	y++;

	ktui_draw_hline(b.x, y, b.w, KT_G_DHL, KT_ERR, KT_BG);
	const char *warn = " EVERYTHING ABOVE THIS LINE IS STILL REVERSIBLE ";
	ktui_draw_text(b.x + (b.w - (int)strlen(warn)) / 2, y, b.w, warn, KT_BG,
		  KT_ERR, KT_A_REVERSE);
	y += 2;

	if (cfg.plan == PLAN_WIPE && d)
		y += ktui_para(b.x, y, b.w - 2,
			  "Starting the install destroys every partition on the "
			  "target and everything on them. There is no undo and "
			  "no confirmation after this page.", KT_ERR);
	else
		y += ktui_para(b.x, y, b.w - 2,
			  "Starting the install reformats the root partition. "
			  "Other partitions on the disk are left alone.", KT_WARN);
	y++;

	if (cfg.dry_run) {
		ktui_draw_text(b.x, y++, b.w,
			  "DRY RUN: commands are logged, nothing is executed.",
			  KT_WARN, KT_BG, 0);
		y++;
	}

	if (ktui_button(krect(b.x, y, 30, 1),
		      cfg.dry_run ? "REHEARSE INSTALL" : "BEGIN INSTALL", 1, 1)) {
		install_plan();
		install_start(0);
		page_goto(8);
	}
	if (ktui_button(krect(b.x + 33, y, 24, 1), "Save answer file", 1, 0)) {
		if (conf_save("/tmp/kinstall.conf") == 0)
			ktui_modal_alert("Saved",
				    "Answer file written to /tmp/kinstall.conf\n"
				    "Copy it off the machine and reuse it with:\n"
				    "  kinstall --config kinstall.conf");
		else
			ktui_modal_alert("Failed", "Could not write /tmp/kinstall.conf");
	}
}

/* Next must not walk off this page. The install starts from the button and
 * only from the button — a page-forward keystroke is not consent to wipe a
 * disk, and Alt+Right is one keystroke away from Alt+Left. */
static int summary_validate(char *err, size_t n)
{
	snprintf(err, n, "press BEGIN INSTALL to start — Next does not");
	return 1;
}

static Page page_summary = {
	"summary", "Summary", "the point of no return",
	NULL, summary_draw, summary_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 9 · INSTALL
 * ════════════════════════════════════════════════════════════════════════ */

static int log_full;
static int log_scroll;

static void install_draw(KRect b)
{
	int y = b.y;

	if (log_full) {
		ktui_section(b.x, y, b.w, "LOG");
		y++;
		int h = b.y + b.h - y - 1;
		int total = inst.nlog < LOG_LINES ? inst.nlog : LOG_LINES;
		int base = inst.nlog > LOG_LINES ? inst.nlog - LOG_LINES : 0;
		int top = total - h - log_scroll;
		if (top < 0)
			top = 0;
		for (int i = 0; i < h && top + i < total; i++)
			ktui_draw_text(b.x, y + i, b.w,
				  inst.log[(base + top + i) % LOG_LINES], KT_MID,
				  KT_BG, 0);
		ktui_draw_text(b.x, b.y + b.h - 1, b.w,
			  "L back to progress   PgUp/PgDn scroll", KT_DIM, KT_BG,
			  0);
		return;
	}

	ktui_section(b.x, y, b.w, "PROGRESS");
	y++;

	int donecount = 0, active = 0;
	for (int i = 0; i < inst.nsteps; i++) {
		if (inst.step[i].state == ST_DONE || inst.step[i].state == ST_SKIP)
			donecount++;
		if (inst.step[i].state == ST_RUNNING)
			active = 1;
	}
	double overall = (double)donecount / inst.nsteps;
	if (active && inst.cur >= 0 && inst.step[inst.cur].frac > 0)
		overall += inst.step[inst.cur].frac / inst.nsteps;

	char pct[32];
	snprintf(pct, sizeof(pct), " %d%% ", (int)(overall * 100));
	ktui_progress(krect(b.x, y, b.w, 1), overall, pct);
	y += 2;

	for (int i = 0; i < inst.nsteps; i++) {
		StepUi *s = &inst.step[i];
		const char *mark = ktui_glyph[KT_G_DOT];
		int fg = KT_DIM;
		switch (s->state) {
		case ST_RUNNING:
			mark = ktui_glyph[KT_G_RIGHT];
			fg = KT_ACCENT;
			break;
		case ST_DONE:
			mark = ktui_glyph[KT_G_SQUARE];
			fg = KT_MID;
			break;
		case ST_FAIL:
			mark = "!";
			fg = KT_ERR;
			break;
		case ST_SKIP:
			mark = "-";
			fg = KT_DIM;
			break;
		default:
			break;
		}
		ktui_draw_text(b.x + 1, y, 2, mark, fg, KT_BG, 0);
		ktui_draw_text(b.x + 4, y, 16, s->title,
			  s->state == ST_RUNNING ? KT_TEXT : fg, KT_BG, 0);

		if (s->state == ST_RUNNING) {
			ktui_draw_text(b.x + 21, y, b.w - 21,
				  s->note[0] ? s->note : s->detail, KT_MID, KT_BG,
				  0);
			if (s->frac >= 0 && b.w > 60) {
				char p[12];
				snprintf(p, sizeof(p), "%3d%%",
					 (int)(s->frac * 100));
				ktui_draw_text(b.x + b.w - 5, y, 5, p, KT_ACCENT,
					  KT_BG, 0);
			}
		} else if (s->state == ST_DONE) {
			char t[24];
			snprintf(t, sizeof(t), "%.1fs", s->t1 - s->t0);
			ktui_draw_text(b.x + 21, y, b.w - 21, t, KT_DIM, KT_BG, 0);
		} else if (s->state == ST_SKIP) {
			ktui_draw_text(b.x + 21, y, b.w - 21, "not needed", KT_DIM,
				  KT_BG, 0);
		} else if (s->state == ST_FAIL) {
			ktui_draw_text(b.x + 21, y, b.w - 21, inst.failmsg, KT_ERR,
				  KT_BG, 0);
		}
		y++;
	}
	y++;

	int elapsed = (int)(kb_now_s() - inst.t0);
	ktui_draw_textf(b.x, y++, b.w, KT_DIM, KT_BG, 0, "elapsed %02d:%02d",
		   elapsed / 60, elapsed % 60);
	y++;

	int tail = b.y + b.h - y - 3;
	if (tail > 2) {
		ktui_section(b.x, y, b.w, "OUTPUT");
		y++;
		int total = inst.nlog < LOG_LINES ? inst.nlog : LOG_LINES;
		int base = inst.nlog > LOG_LINES ? inst.nlog - LOG_LINES : 0;
		int start = total - tail;
		if (start < 0)
			start = 0;
		for (int i = 0; start + i < total && i < tail; i++)
			ktui_draw_text(b.x, y + i, b.w,
				  inst.log[(base + start + i) % LOG_LINES], KT_MID,
				  KT_BG, 0);
		y += tail;
	}

	if (inst.failed) {
		int by = b.y + b.h - 1;
		if (ktui_button(krect(b.x, by, 16, 1), "Retry step", 1, 1))
			install_start(inst.cur < 0 ? 0 : inst.cur);
		if (ktui_button(krect(b.x + 18, by, 14, 1), "View log", 1, 0))
			log_full = 1;
		if (ktui_button(krect(b.x + 34, by, 14, 1), "Shell", 1, 0)) {
			char *a[] = { "/bin/bash", "-l", NULL };
			ktui_run_console(a);
		}
		if (ktui_button(krect(b.x + 50, by, 14, 1), "Abort", 1, 0))
			ki_quit = 1;
	} else if (inst.done) {
		if (ktui_button(krect(b.x, b.y + b.h - 1, 20, 1), "Continue", 1, 1))
			page_goto(9);
	}
}

static int install_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_KEY && (ev->key == 'l' || ev->key == 'L')) {
		log_full = !log_full;
		log_scroll = 0;
		return 1;
	}
	if (log_full && ev->type == KT_EVT_KEY) {
		if (ev->key == KT_K_PGUP) {
			log_scroll += 10;
			return 1;
		}
		if (ev->key == KT_K_PGDN) {
			log_scroll -= 10;
			if (log_scroll < 0)
				log_scroll = 0;
			return 1;
		}
	}
	return 0;
}

static Page page_install = {
	"install", "Install", "writing to disk",
	NULL, install_draw, NULL, install_event, 1
};

/* ════════════════════════════════════════════════════════════════════════
 * 10 · DONE
 * ════════════════════════════════════════════════════════════════════════ */

static void reboot_now(void)
{
	ktui_term_shutdown();
	execlp("reboot", "reboot", NULL);
	_exit(0);
}

static void done_draw(KRect b)
{
	int y = b.y;

	if (b.h >= 20 && b.w > 40 && (ktui_caps & KT_CAP_UTF8)) {
		for (int i = 0; i < 6; i++)
			ktui_draw_text(b.x + (b.w - 33) / 2, y + i, b.w, KDOS_MARK[i],
				  KT_ACCENT, KT_BG, 0);
		y += 7;
	}

	const char *msg = "INSTALLATION COMPLETE";
	ktui_draw_text(b.x + (b.w - (int)strlen(msg)) / 2, y, b.w, msg, KT_ACCENT,
		  KT_BG, 0);
	y += 2;

	int elapsed = 0;
	for (int i = 0; i < inst.nsteps; i++)
		if (inst.step[i].state == ST_DONE)
			elapsed += (int)(inst.step[i].t1 - inst.step[i].t0);

	char v[128];
	snprintf(v, sizeof(v), "%d minutes %d seconds", elapsed / 60, elapsed % 60);
	ktui_kv(b.x, y++, b.w, "took", v, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "installed to", cfg.disk, KT_TEXT);
	ktui_kv(b.x, y++, b.w, "log", "/var/log/kinstall.log", KT_TEXT);
	y += 2;

	y += ktui_para(b.x, y, b.w,
		  "Remove the boot medium before restarting, or the firmware "
		  "will hand control back to the live image.", KT_MID);
	y++;

	if (ktui_button(krect(b.x, y, 20, 1), "Reboot now", 1, 1))
		ktui_modal_confirm("Reboot", "Restart this machine now?", "Reboot",
			      "Wait", reboot_now);
	if (ktui_button(krect(b.x + 22, y, 20, 1), "Back to the live desktop", 1, 0))
		ki_quit = 1;
	if (ktui_button(krect(b.x + 44, y, 14, 1), "View log", 1, 0)) {
		char *a[] = { "less", "/var/log/kinstall.log", NULL };
		if (!kb_have_prog("less")) {
			a[0] = "more";
			ktui_run_console(a);
		} else {
			ktui_run_console(a);
		}
	}
}

static Page page_done = {
	"done", "Done", "reboot into KDOS",
	NULL, done_draw, NULL, NULL, 1
};

/* ──────────────────────────────────────────────────────────────────────── */

Page *ki_pages[] = {
	&page_welcome, &page_keyboard, &page_time, &page_disk, &page_layout,
	&page_accounts, &page_system, &page_summary, &page_install, &page_done,
};

int ki_npages = (int)(sizeof(ki_pages) / sizeof(ki_pages[0]));

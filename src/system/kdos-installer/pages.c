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
 * point of no return. That ordering is the whole contract: a page that
 * partitioned the disk as its question was answered would make backing out
 * of question six impossible.
 * ---------------------------------
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/wait.h>

#include "kinstall.h"

/* ────────────────────────────────────────────────────────────────────────
 * TEXT THAT FITS THE BOX. On an 80-column console the body is 54 cells and
 * a key's value column 37, so a value, a note or a description is wrapped
 * under its own column rather than cut at the edge: a cut line loses the
 * words that say what happens. A radio or a checkbox is one line, so its
 * label is written to fit and anything longer is a wrapped note under it.
 * ──────────────────────────────────────────────────────────────────────── */

/* Far above any screen: a wrap drawn here touches no cell and moves no
 * extent, and its line count is what the same wrap takes on the page. */
#define MEASURE_Y (-0x4000)

/* A key and its value, the value wrapped under its own column as ktui_kv
 * would place it. Returns the rows taken, one for an empty value, so the
 * key keeps its row. */
static int kv_wrap(KRect b, int y, const char *k, const char *v, int fg)
{
	int n;

	if (y != MEASURE_Y)
		ktui_draw_text(b.x, y, KTUI_LAY_LABEL, k, KT_MID, KT_BG, 0);
	n = ktui_para(b.x + KTUI_LAY_LABEL + 1, y, b.w - KTUI_LAY_LABEL - 1, v,
		      fg);
	return n > 0 ? n : 1;
}

/* A list row's title in a column of `w` cells, its last cell always blank so
 * the title never touches the column after it; a longer title is cut a cell
 * shorter and ends in the tier table's ellipsis. */
static void row_title(int x, int y, int w, const char *s, int fg, int bg)
{
	if (w < 2)
		return;
	ktui_draw_text_fit(x, y, w - 1, s, fg, bg, 0);
}

/* One row of a partition table drawn as text: the name and the size in their
 * columns from `x`, the description wrapped under its own column. */
static int part_line(KRect b, int y, int fg, int indent, int namew,
		     const char *name, const char *size, const char *desc)
{
	int dx = indent + namew + 1 + 9 + 1;

	ktui_draw_textf(b.x, y, b.w, fg, KT_BG, 0, "%*s%-*s %-9s", indent, "",
			namew, name, size);
	return ktui_para(b.x + dx, y, b.w - dx, desc, fg);
}

/* ──────────────────────────────────────────────────────────────────────── */

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

/*
 * THE PREFLIGHT, one line each on the Welcome page and checked again by the
 * Summary, which refuses BEGIN INSTALL while any line fails: every one of
 * them is something the install needs after the disk has been erased, so a
 * run started past a failing line wipes the target and then stops with no
 * system on it. A failing line says what is wrong and what it costs, never
 * the requirement in its passing words.
 *
 * ki_preflight_fail stands in for this machine in the page harness: -1
 * measures it, an index fails that one line and passes the rest, and
 * KI_PREFLIGHT_N or more passes every line.
 */
#define KI_PREFLIGHT_N 5

typedef struct {
	int ok;
	const char *msg;
} Preflight;

int ki_preflight_fail = -1;

static int preflight(Preflight *c)
{
	/* probe_disks lists read-only disks too, so the count of visible
	 * disks is not the count of disks an install can write. */
	int nwritable = 0;
	for (int i = 0; i < ki_ndisk; i++)
		if (!ki_disk[i].readonly)
			nwritable++;

	int ok[KI_PREFLIGHT_N] = {
		geteuid() == 0,
		nwritable > 0,
		kb_have_prog("rsync"),
		kb_have_prog("mkfs.ext4") && kb_have_prog("mkfs.vfat"),
		kb_path_exists("/usr/share/limine"),
	};
	static const char *const pass[KI_PREFLIGHT_N] = {
		"running as root",
		"at least one writable disk",
		"rsync present",
		"mkfs.ext4 and mkfs.vfat present",
		"Limine payload present",
	};
	static const char *const fail[KI_PREFLIGHT_N] = {
		"not running as root — start it with sudo kinstall",
		"no writable disk — nothing to install to",
		"rsync missing — nothing can be copied",
		"mkfs.ext4 or mkfs.vfat missing — the target cannot be formatted",
		"no Limine payload at /usr/share/limine — the disk would not start",
	};

	for (int i = 0; i < KI_PREFLIGHT_N; i++) {
		if (ki_preflight_fail >= 0)
			ok[i] = i != ki_preflight_fail;
		c[i].ok = ok[i];
		c[i].msg = ok[i] ? pass[i] : fail[i];
	}
	return KI_PREFLIGHT_N;
}

/* The first failing preflight line, or NULL when every one passes. */
static const char *preflight_failing(void)
{
	Preflight c[KI_PREFLIGHT_N];
	int nc = preflight(c);

	for (int i = 0; i < nc; i++)
		if (!c[i].ok)
			return c[i].msg;
	return NULL;
}

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
	y += kv_wrap(b, y, "processor", v, KT_TEXT);
	y += kv_wrap(b, y, "memory",
		     kb_human_size((unsigned long long)ki_sys.mem_kb * 1024),
		     KT_TEXT);
	snprintf(v, sizeof(v), "%d disk%s visible", ki_ndisk,
		 ki_ndisk == 1 ? "" : "s");
	y += kv_wrap(b, y, "storage", v, ki_ndisk ? KT_TEXT : KT_ERR);
	/* What the copy writes, the total its progress line counts to. */
	if (ki_copy_bytes(1) > 0)
		snprintf(v, sizeof(v), "%s: %s system, %s box store",
			 kb_human_size((unsigned long long)(ki_copy_bytes(0) +
							    ki_copy_bytes(1))),
			 kb_human_size((unsigned long long)ki_copy_bytes(0)),
			 kb_human_size((unsigned long long)ki_copy_bytes(1)));
	else
		snprintf(v, sizeof(v), "%s",
			 kb_human_size((unsigned long long)ki_copy_bytes(0)));
	y += kv_wrap(b, y, "install size", v, KT_TEXT);

	/*
	 * THE WIDTH IS SHOWN ONLY WHERE IT IS THE UNUSUAL ONE. "UEFI (32-bit)"
	 * is the machine a person needs told about — it is what makes the boot
	 * path different from every other x86-64 install, and it is the case a
	 * generic image has no boot path for — while "UEFI (64-bit)" on every
	 * other machine is a word nobody reads twice.
	 */
	snprintf(v, sizeof(v), "%s%s%s", ki_sys.uefi ? "UEFI" : "legacy BIOS",
		 ki_sys.fw_bits == 32 ? "  (32-bit firmware)" : "",
		 ki_sys.secure_boot ? "  (Secure Boot enabled)" : "");
	/* Both firmwares install, so neither is an error colour. */
	y += kv_wrap(b, y, "firmware", v, KT_TEXT);
	y++;

	ktui_section(b.x, y, b.w, "PREFLIGHT");
	y++;

	/* A failing line is longer than the 54 cells of an 80-column body and
	 * wraps under itself; a cut one would lose what it costs. */
	Preflight c[KI_PREFLIGHT_N];
	int nc = preflight(c);

	for (int i = 0; i < nc; i++) {
		ktui_draw_text(b.x, y, 3, c[i].ok ? ktui_glyph[KT_G_SQUARE] : "!",
			  c[i].ok ? KT_ACCENT : KT_ERR, KT_BG, 0);
		y += ktui_para(b.x + 2, y, b.w - 2, c[i].msg,
			       c[i].ok ? KT_TEXT : KT_ERR);
	}

}

static int welcome_validate(char *err, size_t n)
{
	if (geteuid() != 0) {
		snprintf(err, n, "must run as root — try: sudo kinstall");
		return 1;
	}
	if (!ki_ndisk) {
		snprintf(err, n, "no disk found to install to");
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
	(void)u;
	const char *name = keymaps[kmfilter_idx[idx]];
	int cur = !strcmp(name, cfg.keymap);
	int fg, bg;

	/* THE SAME SLOTS ktui_list FILLED THE ROW WITH. A row painter that
	 * picks its own background paints over the list's, so the two disagree
	 * the moment either changes. The bullet keeps the accent whatever the
	 * row is doing: it is the chosen ITEM, which is not the same question
	 * as where the caret is. */
	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ",
		       KT_ACCENT, bg, 0);
	row_title(x + 3, y, w - 3, name, fg, bg);
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
	int km_hit = ktui_list(krect(b.x, y, listw, lh), &kmlist, nkmfilter,
			       keymap_row, NULL, lid);
	/* An empty list says why: a blank box after a filter reads as a
	 * keymap list that failed to load. */
	if (!nkmfilter)
		ktui_para(b.x + 1, y, listw - 1,
			  "(no keymap matches the filter)", KT_MID);
	if (km_hit) {
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

/*
 * THE ZONE LIST IS READ FROM `zone1970.tab`, which tzdata ships here.
 *
 * A table kept here could not follow upstream rule changes; tzdata's list is
 * maintained with the rules and is on the image already. The file is in
 * country-code order, which nobody scrolls by, so the list is sorted by
 * the zone's name. `TZ` is written as `:/etc/localtime` rather than as a
 * rules string, because musl reads a zone FILE when the value starts with a
 * colon — so the two halves of the setting, the symlink and the environment,
 * cannot disagree about the rules for a zone. Neither is shown: the person
 * chooses a zone, and the mechanism is this comment's business.
 */
#define TZ_MAX_ZONES 800

static struct tz_zone {
	char label[64];
	char cc[40];		/* the country-code column, as it stands */
} zones[TZ_MAX_ZONES];
static int nzones;

static const char *tz_dir(void)
{
	const char *d = getenv("KDOS_ZONEINFO");

	return d && *d ? d : "/usr/share/zoneinfo";
}

static int tz_cmp(const void *a, const void *b)
{
	return strcmp(((const struct tz_zone *)a)->label,
		      ((const struct tz_zone *)b)->label);
}

static void tz_load(void)
{
	char path[512], line[512];
	FILE *f;

	nzones = 0;
	snprintf(path, sizeof(path), "%s/zone1970.tab", tz_dir());
	f = fopen(path, "r");
	if (f) {
		while (nzones < TZ_MAX_ZONES && fgets(line, sizeof(line), f)) {
			if (line[0] == '#')
				continue;
			/* `codes<TAB>coords<TAB>name[<TAB>comment]` */
			if (sscanf(line, "%39[^\t]\t%*[^\t]\t%63[^\t\n]",
				   zones[nzones].cc,
				   zones[nzones].label) != 2)
				continue;
			nzones++;
		}
		fclose(f);
		qsort(zones, (size_t)nzones, sizeof(zones[0]), tz_cmp);
	}
	/* An image with no zoneinfo tree still has to offer something, and UTC
	 * is the one zone that needs no data to be right. */
	if (!nzones) {
		kb_strlcpy(zones[0].label, "UTC", sizeof(zones[0].label));
		kb_strlcpy(zones[0].cc, "--", sizeof(zones[0].cc));
		nzones = 1;
	}
}

static int tzidx[TZ_MAX_ZONES], ntzidx;
static char tzfilter[32];
static KtuiList tzlist;
/*
 * WHERE THE CURSOR STOOD WHEN THE ZONE WAS LAST SETTLED: on entering the
 * page, on a commit and on every change of the filter. Focus arriving in the
 * list by Tab finds the cursor on the chosen zone, or on the top row when
 * that zone is not in the filtered list (UTC never is); committing the row
 * focus lands on would choose a zone nobody picked. A zone is committed when
 * the cursor moves off this row, when a row is activated, or when the filter
 * leaves exactly one.
 */
static int tz_cur;

/* The cursor onto the chosen zone where the filtered list holds it, the top
 * row where it does not, and the settled position with it. */
static void tz_place(void)
{
	tzlist.sel = 0;
	for (int i = 0; i < ntzidx; i++)
		if (!strcmp(zones[tzidx[i]].label, cfg.tz_label))
			tzlist.sel = i;
	tz_cur = tzlist.sel;
}

static void tz_commit(int row)
{
	kb_strlcpy(cfg.tz_label, zones[tzidx[row]].label, sizeof(cfg.tz_label));
	kb_strlcpy(cfg.tz, ":/etc/localtime", sizeof(cfg.tz));
	tzlist.sel = tz_cur = row;
}

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
	(void)u;
	int z = tzidx[idx];
	int cur = !strcmp(zones[z].label, cfg.tz_label);
	int fg, bg;

	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ",
		       KT_ACCENT, bg, 0);
	row_title(x + 3, y, w - 3, zones[z].label, fg, bg);
}

static void time_enter(void)
{
	if (!nzones)
		tz_load();
	tz_filter();
	tz_place();
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
		  "The zone the installed system's clock shows. It can be "
		  "changed later in System Settings > Date & Time.", KT_MID);
	y++;

	ktui_draw_text(b.x, y, 8, "filter", KT_MID, KT_BG, 0);
	if (ktui_input(krect(b.x + 8, y, listw - 8, 1), tzfilter, sizeof(tzfilter), 0,
		     "type to narrow")) {
		tz_filter();
		tz_place();
		if (ntzidx == 1)
			tz_commit(0);
	}
	y += 2;

	int lh = b.y + b.h - y - 1;
	if (lh < 3)
		lh = 3;
	int lid = ktui_id();
	int hit = ktui_list(krect(b.x, y, listw, lh), &tzlist, ntzidx, tz_row,
			    NULL, lid);
	if (!ntzidx)
		ktui_para(b.x + 1, y, listw - 1,
			  "(no zone matches the filter)", KT_MID);
	if (tzlist.sel >= 0 && tzlist.sel < ntzidx &&
	    (hit || (ktui_focused(lid) && tzlist.sel != tz_cur)))
		tz_commit(tzlist.sel);

	if (b.w > listw + 24) {
		int rx = b.x + listw + 3;
		int rw = b.x + b.w - rx;
		int ry = y;
		/* The column is about 24 cells at 80, too narrow for a key and
		 * its value side by side: each label has a row of its own and
		 * the value wraps under it, as the zone's name does. */
		ktui_section(rx, ry, rw, "SELECTED");
		ry += 2;
		ry += ktui_para(rx, ry, rw, cfg.tz_label, KT_ACCENT);
		ry++;

		/* The PREVIEW reads the zone file directly: `cfg.tz` names
		 * `/etc/localtime` on the target, which does not exist yet and
		 * would preview this machine's own zone for every row. */
		char pv[320];

		snprintf(pv, sizeof(pv), ":%s/%s", tz_dir(), cfg.tz_label);
		setenv("TZ", pv, 1);
		tzset();
		time_t t = time(NULL);
		struct tm tm;
		localtime_r(&t, &tm);
		char when[64];
		strftime(when, sizeof(when), "%a %d %b  %H:%M:%S", &tm);
		ktui_draw_text(rx, ry++, rw, "local time", KT_MID, KT_BG, 0);
		ry += ktui_para(rx, ry, rw, when, KT_WARN);
		ry++;
		ktui_para(rx, ry, rw,
		     "The hardware clock is left exactly as it is; only the "
		     "environment is set.", KT_MID);
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
	char why[256];
	int warn = ki_disk_check(d, why, sizeof(why));
	int fg, bg;

	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	/* A DISK THE INSTALLER WILL REFUSE STAYS URGENT EVEN ON THE FILL.
	 * KT_WARN on KT_DIM is the one pairing here worth keeping: this is the
	 * row that says "not this one", and it is read precisely when somebody
	 * has moved the caret onto it. */
	if (warn)
		fg = KT_WARN;

	ktui_draw_text(x + 1, y, 2, cur ? ktui_glyph[KT_G_BULLET] : " ",
		       KT_ACCENT, bg, 0);
	char row[160];

	snprintf(row, sizeof(row), "%-10s %8s  %-6s %s", d->name,
		 kb_human_size(ki_disk_bytes(d)), d->tran,
		 d->is_boot_media ? "[boot medium]" : d->model);
	row_title(x + 3, y, w - 3, row, fg, bg);
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
		ktui_draw_text(r.x + 2, r.y, r.w - 4, " unpartitioned ", KT_MID, KT_BG, 0);
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

	unsigned long long need = ki_disk_need_bytes();
	unsigned long long have = ki_disk_bytes(d);

	if (d->nparts) {
		ktui_draw_textf(b.x, y++, b.w, KT_MID, KT_BG, 0,
			   "  %-12s %-10s %-9s %-9s %s", "PART", "SIZE", "TYPE",
			   "LABEL", "MOUNT");
		/* The MOUNT column is what the row leaves, 8 cells at 80: a
		 * mount point longer than that goes on a row of its own under
		 * the partition, wrapped there, rather than losing its end. */
		for (int i = 0; i < d->nparts; i++) {
			Part *p = &d->part[i];
			int fg = p->is_esp ? KT_WARN : KT_TEXT;
			char row[160];
			int cx;

			snprintf(row, sizeof(row), "  %-12s %-10s %-9s %-9s ",
				 p->name, kb_human_size(p->sectors * 512ULL),
				 p->is_esp ? "ESP"
				 : p->is_bios_boot ? "BIOS boot"
				 : p->fstype[0] ? p->fstype : "-",
				 p->label[0] ? p->label : "-");
			cx = ktui_draw_text_fit(b.x, y, b.w, row, fg, KT_BG, 0);

			if (!p->mounted) {
				y++;
			} else if (ktui_utf8_width(p->mountpoint) <= b.w - cx) {
				ktui_draw_text(b.x + cx, y++, b.w - cx, p->mountpoint,
					       fg, KT_BG, 0);
			} else {
				char m[96];

				snprintf(m, sizeof(m), "mounted at %s",
					 p->mountpoint);
				y++;
				y += ktui_para(b.x + 4, y, b.w - 4, m, KT_MID);
			}
		}
	} else {
		ktui_note(b.x + 2, y++, b.w - 2, "no partition table");
	}
	/* A written stick's partitions are windows into the image on the
	 * whole device: the line under them names what the disk holds, in
	 * the words the window's Disk page and the Summary use. */
	if (d->iso_image) {
		char what[256];

		ki_disk_contents(d, 0, what, sizeof(what));
		y += ktui_para(b.x + 2, y, b.w - 2, what, KT_MID);
	}
	y++;

	{
		y += kv_wrap(b, y, "capacity", kb_human_size(have), KT_TEXT);
		char v[64];
		snprintf(v, sizeof(v), "%s", kb_human_size(need));
		y += kv_wrap(b, y, "install needs", v,
		      have >= need ? KT_TEXT : KT_ERR);
	}

	if (d->is_boot_media)
		ktui_para(b.x, y, b.w,
		     "This is the medium KDOS is running from. Installing onto it "
		     "will pull the filesystem out from under the installer.",
		     KT_ERR);
	else if (d->removable)
		ktui_note(b.x, y, b.w, "removable device");
}

/* kinstall-core's rule, so this page, the window and an unattended run
 * refuse the same disks. */
static int disk_validate(char *err, size_t n)
{
	Disk *d = cfg.disk[0] ? disk_by_path(cfg.disk) : NULL;
	if (!d) {
		snprintf(err, n, "select a disk to install onto");
		return 1;
	}
	return ki_disk_check(d, err, n);
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

/* A partition in one of the reuse plan's two lists, 25 cells each at 80
 * columns: the name and the size always show, and the type and the label
 * end in an ellipsis where the row runs out. */
static void part_row_generic(int idx, int x, int y, int w, int selected,
			     int focus, void *u)
{
	Disk *d = (Disk *)u;
	Part *p = &d->part[idx];
	int fg, bg;

	char row[160];

	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	snprintf(row, sizeof(row), "%-9s %6s  %-8s %s", p->name,
		 kb_human_size(p->sectors * 512ULL),
		 p->is_esp ? "ESP"
		 : p->is_bios_boot ? "BIOS boot"
		 : p->fstype[0] ? p->fstype : "-",
		 p->label);
	row_title(x + 1, y, w - 1, row, fg, bg);
}

/*
 * THE ROOT LIST IS THE DISK'S PARTITIONS AND THEN EVERY LOGICAL VOLUME, on
 * any disk: a volume group is not a disk's, and the root need not share one
 * with the ESP — the kernel and initramfs are on the ESP, and the initramfs
 * activates every group before it looks for the root. A volume is named
 * `vg/lv`, which is what `lvs` calls it.
 */
static void root_row(int idx, int x, int y, int w, int selected, int focus,
		     void *u)
{
	Disk *d = (Disk *)u;
	Lv *l;
	int fg, bg;
	char nm[160], row[256];

	if (idx < d->nparts) {
		part_row_generic(idx, x, y, w, selected, focus, u);
		return;
	}
	l = &ki_lv[idx - d->nparts];
	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	snprintf(nm, sizeof(nm), "%s/%s", l->vg, l->lv);
	snprintf(row, sizeof(row), "%-9s %6s  %-8s %s", nm,
		 kb_human_size(l->sectors * 512ULL),
		 l->fstype[0] ? l->fstype : "-",
		 l->mounted ? l->mountpoint : l->label);
	row_title(x + 1, y, w - 1, row, fg, bg);
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

/*
 * The root filesystem. Drawn on BOTH plans, because the reuse plan reformats
 * root as well — it keeps the other partitions, not that one.
 *
 * A filesystem whose mkfs is not on this image is still offered and still
 * selectable; it is labelled, and `do_prepare` refuses it before anything is
 * written. Snapping the selection back would be a control that fights the
 * person using it, and hiding the row would leave them wondering where btrfs
 * went on an image that has the kernel support for it.
 */
static int fs_rows(KRect b, int y)
{
	int idx = 0;
	for (int i = 0; i < ki_nfilesystems; i++)
		if (!strcmp(ki_filesystems[i].name, cfg.fstype))
			idx = i;

	for (int i = 0; i < ki_nfilesystems; i++) {
		const Filesystem *f = &ki_filesystems[i];
		const char *note = kb_have_prog(f->mkfs) ? f->note
							 : "no mkfs on this image";
		char label[96];

		/* The note beside the name where the row holds both; past
		 * that the radio carries the name and the note wraps in its
		 * column after it. */
		snprintf(label, sizeof(label), "%-6s %s", f->name, note);
		if (4 + ktui_utf8_width(label) <= b.w) {
			ktui_radio(b.x, y++, b.w, label, &idx, i);
			continue;
		}
		ktui_radio(b.x, y, 11, f->name, &idx, i);
		y += ktui_para(b.x + 11, y, b.w - 11, note,
			       idx == i ? KT_TEXT : KT_MID);
	}
	kb_strlcpy(cfg.fstype, ki_filesystems[idx].name, sizeof(cfg.fstype));

	if (!kb_have_prog(ki_filesystems[idx].mkfs)) {
		char msg[128];
		snprintf(msg, sizeof(msg),
			 "%s is not installed — the install will stop before it "
			 "writes anything", ki_filesystems[idx].mkfs);
		y += ktui_para(b.x + 4, y, b.w - 4, msg, KT_ERR);
	}
	return y;
}

static void layout_draw(KRect b)
{
	Disk *d = disk_by_path(cfg.disk);
	int y = b.y;

	ktui_section(b.x, y, b.w, "PARTITIONING");
	y++;

	ktui_radio(b.x, y++, b.w, "Erase the whole disk and lay it out for KDOS",
		 &cfg.plan, PLAN_WIPE);
	y += ktui_para(b.x + 4, y, b.w - 4,
		       "GPT: 512M EFI system, 1M BIOS boot, then root; "
		       "starts on UEFI and on legacy BIOS", KT_MID);
	ktui_radio(b.x, y++, b.w, "Use partitions that already exist",
		 &cfg.plan, PLAN_REUSE);
	y += ktui_para(b.x + 4, y, b.w - 4,
		       "keeps every other partition — the dual-boot path",
		       KT_MID);
	ktui_radio(b.x, y++, b.w, "Partition it myself first (cfdisk)",
		 &cfg.plan, PLAN_MANUAL);
	y++;

	if (cfg.plan == PLAN_MANUAL) {
		/* cfdisk writes the table itself, outside every command the
		 * dry run holds back, so a rehearsal cannot open it. */
		if (ktui_button(krect(b.x + 4, y, 26, 1), "Open cfdisk",
				!cfg.dry_run, 0)) {
			char *a[] = { "cfdisk", cfg.disk, NULL };
			ktui_run_console(a);
			probe_disks();
			cfg.plan = PLAN_REUSE;
		}
		y += 2;
		if (cfg.dry_run) {
			ktui_para(b.x, y, b.w,
			     "DRY RUN: cfdisk would write a real partition "
			     "table, so it is not opened. Choose \"Use "
			     "partitions that already exist\" to rehearse "
			     "with the table as it is.", KT_WARN);
			return;
		}
		ktui_para(b.x, y, b.w,
		     "cfdisk takes over the screen. Make an EFI System partition "
		     "and a Linux partition, and a 1 MiB BIOS boot partition "
		     "for the disk to start on legacy BIOS too; write the "
		     "table, quit — the installer comes back and switches to "
		     "picking them.", KT_MID);
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
		int nroot = d->nparts + ki_nlv;
		int lh = nroot > 6 ? 6 : nroot;

		ktui_section(b.x, y, half, "EFI SYSTEM PARTITION");
		ktui_section(b.x + half + 3, y, half,
			     ki_nlv ? "ROOT PARTITION OR VOLUME"
				    : "ROOT PARTITION");
		y++;

		int e = ktui_id();
		ktui_list(krect(b.x, y, half, d->nparts > 6 ? 6 : d->nparts),
			  &esplist, d->nparts, part_row_generic, d, e);
		if (ktui_focused(e) && esplist.sel < d->nparts)
			kb_strlcpy(cfg.part_esp, d->part[esplist.sel].path,
				 sizeof(cfg.part_esp));

		int r = ktui_id();
		ktui_list(krect(b.x + half + 3, y, half, lh), &rootlist, nroot,
			  root_row, d, r);
		if (ktui_focused(r) && rootlist.sel < nroot)
			kb_strlcpy(cfg.part_root,
				   rootlist.sel < d->nparts
					   ? d->part[rootlist.sel].path
					   : ki_lv[rootlist.sel - d->nparts].path,
				   sizeof(cfg.part_root));
		y += lh + 1;

		ktui_check(b.x, y++, b.w, "Reformat the ESP as FAT32", &cfg.format_esp);
		y += ktui_para(b.x + 4, y, b.w - 4,
			       "leave this off when another OS boots from the "
			       "same ESP", KT_MID);
		y++;
		y += kv_wrap(b, y, "ESP", cfg.part_esp[0] ? cfg.part_esp : "-",
			     KT_TEXT);
		y += kv_wrap(b, y, "root",
			     cfg.part_root[0] ? cfg.part_root : "-", KT_ACCENT);
		y += ktui_para(b.x, y, b.w,
			       ki_nlv ? "the root is reformatted, always; a "
					"logical volume keeps its group and "
					"its neighbours"
				      : "the root partition is reformatted, "
					"always", KT_MID);
		/* WHICH FIRMWARES THE REUSED DISK WILL START ON, said before
		 * anything is written: this plan makes no partition, so a GPT
		 * disk without a BIOS boot partition stays UEFI-only. */
		{
			int bb = ki_bios_boot_part();
			char bn[96];

			if (bb > 0) {
				char msg[160];

				ki_part_name(d->path, bb, bn, sizeof(bn));
				snprintf(msg, sizeof(msg),
					 "legacy BIOS: Limine's stage goes "
					 "into BIOS boot partition %s", bn);
				y += ktui_para(b.x, y, b.w, msg, KT_MID);
			} else if (bb == 0) {
				y += ktui_para(b.x, y, b.w,
					       "legacy BIOS: Limine's stage "
					       "goes after the MBR", KT_MID);
			} else {
				y += ktui_para(b.x, y, b.w,
					       "no BIOS boot partition: this "
					       "disk will start on UEFI only",
					       ki_sys.uefi ? KT_WARN : KT_ERR);
			}
		}
		y++;
		ktui_section(b.x, y, b.w, "ROOT FILESYSTEM");
		y++;
		fs_rows(b, y);
		return;
	}

	/* PLAN_WIPE */
	ktui_section(b.x, y, b.w, "FILESYSTEM AND SWAP");
	y++;
	y = fs_rows(b, y);
	y++;

	/* On the A/B layout the swap is a volume of the group whichever kind was
	 * asked for, so the partition row is not offered there. */
	ktui_radio(b.x, y++, b.w, "No swap", &cfg.swap, SWAP_NONE);
	if (ki_ab()) {
		if (cfg.swap == SWAP_PART)
			cfg.swap = SWAP_FILE;
		ktui_radio(b.x, y++, b.w,
			   "Swap volume, shared by both slots", &cfg.swap,
			   SWAP_FILE);
	} else {
		ktui_radio(b.x, y++, b.w, "Swap file on the root filesystem",
			   &cfg.swap, SWAP_FILE);
		ktui_radio(b.x, y++, b.w, "Dedicated swap partition",
			   &cfg.swap, SWAP_PART);
	}

	y++;
	/*
	 * THE A/B LAYOUT. The root partition becomes the one physical volume of
	 * group `kdos`, holding both root slots, the swap and the home they
	 * share, whenever the disk holds them; otherwise `root_a` takes all of
	 * it. The note says which, because a machine whose updates have nowhere
	 * to go must not be something a person finds out at the first update.
	 * Under encryption the group goes INSIDE the container: one passphrase
	 * opens all of it.
	 */
	ktui_check(b.x, y++, b.w,
		   "Two system slots for safe updates (LVM)", &cfg.lvm);
	if (cfg.lvm) {
		char ab[160];

		if (ki_ab())
			snprintf(ab, sizeof(ab),
				 "slots A and B, %s each; the swap and /home "
				 "are shared by both",
				 kb_human_size(ki_slot_mb() << 20));
		else
			snprintf(ab, sizeof(ab),
				 "this disk holds one slot only — updates will "
				 "have no second root");
		y += ktui_para(b.x + 4, y, b.w - 4, ab, KT_MID);
	}
	/*
	 * LUKS2 on the root partition. Offered on the WIPE plan only: encrypting
	 * a partition destroys what is on it, and the reuse plan exists for
	 * people who are keeping something.
	 */
	ktui_check(b.x, y++, b.w, "Encrypt the root filesystem (LUKS2)",
		   &cfg.luks);
	/*
	 * THE GROUPS THAT COME AND GO ARE SCOPED. The passphrase pair exists
	 * only while the box is ticked and the size field only while there is
	 * swap; drawn with positional ids, ticking the box would renumber the
	 * size field and the buttons under it, and the size field's caret
	 * would become the passphrase's. Inside ktui_id_push() each keeps one
	 * id however many controls were drawn before it.
	 */
	ktui_id_push("luks");
	if (cfg.luks) {
		int fw = b.w - 20 > 32 ? 32 : b.w - 20;
		ktui_draw_text(b.x, y, 16, "passphrase", KT_MID, KT_BG, 0);
		ktui_input(krect(b.x + 17, y++, fw, 1), cfg.luks_pass,
			   sizeof(cfg.luks_pass), 1, "");
		ktui_draw_text(b.x, y, 16, "again", KT_MID, KT_BG, 0);
		ktui_input(krect(b.x + 17, y++, fw, 1), cfg.luks_pass2,
			   sizeof(cfg.luks_pass2), 1, "");
		if (cfg.luks_pass2[0] && strcmp(cfg.luks_pass, cfg.luks_pass2))
			y += ktui_para(b.x + 17, y, b.w - 17,
				       "the two do not match", KT_MID);
		else
			y += ktui_para(b.x + 17, y, b.w - 17,
				       "asked for at every boot; there is no "
				       "recovery key and no way back in "
				       "without it", KT_MID);
		/* The initramfs loads cfg.keymap (EFI/kdos/keymap) for the
		 * prompt; a map with no key for a character cannot type it. */
		char km[sizeof(cfg.keymap) + 128];
		snprintf(km, sizeof(km),
			 "typed at boot with the console keymap %s, from "
			 "the Keyboard step; a character it has no key for "
			 "cannot be typed there", cfg.keymap);
		y += ktui_para(b.x + 17, y, b.w - 17, km, KT_MID);
		y++;
	}
	ktui_id_pop();

	ktui_id_push("swap");
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
		ktui_note(b.x + 31, y, b.w - 31, hint);
		y += 2;
	} else {
		y++;
	}
	ktui_id_pop();

	if (!d)
		return;

	ktui_section(b.x, y, b.w, "WHAT WILL BE WRITTEN");
	y++;

	unsigned long long total = ki_disk_bytes(d);
	unsigned long long esp = (unsigned long long)KI_ESP_MB << 20;
	unsigned long long swp = ki_wipe_part_swap()
					 ? (unsigned long long)cfg.swap_mb << 20
					 : 0;
	unsigned long long root = ki_wipe_root_bytes();

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
	/* Slot A in the accent, slot B as shade beside it — a copy kept for
	 * the next update — and the shared home after them. */
	int ab = cfg.lvm && ki_ab();
	int wa = wroot, wb = 0;
	if (ab && root) {
		wa = (int)((double)(ki_slot_mb() << 20) / root * wroot);
		if (wa < 1)
			wa = 1;
		wb = wa;
	}
	for (int i = 0; i < wroot; i++)
		ktui_draw_text(bx + wesp + wswp + i, y, 1,
			       ktui_glyph[i < wa ? KT_G_FULL
					  : i < wa + wb ? KT_G_SHADE : KT_G_FULL],
			       i < wa ? KT_ACCENT : KT_MID, KT_BG, 0);
	y += 2;

	char nm[96];
	ki_part_name(d->path, KI_PART_ESP, nm, sizeof(nm));
	y += part_line(b, y, KT_WARN, 2, 14, nm, kb_human_size(esp),
		       "EFI System, FAT32, mounted at /boot/efi");
	ki_part_name(d->path, KI_PART_BIOS_BOOT, nm, sizeof(nm));
	y += part_line(b, y, KT_MID, 2, 14, nm,
		       kb_human_size((unsigned long long)KI_BIOS_BOOT_MB << 20),
		       "BIOS boot, Limine's legacy-BIOS stage");
	if (swp) {
		ki_part_name(d->path, ki_wipe_part_swap(), nm, sizeof(nm));
		y += part_line(b, y, KT_MID, 2, 14, nm, kb_human_size(swp),
			       "Linux swap");
	}
	ki_part_name(d->path, ki_wipe_part_root(), nm, sizeof(nm));
	char rootdesc[64];
	snprintf(rootdesc, sizeof(rootdesc), "%s, mounted at /", cfg.fstype);
	if (cfg.lvm) {
		y += part_line(b, y, KT_MID, 2, 14, nm, kb_human_size(root),
			       cfg.luks ? "LUKS2, holding LVM group " KI_VG
					: "LVM physical volume, group " KI_VG);
		y += part_line(b, y, KT_ACCENT, 4, 12, KI_VG "/" KI_LV_A,
			       kb_human_size(ab ? ki_slot_mb() << 20 : root),
			       rootdesc);
		if (ab) {
			unsigned long long used = 2 * (ki_slot_mb() << 20) +
						  (ki_swap_lv_mb() << 20);

			y += part_line(b, y, KT_MID, 4, 12, KI_VG "/" KI_LV_B,
				       kb_human_size(ki_slot_mb() << 20),
				       "slot B, a copy of slot A");
			if (ki_swap_lv_mb())
				y += part_line(b, y, KT_MID, 4, 12,
					       KI_VG "/" KI_LV_SWAP,
					       kb_human_size(ki_swap_lv_mb() << 20),
					       "swap, for both slots");
			y += part_line(b, y, KT_MID, 4, 12, KI_VG "/" KI_LV_HOME,
				       kb_human_size(root > used ? root - used : 0),
				       "/home, for both slots");
		}
	} else {
		y += part_line(b, y, KT_ACCENT, 2, 14, nm, kb_human_size(root),
			       rootdesc);
	}
	y++;

	ktui_para(b.x, y, b.w,
	     "Everything currently on this disk is destroyed. Nothing happens "
	     "until you confirm on the summary page.", KT_ERR);
}

static int layout_validate(char *err, size_t n)
{
	if (cfg.luks) {
		if (!cfg.luks_pass[0]) {
			snprintf(err, n, "set a passphrase for the encrypted "
					 "root, or turn encryption off");
			return 1;
		}
		if (strcmp(cfg.luks_pass, cfg.luks_pass2)) {
			snprintf(err, n, "the two passphrases do not match");
			return 1;
		}
	}
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
	}
	if (cfg.swap != SWAP_NONE && cfg.swap_mb <= 0) {
		snprintf(err, n, "swap size must be greater than zero");
		return 1;
	}
	/* THE TARGET RULES ARE kinstall-core's, asked by `--dump plan`, the
	 * window, an unattended run and the Prepare step as well: the reuse
	 * plan's ESP and root, the disk starting on this firmware, the tools
	 * the plan needs, and a volume group of the fixed name on another
	 * disk. Refused here, before the point of no return, they cannot fail
	 * an install after it. */
	return ki_target_check(err, n);
}

static Page page_layout = {
	"layout", "Layout", "partitions, filesystem, swap",
	layout_enter, layout_draw, layout_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 6 · ACCOUNTS
 * ════════════════════════════════════════════════════════════════════════ */

/* A note in the page's secondary slot, wrapped to the cursor's width on as
 * many rows as the wrap takes, measured before the rows are claimed. */
static void acct_note(KtuiLay *l, const char *s)
{
	KRect f = ktui_lay_left(l);
	KRect r = ktui_lay_row(l, ktui_para(f.x, MEASURE_Y, f.w, s, KT_MID));

	ktui_para(r.x, r.y, r.w, s, KT_MID);
}

/* A field's complaint, `s`, or NULL while the value is acceptable. Where the
 * longest complaint the field can raise, `worst`, fits beside the field it
 * is drawn there; where it does not, the row under the field is the
 * complaint's whether or not one is shown, so the page does not move while
 * the value is typed. */
static void acct_complaint(KtuiLay *l, KRect f, int fw, const char *s,
			   const char *worst)
{
	int x = f.x + fw + 1;

	if (x + ktui_utf8_width(worst) <= l->r.x + l->r.w) {
		if (s)
			ktui_draw_text(x, f.y, l->r.x + l->r.w - x, s, KT_ERR,
				       KT_BG, 0);
		return;
	}
	KRect r = ktui_lay_field(l, NULL, 0);

	if (s)
		ktui_draw_text(r.x, r.y, r.w, s, KT_ERR, KT_BG, 0);
}

static void accounts_draw(KRect b)
{
	KtuiLay l;
	KRect f;
	int fw = b.w > 44 ? 34 : b.w - 18;

	/* The rows through the layout cursor: one label column for the page,
	 * and a field that can complain keeps room for it whether or not it
	 * does (acct_complaint). */
	ktui_lay_begin(&l, b);
	ktui_lay_section(&l, "MACHINE");
	f = ktui_lay_field(&l, "hostname", 0);
	f.w = fw;
	ktui_input(f, cfg.hostname, sizeof(cfg.hostname), 0, "kdos");
	acct_complaint(&l, f, fw,
		       ki_host_ok(cfg.hostname) ? NULL
						: "letters a–z, digits, inner -",
		       "letters a–z, digits, inner -");
	ktui_lay_gap(&l, 1);

	/*
	 * SET UP NOW, OR LEAVE IT TO THE FIRST PERSON. The OEM choice makes no
	 * account at all: plasma-setup asks for one on the first boot, and the
	 * fields below are not drawn because nothing reads them.
	 */
	ktui_lay_section(&l, "WHO SETS IT UP");
	f = ktui_lay_row(&l, 1);
	ktui_radio(f.x, f.y, f.w, "Set up my account now", &cfg.oem, 0);
	f = ktui_lay_row(&l, 1);
	ktui_radio(f.x, f.y, f.w,
		   "Let the first person to start it set it up (OEM)",
		   &cfg.oem, 1);
	ktui_lay_gap(&l, 1);

	ktui_id_push("account");
	if (!cfg.oem) {
		ktui_lay_section(&l, "YOU");
		f = ktui_lay_field(&l, "full name", 0);
		f.w = fw;
		ktui_input(f, cfg.fullname, sizeof(cfg.fullname), 0,
			   "KDOS User");
		f = ktui_lay_field(&l, "username", 0);
		f.w = fw;
		ktui_input(f, cfg.username, sizeof(cfg.username), 0, "kdos");
		acct_complaint(&l, f, fw,
			       !ki_name_ok(cfg.username)
				       ? "lowercase, no leading digit or -"
			       : ki_name_reserved(cfg.username)
				       ? "a system account's name"
				       : NULL,
			       "lowercase, no leading digit or -");
		f = ktui_lay_field(&l, "password", 0);
		f.w = fw;
		ktui_input(f, cfg.userpass, sizeof(cfg.userpass), 1,
			   "required");
		f = ktui_lay_field(&l, "confirm", 0);
		f.w = fw;
		ktui_input(f, cfg.userpass2, sizeof(cfg.userpass2), 1, "");
		/* One row under the pair: the mismatch while there is one,
		 * the password's strength otherwise. */
		f = ktui_lay_field(&l, NULL, 0);
		if (cfg.userpass2[0] && strcmp(cfg.userpass, cfg.userpass2))
			ktui_draw_text(f.x, f.y, f.w, "does not match", KT_ERR,
				       KT_BG, 0);
		else
			ktui_pw_meter(f.x, f.y, f.w, cfg.userpass);
		ktui_lay_gap(&l, 1);

		f = ktui_lay_row(&l, 1);
		ktui_check(f.x, f.y, f.w,
			   "Administrator (member of wheel, may use sudo)",
			   &cfg.user_wheel);
		ktui_lay_gap(&l, 1);

		/*
		 * AUTOMATIC LOGIN, ONLY ON AN ENCRYPTED ROOT. The disk's
		 * passphrase at boot already asks who is at the machine; on a
		 * plain root the login screen is that question and is never
		 * skipped. The row is drawn either way, so a person who looks
		 * for it is told why it is not there.
		 */
		ktui_lay_section(&l, "LOGIN");
		if (cfg.luks) {
			f = ktui_lay_row(&l, 1);
			ktui_check(f.x, f.y, f.w,
				   "Log in automatically after the disk "
				   "passphrase",
				   &cfg.autologin);
			ktui_id_push("lock");
			if (cfg.autologin) {
				ktui_lay_indent(&l, 0);
				f = ktui_lay_row(&l, 1);
				ktui_check(f.x, f.y, f.w,
					   "Start the session locked",
					   &cfg.autologin_lock);
				ktui_lay_indent(&l, 0);
				acct_note(&l, "unlock it with this password");
				ktui_lay_unindent(&l, 0);
				ktui_lay_unindent(&l, 0);
			}
			ktui_id_pop();
		} else {
			cfg.autologin = 0;
			acct_note(&l, "the login screen asks for this "
				      "password; automatic login is offered "
				      "on an encrypted root only");
		}
		ktui_lay_gap(&l, 1);
	} else {
		cfg.autologin = 0;
		f = ktui_lay_left(&l);
		ktui_para(f.x, f.y, f.w,
			  "No account is made. The first time the machine is "
			  "switched on, the setup wizard asks for the language, "
			  "keyboard, time zone and network, and makes the "
			  "account, which is an administrator.", KT_MID);
		ktui_lay_gap(&l, 4);
	}
	ktui_id_pop();

	ktui_lay_section(&l, "ROOT");
	f = ktui_lay_row(&l, 1);
	ktui_check(f.x, f.y, f.w, "Lock the root account", &cfg.root_locked);
	ktui_lay_indent(&l, 0);
	acct_note(&l, "log in as yourself and use sudo");
	ktui_lay_unindent(&l, 0);

	/* Scoped for the reason the disk page's LUKS pair is: it comes and
	 * goes with the box above it. */
	ktui_id_push("rootpass");
	ktui_lay_gap(&l, 1);
	if (!cfg.root_locked) {
		f = ktui_lay_field(&l, "root password", 0);
		f.w = fw;
		ktui_input(f, cfg.rootpass, sizeof(cfg.rootpass), 1, "required");
		f = ktui_lay_field(&l, "confirm", 0);
		f.w = fw;
		ktui_input(f, cfg.rootpass2, sizeof(cfg.rootpass2), 1, "");
		acct_complaint(&l, f, fw,
			       cfg.rootpass2[0] &&
					       strcmp(cfg.rootpass, cfg.rootpass2)
				       ? "does not match"
				       : NULL,
			       "does not match");
	} else {
		ktui_lay_indent(&l, 0);
		acct_note(&l, "the live image's root password is not "
			      "carried over");
		ktui_lay_unindent(&l, 0);
	}
	ktui_lay_gap(&l, 1);
	ktui_id_pop();
}

/* The page's own fields, by the rules every front end shares; the disk and
 * look answers are another page's to refuse. */
static int accounts_validate(char *err, size_t n)
{
	if (!ki_host_ok(cfg.hostname)) {
		snprintf(err, n, "a host name is letters a–z, digits and dashes, "
				 "and does not start or end with a dash");
		return 1;
	}
	/* Root first: an OEM install makes no account but still unlocks root
	 * when asked, and an empty password there is a console root login. */
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
	if (cfg.oem)
		return 0;
	if (!ki_name_ok(cfg.username)) {
		snprintf(err, n, "a user name is lowercase letters, digits, - "
				 "and _, and starts with a letter or _");
		return 1;
	}
	if (ki_name_reserved(cfg.username)) {
		snprintf(err, n, "%s belongs to the system — choose another "
				 "user name", cfg.username);
		return 1;
	}
	if (!ki_fullname_ok(cfg.fullname)) {
		snprintf(err, n, "the full name cannot hold ':'");
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
		snprintf(err, n,
			 "root is locked and %s is not an administrator — nobody "
			 "could ever gain privileges",
			 cfg.username);
		return 1;
	}
	return 0;
}

static Page page_accounts = {
	"accounts", "Accounts", "hostname, user or OEM, login",
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
	y += ktui_para(b.x, y, b.w,
		       "the desktop's global theme, the login screen, the boot "
		       "menu, the consoles and this installer", KT_MID);
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
		/* Each accent's own accent, mid and warning colours, as
		 * literal cells. Only a terminal with 24-bit or 256-colour SGR
		 * shows a colour outside the eight palette slots: a VT and a
		 * plain ANSI terminal reduce a literal to the current theme's
		 * slot, so every swatch would read the same. There no swatch
		 * is drawn, and choosing a radio repaints the whole installer
		 * in that accent, which is the preview. */
		if (!(ktui_caps & (KT_CAP_TRUECOLOR | KT_CAP_256)))
			continue;
		for (int k = 0; k < 6; k++) {
			int sl = k < 2 ? KT_ACCENT : k < 4 ? KT_MID : KT_WARN;
			ktui_draw_swatch(x + 4 + k, yy + 1, sl,
					 ktui_themes[i].slot[sl]);
		}
	}
	y += ((ktui_ntheme + 3) / 4) * 2 + 1;

	/* The panel layout the first login lays out. Every desk can be changed
	 * later from the welcome tour or with `kdos desk`. The radio carries
	 * the name and the description wraps under it, as the alien-apps
	 * note does. */
	ktui_section(b.x, y, b.w, "DESK");
	y++;
	{
		static const char *const label[] = { "KDOS", "Dock", "Tiling" };
		static const char *const desc[] = {
			"one bottom panel: menu, tasks, meters, tray",
			"a top bar and a floating dock",
			"a top bar, and windows tiled as they open",
		};
		int idx = 0;

		for (int i = 0; i < ki_ndesks; i++)
			if (!strcmp(cfg.desk, ki_desks[i]))
				idx = i;
		for (int i = 0; i < ki_ndesks && i < 3; i++) {
			ktui_radio(b.x, y++, b.w, label[i], &idx, i);
			y += ktui_para(b.x + 4, y, b.w - 4, desc[i], KT_MID);
		}
		kb_strlcpy(cfg.desk, ki_desks[idx], sizeof(cfg.desk));
	}
	y++;

	ktui_section(b.x, y, b.w, "ALIEN APPS");
	y++;
	char lbl[128];
	snprintf(lbl, sizeof(lbl), "Install the alien app library (%s)",
		 kb_human_size((unsigned long long)ki_sys.appbox_kb * 1024));
	ktui_check(b.x, y++, b.w, lbl, &cfg.with_appbox);
	y += ktui_para(b.x + 4, y, b.w - 4,
		  "The live session's Podman store: the container images "
		  "this session has pulled or built, often none — the medium "
		  "ships no prebuilt images. Leaving it out does not stop "
		  "applications being installed later.", KT_MID);
	y++;

	ktui_section(b.x, y, b.w, "SERVICES AT BOOT");
	y++;
	for (int i = 0; i < ki_nservices; i++) {
		int on = !(cfg.svc_off & (1u << i));
		char label[96];

		/* The name `service` takes, beside the one people know: the note
		 * below points at it, and nothing else on screen shows it. */
		snprintf(label, sizeof(label), "%s (%s)", ki_services[i].label,
			 ki_services[i].name);
		if (ktui_check(b.x, y++, b.w, label, &on)) {
			if (on)
				cfg.svc_off &= ~(1u << i);
			else
				cfg.svc_off |= 1u << i;
		}
		y += ktui_para(b.x + 4, y, b.w - 4, ki_services[i].note, KT_MID);
	}
	y++;
	ktui_para(b.x, y, b.w,
		  "Each can be changed later with `sudo service enable <name>` "
		  "or `sudo service disable <name>`, <name> being the one in "
		  "brackets.", KT_MID);
}

static Page page_system = {
	"system", "System", "accent, desk, alien apps, services",
	NULL, system_draw, NULL, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 8 · APPLICATIONS
 *
 * A page listing the application GROUPS this medium knows how to build, with
 * `essential` already ticked. Nothing is baked onto the medium — what ships is
 * a catalogue, and an application is built by podman on the installed machine.
 *
 * NOTHING IS WRITTEN HERE. Every tick sets a flag in `ki_group[]`, exactly as
 * every other page fills `cfg` and only `cfg`; the work happens in the install
 * step, after the summary, which is this program's first design decision.
 *
 * THE UNIT IS A GROUP, not an application. Seven named bundles is a thing to
 * read during an install; a row per application is not. An answer file names
 * groups too: ki_apps_enter() matches group ids only, and a line naming none
 * falls back to `essential`.
 *
 * AND THE PAGE SAYS HOW THEY WILL ARRIVE: recorded, and offered by the first
 * login. Nothing is built during the install (kinstall-core.h says why). A
 * person must not discover at first boot that nothing was installed.
 *
 * THE NATIVE GROUPS COME FIRST, in the same list: sets of ports the
 * release's binhost holds prebuilt (/ports/groups), downloaded into the new
 * system during the install when the network answers, recorded for `kdos
 * update install --pending` when it does not. None is ticked by default
 * but the language group the keyboard or the time zone points at, and the
 * page says which choice ticked it.
 * ════════════════════════════════════════════════════════════════════════ */

static KtuiList applist;

static void native_row(int idx, int x, int y, int w, int selected, int focus)
{
	const KiNative *g = &ki_native[idx];
	int fg, bg;

	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	char n[24];
	int szw = 10, nw = 7;
	int descw = w - 4 - szw - nw;

	snprintf(n, sizeof(n), "%d", g->nports);
	ktui_draw_text(x + 1, y, 2, g->chosen ? ktui_glyph[KT_G_FULL] : " ",
		       selected ? fg : KT_ACCENT, bg, 0);
	row_title(x + 4, y, descw, g->title[0] ? g->title : g->id, fg, bg);
	ktui_draw_text(x + 4 + descw, y, nw, n, ktui_sel_dim(selected, focus), bg, 0);
	ktui_draw_text(x + 4 + descw + nw, y, szw, "native",
		       ktui_sel_dim(selected, focus), bg, 0);
}

static void app_row(int idx, int x, int y, int w, int selected, int focus,
		    void *u)
{
	(void)u;
	if (idx < ki_nnative) {
		native_row(idx, x, y, w, selected, focus);
		return;
	}
	idx -= ki_nnative;
	const KiGroup *g = &ki_group[idx];
	int fg, bg;

	ktui_sel_slots(selected, focus, KT_BG, &fg, &bg);
	char size[16], n[16];
	int szw = 10, nw = 7;
	int descw = w - 4 - szw - nw;

	kb_strlcpy(size, kb_human_size(g->bytes), sizeof(size));
	snprintf(n, sizeof(n), "%d", g->napp);

	/* The tick glyph out of the tier table, never a literal: a mark
	 * written into a format string draws as `?` wherever UTF-8 does not
	 * reach, and this page is read on the console font. */
	ktui_draw_text(x + 1, y, 2, g->chosen ? ktui_glyph[KT_G_FULL] : " ",
		       selected ? fg : KT_ACCENT, bg, 0);
	row_title(x + 4, y, descw, g->desc[0] ? g->desc : g->id, fg, bg);
	ktui_draw_text(x + 4 + descw, y, nw, n, ktui_sel_dim(selected, focus), bg, 0);
	ktui_draw_text(x + 4 + descw + nw, y, szw, size,
		       ktui_sel_dim(selected, focus), bg, 0);
}

static void apps_draw(KRect b)
{
	int y = b.y;
	/* By the number of boxed groups chosen: none, one, more. */
	static const char *const ROUTE[] = {
		"nothing ticked",
		"recorded; the first login offers to build its applications",
		"recorded; the first login offers to build their applications",
	};

	ktui_section(b.x, y, b.w, "APPLICATIONS");
	y++;

	int nboxed = ki_apps_present ? ki_ngroup : 0;
	if (!ki_nnative && !nboxed) {
		y += ktui_para(b.x, y, b.w,
			  "This medium names no application groups, so "
			  "nothing can be chosen here. Applications are "
			  "installed later from Discover.", KT_MID);
		return;
	}

	/* With no binary host named, or no network, nothing is downloaded
	 * during the install, so the paragraph says the groups are only
	 * recorded, and why, and names the command that installs them. The
	 * command stands on a line of its own: the paragraph wraps at a
	 * space, and a wrap inside it splits the command a reader types. */
	if (ki_binhost_named && ki_sys.network) {
		y += ktui_para(b.x, y, b.w,
			  "A native group is downloaded prebuilt into the new "
			  "system during the install, when the network "
			  "answers. A boxed group is built by podman from the "
			  "catalogue after the first login. Space toggles a "
			  "group.", KT_MID);
	} else {
		y += ktui_para(b.x, y, b.w, ki_binhost_named
			  ? "No network was found, so a native group is only "
			    "recorded; this installs it after the first boot "
			    "once there is a network:"
			  : "This system names no binary host, so a native group "
			    "is only recorded; this installs it once a binary "
			    "host answers:", KT_MID);
		ktui_draw_text(b.x + 2, y++, b.w - 2,
			       "sudo kdos update install --pending",
			       KT_MID, KT_BG, 0);
		y += ktui_para(b.x, y, b.w,
			  "A boxed group is built by podman from the catalogue "
			  "after the first login. Space toggles a group.",
			  KT_MID);
	}
	y++;

	/*
	 * WHAT IT COSTS AND HOW IT ARRIVES, on the page where it is chosen.
	 * The route matters more than the number: a person who ticks four
	 * groups has chosen a first login that offers to build them, and
	 * finding that out here rather than then is the whole point of putting
	 * it on the screen.
	 *
	 * Each value wraps under its own column rather than being cut at the
	 * box's edge: on an 80-column console the value column is 37 wide, and
	 * a cut route line loses the words that say what happens. The rows are
	 * counted before the list is sized, so the list gives up what they take.
	 */
	int nchosen = 0;
	for (int i = 0; i < ki_ngroup; i++)
		if (ki_group[i].chosen)
			nchosen++;
	char boxed[128], native[128], lang[256];
	const char *how = ROUTE[nchosen > 1 ? 2 : nchosen];

	snprintf(boxed, sizeof(boxed), "%s in %d group%s (an estimate)",
		 kb_human_size(ki_apps_bytes()),
		 nchosen, nchosen == 1 ? "" : "s");
	int nn = ki_groups_chosen();
	/* With no binary host named, or no network, the install records
	 * the groups and the Packs step's warning says so; this line says
	 * it first. The command that installs them is on its own line
	 * above. */
	if (nn && ki_binhost_named && ki_sys.network)
		snprintf(native, sizeof(native),
			 "%d group%s, downloaded during the install",
			 nn, nn == 1 ? "" : "s");
	else if (nn && ki_binhost_named)
		snprintf(native, sizeof(native),
			 "%d group%s, recorded; installed after the first "
			 "boot once there is a network",
			 nn, nn == 1 ? "" : "s");
	else if (nn)
		snprintf(native, sizeof(native),
			 "%d group%s, recorded for later (no binary host)",
			 nn, nn == 1 ? "" : "s");
	else
		kb_strlcpy(native, "none", sizeof(native));

	/* WHY A LANGUAGE GROUP IS TICKED, since nobody ticked it: the
	 * keyboard or the time zone chosen earlier points at it. */
	lang[0] = 0;
	for (int i = 0; i < ki_nnative; i++) {
		const char *part[] = {
			lang[0] ? "; " : "", ki_native[i].id,
			ki_native[i].chosen ? " ticked for " : " suggested for ",
			ki_native[i].why,
		};

		if (!ki_native[i].why[0])
			continue;
		for (size_t k = 0; k < sizeof(part) / sizeof(part[0]); k++)
			strncat(lang, part[k], sizeof(lang) - strlen(lang) - 1);
	}

	int rows = 1 + kv_wrap(b, MEASURE_Y, "boxed", boxed, KT_TEXT) +
		   kv_wrap(b, MEASURE_Y, "how", how, KT_TEXT) +
		   kv_wrap(b, MEASURE_Y, "native", native, KT_TEXT) +
		   (lang[0] ? kv_wrap(b, MEASURE_Y, "language", lang,
				      KT_TEXT) : 0);
	int lh = b.y + b.h - y - rows;
	if (lh < 3)
		lh = 3;
	/*
	 * THE WIDGET'S OWN RETURN VALUE, not a page-level key handler. Enter,
	 * Space and a CLICK all come back through it, so the row answers the
	 * pointer — which a `case ' '` in the page's event hook would not.
	 */
	if (ktui_list(krect(b.x, y, b.w, lh), &applist, ki_nnative + nboxed,
		      app_row, NULL, ktui_id())) {
		int i = applist.sel;

		if (i >= 0 && i < ki_nnative) {
			ki_native[i].chosen = !ki_native[i].chosen;
			ki_groups_collect();
		} else if (i >= ki_nnative && i < ki_nnative + nboxed) {
			i -= ki_nnative;
			ki_group[i].chosen = !ki_group[i].chosen;
			ki_apps_collect();
		}
	}
	y += lh + 1;

	y += kv_wrap(b, y, "boxed", boxed, KT_TEXT);
	y += kv_wrap(b, y, "how", how,
		     ki_apps_route() == APPS_PENDING ? KT_WARN : KT_TEXT);
	y += kv_wrap(b, y, "native", native, KT_TEXT);
	if (lang[0])
		kv_wrap(b, y, "language", lang, KT_TEXT);
}

static void apps_enter(void)
{
	ki_probe_network();
	ki_apps_enter();
	ki_groups_enter();
}

static Page page_apps = {
	"apps", "Applications", "native groups and boxed applications",
	apps_enter, apps_draw, NULL, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 9 · SUMMARY
 * ════════════════════════════════════════════════════════════════════════ */

/* Whether the Summary drew BEGIN INSTALL refused, -1 before its first
 * frame of a visit (FOCUS STAYS ON ITS CONTROL). */
static int summary_refused = -1;

static void summary_go(void)
{
	if (!cfg.dry_run && preflight_failing())
		return;
	install_plan();
	install_start(0);
	page_goto(page_index("install"));
}

/* ktui_modal_draw breaks lines only at '\n', and its text column is 50
 * cells wide at every size the installer draws at (a 56-cell dialog with
 * three cells of margin each side; below MIN_W the installer draws no
 * page). Breaking at the last space that fits keeps a long disk model or
 * partition list from being clipped at the dialog's edge. */
#define CONFIRM_COLS 50

static void confirm_wrap(const char *src, char *dst, size_t n)
{
	kb_strlcpy(dst, src, n);
	size_t len = strlen(dst);

	size_t start = 0, brk = 0;
	int cols = 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)dst[i];

		if (c == '\n') {
			start = i + 1;
			brk = 0;
			cols = 0;
			continue;
		}
		if (c == ' ')
			brk = i;
		if ((c & 0xc0) != 0x80)
			cols++;
		if (cols > CONFIRM_COLS && brk > start) {
			dst[brk] = '\n';
			start = brk + 1;
			brk = 0;
			cols = 0;
			for (size_t j = start; j <= i; j++)
				if (((unsigned char)dst[j] & 0xc0) != 0x80)
					cols++;
		}
	}
}

/* BEGIN INSTALL asks once more, naming the disk by model, size and node and
 * what is on it, and the dialog opens on Back: the Enter that reached this
 * page, or a doubled one, must not be the consent that erases a disk. */
static void summary_confirm(void)
{
	Disk *d = disk_by_path(cfg.disk);
	char title[176], msg[512], gone[384] = "";

	if (cfg.plan == PLAN_WIPE) {
		char disk[192];

		if (d) {
			snprintf(disk, sizeof(disk), "%s, %s (%s)",
				 d->model[0] ? d->model : "Disk",
				 kb_human_size(ki_disk_bytes(d)), d->path);
			ki_disk_contents(d, 3, gone, sizeof(gone));
		} else {
			kb_strlcpy(disk, cfg.disk, sizeof(disk));
		}
		kb_strlcpy(title, "Erase the disk?", sizeof(title));
		if (gone[0])
			snprintf(msg, sizeof(msg), "Everything on %s is erased: "
				 "%s. There is no undo.", disk, gone);
		else
			snprintf(msg, sizeof(msg), "Everything on %s is erased. "
				 "There is no undo.", disk);
	} else {
		snprintf(title, sizeof(title), "Format %s?", cfg.part_root);
		if (cfg.format_esp)
			snprintf(msg, sizeof(msg), "The root partition %s and the "
				 "EFI system partition %s are reformatted. Other "
				 "partitions are left alone. There is no undo.",
				 cfg.part_root, cfg.part_esp);
		else
			snprintf(msg, sizeof(msg), "The root partition %s is "
				 "reformatted. Other partitions are left alone. "
				 "There is no undo.", cfg.part_root);
	}

	char wrapped[512];

	confirm_wrap(msg, wrapped, sizeof(wrapped));
	ktui_modal_confirm(title, wrapped,
			   cfg.plan == PLAN_WIPE ? "Erase and install"
						 : "Format and install",
			   "Back", summary_go);
}

static void summary_draw(KRect b)
{
	int y = b.y;
	Disk *d = disk_by_path(cfg.disk);
	char v[192];

	ktui_section(b.x, y, b.w, "REVIEW");
	y++;

	y += kv_wrap(b, y, "keymap", cfg.keymap, KT_TEXT);
	y += kv_wrap(b, y, "time zone", cfg.tz_label, KT_TEXT);
	y += kv_wrap(b, y, "hostname", cfg.hostname, KT_TEXT);
	if (cfg.oem) {
		y += kv_wrap(b, y, "user",
			"none — the first boot sets one up", KT_TEXT);
	} else {
		snprintf(v, sizeof(v), "%s (%s)%s", cfg.username, cfg.fullname,
			 cfg.user_wheel ? ", administrator" : "");
		y += kv_wrap(b, y, "user", v, KT_TEXT);
	}
	y += kv_wrap(b, y, "root account",
	      cfg.root_locked ? "locked" : "password set",
	      cfg.root_locked ? KT_TEXT : KT_WARN);
	y += kv_wrap(b, y, "login",
		cfg.oem ? "the setup wizard, on the first boot"
		: !cfg.autologin ? "the login screen"
		: cfg.autologin_lock ? "automatic, into a locked session"
				     : "automatic, no password after the disk's",
		cfg.autologin && !cfg.autologin_lock ? KT_WARN : KT_TEXT);
	snprintf(v, sizeof(v), "%s, %s desk", cfg.theme, cfg.desk);
	y += kv_wrap(b, y, "look", v, KT_TEXT);
	y += kv_wrap(b, y, "alien apps",
	      cfg.with_appbox ? "installed" : "left out",
	      cfg.with_appbox ? KT_TEXT : KT_WARN);
	if (ki_apps_present) {
		static const char *const HOW[] = { "nothing chosen",
						   "at first login" };
		int ngrp = 0, napp = 0;

		for (int i = 0; i < ki_ngroup; i++)
			if (ki_group[i].chosen) {
				ngrp++;
				napp += ki_group[i].napp;
			}
		snprintf(v, sizeof(v), "%d group%s, %d app%s, ~%s %s", ngrp,
			 ngrp == 1 ? "" : "s", napp, napp == 1 ? "" : "s",
			 kb_human_size(ki_apps_bytes()),
			 HOW[ki_apps_route()]);
		y += kv_wrap(b, y, "applications", v,
			     ki_apps_route() == APPS_PENDING ? KT_WARN : KT_TEXT);
	}
	/* The groups by the titles the Applications page shows them under,
	 * as the graphical Summary names them; the id only for a group the
	 * list gives no title. */
	if (ki_groups_chosen()) {
		char ng[512] = "";
		size_t at = 0;

		for (int i = 0; i < ki_nnative && at < sizeof(ng); i++) {
			const KiNative *g = &ki_native[i];

			if (!g->chosen)
				continue;
			at += (size_t)snprintf(ng + at, sizeof(ng) - at, "%s%s",
					       at ? ", " : "",
					       g->title[0] ? g->title : g->id);
		}
		y += kv_wrap(b, y, "native groups", ng, KT_TEXT);
	}

	char svc[160] = "";
	for (int i = 0; i < ki_nservices; i++) {
		if (cfg.svc_off & (1u << i))
			continue;
		if (svc[0])
			strncat(svc, ", ", sizeof(svc) - strlen(svc) - 1);
		strncat(svc, ki_services[i].label, sizeof(svc) - strlen(svc) - 1);
	}
	y += kv_wrap(b, y, "services", svc[0] ? svc : "none", KT_TEXT);
	y++;

	ktui_section(b.x, y, b.w, "DISK");
	y++;
	if (d) {
		snprintf(v, sizeof(v), "%s, %s (%s)",
			 d->model[0] ? d->model : "Disk",
			 kb_human_size(ki_disk_bytes(d)), d->path);
		y += kv_wrap(b, y, "target", v, KT_TEXT);
	}
	y += kv_wrap(b, y, "mode",
	      cfg.plan == PLAN_WIPE ? "erase the disk and repartition"
				    : "use existing partitions",
	      cfg.plan == PLAN_WIPE ? KT_ERR : KT_TEXT);
	if (cfg.plan == PLAN_WIPE && cfg.lvm) {
		if (ki_ab())
			snprintf(v, sizeof(v), "LVM%s, slots A and B of %s each, "
				 "/home shared", cfg.luks ? " inside LUKS2" : "",
				 kb_human_size(ki_slot_mb() << 20));
		else
			snprintf(v, sizeof(v), "LVM%s, %s/%s on all of group %s "
				 "— one slot", cfg.luks ? " inside LUKS2" : "",
				 KI_VG, KI_LV_A, KI_VG);
		y += kv_wrap(b, y, "root", v, ki_ab() ? KT_TEXT : KT_WARN);
	} else if (cfg.luks) {
		/* install_plan honours cfg.luks on every plan, so the review
		 * names the container wherever the LVM line does not. */
		y += kv_wrap(b, y, "encryption", "root inside LUKS2",
			KT_TEXT);
	}
	if (cfg.plan == PLAN_REUSE) {
		y += kv_wrap(b, y, "ESP", cfg.part_esp, KT_TEXT);
		snprintf(v, sizeof(v), "%s, reformatted", cfg.part_root);
		y += kv_wrap(b, y, "root", v, KT_ERR);
		y += kv_wrap(b, y, "format ESP", cfg.format_esp ? "yes" : "no",
		      KT_TEXT);
	}
	y += kv_wrap(b, y, "starts on",
		ki_bios_boot_part() < 0 ? "UEFI only (no BIOS boot partition)"
					: "UEFI and legacy BIOS",
		ki_bios_boot_part() < 0 ? KT_WARN : KT_TEXT);
	/* The words the Layout radio uses: on the A/B layout the swap is a
	 * volume of the group, which both slots share, whichever kind was
	 * asked for. The key carries "swap volume" so the row stays one line
	 * at 80 columns, where the Summary has no row to spare above BEGIN
	 * INSTALL. */
	int abswap = cfg.swap != SWAP_NONE && ki_ab();

	snprintf(v, sizeof(v), "%s",
		 cfg.swap == SWAP_NONE ? "none"
		 : abswap ? "shared by both slots"
		 : cfg.swap == SWAP_FILE ? "swap file"
					 : "swap partition");
	if (cfg.swap != SWAP_NONE)
		snprintf(v + strlen(v), sizeof(v) - strlen(v), ", %ld MiB",
			 cfg.swap_mb);
	y += kv_wrap(b, y, abswap ? "swap volume" : "swap", v, KT_TEXT);
	/* A one-slot root left with less than KI_ROOT_LOW_BYTES installs and
	 * starts, and then has no room for what the first login offers to
	 * add; the row is drawn only then, so an ample root costs the page
	 * no line. */
	if (ki_root_free_bytes() >= 0 &&
	    (unsigned long long)ki_root_free_bytes() < KI_ROOT_LOW_BYTES) {
		snprintf(v, sizeof(v), "%s free after the install: too little "
			 "for application groups or boxed apps",
			 kb_human_size((unsigned long long)ki_root_free_bytes()));
		y += kv_wrap(b, y, "root space", v, KT_WARN);
	}

	/*
	 * THE BUTTONS STAY ON THE SCREEN. Under the review come the line, the
	 * warning, the dry-run note and the buttons, each block a blank row
	 * apart; where the wrapped review leaves too few rows for all of
	 * that, the blank rows go, the one under the line first, then the one
	 * above the dry-run note, the one above the buttons and last the one
	 * above the line, so BEGIN INSTALL is drawn inside the body.
	 */
	char gone[384] = "", whatbuf[512];
	const char *what = whatbuf;
	int nwhat;

	/* A failing preflight line stands above the buttons and BEGIN INSTALL
	 * is refused while it fails (THE PREFLIGHT); a rehearsal writes
	 * nothing and still runs. The status line names the way out. */
	const char *pf = preflight_failing();
	char pfbuf[160] = "";
	int npf = 0;

	if (pf) {
		snprintf(pfbuf, sizeof(pfbuf), "! %s", pf);
		npf = ktui_para(b.x, MEASURE_Y, b.w - 2, pfbuf, KT_ERR);
	}
	ki_pages[ki_page]->subtitle = pf && !cfg.dry_run
		? "fix the marked Welcome line first" : "the point of no return";

	/* The erase names what it destroys, partition by partition, so the
	 * disk holding another system is told from an empty one here: three
	 * of them and a count of the rest, or fewer, down to none, until the
	 * line, the warning, the dry-run note and the buttons fit under the
	 * review with no blank rows. */
	if (cfg.plan == PLAN_WIPE && d) {
		int room = b.y + b.h - y - 1 - (cfg.dry_run ? 1 : 0) - npf - 1;
		/* A rehearsal has no confirm to promise: it writes nothing. */
		const char *then = cfg.dry_run
			? "REHEARSE INSTALL writes nothing."
			: "BEGIN INSTALL asks once more, naming the disk; "
			  "after that there is no undo.";

		for (int max = 3; ; max--) {
			if (max > 0)
				ki_disk_contents(d, max, gone, sizeof(gone));
			if (max > 0 && gone[0])
				snprintf(whatbuf, sizeof(whatbuf), "Starting the "
					 "install destroys everything on the "
					 "target: %s. %s", gone, then);
			else
				snprintf(whatbuf, sizeof(whatbuf), "Starting the "
					 "install erases the target. %s", then);
			nwhat = ktui_para(b.x, MEASURE_Y, b.w - 2, what, KT_ERR);
			if (max <= 0 || !gone[0] || nwhat <= room)
				break;
		}
	} else {
		if (cfg.format_esp)
			snprintf(whatbuf, sizeof(whatbuf), "Starting the install "
				 "reformats the root partition %s and the EFI "
				 "system partition %s. Other partitions on the "
				 "disk are left alone.", cfg.part_root,
				 cfg.part_esp);
		else
			snprintf(whatbuf, sizeof(whatbuf), "Starting the install "
				 "reformats the root partition %s. Other "
				 "partitions on the disk are left alone.",
				 cfg.part_root);
		nwhat = ktui_para(b.x, MEASURE_Y, b.w - 2, what, KT_ERR);
	}
	int need = 1 + nwhat + (cfg.dry_run ? 1 : 0) + npf + 1;
	int slack = b.y + b.h - y - need;
	int g_above = slack-- > 0;
	int g_button = slack-- > 0;
	int g_dry = cfg.dry_run && slack-- > 0;
	int g_line = slack-- > 0;

	y += g_above;
	ktui_draw_hline(b.x, y, b.w, KT_G_DHL, KT_ERR, KT_BG);
	const char *warn = " EVERYTHING ABOVE THIS LINE IS STILL REVERSIBLE ";
	ktui_draw_text(b.x + (b.w - (int)strlen(warn)) / 2, y, b.w, warn, KT_BG,
		  KT_ERR, KT_A_REVERSE);
	y += 1 + g_line;

	y += ktui_para(b.x, y, b.w - 2, what,
		       cfg.plan == PLAN_WIPE && d ? KT_ERR : KT_WARN);
	if (cfg.dry_run) {
		y += g_dry;
		ktui_draw_text(b.x, y++, b.w,
			  "DRY RUN: commands are logged, nothing is executed.",
			  KT_WARN, KT_BG, 0);
	}
	y += g_button;
	if (pf)
		y += ktui_para(b.x, y, b.w - 2, pfbuf, KT_ERR);

	/*
	 * FOCUS STAYS ON ITS CONTROL WHEN THE VERDICT FLIPS. A disabled button
	 * claims no focus id, so a preflight line that starts or stops failing
	 * while the page is up (fixed on another console) moves every later id
	 * by one; unadjusted, the caret would jump from Save answer file onto
	 * BEGIN INSTALL with nothing pressed. A focus on BEGIN INSTALL as it is
	 * refused lands on the control that takes its id.
	 */
	int refused = pf && !cfg.dry_run;
	int base = ktui_id_base();
	if (summary_refused >= 0 && refused != summary_refused) {
		int f = ktui_focus_get();
		if (f < KTUI_ID_CHROME && (refused ? f > base : f >= base))
			ktui_focus_set(f + (refused ? -1 : 1));
	}
	summary_refused = refused;

	/* Both buttons inside the 54 cells of an 80-column body. */
	if (ktui_button(krect(b.x, y, 28, 1),
		      cfg.dry_run ? "REHEARSE INSTALL" : "BEGIN INSTALL",
		      !refused, 1)) {
		/* A rehearsal executes nothing, so it starts at once. */
		if (cfg.dry_run)
			summary_go();
		else
			summary_confirm();
	}
	if (ktui_button(krect(b.x + 30, y, 20, 1), "Save answer file", 1, 0)) {
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
 * disk, and Alt+N is one keystroke away from Alt+B. */
static int summary_validate(char *err, size_t n)
{
	/* A refused BEGIN INSTALL is not a button to send anyone to. */
	if (!cfg.dry_run && preflight_failing())
		snprintf(err, n, "fix the marked Welcome line first");
	else
		snprintf(err, n, "press %s to start — Next does not",
			 cfg.dry_run ? "REHEARSE INSTALL" : "BEGIN INSTALL");
	return 1;
}

/* A visit starts with no verdict to compare against: page_goto has reset
 * focus already. */
static void summary_enter(void)
{
	summary_refused = -1;
}

static Page page_summary = {
	"summary", "Summary", "the point of no return",
	summary_enter, summary_draw, summary_validate, NULL, 0
};

/* ════════════════════════════════════════════════════════════════════════
 * 10 · INSTALL
 * ════════════════════════════════════════════════════════════════════════ */

static int log_full;
static int log_scroll;

/* The install's run time: still counting while it runs, and the figure it
 * ended at once it finished or stopped, so the Install and Done pages read
 * the same number. */
static int install_seconds(void)
{
	double end = inst.done || inst.failed ? inst.t1 : kb_now_s();

	return end > inst.t0 ? (int)(end - inst.t0) : 0;
}

/* A duration in words that agree with their numbers: "1 second",
 * "2 minutes 1 second". */
static void duration_words(char *out, size_t n, int secs)
{
	int m = secs / 60, sec = secs % 60;

	if (!m)
		snprintf(out, n, "%d second%s", sec, sec == 1 ? "" : "s");
	else
		snprintf(out, n, "%d minute%s %d second%s", m, m == 1 ? "" : "s",
			 sec, sec == 1 ? "" : "s");
}

/*
 * The install's warnings as a bulleted list with a hanging indent, each
 * shown once, its first letter upper-cased and its backticks dropped, as
 * the window's WarningList shows them: two paragraphs drawn one under the
 * other read as one. `gap` leaves a blank row between two warnings.
 * Returns the rows drawn.
 */
static int warning_list(KRect b, int y, int gap)
{
	int y0 = y, shown = 0;

	for (int i = 0; i < inst.nwarn; i++) {
		int seen = 0;
		for (int j = 0; j < i; j++)
			seen |= !strcmp(inst.warn[i], inst.warn[j]);
		if (seen)
			continue;

		char t[sizeof(inst.warn[0])];
		size_t o = 0;
		for (const char *c = inst.warn[i]; *c && o + 1 < sizeof(t); c++)
			if (*c != '`')
				t[o++] = *c;
		t[o] = 0;
		t[0] = (char)toupper((unsigned char)t[0]);

		if (shown++ && gap)
			y++;
		ktui_draw_text(b.x, y, 2, ktui_glyph[KT_G_BULLET], KT_WARN,
			       KT_BG, 0);
		y += ktui_para(b.x + 2, y, b.w - 2, t, KT_WARN);
	}
	return y - y0;
}

/* Set by main.c for `kinstall --unattended`. */
int ki_unattended;

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
			ktui_draw_text_fit(b.x, y + i, b.w,
					   inst.log[(base + top + i) % LOG_LINES],
					   KT_MID, KT_BG, 0);
		ktui_draw_text(b.x, b.y + b.h - 1, b.w,
			  "L back to progress   PgUp/PgDn scroll", KT_MID, KT_BG,
			  0);
		return;
	}

	ktui_section(b.x, y, b.w, "PROGRESS");
	y++;

	/* Each step counts by its weight (install.c, THE STEPS' WEIGHTS), so
	 * the copy moves the bar as far as it takes time; a skipped step
	 * counts for nothing. */
	double whole = 0, done = 0;
	for (int i = 0; i < inst.nsteps; i++) {
		const StepUi *s = &inst.step[i];
		int w = s->weight > 0 ? s->weight : 1;

		if (s->state == ST_SKIP)
			continue;
		whole += w;
		if (s->state == ST_DONE)
			done += w;
		else if (s->state == ST_RUNNING && s->frac > 0)
			done += s->frac * w;
	}
	double overall = whole > 0 ? done / whole : 0;

	char pct[32];
	snprintf(pct, sizeof(pct), " %d%% ", (int)(overall * 100));
	ktui_progress(krect(b.x, y, b.w, 1), overall, pct);
	y += 2;

	for (int i = 0; i < inst.nsteps; i++) {
		StepUi *s = &inst.step[i];
		const char *mark = ktui_glyph[KT_G_DOT];
		int fg = KT_MID;
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
			fg = KT_MID;
			break;
		default:
			break;
		}
		ktui_draw_text(b.x + 1, y, 2, mark, fg, KT_BG, 0);
		ktui_draw_text(b.x + 4, y, 16, s->title,
			  s->state == ST_RUNNING ? KT_TEXT : fg, KT_BG, 0);

		/* The running step's note and a failure's message wrap under
		 * column 21, 33 cells at 80: the copy's line is longer than
		 * that, and its end is the time left. */
		if (s->state == ST_RUNNING) {
			int pct = s->frac >= 0 && b.w > 60;
			int n = ktui_para(b.x + 21, y, b.w - 21 - (pct ? 6 : 0),
					  s->note[0] ? s->note : s->detail,
					  KT_MID);
			if (pct) {
				char p[12];
				snprintf(p, sizeof(p), "%3d%%",
					 (int)(s->frac * 100));
				ktui_draw_text(b.x + b.w - 5, y, 5, p, KT_ACCENT,
					  KT_BG, 0);
			}
			y += n > 1 ? n - 1 : 0;
		} else if (s->state == ST_DONE) {
			char t[24];
			snprintf(t, sizeof(t), "%.1fs", s->t1 - s->t0);
			ktui_draw_text(b.x + 21, y, b.w - 21, t, KT_MID, KT_BG, 0);
		} else if (s->state == ST_SKIP) {
			ktui_draw_text(b.x + 21, y, b.w - 21, "not needed", KT_MID,
				  KT_BG, 0);
		} else if (s->state == ST_FAIL) {
			int n = ktui_para(b.x + 21, y, b.w - 21, inst.failmsg,
					  KT_ERR);

			y += n > 1 ? n - 1 : 0;
		}
		y++;
	}
	y++;

	int elapsed = install_seconds();
	ktui_draw_textf(b.x, y++, b.w, KT_MID, KT_BG, 0, "elapsed %02d:%02d",
		   elapsed / 60, elapsed % 60);
	y++;

	/* What the installed system will lack, as it happens: a warning in
	 * the scrolling output is gone by the next step. */
	if (inst.nwarn) {
		ktui_section(b.x, y, b.w, "WARNINGS");
		y++;
		y += warning_list(b, y, b.y + b.h - y > 12);
		y++;
	}

	int tail = b.y + b.h - y - 3;
	if (tail > 2) {
		ktui_section(b.x, y, b.w, "OUTPUT");
		y++;
		int total = inst.nlog < LOG_LINES ? inst.nlog : LOG_LINES;
		int base = inst.nlog > LOG_LINES ? inst.nlog - LOG_LINES : 0;
		int start = total - tail;
		if (start < 0)
			start = 0;
		/* A line is fitted with an ellipsis; L shows it whole. */
		for (int i = 0; start + i < total && i < tail; i++)
			ktui_draw_text_fit(b.x, y + i, b.w,
					   inst.log[(base + start + i) % LOG_LINES],
					   KT_MID, KT_BG, 0);
		y += tail;
	}

	/* An unattended run has nobody to press a button and ends by itself
	 * (main.c), so it draws none; its last line says where it goes. */
	if (ki_unattended && (inst.failed || inst.done)) {
		ktui_draw_text(b.x, b.y + b.h - 1, b.w,
			       inst.failed ? "unattended: exiting, the reason "
					     "to standard error"
					   : "unattended: ending by itself",
			       KT_MID, KT_BG, 0);
		return;
	}

	/* The four buttons sized to their labels, inside the 54 cells of an
	 * 80-column body. */
	if (inst.failed) {
		int by = b.y + b.h - 1;
		if (ktui_button(krect(b.x, by, 14, 1), "Retry step", 1, 1))
			install_start(inst.cur < 0 ? 0 : inst.cur);
		if (ktui_button(krect(b.x + 16, by, 12, 1), "View log", 1, 0))
			log_full = 1;
		if (ktui_button(krect(b.x + 30, by, 9, 1), "Shell", 1, 0)) {
			char *a[] = { "/bin/bash", "-l", NULL };
			ktui_run_console_note("A root shell. Type exit to return "
					      "to the installer.", a);
		}
		if (ktui_button(krect(b.x + 41, by, 9, 1), "Abort", 1, 0))
			ki_quit = 1;
	} else if (inst.done) {
		if (ktui_button(krect(b.x, b.y + b.h - 1, 20, 1), "Continue", 1, 1))
			page_goto(page_index("done"));
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
 * 11 · DONE
 * ════════════════════════════════════════════════════════════════════════ */

/* The live root is a squashfs read from the boot medium, so with the medium
 * out no program can page in, `reboot` and init's own shutdown among them,
 * and neither can a page of kinstall or its libraries that was dropped.
 * Entering this page locks what kinstall has mapped into memory while the
 * medium is still in, so the fallback below can run without it. */
static void done_enter(void)
{
	(void)mlockall(MCL_CURRENT);
}

/* Sync first, while the disks are reachable; then run `reboot`, and when it
 * cannot run or does not succeed, try the system call, which reads no file.
 * The system call fails without CAP_SYS_BOOT; say so on the restored
 * terminal rather than end without a word. */
static void reboot_now(void)
{
	sync();
	ktui_term_shutdown();
	pid_t pid = fork();

	if (pid == 0) {
		execlp("reboot", "reboot", (char *)NULL);
		_exit(127);
	}
	int st = 0;

	if (pid > 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) &&
	    WEXITSTATUS(st) == 0)
		_exit(0);
	sync();
	reboot(RB_AUTOBOOT);
	fprintf(stderr, "kinstall: could not restart: %s\n", strerror(errno));
	_exit(1);
}

static void done_view_log(void)
{
	char *a[] = { "less", "/var/log/kinstall.log", NULL };
	if (!kb_have_prog("less"))
		a[0] = "more";
	ktui_run_console(a);
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

	char v[128];
	duration_words(v, sizeof(v), install_seconds());
	y += kv_wrap(b, y, "took", v, KT_TEXT);
	y += kv_wrap(b, y, "installed to", cfg.disk, KT_TEXT);
	y += kv_wrap(b, y, "log", "/var/log/kinstall.log", KT_TEXT);
	y++;
	if (inst.nwarn) {
		ktui_section(b.x, y, b.w, "FINISHED WITH WARNINGS");
		y++;
		y += warning_list(b, y, b.h >= 20);
	}
	y++;

	y += ktui_para(b.x, y, b.w,
		  "Choose Reboot now, and take the boot medium out when the "
		  "screen goes dark, before the firmware starts it again.",
		  KT_MID);
	y++;

	/* Two rows, so every button and its whole label sit inside the 54
	 * cells of an 80-column body. */
	if (ktui_button(krect(b.x, y, 16, 1), "Reboot now", 1, 1))
		ktui_modal_confirm("Reboot", "Restart this machine now?", "Reboot",
			      "Wait", reboot_now);
	if (ktui_button(krect(b.x + 18, y, 28, 1), "Back to the live desktop", 1,
			0))
		ki_quit = 1;
	y += 2;
	if (ktui_button(krect(b.x, y, 16, 1), "View log", 1, 0))
		done_view_log();
}

/* L opens the same pager as "View log": the hint bar names L on every page
 * that hides the navigation, and this is one of them. */
static int done_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_KEY && (ev->key == 'l' || ev->key == 'L')) {
		done_view_log();
		return 1;
	}
	return 0;
}

static Page page_done = {
	"done", "Done", "reboot into KDOS",
	done_enter, done_draw, NULL, done_event, 1
};

/* ──────────────────────────────────────────────────────────────────────── */

Page *ki_pages[] = {
	&page_welcome, &page_keyboard, &page_time, &page_disk, &page_layout,
	&page_accounts, &page_system, &page_apps, &page_summary, &page_install,
	&page_done,
};

int ki_npages = (int)(sizeof(ki_pages) / sizeof(ki_pages[0]));

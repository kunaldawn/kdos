/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   One kinstall page, drawn with no terminal
 *
 *   pagedraw <page-id> <cols> <rows> [<answers>]
 *       enters the page with the answers loaded (the Applications page
 *       first, for a page after it), draws its body into the
 *       rectangle kinstall's main.c gives it on a terminal of that size
 *       (the sidebar's 22 columns at 78 and wider, the header above and
 *       the navigation below) and prints `# body <x> <y> <w> <h>`, the
 *       frame as plain text, one terminal row per line, and the chosen
 *       zone as `# tz <label>`
 *
 *   PAGEDRAW_KEYS plays keys into the page before the frame printed
 *   (p a frame in which the Limine preflight line flips, a capital
 *   letter its lowercase typed into the focused field),
 *   PAGEDRAW_DONE an install that finished that many seconds after it
 *   started, long ago, with PAGEDRAW_WARN's `|`-separated warnings and
 *   PAGEDRAW_LOG's line, PAGEDRAW_CAPS capability bits added to the
 *   terminal's (8, KT_CAP_UTF8, the box-drawing glyph table), with
 *   every cell drawn in a literal colour printed as
 *   `# rgb <x> <y> <rrggbb>`,
 *   PAGEDRAW_DISK_GB stands in one disk of that size, PAGEDRAW_MOUNT a
 *   partition of it mounted at that path and PAGEDRAW_PARTS that many
 *   labelled ext4 partitions on it, PAGEDRAW_CPU a processor's name,
 *   PAGEDRAW_COPY, PAGEDRAW_FAIL and PAGEDRAW_LOG an install running
 *   its copy or failed in it, and PAGEDRAW_DRYRUN=1 the pages of
 *   `kinstall --dry-run`, PAGEDRAW_UNATTENDED=1 those of
 *   `kinstall --unattended`, and PAGEDRAW_PREFLIGHT the index of the one
 *   Welcome preflight line that fails (unset, every line passes, whatever
 *   this host has installed). The page's status line is printed as
 *   `# status <text>`, before the zone.
 *
 *   PAGEDRAW_KDOSLAYOUT=1, with PAGEDRAW_DISK_GB, lays that disk out as a
 *   KDOS install does: vda1 a 512M ESP, vda2 a 1M BIOS boot partition and
 *   vda3 an LVM2_member over the rest, and PAGEDRAW_ESPLAYOUT=1 the table
 *   sfdisk's default alignment leaves for a 512M ESP and one partition
 *   over the rest: vda1 the ESP at sector 2048 and vda2 from its end to
 *   2048 sectors short of the disk's. PAGEDRAW_CELLROW=<y> prints every
 *   cell of terminal row y as `# cell <x> <fg> <bg>`, the slots a terminal
 *   shows once KT_A_REVERSE has swapped them.
 *
 *   PAGEDRAW_THEME=<scheme> draws in that libkcolor scheme instead of the
 *   default one and prints its eight slots, as drawn, as
 *   `# slot <n> <rrggbb>`.
 *
 *   PAGEDRAW_MODAL=`title|msg|yes|no` raises that confirm over the drawn
 *   page, as kinstall's Ctrl+Q and BEGIN INSTALL do, and PAGEDRAW_HELP=1
 *   draws the F1 key card over it instead, with `# cuts` counting the
 *   card's text alone.
 *
 * It links pages.c with kinstall-core and stands in for main.c's wizard
 * state, so a page's layout is checked on the host: a command wrapped
 * inside a paragraph, a list squeezed to nothing on a small console.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kinstall.h"

int ki_page;
int ki_quit;
int ki_help;
extern int ki_unattended;
extern int ki_preflight_fail;

int page_index(const char *id)
{
	for (int i = 0; i < ki_npages; i++)
		if (!strcmp(ki_pages[i]->id, id))
			return i;
	return -1;
}

void page_goto(int n)
{
	if (n >= 0 && n < ki_npages)
		ki_page = n;
}

void nav_next(void)
{
}

void nav_back(void)
{
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: pagedraw <page-id> <cols> <rows> "
				"[<answers>]\n");
		return 2;
	}
	int w = atoi(argv[2]), h = atoi(argv[3]);
	int n = page_index(argv[1]);

	if (n < 0 || w < 62 || h < 18) {
		fprintf(stderr, "pagedraw: no page %s, or %dx%d is under "
				"kinstall's 62x18\n", argv[1], w, h);
		return 2;
	}
	/* PAGEDRAW_DISK_GB stands in for probe_disks(): one GPT disk of that
	 * size at the answers' `disk`, so the Layout page's A/B rows draw. */
	const char *gb = getenv("PAGEDRAW_DISK_GB");

	if (gb && *gb) {
		memset(&ki_disk[0], 0, sizeof(ki_disk[0]));
		kb_strlcpy(ki_disk[0].name, "vda", sizeof(ki_disk[0].name));
		kb_strlcpy(ki_disk[0].path, "/dev/vda", sizeof(ki_disk[0].path));
		kb_strlcpy(ki_disk[0].model, "QEMU disk",
			   sizeof(ki_disk[0].model));
		kb_strlcpy(ki_disk[0].table, "gpt", sizeof(ki_disk[0].table));
		ki_disk[0].sectors = strtoull(gb, NULL, 10) << 21;
		ki_disk[0].sector_size = 512;
		ki_ndisk = 1;
	}
	/* PAGEDRAW_MOUNT gives that disk one partition, mounted there. */
	const char *mnt = getenv("PAGEDRAW_MOUNT");

	if (ki_ndisk && mnt && *mnt) {
		Part *p = &ki_disk[0].part[0];

		memset(p, 0, sizeof(*p));
		kb_strlcpy(p->name, "vda1", sizeof(p->name));
		kb_strlcpy(p->path, "/dev/vda1", sizeof(p->path));
		kb_strlcpy(p->fstype, "iso9660", sizeof(p->fstype));
		kb_strlcpy(p->label, "KDOS_LIVE", sizeof(p->label));
		kb_strlcpy(p->mountpoint, mnt, sizeof(p->mountpoint));
		p->start = 2048;
		p->sectors = ki_disk[0].sectors / 2;
		p->num = 1;
		p->mounted = 1;
		ki_disk[0].nparts = 1;
	}
	/* PAGEDRAW_PARTS gives that disk as many ext4 partitions, each with
	 * a label, so a Summary names what an erase destroys. */
	const char *np = getenv("PAGEDRAW_PARTS");

	if (ki_ndisk && np && *np) {
		Disk *d = &ki_disk[0];
		int k = atoi(np);

		if (k > MAX_PARTS)
			k = MAX_PARTS;
		for (int i = 0; i < k; i++) {
			Part *p = &d->part[i];

			memset(p, 0, sizeof(*p));
			snprintf(p->name, sizeof(p->name), "vda%d", i + 1);
			snprintf(p->path, sizeof(p->path), "/dev/vda%d", i + 1);
			kb_strlcpy(p->fstype, "ext4", sizeof(p->fstype));
			snprintf(p->label, sizeof(p->label), "data%d", i + 1);
			p->sectors = d->sectors / (unsigned long long)(k + 1);
			p->start = 2048 + (unsigned long long)i * p->sectors;
			p->num = i + 1;
		}
		d->nparts = k;
	}
	/* PAGEDRAW_KDOSLAYOUT gives that disk the table a KDOS install
	 * leaves: the ESP, the BIOS boot partition and the LVM PV. */
	const char *kl = getenv("PAGEDRAW_KDOSLAYOUT");

	if (ki_ndisk && kl && atoi(kl)) {
		Disk *d = &ki_disk[0];
		static const unsigned long long len[2] = { 512ULL << 11, 1ULL << 11 };
		unsigned long long at = 2048;

		for (int i = 0; i < 3; i++) {
			Part *p = &d->part[i];

			memset(p, 0, sizeof(*p));
			snprintf(p->name, sizeof(p->name), "vda%d", i + 1);
			snprintf(p->path, sizeof(p->path), "/dev/vda%d", i + 1);
			p->num = i + 1;
			p->start = at;
			p->sectors = i < 2 ? len[i] : d->sectors - at - 34;
			at += p->sectors;
		}
		kb_strlcpy(d->part[0].fstype, "vfat", sizeof(d->part[0].fstype));
		d->part[0].is_esp = 1;
		d->part[1].is_bios_boot = 1;
		kb_strlcpy(d->part[2].fstype, "LVM2_member",
			   sizeof(d->part[2].fstype));
		d->nparts = 3;
	}
	/* PAGEDRAW_ESPLAYOUT gives that disk sfdisk's table for a 512M ESP
	 * and one partition over the rest, which ends 2048 sectors short of
	 * the disk's end, under one cell of the bar on any terminal. */
	const char *el = getenv("PAGEDRAW_ESPLAYOUT");

	if (ki_ndisk && el && atoi(el)) {
		Disk *d = &ki_disk[0];

		for (int i = 0; i < 2; i++) {
			Part *p = &d->part[i];

			memset(p, 0, sizeof(*p));
			snprintf(p->name, sizeof(p->name), "vda%d", i + 1);
			snprintf(p->path, sizeof(p->path), "/dev/vda%d", i + 1);
			p->num = i + 1;
		}
		d->part[0].start = 2048;
		d->part[0].sectors = 512ULL << 11;
		kb_strlcpy(d->part[0].fstype, "vfat", sizeof(d->part[0].fstype));
		d->part[0].is_esp = 1;
		d->part[1].start = 2048 + d->part[0].sectors;
		d->part[1].sectors = d->sectors - 2048 - d->part[1].start;
		kb_strlcpy(d->part[1].fstype, "ext4", sizeof(d->part[1].fstype));
		d->nparts = 2;
	}
	/* PAGEDRAW_CPU stands in for probe_system()'s processor, on a 32-bit
	 * UEFI with Secure Boot on: the Welcome page's longest values. */
	const char *cpu = getenv("PAGEDRAW_CPU");

	if (cpu && *cpu) {
		kb_strlcpy(ki_sys.cpu, cpu, sizeof(ki_sys.cpu));
		ki_sys.cores = 16;
		ki_sys.uefi = 1;
		ki_sys.fw_bits = 32;
		ki_sys.secure_boot = 1;
	}
	if (argc > 4 && conf_load(argv[4]) < 0)
		return 1;
	/* PAGEDRAW_COPY is the Copy step's note, the step running and every
	 * step before it done; PAGEDRAW_FAIL fails it with that message
	 * instead; PAGEDRAW_LOG is a line of the install's output. */
	const char *copy = getenv("PAGEDRAW_COPY");
	const char *fail = getenv("PAGEDRAW_FAIL");
	const char *logl = getenv("PAGEDRAW_LOG");

	if ((copy && *copy) || (fail && *fail)) {
		install_plan();
		inst.logfd = -1;
		inst.t0 = kb_now_s();
		for (int i = 0; i < inst.nsteps; i++) {
			StepUi *st = &inst.step[i];

			if (strcmp(st->title, "Copy system")) {
				st->state = ST_DONE;
				continue;
			}
			if (fail && *fail) {
				st->state = ST_FAIL;
				kb_strlcpy(inst.failmsg, fail, sizeof(inst.failmsg));
				inst.failed = 1;
				inst.cur = i;
			} else {
				st->state = ST_RUNNING;
				st->frac = 0.1;
				kb_strlcpy(st->note, copy, sizeof(st->note));
				inst.running = 1;
			}
			break;
		}
		if (logl && *logl)
			install_log(logl);
	}
	const char *fin = getenv("PAGEDRAW_DONE");

	if (fin && *fin) {
		install_plan();
		inst.logfd = -1;
		for (int i = 0; i < inst.nsteps; i++)
			if (inst.step[i].state != ST_SKIP)
				inst.step[i].state = ST_DONE;
		inst.t0 = kb_now_s() - 500;
		inst.t1 = inst.t0 + atoi(fin);
		inst.done = 1;
		const char *wl = getenv("PAGEDRAW_WARN");
		for (const char *c = wl; c && *c && inst.nwarn < 8; ) {
			const char *e = strchr(c, '|');
			size_t l = e ? (size_t)(e - c) : strlen(c);
			if (l >= sizeof(inst.warn[0]))
				l = sizeof(inst.warn[0]) - 1;
			memcpy(inst.warn[inst.nwarn], c, l);
			inst.warn[inst.nwarn++][l] = 0;
			c = e ? e + 1 : c + l;
		}
		if (logl && *logl)
			install_log(logl);
	}
	const char *th = getenv("PAGEDRAW_THEME");

	if (ktui_theme_set(th && *th ? th : KCOL_DEFAULT_NAME) < 0)
		return 1;
	ktui_ramp_init();
	if (ktui_offscreen_init(w, h) != 0)
		return 1;
	const char *caps = getenv("PAGEDRAW_CAPS");
	/* The glyph table follows the caps, so it is chosen again with them:
	 * 8 (KT_CAP_UTF8) draws the frame in the box-drawing glyphs a VT
	 * shows, which the dump writes as UTF-8. */
	if (caps && *caps) {
		ktui_caps |= atoi(caps);
		ktui_draw_init();
	}
	const char *dry = getenv("PAGEDRAW_DRYRUN");
	if (dry && *dry)
		cfg.dry_run = atoi(dry);
	const char *una = getenv("PAGEDRAW_UNATTENDED");
	if (una && *una)
		ki_unattended = atoi(una);
	const char *pf = getenv("PAGEDRAW_PREFLIGHT");
	ki_preflight_fail = pf && *pf ? atoi(pf) : 99;

	/* The wizard passes the Applications page on its way to every page
	 * after it, and the Summary reads what that page chose. */
	int apps = page_index("apps");
	if (n > apps && apps >= 0 && ki_pages[apps]->enter)
		ki_pages[apps]->enter();
	ki_page = n;
	/* The Disk page's enter is probe_disks(), which would put this
	 * machine's own disks in place of the one stood in. */
	if (ki_pages[n]->enter && !(gb && *gb && !strcmp(argv[1], "disk")))
		ki_pages[n]->enter();

	/* main.c's render(), the body rectangle and nothing around it. */
	int content_y = 2;
	int content_h = h - 4 - content_y + 1;
	int sb = w >= 78 ? 22 : 0;
	KRect body = krect(sb ? sb + 3 : 1, content_y, w - (sb ? sb + 4 : 2),
			   content_h);
	KtuiEvent ev;
	int used = 0;
	/* PAGEDRAW_KEYS plays keys first, a frame each: t Tab, d Down, u Up,
	 * r Return, a capital letter its lowercase typed, and p a frame with
	 * no key in which the Limine preflight line starts failing, or stops
	 * if it was. Tab moves focus at the end
	 * of its frame, so the frame drawn last is the one that sees where
	 * focus landed. */
	const char *keys = getenv("PAGEDRAW_KEYS");

	for (const char *k = keys ? keys : ""; ; k++) {
		memset(&ev, 0, sizeof(ev));
		if (*k == 'p')
			ki_preflight_fail = ki_preflight_fail == 4 ? 99 : 4;
		else if (*k) {
			ev.type = KT_EVT_KEY;
			ev.key = *k >= 'A' && *k <= 'Z' ? *k - 'A' + 'a'
			       : *k == 't' ? KT_K_TAB : *k == 'd' ? KT_K_DOWN
			       : *k == 'u' ? KT_K_UP : KT_K_ENTER;
		}
		ktui_draw_clear();
		ktui_draw_cut_reset();
		ktui_frame_begin(&ev);
		ktui_draw_clip(body);
		ktui_extent_reset(body.y - 1);
		ki_pages[n]->draw(body);
		used = ktui_extent() - body.y + 1;
		ktui_draw_clip_none();
		if (!*k)
			break;
		ktui_frame_end();
	}
	/* main.c's scroll bar in the last column, for a page taller than the
	 * pane, so the frame holds every cell kinstall itself writes there. */
	if (used > body.h)
		ktui_scrollbar(krect(w - 1, body.y, 1, body.h), used, body.h, 0);

	const char *modal = getenv("PAGEDRAW_MODAL");
	const char *help = getenv("PAGEDRAW_HELP");

	if (modal && *modal) {
		static char mbuf[512];
		char *f[4] = { mbuf, "", "Yes", "No" };

		kb_strlcpy(mbuf, modal, sizeof(mbuf));
		for (int i = 1; i < 4; i++) {
			char *bar = strchr(f[i - 1], '|');
			if (!bar)
				break;
			*bar = 0;
			f[i] = bar + 1;
		}
		ktui_modal_confirm(f[0], f[1], f[2], f[3], NULL);
		ktui_modal_draw();
	} else if (help && atoi(help)) {
		ktui_draw_cut_reset();
		ki_draw_help();
	}

	printf("# body %d %d %d %d\n", body.x, body.y, body.w, body.h);
	ktui_draw_dump();
	printf("# cuts %d\n", ktui_draw_cut_count());
	int cw, ch;
	const KtuiCell *cell = ktui_draw_cells(&cw, &ch);
	for (int i = 0; cell && i < cw * ch; i++)
		if (cell[i].attr & KT_A_FGRGB)
			printf("# rgb %d %d %06x\n", i % cw, i / cw,
			       cell[i].fgc & 0xffffffu);
	const char *crow = getenv("PAGEDRAW_CELLROW");
	int cy = crow && *crow ? atoi(crow) : -1;
	for (int x = 0; cell && cy >= 0 && cy < ch && x < cw; x++) {
		const KtuiCell *c = &cell[cy * cw + x];
		int rev = (c->attr & KT_A_REVERSE) != 0;
		printf("# cell %d %d %d\n", x, rev ? c->bg : c->fg,
		       rev ? c->fg : c->bg);
	}
	for (int i = 0; th && *th && i < KT_NCOLOR; i++) {
		const KRgb *c = &ktui_theme->slot[i];

		printf("# slot %d %02x%02x%02x\n", i, c->r, c->g, c->b);
	}
	printf("# status %s\n", ki_pages[n]->subtitle ? ki_pages[n]->subtitle
						       : "");
	printf("# tz %s\n", cfg.tz_label);
	return 0;
}

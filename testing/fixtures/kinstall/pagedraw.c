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
 *   PAGEDRAW_KEYS plays keys into the page before the frame printed,
 *   PAGEDRAW_DONE an install that finished that many seconds after it
 *   started, long ago, with PAGEDRAW_WARN's `|`-separated warnings and
 *   PAGEDRAW_LOG's line, PAGEDRAW_CAPS capability bits added to the
 *   terminal's, with every cell drawn in a literal colour printed as
 *   `# rgb <x> <y> <rrggbb>`,
 *   PAGEDRAW_DISK_GB stands in one disk of that size and PAGEDRAW_MOUNT a
 *   partition of it mounted at that path, PAGEDRAW_CPU a processor's name,
 *   and PAGEDRAW_COPY, PAGEDRAW_FAIL and PAGEDRAW_LOG an install running
 *   its copy or failed in it.
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
	if (ktui_theme_set(KCOL_DEFAULT_NAME) < 0)
		return 1;
	ktui_ramp_init();
	if (ktui_offscreen_init(w, h) != 0)
		return 1;
	const char *caps = getenv("PAGEDRAW_CAPS");
	if (caps && *caps)
		ktui_caps |= atoi(caps);

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
	 * r Return. Tab moves focus at the end of its frame, so the frame
	 * drawn last is the one that sees where focus landed. */
	const char *keys = getenv("PAGEDRAW_KEYS");

	for (const char *k = keys ? keys : ""; ; k++) {
		memset(&ev, 0, sizeof(ev));
		if (*k) {
			ev.type = KT_EVT_KEY;
			ev.key = *k == 't' ? KT_K_TAB : *k == 'd' ? KT_K_DOWN
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

	printf("# body %d %d %d %d\n", body.x, body.y, body.w, body.h);
	ktui_draw_dump();
	printf("# cuts %d\n", ktui_draw_cut_count());
	int cw, ch;
	const KtuiCell *cell = ktui_draw_cells(&cw, &ch);
	for (int i = 0; cell && i < cw * ch; i++)
		if (cell[i].attr & KT_A_FGRGB)
			printf("# rgb %d %d %06x\n", i % cw, i / cw,
			       cell[i].fgc & 0xffffffu);
	printf("# tz %s\n", cfg.tz_label);
	return 0;
}

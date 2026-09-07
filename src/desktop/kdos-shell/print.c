/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-print — printers, over the tools CUPS already ships
 *
 *   ┌ Printers ────────────────────────────────────────────┐
 *   │ Printers │ Discovered                                │
 *   ├──────────────────────────────────────────────────────┤
 *   │ * HP_LaserJet    idle      accepting                  │
 *   │   Brother_DCP    printing  accepting                  │
 *   ├──────────────────────────────────────────────────────┤
 *   │ [ Add ] [ Default ] [ Remove ]                        │
 *   └──────────────────────────────────────────────────────┘
 *
 * NO libcups AND NO IPP. `lpstat`, `lpinfo` and `lpadmin` are on the image,
 * they are what the CUPS documentation tells a person to type, and they are
 * the interface upstream keeps stable. Linking libcups would put a second
 * client library and its config parsing in the panel binary to re-derive
 * answers three programs beside it already give.
 *
 * `lpadmin` GROUP, NOT ROOT. `/etc/group` grants the desktop user `lpadmin`,
 * which is exactly the authority CUPS defines for administering printers —
 * so this surface runs as the user and needs no daemon of ours in front of it.
 * That is the difference between printing and mounting: CUPS shipped the
 * privilege split, and the kernel did not.
 *
 * EVERYTHING EXECS THROUGH AN ARGUMENT VECTOR. A printer's name and a device
 * URI both come from outside this program, and a shell in the middle turns
 * either into an injection point.
 *
 * `-m everywhere` AND NOTHING ELSE. IPP Everywhere is what a driverless
 * printer advertises and what CUPS 2.x resolves without a PPD; a driver
 * picker would be a list of a thousand `lpinfo -m` rows to choose from, which
 * is the dialog that made printing on Linux notorious. A printer that needs
 * more than this needs its vendor's own tooling, and the surface says so
 * rather than pretending otherwise.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define PR_COLS 72
#define PR_ROWS 18
#define PR_MAX 32
#define PR_NAME 64

enum { PG_PRINTERS, PG_FOUND, PG_N };
enum { PB_ADD, PB_DEFAULT, PB_REMOVE, PB_REFRESH, PB_CLOSE, PB_N };

static const KtuiTab PAGES[PG_N] = {
	{ "Printers", NULL }, { "Discovered", NULL }
};

struct printer {
	char name[PR_NAME];
	char state[32];
	int accepting;
	int is_default;
};

struct found {
	char uri[192];
	char kind[16];		/* direct, network, serial, file          */
};

/*
 * THE RECORDED ANSWERS, for the golden and for anybody without a printer.
 *
 * A surface whose picture depends on three programs on the host's PATH cannot
 * be goldened at all: a machine with CUPS set up and one without draw
 * different frames, and neither is wrong. `--fixture DIR` reads `lpstat-p`,
 * `lpstat-d` and `lpinfo-v` out of a directory instead of running them, which
 * is the same seam kdos-mountd and kdos-energyd use and the same reason.
 */
static const char *fixture;

static int recorded(const char *name, char *out, size_t n)
{
	char path[512];

	if (!fixture)
		return 0;
	snprintf(path, sizeof(path), "%s/%s", fixture, name);
	out[0] = '\0';
	kb_read_file(path, out, n);
	return 1;
}

static KtuiKeys keys;
static struct printer prn[PR_MAX];
static int nprn;
static struct found fnd[PR_MAX];
static int nfnd;
static char why[128];
static char status[160];
static int page;
static KtuiTable tprn, tfnd;
static int icons_on = 1;

static KtuiTable *cur_table(void)
{
	return page == PG_PRINTERS ? &tprn : &tfnd;
}

static int cur_count(void)
{
	return page == PG_PRINTERS ? nprn : nfnd;
}

/* ── reading ───────────────────────────────────────────────────────────── */

/*
 * `lpstat -p` PER PRINTER AND `lpstat -d` FOR THE DEFAULT, which is two calls
 * and not one: `lpstat -p -d` interleaves the default line among the printer
 * lines, and a parser that scanned for it in the middle would take a printer
 * named `system` for the answer.
 */
static void scan_printers(void)
{
	char buf[8192];
	KbArgv a = { 0 };

	nprn = 0;
	why[0] = '\0';

	if (!recorded("lpstat-p", buf, sizeof(buf))) {
		kb_argv_add(&a, "lpstat");
		kb_argv_add(&a, "-p");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}

	if (strstr(buf, "Scheduler is not running") || !buf[0]) {
		snprintf(why, sizeof(why),
			 "cupsd is not running (service start 80_cups)");
		return;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp);
	     ln && nprn < PR_MAX; ln = strtok_r(NULL, "\n", &sp)) {
		struct printer *p = &prn[nprn];

		/* `printer NAME is idle.  enabled since ...` — the word after
		 * `is`, up to the full stop, is the state. */
		if (sscanf(ln, "printer %63s is %31[^.]", p->name, p->state) != 2)
			continue;
		p->accepting = 1;
		p->is_default = 0;
		nprn++;
	}

	char dbuf[256];
	KbArgv d = { 0 };

	if (!recorded("lpstat-d", dbuf, sizeof(dbuf))) {
		kb_argv_add(&d, "lpstat");
		kb_argv_add(&d, "-d");
		kb_argv_end(&d);
		kb_run_capture(&d, dbuf, sizeof(dbuf));
	}

	const char *colon = strchr(dbuf, ':');

	if (colon) {
		char name[PR_NAME];

		if (sscanf(colon + 1, "%63s", name) == 1)
			for (int i = 0; i < nprn; i++)
				if (!strcmp(prn[i].name, name))
					prn[i].is_default = 1;
	}
}

static void scan_found(void)
{
	char buf[8192];
	KbArgv a = { 0 };

	nfnd = 0;
	if (!recorded("lpinfo-v", buf, sizeof(buf))) {
		kb_argv_add(&a, "lpinfo");
		kb_argv_add(&a, "-v");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}

	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp);
	     ln && nfnd < PR_MAX; ln = strtok_r(NULL, "\n", &sp)) {
		struct found *f = &fnd[nfnd];

		if (sscanf(ln, "%15s %191s", f->kind, f->uri) != 2)
			continue;
		/*
		 * `file` AND `serial` ARE DROPPED. `lpinfo -v` lists every
		 * backend CUPS has, including ones that print to a file and
		 * ones that name a serial port with nothing on it — offering
		 * them as printers to add is offering a queue that will never
		 * produce a page.
		 */
		if (!strcmp(f->kind, "file") || !strcmp(f->kind, "serial"))
			continue;
		nfnd++;
	}
}

static void refresh(void)
{
	scan_printers();
	if (!why[0])
		scan_found();
	else
		nfnd = 0;
	ktui_table_clamp(&tprn, nprn, ktui_h > 8 ? ktui_h - 8 : 1);
	ktui_table_clamp(&tfnd, nfnd, ktui_h > 8 ? ktui_h - 8 : 1);
}

/* ── the verbs ─────────────────────────────────────────────────────────── */

/*
 * A QUEUE NAME CUPS WILL ACCEPT, derived from the URI. CUPS refuses a name
 * with a space, a slash or a `#` in it, and a device URI is full of all three
 * — so the name is built from the URI's own characters with everything else
 * turned into an underscore rather than being asked for. Somebody who wants a
 * different name renames it in CUPS; somebody adding a printer wants it added.
 */
static void queue_name(const char *uri, char *out, size_t n)
{
	const char *p = strstr(uri, "://");
	size_t k = 0;

	p = p ? p + 3 : uri;
	for (; *p && k + 1 < n; p++) {
		char c = *p;

		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9'))
			out[k++] = c;
		else if (k && out[k - 1] != '_')
			out[k++] = '_';
	}
	while (k && out[k - 1] == '_')
		k--;
	if (!k)
		k = (size_t)snprintf(out, n, "%s", "printer");
	out[k] = '\0';
}

static void do_add(void)
{
	const struct found *f;
	char name[PR_NAME], buf[512];
	KbArgv a = { 0 };

	if (tfnd.sel < 0 || tfnd.sel >= nfnd)
		return;
	f = &fnd[tfnd.sel];
	queue_name(f->uri, name, sizeof(name));

	kb_argv_add(&a, "lpadmin");
	kb_argv_add(&a, "-p");
	kb_argv_add(&a, name);
	kb_argv_add(&a, "-E");		/* enabled and accepting */
	kb_argv_add(&a, "-v");
	kb_argv_add(&a, f->uri);
	kb_argv_add(&a, "-m");
	kb_argv_add(&a, "everywhere");
	kb_argv_end(&a);

	if (kb_run_capture(&a, buf, sizeof(buf)) == 0)
		snprintf(status, sizeof(status), "added %s", name);
	else
		snprintf(status, sizeof(status), "%s",
			 buf[0] ? buf : "lpadmin refused it — "
					"this printer is not IPP Everywhere");
	refresh();
}

static void do_named(const char *flag, const char *what)
{
	char buf[512];
	KbArgv a = { 0 };

	if (tprn.sel < 0 || tprn.sel >= nprn)
		return;
	kb_argv_add(&a, "lpadmin");
	kb_argv_add(&a, flag);
	kb_argv_add(&a, prn[tprn.sel].name);
	kb_argv_end(&a);

	if (kb_run_capture(&a, buf, sizeof(buf)) == 0)
		snprintf(status, sizeof(status), "%s %s", what,
			 prn[tprn.sel].name);
	else
		snprintf(status, sizeof(status), "%s",
			 buf[0] ? buf : "lpadmin refused it");
	refresh();
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol PRN_COL[] = {
	{ "PRINTER", 0 }, { "STATE", 14 }, { "DEFAULT", 9 }
};
#define PRN_NCOL 3

static const KtuiCol FND_COL[] = { { "HOW", 10 }, { "DEVICE", 0 } };
#define FND_NCOL 2

static void prn_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		     void *user)
{
	const struct printer *p = &prn[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, p->name, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_text(x, y, w, p->state, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	default:
		if (p->is_default)
			ktui_draw_text(x, y, w, "default",
				       on ? KT_SURFACE : KT_ACCENT, bg,
				       KT_A_NONE);
		break;
	}
}

static void fnd_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		     void *user)
{
	const struct found *f = &fnd[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	if (col == 0)
		ktui_draw_text(x, y, w, f->kind, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
	else
		ktui_draw_text(x, y, w, f->uri, fg, bg, KT_A_NONE);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int top, body;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Printers", KT_ACCENT, KT_BG, 1);
	top = kch_header(w, "printer", "Printers",
			 "queues on this machine, and what is on the network",
			 icons_on);

	ktui_tabs_draw(krect(2, top, w - 4, 1), PAGES, PG_N, page, -1, 0);
	body = h - top - 5;
	if (body < 1)
		body = 1;

	if (why[0]) {
		ktui_draw_text(2, top + 2, w - 4, why, KT_ERR, KT_BG,
			       KT_A_NONE);
	} else if (page == PG_PRINTERS && !nprn) {
		ktui_draw_text(2, top + 2, w - 4,
			       "no printer is set up — Discovered lists what is there",
			       KT_MID, KT_BG, KT_A_NONE);
	} else if (page == PG_FOUND && !nfnd) {
		ktui_draw_text(2, top + 2, w - 4,
			       "nothing found — a network printer must be on and "
			       "advertising",
			       KT_MID, KT_BG, KT_A_NONE);
	} else if (page == PG_PRINTERS) {
		ktui_table_draw(krect(2, top + 1, w - 4, body), &tprn, nprn,
				PRN_COL, PRN_NCOL, prn_cell, NULL, NULL, -1);
	} else {
		ktui_table_draw(krect(2, top + 1, w - 4, body), &tfnd, nfnd,
				FND_COL, FND_NCOL, fnd_cell, NULL, NULL, -1);
	}

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_BG);

	struct kch_button b[PB_N];
	int have_prn = page == PG_PRINTERS && nprn > 0;

	b[PB_ADD] = (struct kch_button){ "Add", page == PG_FOUND && nfnd > 0 };
	b[PB_DEFAULT] = (struct kch_button){ "Default", have_prn };
	b[PB_REMOVE] = (struct kch_button){ "Remove", have_prn };
	b[PB_REFRESH] = (struct kch_button){ "Refresh", 1 };
	b[PB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, PB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room, status, KT_MID, KT_BG,
			       KT_A_NONE);

	/*
	 * `d` AND `x` ARE DELIBERATELY NOT PUSHED, and the Default and Remove
	 * buttons name those two actions instead. The row stops at the first
	 * hint that does not fit and drops the rest from the TAIL, so at
	 * fifty-six columns a fuller row loses the Esc hint — the one that
	 * must always be there. Widening the window is the fix if they are
	 * wanted here; never reordering Esc.
	 */
	ktui_hint("Left/Right", "page");
	ktui_hint_if(page == PG_FOUND && nfnd > 0, "Enter", "add");
	ktui_hint("r", "refresh");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
}

static int on_key(int k)
{
	if (ktui_tabs_key(&page, PG_N, 0, k))
		return 0;
	if (ktui_table_key(cur_table(), cur_count(), ktui_h - 8, k, NULL, NULL))
		return 0;

	switch (k) {
	case KT_K_ENTER:
		if (page == PG_FOUND)
			do_add();
		break;
	case 'd':
		if (page == PG_PRINTERS)
			do_named("-d", "default is");
		break;
	case 'x':
		if (page == PG_PRINTERS)
			do_named("-x", "removed");
		break;
	case 'r':
		refresh();
		break;
	case 'q':
		return 1;
	}
	return 0;
}

int print_main(int argc, char **argv)
{
	const char *font = NULL;
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
		else if (!strcmp(argv[i], "--found"))
			page = PG_FOUND;
		else if (!strcmp(argv[i], "--fixture") && i + 1 < argc)
			fixture = argv[++i];
		else {
			fprintf(stderr, "usage: kdos-print [--font NAME] "
					"[--no-icons] [--found] "
					"[--fixture DIR] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = PR_COLS,
		.rows = PR_ROWS,
		.min_cols = 56,
		.min_rows = 12,
		.title = "Printers",
		.app_id = "kdos-print",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(PR_COLS, PR_ROWS);
		ktui_draw_init();
		refresh();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-print: no display server\n");
		return 1;
	}
	ktui_draw_init();
	refresh();

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (bi == PB_CLOSE)
				break;
			switch (bi) {
			case PB_ADD:
				do_add();
				continue;
			case PB_DEFAULT:
				do_named("-d", "default is");
				continue;
			case PB_REMOVE:
				do_named("-x", "removed");
				continue;
			case PB_REFRESH:
				refresh();
				continue;
			}

			int t = ktui_tabs_hit(krect(2, 4, ktui_w - 4, 1), PAGES,
					      PG_N, 0, ev.mx, ev.my);

			if (t >= 0) {
				page = t;
				continue;
			}
			int idx = ktui_table_hit(krect(2, 5, ktui_w - 4,
						       ktui_h - 9),
						 cur_table(), cur_count(),
						 page == PG_PRINTERS ? PRN_NCOL
								     : FND_NCOL,
						 page == PG_PRINTERS ? PRN_COL
								     : FND_COL,
						 ev.mx, ev.my);
			ktui_table_pick(cur_table(), cur_count(), idx, NULL,
					NULL);
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;
		if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
			break;
		if (on_key(ev.key))
			break;
	}

	kdisp_shutdown();
	return 0;
}

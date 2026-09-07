/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-firewall — which of this machine's services answer the network
 *
 *   ┌ Firewall ────────────────────────────────────────────┐
 *   │ SERVICE   STATE   WHAT                               │
 *   │ ssh       open    incoming SSH (70_sshd.sh)          │
 *   │ ipp       closed  sharing a printer with CUPS        │
 *   ├──────────────────────────────────────────────────────┤
 *   │ default: anything this machine started comes back     │
 *   └──────────────────────────────────────────────────────┘
 *
 * NAMED SERVICES ONLY, AND THE NAMES ARE NOT HERE. `kdos-powerd` owns the
 * table of what a name means and this surface asks for it — a client that
 * could name a port could open any port, and a second copy of the table would
 * be a second answer to what `ssh` is. `kdos-power firewall list` is the whole
 * of what this program knows.
 *
 * IT EDITS ONE FILE AND ONLY ONE. `/etc/nftables.d/50-kdos-services.nft` is
 * rewritten whole by the daemon on every toggle; anything hand-written belongs
 * in another file beside it, which nothing here reads or touches. That is said
 * on the surface as well as in the file, because a person who hand-edited the
 * wrong one would lose it on the next click.
 *
 * IT IS NOT A FIREWALL EDITOR. The default ruleset is a workstation's — what
 * this machine started comes back, loopback is trusted, ICMP is answered,
 * everything else arriving unasked is dropped — and the only question this
 * surface answers is which of a short list of services may be reached from
 * outside. A rule this table cannot express is an edit to `/etc/nftables.conf`,
 * and the surface says so rather than growing a syntax.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define FW_COLS 68
#define FW_ROWS 16
#define FW_MAX 32

enum { FB_TOGGLE, FB_REFRESH, FB_CLOSE, FB_N };

struct svc {
	char name[32];
	char what[96];
	int on;
};

static KtuiKeys keys;
static struct svc svc[FW_MAX];
static int nsvc;
static char why[128];
static char status[160];
static KtuiTable tbl;
static int icons_on = 1;

static void refresh(void)
{
	char buf[4096];
	KbArgv a = { 0 };
	const char *fix = getenv("KDOS_FIREWALL_LIST");

	nsvc = 0;
	why[0] = '\0';
	if (fix && *fix) {
		kb_read_file(fix, buf, sizeof(buf));
	} else {
		kb_argv_add(&a, "kdos-power");
		kb_argv_add(&a, "firewall");
		kb_argv_add(&a, "list");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}
	if (!buf[0]) {
		snprintf(why, sizeof(why),
			 "kdos-powerd is not running (service start 55_powerd)");
		return;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp);
	     ln && nsvc < FW_MAX; ln = strtok_r(NULL, "\n", &sp)) {
		char st[16];

		if (!strcmp(ln, "ok"))
			break;
		if (!strncmp(ln, "err ", 4)) {
			snprintf(why, sizeof(why), "%s", ln + 4);
			return;
		}
		if (sscanf(ln, "%31[^\t]\t%15[^\t]\t%95[^\n]", svc[nsvc].name,
			   st, svc[nsvc].what) != 3)
			continue;
		svc[nsvc].on = !strcmp(st, "on");
		nsvc++;
	}
	ktui_table_clamp(&tbl, nsvc, ktui_h > 8 ? ktui_h - 8 : 1);
}

static void toggle(void)
{
	char buf[512];
	KbArgv a = { 0 };

	if (tbl.sel < 0 || tbl.sel >= nsvc)
		return;
	kb_argv_add(&a, "kdos-power");
	kb_argv_add(&a, "firewall");
	kb_argv_add(&a, svc[tbl.sel].name);
	kb_argv_add(&a, svc[tbl.sel].on ? "off" : "on");
	kb_argv_end(&a);

	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || strncmp(buf, "ok", 2))
		snprintf(status, sizeof(status), "%s",
			 buf[0] ? buf : "kdos-powerd refused it");
	else
		snprintf(status, sizeof(status), "%s", buf);
	refresh();
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol FW_COL[] = {
	{ "SERVICE", 12 }, { "STATE", 10 }, { "WHAT", 0 }
};
#define FW_NCOL 3

static void fw_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct svc *v = &svc[idx];
	int on = bg == KT_ACCENT;

	(void)user;
	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, v->name, fg, bg, KT_A_NONE);
		break;
	case 1:
		/* OPEN IS THE WARNING COLOUR, not the accent. A port that
		 * answers the network is the state worth noticing, and the
		 * accent here would read as "this one is fine". */
		ktui_draw_text(x, y, w, v->on ? "open" : "closed",
			       on	 ? KT_SURFACE
			       : v->on	 ? KT_WARN
					 : KT_DIM,
			       bg, KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, v->what, on ? KT_SURFACE : KT_MID, bg,
			       KT_A_NONE);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	const struct svc *v = tbl.sel >= 0 && tbl.sel < nsvc ? &svc[tbl.sel]
							     : NULL;
	int top, body;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Firewall", KT_ACCENT, KT_BG, 1);
	top = kch_header(w, "security-high", "Firewall",
			 "which of this machine's services answer the network",
			 icons_on);

	body = h - top - 6;
	if (body < 1)
		body = 1;

	if (why[0])
		ktui_draw_text(2, top, w - 4, why, KT_ERR, KT_BG, KT_A_NONE);
	else if (!nsvc)
		ktui_draw_text(2, top, w - 4, "no service table", KT_MID, KT_BG,
			       KT_A_NONE);
	else
		ktui_table_draw(krect(2, top, w - 4, body), &tbl, nsvc, FW_COL,
				FW_NCOL, fw_cell, NULL, NULL, -1);

	ktui_draw_hline(1, h - 5, w - 2, KT_G_HL, KT_DIM, KT_BG);
	/*
	 * THE DEFAULT IS ON THE SCREEN. Every row here is an EXCEPTION to it,
	 * and a list of exceptions with the rule missing reads as the whole
	 * policy — somebody would take an empty list for "nothing is
	 * protected" rather than "nothing is opened".
	 */
	/* It has to FIT at min_cols, which is 56: the key column is `w - 4`,
	 * and a sentence that truncates says something other than what it
	 * means — "anything unasked is dr" is not a policy. */
	ktui_draw_text(2, h - 4, w - 4,
		       "otherwise: replies and ping get in, nothing else",
		       KT_DIM, KT_BG, KT_A_NONE);

	struct kch_button b[FB_N];

	b[FB_TOGGLE] = (struct kch_button){ v && v->on ? "Close" : "Open",
					    v != NULL };
	b[FB_REFRESH] = (struct kch_button){ "Refresh", 1 };
	b[FB_CLOSE] = (struct kch_button){ "Done", 1 };

	int bx = kch_buttons(w, h - 2, b, FB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room, status, KT_MID, KT_BG,
			       KT_A_NONE);

	ktui_hint_if(v != NULL, "Enter", v && v->on ? "close" : "open");
	ktui_hint("r", "refresh");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
}

static int on_key(int k)
{
	if (ktui_table_key(&tbl, nsvc, ktui_h - 8, k, NULL, NULL))
		return 0;
	switch (k) {
	case KT_K_ENTER:
	case ' ':
		toggle();
		break;
	case 'r':
		refresh();
		break;
	case 'q':
		return 1;
	}
	return 0;
}

int firewall_main(int argc, char **argv)
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
		else {
			fprintf(stderr, "usage: kdos-firewall [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = FW_COLS,
		.rows = FW_ROWS,
		.min_cols = 56,
		.min_rows = 12,
		.title = "Firewall",
		.app_id = "kdos-firewall",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	refresh();
	if (dump) {
		ktui_offscreen_init(FW_COLS, FW_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-firewall: no display server\n");
		return 1;
	}
	ktui_draw_init();

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

			if (bi == FB_CLOSE)
				break;
			if (bi == FB_TOGGLE) {
				toggle();
				continue;
			}
			if (bi == FB_REFRESH) {
				refresh();
				continue;
			}
			int idx = ktui_table_hit(krect(2, 4, ktui_w - 4,
						       ktui_h - 10),
						 &tbl, nsvc, FW_NCOL, FW_COL,
						 ev.mx, ev.my);
			ktui_table_pick(&tbl, nsvc, idx, NULL, NULL);
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

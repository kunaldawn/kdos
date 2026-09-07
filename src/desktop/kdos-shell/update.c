/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-update — what is behind, what is vulnerable, and which slot is live
 *
 *   ┌ Updates ─────────────────────────────────────────────────────────┐
 *   │ Behind │ Security                                                │
 *   ├──────────────────────────────────────────────────────────────────┤
 *   │ aerc          0.22.0        → 0.23.0-1                           │
 *   │ curl          8.11.0        → 8.12.1-1                           │
 *   ├──────────────────────────────────────────────────────────────────┤
 *   │ one root, no rollback     binhost: signed index present          │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * IT COMPUTES NOTHING. `kdos update check --json`, `kdos cve --json` and
 * `kdos-bootctl status` already answer these three questions, and a surface
 * that re-derived any of them would be a second answer that drifts — the
 * version comparison in particular is the packaging system's and is subtle.
 *
 * IT ALSO APPLIES NOTHING. `kdos update apply` compiles packages, can take
 * hours, and on an A/B machine writes the OTHER slot; a button that started it
 * behind a surface with one status line would be a progress bar over an
 * unattended build with no way to see what it was doing. The surface says what
 * to type and where it will happen.
 *
 * THE SECURITY LIST IS OFFLINE AND SAYS HOW OLD IT IS. `kdos cve` reads a
 * vendored table with a generation date; a database three months stale
 * reporting "nothing to fix" is worse than no answer, so the age is on the
 * screen beside the count rather than in the command's own output only.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define UP_COLS 76
#define UP_ROWS 20
#define UP_MAX 256
#define UP_NAME 64

enum { PG_BEHIND, PG_CVE, PG_N };
enum { UB_REFRESH, UB_CLOSE, UB_N };

static const KtuiTab PAGES[PG_N] = { { "Behind", NULL }, { "Security", NULL } };

struct row {
	char name[UP_NAME];
	char have[48];
	char want[48];
	char note[64];		/* the CVE ids, on the security page */
};

static KtuiKeys keys;
static struct row behind[UP_MAX];
static int nbehind;
static struct row cve[UP_MAX];
static int ncve;
static char binhost[128];
static char slot[160];
static char secage[80];
static char why[128];
static int page;
static KtuiTable tbehind, tcve;
static int icons_on = 1;

static KtuiTable *cur_table(void) { return page == PG_BEHIND ? &tbehind : &tcve; }
static int cur_count(void) { return page == PG_BEHIND ? nbehind : ncve; }

/* ── reading ───────────────────────────────────────────────────────────── */

/*
 * A STRING FIELD OUT OF ONE JSON OBJECT, by key, without a parser.
 *
 * These two producers are in this tree and their shape is fixed; a JSON
 * library in the panel binary to read two documents it also writes would be a
 * dependency bought for nothing. The scan is deliberately dumb and it is
 * bounded: it finds `"key":` and copies what follows up to the closing quote,
 * so a value containing an escaped quote would be cut short — and neither
 * producer emits one, because every field here is a package name, a version
 * or a path.
 */
static const char *jfind(const char *p, const char *key, char *out, size_t n)
{
	char pat[64];
	const char *q;

	out[0] = '\0';
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	q = strstr(p, pat);
	if (!q)
		return NULL;
	q += strlen(pat);
	while (*q == ' ')
		q++;
	if (*q == '"') {
		size_t k = 0;

		for (q++; *q && *q != '"' && k + 1 < n; q++)
			out[k++] = *q;
		out[k] = '\0';
		return *q ? q + 1 : q;
	}
	/* A bare number or `null`, copied up to the next delimiter. */
	size_t k = 0;

	while (*q && *q != ',' && *q != '}' && *q != ']' && k + 1 < n)
		out[k++] = *q++;
	out[k] = '\0';
	return q;
}

static void scan_behind(void)
{
	static char buf[65536];
	char v[128];
	KbArgv a = { 0 };
	const char *fix = getenv("KDOS_UPDATE_JSON");

	nbehind = 0;
	binhost[0] = why[0] = '\0';
	if (fix && *fix) {
		kb_read_file(fix, buf, sizeof(buf));
	} else {
		kb_argv_add(&a, "kdos");
		kb_argv_add(&a, "update");
		kb_argv_add(&a, "check");
		kb_argv_add(&a, "--json");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}
	if (!buf[0] || buf[0] != '{') {
		snprintf(why, sizeof(why),
			 "`kdos update check` answered nothing — is there a "
			 "ports tree on this machine?");
		return;
	}
	if (jfind(buf, "error", v, sizeof(v))) {
		snprintf(why, sizeof(why), "%s", v);
		return;
	}

	jfind(buf, "binhost", v, sizeof(v));
	if (!v[0] || !strcmp(v, "null")) {
		snprintf(binhost, sizeof(binhost),
			 "no binhost — an apply would compile everything");
	} else {
		char sg[16];

		jfind(buf, "signed", sg, sizeof(sg));
		snprintf(binhost, sizeof(binhost), "binhost %s — %s", v,
			 !strcmp(sg, "true") ? "signed index"
					     : "UNSIGNED, an apply will refuse it");
	}

	const char *p = strstr(buf, "\"packages\":");

	if (!p)
		return;
	while (nbehind < UP_MAX && (p = strchr(p, '{')) != NULL) {
		struct row *r = &behind[nbehind];

		memset(r, 0, sizeof(*r));
		if (!jfind(p, "name", r->name, sizeof(r->name)))
			break;
		jfind(p, "have", r->have, sizeof(r->have));
		jfind(p, "want", r->want, sizeof(r->want));
		nbehind++;
		p = strchr(p, '}');
		if (!p)
			break;
	}
}

static void scan_cve(void)
{
	static char buf[262144];
	KbArgv a = { 0 };
	const char *fix = getenv("KDOS_CVE_JSON");

	ncve = 0;
	secage[0] = '\0';
	if (fix && *fix) {
		kb_read_file(fix, buf, sizeof(buf));
	} else {
		kb_argv_add(&a, "kdos");
		kb_argv_add(&a, "cve");
		kb_argv_add(&a, "--json");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}
	if (!buf[0] || buf[0] != '{') {
		snprintf(secage, sizeof(secage), "%s",
			 "no security database — this check is offline");
		return;
	}

	char gen[32], age[16], unk[16];

	jfind(buf, "generated", gen, sizeof(gen));
	jfind(buf, "age_days", age, sizeof(age));
	jfind(buf, "unknown", unk, sizeof(unk));
	/* THE AGE IS ON THE SCREEN, not in the command's output only: a table
	 * three months stale reporting nothing to fix is worse than no answer,
	 * and so is a scope that could not check six hundred packages. */
	snprintf(secage, sizeof(secage), "table %s, %s days old, %s unchecked",
		 gen[0] ? gen : "?", age[0] ? age : "?", unk[0] ? unk : "?");

	const char *p = strstr(buf, "\"findings\":");

	if (!p)
		return;
	while (ncve < UP_MAX && (p = strchr(p, '{')) != NULL) {
		struct row *r = &cve[ncve];

		memset(r, 0, sizeof(*r));
		if (!jfind(p, "port", r->name, sizeof(r->name)))
			break;
		jfind(p, "version", r->have, sizeof(r->have));
		jfind(p, "fixed_in", r->want, sizeof(r->want));
		jfind(p, "cves", r->note, sizeof(r->note));
		ncve++;
		p = strchr(p, '}');
		if (!p)
			break;
	}
}

static void scan_slot(void)
{
	char buf[1024];
	KbArgv a = { 0 };
	const char *fix = getenv("KDOS_SLOT_TEXT");

	slot[0] = '\0';
	if (fix && *fix) {
		kb_read_file(fix, buf, sizeof(buf));
	} else {
		kb_argv_add(&a, "kdos-bootctl");
		kb_argv_add(&a, "status");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}
	buf[strcspn(buf, "\r\n")] = '\0';
	snprintf(slot, sizeof(slot), "%s", buf[0] ? buf : "no boot state");
}

static void refresh(void)
{
	scan_behind();
	scan_cve();
	scan_slot();
	ktui_table_clamp(&tbehind, nbehind, ktui_h > 9 ? ktui_h - 9 : 1);
	ktui_table_clamp(&tcve, ncve, ktui_h > 9 ? ktui_h - 9 : 1);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol BE_COL[] = {
	{ "PACKAGE", 0 }, { "INSTALLED", 16 }, { "PINNED", 18 }
};
#define BE_NCOL 3

static const KtuiCol CV_COL[] = {
	{ "PACKAGE", 0 }, { "INSTALLED", 14 }, { "FIXED IN", 14 },
	{ "CVE", 24 }
};
#define CV_NCOL 4

static void row_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		     void *user)
{
	const struct row *r = user ? &cve[idx] : &behind[idx];
	int on = bg == KT_ACCENT;

	switch (col) {
	case 0:
		ktui_draw_text(x, y, w, r->name, fg, bg, KT_A_NONE);
		break;
	case 1:
		ktui_draw_text(x, y, w, r->have, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
		break;
	case 2:
		ktui_draw_text(x, y, w, r->want,
			       on ? KT_SURFACE : user ? KT_ERR : KT_ACCENT, bg,
			       KT_A_NONE);
		break;
	default:
		ktui_draw_text(x, y, w, r->note, on ? KT_SURFACE : KT_WARN, bg,
			       KT_A_NONE);
		break;
	}
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	int top, body;
	char sub[128];

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Updates", KT_ACCENT, KT_BG, 1);

	snprintf(sub, sizeof(sub), "%d behind, %d with a known vulnerability",
		 nbehind, ncve);
	top = kch_header(w, "system-software-update", "Updates", sub, icons_on);

	ktui_tabs_draw(krect(2, top, w - 4, 1), PAGES, PG_N, page, -1, 0);
	/*
	 * FIVE ROWS BELOW THE LIST: a rule, the page's own footnote, the hint
	 * row and the button bar. The hint row sits where it sits on every
	 * other surface here — a person's eye goes to the same line for the
	 * keys whichever window is open, and that is worth more than any
	 * ordering this surface could invent for itself.
	 */
	body = h - top - 6;
	if (body < 1)
		body = 1;

	if (page == PG_BEHIND && why[0]) {
		ktui_draw_text(2, top + 2, w - 4, why, KT_ERR, KT_BG,
			       KT_A_NONE);
	} else if (page == PG_BEHIND && !nbehind) {
		ktui_draw_text(2, top + 2, w - 4,
			       "everything the ports tree pins is installed",
			       KT_MID, KT_BG, KT_A_NONE);
	} else if (page == PG_CVE && !ncve) {
		ktui_draw_text(2, top + 2, w - 4,
			       "nothing installed matches the security table",
			       KT_MID, KT_BG, KT_A_NONE);
	} else if (page == PG_BEHIND) {
		ktui_table_draw(krect(2, top + 1, w - 4, body), &tbehind,
				nbehind, BE_COL, BE_NCOL, row_cell, NULL, NULL,
				-1);
	} else {
		/* `user` non-NULL selects the security table in the shared
		 * painter: the two pages differ in their source and in one
		 * colour, which is not two painters' worth of difference. */
		ktui_table_draw(krect(2, top + 1, w - 4, body), &tcve, ncve,
				CV_COL, CV_NCOL, row_cell, NULL, cve, -1);
	}

	ktui_draw_hline(1, h - 5, w - 2, KT_G_HL, KT_DIM, KT_BG);
	ktui_draw_text(2, h - 4, w - 4, page == PG_CVE ? secage : binhost,
		       KT_MID, KT_BG, KT_A_NONE);

	struct kch_button b[UB_N];

	b[UB_REFRESH] = (struct kch_button){ "Refresh", 1 };
	b[UB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, UB_N, -1);
	int room = bx - 3;

	/*
	 * THE SLOT IS ON THE SCREEN AND `apply` IS NOT A BUTTON. An apply
	 * compiles packages, can take hours and on an A/B machine writes the
	 * OTHER slot; what a person needs before starting one is to know where
	 * it will land.
	 */
	if (room > 0)
		ktui_draw_text(2, h - 2, room, slot, KT_DIM, KT_BG, KT_A_NONE);

	ktui_hint("Left/Right", "page");
	ktui_hint("r", "refresh");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
}

static int on_key(int k)
{
	if (ktui_tabs_key(&page, PG_N, 0, k))
		return 0;
	if (ktui_table_key(cur_table(), cur_count(), ktui_h - 9, k, NULL, NULL))
		return 0;
	if (k == 'r')
		refresh();
	else if (k == 'q')
		return 1;
	return 0;
}

int update_main(int argc, char **argv)
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
		else if (!strcmp(argv[i], "--security"))
			page = PG_CVE;
		else {
			fprintf(stderr, "usage: kdos-update [--font NAME] "
					"[--no-icons] [--security] [--dump]\n");
			return 2;
		}
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = UP_COLS,
		.rows = UP_ROWS,
		.min_cols = 56,
		.min_rows = 14,
		.title = "Updates",
		.app_id = "kdos-update",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	refresh();
	if (dump) {
		ktui_offscreen_init(UP_COLS, UP_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-update: no display server\n");
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

			if (bi == UB_CLOSE)
				break;
			if (bi == UB_REFRESH) {
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
						       ktui_h - 10),
						 cur_table(), cur_count(),
						 page == PG_BEHIND ? BE_NCOL
								   : CV_NCOL,
						 page == PG_BEHIND ? BE_COL
								   : CV_COL,
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

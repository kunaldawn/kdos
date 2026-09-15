/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-time — the zone, the clock, and whether the clock is right
 *
 *   ┌ Time ────────────────────────────────────────────────┐
 *   │ 2025-01-01 12:00:00   Europe/London                  │
 *   │ chrony: synchronised, 0.4 ms from time.example.net   │
 *   ├──────────────────────────────────────────────────────┤
 *   │ eur    Europe/London                                 │
 *   │ eur    Europe/Paris                                  │
 *   └──────────────────────────────────────────────────────┘
 *
 * THE ZONE LIST IS `zone1970.tab`, READ, NOT A TABLE IN THIS FILE. tzdata
 * ships here and carries the canonical list; a hand-written one goes stale the
 * first time a country changes its rules, and it went stale in the installer
 * before this was written.
 *
 * SETTING IT IS A `kdos-powerd` VERB, because `/etc/localtime` and
 * `/etc/profile.d/20-timezone.sh` are root's and the person setting a zone is
 * the one administering the machine — which is what `wheel` already means. A
 * setuid helper for one write would be a worse answer to a question that
 * daemon already answers.
 *
 * BOTH HALVES OR NEITHER, and the daemon enforces it: `/etc/localtime` is what
 * a program reading the zoneinfo tree follows, and `TZ` in the profile is what
 * musl reads when it is set — which it is, on every KDOS login. Writing only
 * the symlink leaves `date` reporting the old zone in every shell that had
 * already sourced the profile, which reads as the setting having done nothing.
 *
 * `chronyc tracking` IS READ AND NEVER DRIVEN. Whether to step the clock, how
 * far and how fast is chrony's decision and a good one; this surface reports
 * what it decided. A "sync now" button would be `chronyc makestep`, which is
 * the wrong thing to offer beside a clock that is already being disciplined.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define TZ_COLS 72
#define TZ_ROWS 20
#define TZ_MAX 600
#define TZ_NAME 64
#define TZ_QUERY 32

enum { TB_SET, TB_REFRESH, TB_CLOSE, TB_N };

struct zone {
	char name[TZ_NAME];
	char cc[40];		/* the country codes column, as it stands */
};

static KtuiKeys keys;
static struct zone zones[TZ_MAX];
static int nzone;
static int hit[TZ_MAX];		/* indices into zones[], the current filter */
static int nhit;
static char query[TZ_QUERY];
static int caret;
static char current[TZ_NAME] = "UTC";
static char chrony[160];
static char status[160];
static KtuiTable tbl;
static int icons_on = 1;

static const char *zonedir(void)
{
	const char *d = getenv("KDOS_ZONEINFO");

	return d && *d ? d : "/usr/share/zoneinfo";
}

/* ── reading ───────────────────────────────────────────────────────────── */

static void load_zones(void)
{
	char path[512], line[512];
	FILE *f;

	nzone = 0;
	snprintf(path, sizeof(path), "%s/zone1970.tab", zonedir());
	f = fopen(path, "r");
	if (!f)
		return;
	while (nzone < TZ_MAX && fgets(line, sizeof(line), f)) {
		struct zone *z = &zones[nzone];

		if (line[0] == '#')
			continue;
		/* `codes<TAB>coords<TAB>name[<TAB>comment]` — the name is the
		 * third field and the comment, where there is one, is not part
		 * of it. */
		if (sscanf(line, "%39[^\t]\t%*[^\t]\t%63[^\t\n]", z->cc,
			   z->name) != 2)
			continue;
		nzone++;
	}
	fclose(f);
}

/* Where `/etc/localtime` points, which is what every program but musl reads. */
static void read_current(void)
{
	char link[512];
	const char *etc = getenv("KDOS_ETC");
	char path[512];
	ssize_t n;

	snprintf(path, sizeof(path), "%s/localtime",
		 etc && *etc ? etc : "/etc");
	n = readlink(path, link, sizeof(link) - 1);
	if (n <= 0) {
		snprintf(current, sizeof(current), "%s", "UTC");
		return;
	}
	link[n] = '\0';

	/* The tail after `zoneinfo/`, which is the zone's own name. A path
	 * that is not under the tree at all is reported whole rather than
	 * guessed at. */
	const char *p = strstr(link, "zoneinfo/");

	snprintf(current, sizeof(current), "%s", p ? p + 9 : link);
}

/*
 * `chronyc tracking`, reduced to one line. Its output is fourteen labelled
 * rows and what a person wants beside a clock is whether it is right and by
 * how much — the rest is chrony's own diagnostics and belongs in `chronyc`.
 */
static void read_chrony(void)
{
	char buf[4096], ref[96] = "", offset[64] = "", stratum[16] = "";
	KbArgv a = { 0 };
	const char *fix = getenv("KDOS_CHRONY");

	chrony[0] = '\0';
	if (fix && *fix) {
		kb_read_file(fix, buf, sizeof(buf));
	} else {
		kb_argv_add(&a, "chronyc");
		kb_argv_add(&a, "tracking");
		kb_argv_end(&a);
		kb_run_capture(&a, buf, sizeof(buf));
	}
	if (!buf[0] || strstr(buf, "Cannot talk to daemon")) {
		snprintf(chrony, sizeof(chrony),
			 "chronyd is not running (service start 35_chrony)");
		return;
	}
	for (char *sp = NULL, *ln = strtok_r(buf, "\n", &sp); ln;
	     ln = strtok_r(NULL, "\n", &sp)) {
		const char *v = strchr(ln, ':');

		if (!v)
			continue;
		v++;
		while (*v == ' ')
			v++;
		if (!strncmp(ln, "Reference ID", 12))
			snprintf(ref, sizeof(ref), "%s", v);
		else if (!strncmp(ln, "Stratum", 7))
			snprintf(stratum, sizeof(stratum), "%s", v);
		else if (!strncmp(ln, "System time", 11))
			snprintf(offset, sizeof(offset), "%s", v);
	}
	if (!ref[0]) {
		snprintf(chrony, sizeof(chrony), "%s",
			 "chrony answered nothing this surface understands");
		return;
	}
	/* Stratum 0 is chrony's own "not synchronised yet", and reporting an
	 * offset from a source it has not agreed with would be a number that
	 * means nothing. */
	if (!strcmp(stratum, "0"))
		snprintf(chrony, sizeof(chrony), "chrony: not synchronised yet");
	else
		snprintf(chrony, sizeof(chrony), "chrony: %s, %s", ref,
			 offset[0] ? offset : "no offset reported");
}

static void filter(void)
{
	char up[TZ_QUERY];
	size_t qn = strlen(query);

	nhit = 0;
	for (size_t i = 0; i < qn; i++)
		up[i] = query[i] >= 'A' && query[i] <= 'Z'
				? (char)(query[i] - 'A' + 'a')
				: query[i];
	up[qn] = '\0';

	for (int i = 0; i < nzone; i++) {
		if (qn) {
			char low[TZ_NAME];
			int k = 0;

			for (; zones[i].name[k] && k < TZ_NAME - 1; k++)
				low[k] = zones[i].name[k] >= 'A' &&
						 zones[i].name[k] <= 'Z'
						 ? (char)(zones[i].name[k] -
							  'A' + 'a')
						 : zones[i].name[k];
			low[k] = '\0';
			if (!strstr(low, up))
				continue;
		}
		hit[nhit++] = i;
	}
	tbl.sel = 0;
	tbl.top = 0;
}

static void refresh(void)
{
	read_current();
	read_chrony();
	filter();
	ktui_table_clamp(&tbl, nhit, ktui_h > 9 ? ktui_h - 9 : 1);
}

/* ── setting ───────────────────────────────────────────────────────────── */

/*
 * `kdos-power timezone <zone>` — the same client every other power verb goes
 * through, so there is one socket and one authorisation rule rather than a
 * second of each for one write.
 */
static void do_set(void)
{
	char buf[256];
	KbArgv a = { 0 };

	if (tbl.sel < 0 || tbl.sel >= nhit)
		return;

	const char *z = zones[hit[tbl.sel]].name;

	kb_argv_add(&a, "kdos-power");
	kb_argv_add(&a, "timezone");
	kb_argv_add(&a, z);
	kb_argv_end(&a);

	if (kb_run_capture(&a, buf, sizeof(buf)) != 0 || strncmp(buf, "ok", 2)) {
		snprintf(status, sizeof(status), "%s",
			 buf[0] ? buf : "kdos-powerd refused it");
		return;
	}
	/*
	 * THIS PROCESS'S OWN IDEA OF THE ZONE IS RESET TOO. `tzset()` caches
	 * what `TZ` said at the first call, so a clock drawn after the change
	 * would go on showing the old zone until the surface was restarted —
	 * which reads as the setting having failed.
	 */
	setenv("TZ", ":/etc/localtime", 1);
	tzset();
	snprintf(status, sizeof(status), "timezone is %s", z);
	refresh();
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static const KtuiCol TZ_COL[] = { { "WHERE", 10 }, { "ZONE", 0 } };
#define TZ_NCOL 2

static void tz_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		    void *user)
{
	const struct zone *z = &zones[hit[idx]];
	int on = bg == KT_ACCENT;

	(void)user;
	if (col == 0)
		ktui_draw_text(x, y, w, z->cc, on ? KT_SURFACE : KT_DIM, bg,
			       KT_A_NONE);
	else
		ktui_draw_text(x, y, w, z->name,
			       on		 ? KT_SURFACE
			       : !strcmp(z->name, current) ? KT_ACCENT
							   : fg,
			       bg, KT_A_NONE);
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;
	char clock[64];
	time_t now = time(NULL);
	struct tm tmv;
	int top, body;

	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, "Time", KT_ACCENT, KT_BG, 1);
	top = kch_header(w, "preferences-system-time", "Time",
			 "the zone this machine keeps, and whether it is right",
			 icons_on);

	/*
	 * `localtime_r` AND NOT `localtime`, on a surface that also calls
	 * `tzset()`: the static one hands back a buffer the next call
	 * overwrites, and the clock and the date would then be from two
	 * different reads.
	 */
	if (localtime_r(&now, &tmv))
		strftime(clock, sizeof(clock), "%Y-%m-%d %H:%M:%S", &tmv);
	else
		snprintf(clock, sizeof(clock), "%s", "--");

	ktui_draw_textf(2, top, w - 4, KT_TEXT, KT_BG, KT_A_NONE, "%s   %s",
			clock, current);
	ktui_draw_text(2, top + 1, w - 4, chrony,
		       strstr(chrony, "not running") ||
			       strstr(chrony, "not synchronised")
			       ? KT_WARN
			       : KT_MID,
		       KT_BG, KT_A_NONE);

	ktui_draw_text(2, top + 2, w - 4,
		       query[0] ? query : "type to narrow the list",
		       query[0] ? KT_TEXT : KT_DIM, KT_BG, KT_A_NONE);
	ktui_draw_hline(1, top + 3, w - 2, KT_G_HL, KT_DIM, KT_BG);

	body = h - top - 7;
	if (body < 1)
		body = 1;
	if (!nzone)
		ktui_draw_text(2, top + 4, w - 4,
			       "no zoneinfo tree on this image", KT_ERR, KT_BG,
			       KT_A_NONE);
	else if (!nhit)
		ktui_draw_text(2, top + 4, w - 4, "no zone has that in its name",
			       KT_MID, KT_BG, KT_A_NONE);
	else
		ktui_table_draw(krect(2, top + 4, w - 4, body), &tbl, nhit,
				TZ_COL, TZ_NCOL, tz_cell, NULL, NULL, -1);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_BG);

	struct kch_button b[TB_N];

	b[TB_SET] = (struct kch_button){ "Set", nhit > 0 };
	b[TB_REFRESH] = (struct kch_button){ "Refresh", 1 };
	b[TB_CLOSE] = (struct kch_button){ "Close", 1 };

	int bx = kch_buttons(w, h - 2, b, TB_N, -1);
	int room = bx - 3;

	if (status[0] && room > 0)
		ktui_draw_text(2, h - 2, room, status, KT_MID, KT_BG,
			       KT_A_NONE);

	ktui_hint_if(nhit > 0, "Enter", "set");
	ktui_hint("r", "refresh");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_BG);
	ktui_term_caret(2 + caret, top + 2);
}

static int on_key(int k)
{
	int qn = (int)strlen(query);

	/* The four list keys are the list's; every printable character is the
	 * filter's, which is what makes this a search and not a menu. */
	if (k == KT_K_UP || k == KT_K_DOWN || k == KT_K_PGUP ||
	    k == KT_K_PGDN) {
		ktui_table_key(&tbl, nhit, ktui_h - 9, k, NULL, NULL);
		return 0;
	}
	switch (k) {
	case KT_K_ENTER:
		do_set();
		return 0;
	case KT_K_BACKSPACE:
		if (caret > 0) {
			memmove(query + caret - 1, query + caret,
				(size_t)(qn - caret) + 1);
			caret--;
			filter();
		}
		return 0;
	case KT_K_LEFT:
		if (caret > 0)
			caret--;
		return 0;
	case KT_K_RIGHT:
		if (caret < qn)
			caret++;
		return 0;
	case KT_K_HOME:
		caret = 0;
		return 0;
	case KT_K_END:
		caret = qn;
		return 0;
	}
	if (k >= 0x20 && k < 0x7f && qn + 1 < TZ_QUERY) {
		memmove(query + caret + 1, query + caret,
			(size_t)(qn - caret) + 1);
		query[caret++] = (char)k;
		filter();
	}
	return 0;
}

int timezone_main(int argc, char **argv)
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
		else if (argv[i][0] != '-')
			snprintf(query, sizeof(query), "%s", argv[i]);
		else {
			fprintf(stderr, "usage: kdos-time [--font NAME] "
					"[--no-icons] [--dump] [QUERY]\n");
			return 2;
		}
	}
	caret = (int)strlen(query);

	load_zones();
	refresh();

	KDispConfig cfg = {
		.role = KDISP_ROLE_TOPLEVEL,
		.cols = TZ_COLS,
		.rows = TZ_ROWS,
		.min_cols = 56,
		.min_rows = 14,
		.title = "Time",
		.app_id = "kdos-time",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (dump) {
		ktui_offscreen_init(TZ_COLS, TZ_ROWS);
		ktui_draw_init();
		draw();
		ktui_draw_dump();
		return 0;
	}
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-time: no display server\n");
		return 1;
	}
	ktui_draw_init();

	while (!kdisp_should_close()) {
		draw();
		ktui_draw_flush();

		KtuiEvent ev;

		/* One second, because there is a CLOCK on this surface: a
		 * longer timeout is a clock that visibly skips. */
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

			if (bi == TB_CLOSE)
				break;
			if (bi == TB_SET) {
				do_set();
				continue;
			}
			if (bi == TB_REFRESH) {
				refresh();
				continue;
			}
			int idx = ktui_table_hit(krect(2, 8, ktui_w - 4,
						       ktui_h - 12),
						 &tbl, nhit, TZ_NCOL, TZ_COL,
						 ev.mx, ev.my);
			ktui_table_pick(&tbl, nhit, idx, NULL, NULL);
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

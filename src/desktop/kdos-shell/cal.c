/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-cal — the clock's other half
 *
 *   ┌ August 2026 ───────────┐
 *   │ Mo Tu We Th Fr Sa Su   │
 *   │              1  2      │
 *   │  3  4  5  6  7  8  9   │
 *   │ 10 11 12 13 ▓14▓15 16  │
 *   │ ...                    │
 *   │ 14 August 2026  21:07  │
 *   └────────────────────────┘
 *
 * Clicking a clock and getting a calendar is the oldest expectation there is of
 * a panel, and this desktop's clock answered nothing at all. It opens under the
 * clock, the same way kdos-menu opens under the word that was clicked — layer-
 * shell has no coordinates, so "at x" is an anchor plus a margin.
 *
 * `cal` is not spawned to produce this. That would be a terminal window with a
 * shell in it for a grid of numbers, and the arrow keys could not then walk the
 * months. The arithmetic is mktime's: a struct tm with a day-of-month outside
 * the month is NORMALISED by it, which is how the next month is reached without
 * a table of month lengths or a leap-year rule of our own.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kwl.h"
#include "shell.h"

#define CAL_COLS 26
#define CAL_ROWS 12

static const char *const MONTHS[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

/* Monday-first, which is what the ISO week is and what every European desktop
 * shows. tm_wday is Sunday-first, so it is rotated once on the way in. */
static int mon_first(int tm_wday)
{
	return (tm_wday + 6) % 7;
}

/*
 * WHERE THE POINTER IS, AND THE THREE THINGS IT CAN PRESS.
 *
 * This popup had the wheel and nothing else: no way at all to change month
 * with a pointer except to guess that the wheel did it, and no sign under the
 * hand that anything here answered a click. Every calendar drawn since 1995
 * has two arrows and a way back to today, and on a 26-column grid they cost
 * one row that was already empty.
 */
static int hov_x = -1, hov_y = -1;		/* the pointer, in cells */
static int prev_x, prev_end, next_x, next_end, today_x, today_end, nav_row;

/* No layers: Esc closes the dropdown. */
static KtuiKeys keys;

static int cal_in(int v, int a, int b)
{
	return b > a && v >= a && v < b;
}

static void draw(int year, int mon, int today_y, int today_m, int today_d)
{
	int w = ktui_w, h = ktui_h;
	char title[64];

	snprintf(title, sizeof(title), " %s %d ", MONTHS[mon], year);
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), title, KT_ACCENT, KT_SURFACE, 0);
	ktui_draw_text(2, 1, w - 4, "Mo Tu We Th Fr Sa Su", KT_MID, KT_SURFACE,
		       KT_A_NONE);

	/*
	 * Day one of the month, normalised by mktime — which also fills in
	 * tm_wday, and is the whole reason there is no month-length table here.
	 * tm_isdst = -1 lets it work the offset out; a hard 0 shifts the first
	 * of the month by an hour twice a year, which is enough to land it on
	 * the previous day.
	 */
	struct tm t = { 0 };
	t.tm_year = year - 1900;
	t.tm_mon = mon;
	t.tm_mday = 1;
	t.tm_hour = 12;
	t.tm_isdst = -1;
	if (mktime(&t) == (time_t)-1)
		return;
	int first = mon_first(t.tm_wday);

	/* How many days: the 0th of the NEXT month, normalised back. */
	struct tm e = { 0 };
	e.tm_year = year - 1900;
	e.tm_mon = mon + 1;
	e.tm_mday = 0;
	e.tm_hour = 12;
	e.tm_isdst = -1;
	if (mktime(&e) == (time_t)-1)
		return;
	int ndays = e.tm_mday;

	for (int d = 1; d <= ndays; d++) {
		int cell = first + d - 1;
		int col = cell % 7, row = cell / 7;
		int x = 2 + col * 3, y = 2 + row;
		/* Eight, not three: the value is bounded by ndays and the
		 * compiler cannot know that, and a -Wformat-truncation warning
		 * in a build that treats warnings as errors is a build that
		 * fails for a day number that cannot happen. */
		char num[12];

		if (y >= h - 2)
			break;
		snprintf(num, sizeof(num), "%2d", d);
		int on = year == today_y && mon == today_m && d == today_d;
		/* The cell under the pointer lifts. There is nothing to click
		 * — this calendar has no events to open — and a day that
		 * answers the hand is still what makes the grid readable as a
		 * grid rather than as a block of numbers. */
		int hot = !on && hov_y == y && cal_in(hov_x, x, x + 2);
		ktui_draw_text(x, y, 2, num, on ? KT_SURFACE : KT_TEXT,
			       on    ? KT_ACCENT
			       : hot ? KT_DIM
				     : KT_SURFACE,
			       KT_A_NONE);
	}

	/*
	 * The navigation row, on the two rows a month can never reach: six
	 * week rows starting at 2 end at 7 in the worst case (a 31-day month
	 * beginning on a Sunday), and the date line is at h - 2.
	 */
	const char *prev = "[ \xe2\x97\x80 ]", *next = "[ \xe2\x96\xb6 ]";
	const char *tdy = "[ Today ]";
	int pw = ktui_utf8_width(prev), nw = ktui_utf8_width(next);
	int tw = ktui_utf8_width(tdy);

	nav_row = h - 3;
	prev_x = 2;
	prev_end = prev_x + pw;
	next_x = w - 2 - nw;
	next_end = next_x + nw;
	today_x = (w - tw) / 2;
	today_end = today_x + tw;
	if (nav_row > 1 && next_x > today_end && today_x > prev_end) {
		int hp = hov_y == nav_row && cal_in(hov_x, prev_x, prev_end);
		int hn = hov_y == nav_row && cal_in(hov_x, next_x, next_end);
		int ht = hov_y == nav_row && cal_in(hov_x, today_x, today_end);

		if (hp)
			ktui_draw_fill(krect(prev_x, nav_row, pw, 1), KT_DIM);
		if (hn)
			ktui_draw_fill(krect(next_x, nav_row, nw, 1), KT_DIM);
		if (ht)
			ktui_draw_fill(krect(today_x, nav_row, tw, 1), KT_DIM);
		ktui_draw_text(prev_x, nav_row, pw, prev, KT_TEXT,
			       hp ? KT_DIM : KT_SURFACE, KT_A_NONE);
		ktui_draw_text(today_x, nav_row, tw, tdy, KT_TEXT,
			       ht ? KT_DIM : KT_SURFACE, KT_A_NONE);
		ktui_draw_text(next_x, nav_row, nw, next, KT_TEXT,
			       hn ? KT_DIM : KT_SURFACE, KT_A_NONE);
	} else {
		/* Not drawn is not clickable — the rule every hit map on this
		 * desktop keeps. */
		prev_x = prev_end = next_x = next_end = today_x = today_end = 0;
	}

	/* The date in words, and the time — the two things the clock itself
	 * cannot fit into one row of a panel. */
	time_t now = time(NULL);
	struct tm nt;
	char line[64];
	localtime_r(&now, &nt);
	snprintf(line, sizeof(line), "%d %s %d   %02d:%02d", nt.tm_mday,
		 MONTHS[nt.tm_mon], nt.tm_year + 1900, nt.tm_hour, nt.tm_min);
	ktui_draw_text(2, h - 2, w - 4, line, KT_MID, KT_SURFACE, KT_A_NONE);

	/*
	 * THE ROW DROPS ITS TAIL, and Esc is the tail — so the month hint is
	 * gated on there being room for BOTH. Twenty-six columns is what this
	 * dropdown asks for and "Left/Right month  Esc Close" needs thirty-one;
	 * without the gate the wider hint would push the only one a person
	 * cannot guess off the end.
	 */
	ktui_hint_if(w >= 31, "Left/Right", "month");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 4, w - 4, 1), KT_SURFACE);
	ktui_draw_flush();
}

int cal_main(int argc, char **argv)
{
	const char *font = NULL;
	int at_x = -1, at_y = 0, at_bottom = 0, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		}
		/* The panel is on the BOTTOM edge and has spawned
		 * `kdos-cal --at-bottom X Y` for a release; this program only
		 * knew `--at`, so it fell into the usage branch and exited
		 * before a surface existed — a click on the clock did nothing
		 * whatsoever. Anchoring TOP cannot express "above the bar":
		 * the client does not know the output's pixel height. */
		else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;	/* see kdos-launcher --dump */
		} else {
			fprintf(stderr, "usage: kdos-cal [--at X Y] "
					"[--at-bottom X Y] [--dump] "
					"[--font NAME]\n");
			return 2;
		}
	}

	time_t now = time(NULL);
	struct tm nt;
	localtime_r(&now, &nt);
	int today_y = nt.tm_year + 1900, today_m = nt.tm_mon,
	    today_d = nt.tm_mday;
	int year = today_y, mon = today_m;

	if (dump) {
		sh_theme_from_cache();
		ktui_offscreen_init(CAL_COLS, CAL_ROWS);
		draw(year, mon, today_y, today_m, today_d);
		ktui_draw_dump();
		return 0;
	}

	KDispConfig cfg = {
		.role = KDISP_ROLE_OVERLAY,
		.cols = CAL_COLS,
		.rows = CAL_ROWS,
		.corner = at_x < 0	? KDISP_CORNER_CENTER
			  : at_bottom	? KDISP_CORNER_BOTTOM_LEFT
					: KDISP_CORNER_TOP_LEFT,
		.margin_x = at_x >= 0 ? at_x : 0,
		.margin_y = at_x >= 0 ? at_y : 0,
		.app_id = "kdos-cal",
		.font = font,
		.keyboard = 1,
		/* A dropdown, not a dialog: clicking elsewhere closes it. */
		.dismiss_on_unfocus = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-cal: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);

	while (!kdisp_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		draw(year, mon, today_y, today_m, today_d);

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		int step = 0;
		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				/* Plain movement — libkwl's spelling. It
				 * reports one only when the pointer changed
				 * cell, so this is never the position that
				 * rides along with a wheel event. */
				hov_x = ev.mx;
				hov_y = ev.my;
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn == KT_MB_WHEEL_UP)
				step = -1;
			else if (ev.btn == KT_MB_WHEEL_DOWN)
				step = 1;
			else if (ev.btn == KT_MB_LEFT && ev.my == nav_row) {
				if (cal_in(ev.mx, prev_x, prev_end)) {
					step = -1;
				} else if (cal_in(ev.mx, next_x, next_end)) {
					step = 1;
				} else if (cal_in(ev.mx, today_x, today_end)) {
					year = today_y;
					mon = today_m;
				} else {
					continue;
				}
			} else {
				continue;
			}
		} else if (ev.type == KT_EVT_KEY) {
			if (ktui_keys(&keys, &ev) == KTUI_KEY_CLOSE)
				goto done;

			switch (ev.key) {
			case KT_K_ENTER:
				goto done;
			case KT_K_LEFT:
			case KT_K_PGUP:
				step = -1;
				break;
			case KT_K_RIGHT:
			case KT_K_PGDN:
				step = 1;
				break;
			case KT_K_UP:
				step = -12;
				break;
			case KT_K_DOWN:
				step = 12;
				break;
			case KT_K_HOME:
			case 't':
				year = today_y;
				mon = today_m;
				break;
			default:
				break;
			}
		} else {
			continue;
		}

		mon += step;
		while (mon < 0) {
			mon += 12;
			year--;
		}
		while (mon > 11) {
			mon -= 12;
			year++;
		}
	}
done:
	kdisp_shutdown();
	return 0;
}

/*
 * The four views, checked where a dump can see them.
 *
 * A DUMP PROVES A CHARACTER AND NEVER A COLOUR, so the claims here are the
 * ones a cell grid can carry: which rows a table put on the screen after it
 * scrolled, which tab abbreviated, where the selection landed when the keys
 * stepped over a heading, and what the text block held after an edit. The
 * colour rules are in the widget and belong to the eye.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ktui.h"

static int fails;

static void ok(int cond, const char *what)
{
	if (!cond) {
		printf("  FAIL  %s\n", what);
		fails++;
	}
}

/* The dumped grid, read back as lines. */
static char grid[64][256];
static int ngrid;

static void snap(void)
{
	char path[] = "/tmp/kdos-viewcheck.XXXXXX";
	int fd = mkstemp(path);
	FILE *f;

	if (fd < 0)
		return;
	fflush(stdout);
	int save = dup(1);
	dup2(fd, 1);
	ktui_draw_dump();
	fflush(stdout);
	dup2(save, 1);
	close(save);
	close(fd);

	ngrid = 0;
	f = fopen(path, "r");
	if (f) {
		while (ngrid < 64 && fgets(grid[ngrid], sizeof(grid[0]), f)) {
			grid[ngrid][strcspn(grid[ngrid], "\n")] = '\0';
			ngrid++;
		}
		fclose(f);
	}
	remove(path);
}

static int grid_has(const char *s)
{
	for (int i = 0; i < ngrid; i++)
		if (strstr(grid[i], s))
			return 1;
	return 0;
}

/* ── tabs ──────────────────────────────────────────────────────────────── */

static const KtuiTab TABS[] = {
	{ "Applications", NULL }, { "Batteries", NULL }, { "Boxes", NULL }
};
#define NTABS 3

static void test_tabs(void)
{
	int sel = 0;

	ktui_draw_clear();
	ktui_tabs_draw(krect(0, 0, 18, 3), TABS, NTABS, sel, -1, 1);
	snap();
	ok(grid_has("Applications"), "a wide strip writes the whole name");

	ktui_draw_clear();
	ktui_tabs_draw(krect(0, 0, 6, 3), TABS, NTABS, sel, -1, 1);
	snap();
	/* THREE CHARACTERS, NOT ONE: `Batteries` and `Boxes` share an initial
	 * and a one-letter strip makes them the same control. */
	ok(grid_has("Bat") && grid_has("Box"),
	   "a narrow strip abbreviates to three and keeps them distinct");
	ok(!grid_has("Batteries"), "and does not overflow its column");

	ok(ktui_tabs_key(&sel, NTABS, 1, KT_K_DOWN) && sel == 1,
	   "Down moves a vertical strip");
	ok(!ktui_tabs_key(&sel, NTABS, 1, KT_K_RIGHT),
	   "Right does not: a column answers the column keys");
	sel = NTABS - 1;
	ok(!ktui_tabs_key(&sel, NTABS, 1, KT_K_DOWN) && sel == NTABS - 1,
	   "and the last tab is the last: the strip does not wrap");

	ok(ktui_tabs_hit(krect(0, 2, 18, 3), TABS, NTABS, 1, 4, 3) == 1,
	   "a click is measured from the rect the draw took");
	ok(ktui_tabs_hit(krect(0, 2, 18, 3), TABS, NTABS, 1, 4, 9) == -1,
	   "and a click past the strip is no tab");
}

/* ── table ─────────────────────────────────────────────────────────────── */

/* Two sections of two records each: rows 0 and 3 are headings. */
static const char *const TROW[] = { "CAMERAS", "video0", "video1", "INPUT",
				    "keyboard", "mouse" };
#define NTROW 6

static int t_span(int idx, void *user)
{
	(void)user;
	return (idx == 0 || idx == 3) ? KT_TABLE_SKIP : 0;
}

static void t_cell(int idx, int col, int x, int y, int w, int fg, int bg,
		   void *user)
{
	(void)col;
	(void)user;
	ktui_draw_text(x, y, w, TROW[idx], fg, bg, KT_A_NONE);
}

static const KtuiCol TCOL[] = { { NULL, 0 } };

static void test_table(void)
{
	KtuiTable st = { 0, 0 };

	/* The selection must not start on a heading. */
	st.sel = 1;
	ok(ktui_table_key(&st, NTROW, 4, KT_K_DOWN, t_span, NULL) &&
		   st.sel == 2,
	   "Down moves one record");
	ok(ktui_table_key(&st, NTROW, 4, KT_K_DOWN, t_span, NULL) &&
		   st.sel == 4,
	   "and steps OVER a heading rather than landing on it");
	ok(ktui_table_key(&st, NTROW, 4, KT_K_UP, t_span, NULL) && st.sel == 2,
	   "and steps over it going back, in the direction it started");

	ok(!ktui_table_pick(&st, NTROW, 3, t_span, NULL) && st.sel == 2,
	   "a click on a skipped heading is refused");
	ok(ktui_table_pick(&st, NTROW, 5, t_span, NULL) && st.sel == 5,
	   "a click on a record is taken");

	/* A window of three rows over six: the tail must be reachable. */
	st.sel = 5;
	st.top = 0;
	ktui_table_clamp(&st, NTROW, 3);
	ok(st.top == 3, "the view follows the selection to the end");

	ktui_draw_clear();
	ktui_table_draw(krect(0, 0, 20, 3), &st, NTROW, TCOL, 1, t_cell, t_span,
			NULL, -1);
	snap();
	ok(grid_has("mouse"), "the last row is drawn once the view scrolled");
	ok(!grid_has("video0"), "and the first is not");

	/* Column layout: one elastic column takes what the fixed ones leave. */
	const KtuiCol C[] = { { "PID", 7 }, { "USER", 9 }, { "NAME", 0 } };
	int x[3], cw[3];

	ktui_table_layout(C, 3, 40, x, cw);
	ok(x[0] == 0 && x[1] == 8 && x[2] == 18,
	   "columns start where the widths before them end");
	ok(cw[2] == 22, "and the elastic column takes the remainder");
}

/* ── dropdown ──────────────────────────────────────────────────────────── */

static const char *const OPT[] = { "1920x1080@60", "1280x720@60",
				   "800x600@75" };

static void test_dropdown(void)
{
	KtuiDrop d = { 0, 0, 0 };

	ok(!ktui_dropdown_key(&d, 3, KT_K_ENTER) && d.open,
	   "Enter opens the list and chooses nothing");
	ok(!ktui_dropdown_key(&d, 3, KT_K_DOWN) && d.hi == 1,
	   "Down highlights without choosing");
	ok(ktui_dropdown_key(&d, 3, KT_K_ENTER) && d.sel == 1 && !d.open,
	   "and Enter takes the highlight and closes");

	ok(!ktui_dropdown_key(&d, 3, KT_K_ENTER) && d.open, "opened again");
	ktui_dropdown_key(&d, 3, KT_K_DOWN);
	ok(!ktui_dropdown_key(&d, 3, KT_K_ESC) && !d.open && d.sel == 1,
	   "Escape closes it and keeps the old choice");

	ktui_draw_clear();
	d.open = 1;
	d.hi = 0;
	ktui_dropdown_draw_open(krect(0, 0, 20, 1), &d, OPT, 3);
	snap();
	ok(grid_has("800x600@75"), "the open list shows every option");

	d.open = 1;
	ok(!ktui_dropdown_hit(krect(0, 0, 20, 1), &d, 3, 40, 40) && !d.open &&
		   d.sel == 1,
	   "a click outside closes it without choosing");
	d.open = 1;
	ok(ktui_dropdown_hit(krect(0, 0, 20, 1), &d, 3, 5, 3) && d.sel == 2,
	   "a click on a row takes it");
}

/* ── text area ─────────────────────────────────────────────────────────── */

#define TA_LINES 8
#define TA_STRIDE 32

static void test_textarea(void)
{
	static char text[TA_LINES][TA_STRIDE];
	KtuiTextArea ta = { 0, 0, 0 };
	int n = 1;

	snprintf(text[0], TA_STRIDE, "%s", "hello");
	ta.cx = 5;
	ok(ktui_textarea_key(&ta, text[0], &n, TA_LINES, TA_STRIDE, '!') &&
		   !strcmp(text[0], "hello!"),
	   "a printable character is inserted at the caret");

	ta.cx = 5;
	ok(ktui_textarea_key(&ta, text[0], &n, TA_LINES, TA_STRIDE,
			     KT_K_ENTER) &&
		   n == 2 && !strcmp(text[0], "hello") &&
		   !strcmp(text[1], "!"),
	   "Enter splits the line at the caret");
	ok(ta.cy == 1 && ta.cx == 0, "and the caret follows the tail down");

	ok(ktui_textarea_key(&ta, text[0], &n, TA_LINES, TA_STRIDE,
			     KT_K_BACKSPACE) &&
		   n == 1 && !strcmp(text[0], "hello!"),
	   "Backspace at column zero joins the line back");
	ok(ta.cy == 0 && ta.cx == 5, "and the caret lands where the join was");

	/* A join that would not fit is refused rather than truncated. */
	n = 2;
	memset(text[0], 'a', TA_STRIDE - 1);
	text[0][TA_STRIDE - 1] = '\0';
	snprintf(text[1], TA_STRIDE, "%s", "tail");
	ta.cy = 1;
	ta.cx = 0;
	ok(!ktui_textarea_key(&ta, text[0], &n, TA_LINES, TA_STRIDE,
			      KT_K_BACKSPACE) &&
		   n == 2 && !strcmp(text[1], "tail"),
	   "a join with no room is refused, never truncated");

	/* The view follows the caret past the bottom of the rect. */
	n = TA_LINES;
	for (int i = 0; i < TA_LINES; i++)
		snprintf(text[i], TA_STRIDE, "line%d", i);
	ta.cy = TA_LINES - 1;
	ta.cx = 0;
	ta.top = 0;
	ktui_draw_clear();
	ktui_textarea_draw(krect(0, 0, 12, 3), &ta, text[0], n, TA_STRIDE,
			   KT_TEXT, KT_BG);
	snap();
	ok(grid_has("line7"), "the caret's line is on the screen");
	ok(!grid_has("line0"), "and the first line has scrolled off");
}

int main(void)
{
	if (ktui_offscreen_init(40, 12) != 0) {
		printf("  cannot render offscreen\n");
		return 1;
	}
	ktui_draw_init();

	test_tabs();
	test_table();
	test_dropdown();
	test_textarea();

	if (fails) {
		printf("  %d failed\n", fails);
		return 1;
	}
	printf("  the page strip, the table, the choice and the text block\n");
	return 0;
}

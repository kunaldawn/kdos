/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   inspcheck — the KDOS_INSPECT overlay, with no compositor
 *
 * kwl_insp.c has no Wayland in it, so it is driven here directly with the
 * clock handed in:
 *
 *   - the panel is laid on a COPY of the cells and the frame handed in is
 *     untouched; a wide glyph the panel would cut loses both halves; a
 *     surface too short for the panel gets one line, and one too narrow none;
 *   - a changed row is tinted over exactly its changed cells, less as it
 *     ages, and not at all after a second; a full commit lights every row;
 *   - the numbers: commits and stashes a second, the frame latency — which
 *     the overlay's own refreshes never feed;
 *   - the refresh cadence: due while a tint fades, due when the panel would
 *     read differently, and then never again on a still surface — an
 *     overlay whose refresh changed its own panel would refresh for ever;
 *   - a frame surface's hit rects are outlined, the focused one in the
 *     accent, chrome in KT_MID, and nothing inside them is painted.
 *
 * usage: inspcheck        (KDOS_INSPECT set)
 *        inspcheck off    (KDOS_INSPECT unset or "0": the overlay is off)
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kwl_insp.h"

#define W 60
#define H 20
#define CW 8
#define CH 16

static int fails, checks;

static void ok(int c, const char *what)
{
	checks++;
	if (!c) {
		fails++;
		fprintf(stderr, "inspcheck: FAIL %s\n", what);
	}
}

static const KtuiTheme theme = {
	.name = "inspcheck",
	.label = "inspcheck",
	.slot = {
		{ 0x00, 0x00, 0x00 },	/* KT_BG      */
		{ 0xe0, 0x30, 0x30 },	/* KT_ERR     */
		{ 0xff, 0xaa, 0x33 },	/* KT_ACCENT  */
		{ 0x33, 0xaa, 0xff },	/* KT_WARN    */
		{ 0x40, 0x40, 0x40 },	/* KT_DIM     */
		{ 0x80, 0x80, 0x80 },	/* KT_MID     */
		{ 0x20, 0x20, 0x20 },	/* KT_SURFACE */
		{ 0xf0, 0xf0, 0xf0 },	/* KT_TEXT    */
	},
};

static KtuiCell cur[W * H];
static pixman_image_t *img;

static void grid_fill(void)
{
	for (int i = 0; i < W * H; i++) {
		memset(&cur[i], 0, sizeof(cur[i]));
		cur[i].ch = 'x';
		cur[i].fg = KT_TEXT;
		cur[i].bg = KT_BG;
	}
}

static void img_clear(void)
{
	uint32_t *p = pixman_image_get_data(img);

	for (int i = 0; i < W * CW * H * CH; i++)
		p[i] = 0xff000000u;
}

static uint32_t px(int x, int y)
{
	return pixman_image_get_data(img)[y * W * CW + x];
}

/* The panel's text on row `y`, from the copy the overlay returned. */
static void row_text(const KtuiCell *c, int w, int y, char *out, int n)
{
	int x0 = w - 30, k = 0;

	for (int x = x0; x < w && k < n - 1; x++)
		out[k++] = (char)c[y * w + x].ch;
	out[k] = 0;
	while (k > 0 && out[k - 1] == ' ')
		out[--k] = 0;
}

static int same_cell(const KtuiCell *a, const KtuiCell *b)
{
	return !memcmp(a, b, sizeof(*a));
}

int main(int argc, char **argv)
{
	const KtuiCell *out;
	KCellSpan sp[H];
	char t[40];
	int64_t now;

	if (argc > 1 && !strcmp(argv[1], "off")) {
		if (kwl_insp_on()) {
			fprintf(stderr, "inspcheck: the overlay is on with "
					"KDOS_INSPECT unset or 0\n");
			return 1;
		}
		return 0;
	}
	ok(kwl_insp_on(), "KDOS_INSPECT=1 turns the overlay on");
	ktui_theme = &theme;
	ktui_offscreen_init(W, H);
	ktui_draw_init();
	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, W * CW, H * CH, NULL,
				       0);

	/* ── the panel is laid on a copy ───────────────────────────── */
	grid_fill();
	cur[2 * W + 29].ch = 0x4e00;
	cur[2 * W + 30].ch = KTUI_WIDE_CONT;
	cur[9 * W + 29].ch = 0x4e00;
	cur[9 * W + 30].ch = KTUI_WIDE_CONT;
	{
		KtuiCell keep[W * H];

		memcpy(keep, cur, sizeof(keep));
		out = kwl_insp_cells(cur, W, H, 5000);
		ok(out != cur && !memcmp(keep, cur, sizeof(keep)),
		   "the panel goes on a copy and the frame handed in is untouched");
	}
	ok(out[0 * W + 30].bg == KT_SURFACE && out[7 * W + 59].bg == KT_SURFACE,
	   "the panel covers the top right corner, 30 x 8");
	ok(out[0 * W + 31].ch == 'K' && out[0 * W + 31].fg == KT_ACCENT,
	   "its first line is the title, in the accent");
	ok(out[1 * W + 31].fg == KT_MID && out[1 * W + 40].fg == KT_TEXT,
	   "then a label column in KT_MID and values in KT_TEXT");
	ok(same_cell(&out[0 * W + 29], &cur[0 * W + 29]) &&
	   same_cell(&out[8 * W + 45], &cur[8 * W + 45]),
	   "nothing outside the panel moves");
	ok(out[2 * W + 29].ch == ' ',
	   "a wide glyph the panel cuts loses its left half too");
	ok(out[9 * W + 29].ch == 0x4e00,
	   "and one below the panel keeps it");
	row_text(out, W, 0, t, sizeof(t));
	ok(!strcmp(t, " KDOS_INSPECT 60x20"), "the title says the grid");
	out = kwl_insp_cells(cur, W, 5, 5000);
	ok(out[0 * W + 30].bg == KT_SURFACE && out[1 * W + 30].bg == KT_BG,
	   "a surface too short for the panel gets one line of it");
	ok(kwl_insp_cells(cur, 20, H, 5000) == cur,
	   "and one too narrow gets none");

	/* ── the tint ─────────────────────────────────────────────── */
	memset(sp, 0, sizeof(sp));
	sp[3] = (KCellSpan){ 5, 10 };
	kwl_insp_damage(sp, W, H, 0, 10000);
	img_clear();
	kwl_insp_paint(img, W, H, CW, CH, 10000);
	uint32_t fresh = px(7 * CW + 3, 3 * CH + 5);
	ok(fresh != 0xff000000u, "a changed row is tinted over its changed cells");
	ok(px(4 * CW + 3, 3 * CH + 5) == 0xff000000u &&
	   px(10 * CW + 1, 3 * CH + 5) == 0xff000000u,
	   "and not past either end of the span");
	ok(px(7 * CW + 3, 4 * CH + 5) == 0xff000000u, "an unchanged row is not");
	img_clear();
	kwl_insp_paint(img, W, H, CW, CH, 10500);
	uint32_t half = px(7 * CW + 3, 3 * CH + 5);
	ok(half != 0xff000000u && (half & 0xff0000u) < (fresh & 0xff0000u),
	   "half a second on it is fainter");
	img_clear();
	kwl_insp_paint(img, W, H, CW, CH, 11000);
	ok(px(7 * CW + 3, 3 * CH + 5) == 0xff000000u, "and after a second, gone");
	out = kwl_insp_cells(cur, W, H, 11000);
	row_text(out, W, 5, t, sizeof(t));
	ok(!strcmp(t, " changed  1 rows 5 cells"),
	   "the panel counts the rows and cells the commit changed");
	kwl_insp_damage(sp, W, H, 1, 11000);
	img_clear();
	kwl_insp_paint(img, W, H, CW, CH, 11000);
	ok(px(0, 0) != 0xff000000u && px(W * CW - 1, H * CH - 1) != 0xff000000u,
	   "a full commit lights every row, edge to edge");
	out = kwl_insp_cells(cur, W, H, 11000);
	row_text(out, W, 5, t, sizeof(t));
	ok(!strcmp(t, " changed  all 20 rows"), "and says it was all of them");

	/* ── the numbers ─────────────────────────────────────────── */
	now = 20000;
	kwl_insp_committed(0, 1500, now);	/* opens a window at 20000 */
	for (int i = 0; i < 30; i++)
		kwl_insp_stash(now + i);
	for (int i = 1; i < 9; i++)
		kwl_insp_committed(0, 2500, now + 100 * i);
	kwl_insp_committed(1, 99000, now + 950);
	kwl_insp_frame_done(40);
	kwl_insp_committed(0, 1500, now + 960);
	kwl_insp_frame_done(16);
	out = kwl_insp_cells(cur, W, H, now + 1000);
	row_text(out, W, 1, t, sizeof(t));
	ok(!strcmp(t, " commits  10/s"),
	   "commits a second, with the overlay's own not among them");
	row_text(out, W, 2, t, sizeof(t));
	ok(!strcmp(t, " stashed  30/s"), "stashes a second");
	row_text(out, W, 3, t, sizeof(t));
	ok(!strcmp(t, " paint    1.50 ms  max 2.50"),
	   "the last paint, and the longest the window saw");
	row_text(out, W, 4, t, sizeof(t));
	ok(!strcmp(t, " frame    16 ms  max 16"),
	   "the frame latency; a refresh's callback is not the surface's");

	/* ── the refresh cadence ─────────────────────────────────── */
	{
		int refreshes = 0, due;

		now = 30000;
		kwl_insp_damage(sp, W, H, 0, now);
		kwl_insp_cells(cur, W, H, now);
		kwl_insp_committed(0, 1500, now);
		ok(kwl_insp_wait(now + 10) == 90,
		   "a fading tint asks for a refresh a tenth of a second on");
		for (int64_t tt = now + 10; tt < now + 8000; tt += 10) {
			due = kwl_insp_wait(tt);
			if (due != 0)
				continue;
			refreshes++;
			kwl_insp_cells(cur, W, H, tt);
			kwl_insp_committed(1, 5000, tt);
			kwl_insp_frame_done(3);
		}
		ok(refreshes >= 10 && refreshes <= 14,
		   "the fade is a tenth of a second a step, then the numbers settle");
		ok(kwl_insp_wait(now + 8000) == -1,
		   "and a still surface under the overlay goes still");
		kwl_insp_committed(0, 1500, now + 8000);
		kwl_insp_cells(cur, W, H, now + 8000);
		due = kwl_insp_wait(now + 8010);
		ok(due > 0 && due <= 1000,
		   "a commit with no tint asks to be woken when its second closes");
	}

	/* ── outlines round a frame's hit rects ─────────────────── */
	{
		KtuiEvent ev = { .type = KT_EVT_NONE };

		ktui_focus_set(0);
		ktui_frame_begin(&ev);
		ktui_button(krect(2, 10, 6, 1), "One", 1, 0);
		ktui_button(krect(12, 10, 6, 1), "Two", 1, 0);
		ktui_hit_chrome(krect(30, 14, 4, 2), 0);
		ktui_frame_end();
		img_clear();
		kwl_insp_paint(img, W, H, CW, CH, 90000);
		ok(px(2 * CW, 10 * CH) == 0xffffaa33u &&
		   px(8 * CW - 1, 11 * CH - 1) == 0xffffaa33u,
		   "the focused control is outlined in the accent");
		ok(px(12 * CW, 10 * CH + 5) == 0xff33aaffu,
		   "another in KT_WARN");
		ok(px(30 * CW + 3, 14 * CH) == 0xff808080u, "chrome in KT_MID");
		ok(px(2 * CW + 4, 10 * CH + 8) == 0xff000000u,
		   "and nothing inside a rect is painted");
	}

	printf("inspcheck: %d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}

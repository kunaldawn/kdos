/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   plotcheck — the data marks and the chart, pixel for pixel
 *
 * The goldens are cell frames, and a chart is pixels only where a display
 * has them, so nothing else in the suite ever looks at the antialiased
 * chart. This does, two ways:
 *
 *   - PROPERTIES that hold whatever the arithmetic: nothing is written
 *     outside the rectangle or the clip; every pixel stays premultiplied; a
 *     series at rest is exactly one row at the rest weight; a sample above
 *     zero is at least a pixel; a mirrored series is the upright one flipped;
 *     the oldest value is held to the left edge only when asked; a line is
 *     the same drawn from either end, a steep line is the shallow one
 *     transposed, and a line's total coverage is its area; the clip narrows
 *     fills as well as marks, a refused push is refused, and a clear empties
 *     the stack; kch_plot() rasterises only when its content moves.
 *   - DIGESTS of whole canvases for fixed inputs. The marks are computed in
 *     fixed point from one conversion per input, so the bytes are the same on
 *     every build; a digest that moves is a picture that changed, and a
 *     change on purpose updates the digest here with the reason.
 *
 * `plotcheck --print` prints the digests instead of checking them.
 *
 * The palette is this file's own, so a change to a shipped theme moves no
 * digest. libkdisp and libkicon are stubbed: the cell size and the icon
 * switch are all kch_tile.c asks them.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kchrome.h"
#include "kdisp.h"
#include "kicon.h"

#define CW 8
#define CH 16

int kicon_enabled(void) { return 1; }
int kdisp_cell_w(void) { return CW; }
int kdisp_cell_h(void) { return CH; }
int kdisp_scale(void) { return 1; }

static const KtuiTheme theme = {
	.name = "plotcheck",
	.label = "plotcheck",
	.slot = {
		{ 0x10, 0x12, 0x14 },	/* KT_BG      */
		{ 0xe0, 0x30, 0x30 },	/* KT_ERR     */
		{ 0xff, 0xaa, 0x33 },	/* KT_ACCENT  */
		{ 0x33, 0xaa, 0xff },	/* KT_WARN    */
		{ 0x40, 0x44, 0x48 },	/* KT_DIM     */
		{ 0x90, 0x94, 0x98 },	/* KT_MID     */
		{ 0x20, 0x22, 0x24 },	/* KT_SURFACE */
		{ 0xf0, 0xf0, 0xf0 },	/* KT_TEXT    */
	},
};

static int fails, print;

static void ok(int c, const char *what)
{
	if (!c) {
		printf("FAIL  %s\n", what);
		fails++;
	}
}

/* ── the cell side, for kch_plot()'s tile ─────────────────────────────── */

static void flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)ff;
	memcpy(prev, cur, (size_t)w * h * sizeof(*prev));
}

static void size(int *w, int *h)
{
	*w = 120;
	*h = 40;
}

static int caps(void) { return 0; }

static int poll_ev(KtuiEvent *ev, int ms)
{
	(void)ev;
	(void)ms;
	return 0;
}

static const KtuiBackend backend = {
	.name = "plotcheck",
	.flush = flush,
	.poll_event = poll_ev,
	.size = size,
	.caps = caps,
};

/* ── pixels ───────────────────────────────────────────────────────────── */

static uint32_t *bits(KCellCanvas *cv)
{
	return pixman_image_get_data(kcell_canvas_image(cv));
}

static uint32_t px_at(KCellCanvas *cv, int x, int y)
{
	return bits(cv)[(size_t)y * kcell_canvas_w(cv) + x];
}

static uint64_t digest(KCellCanvas *cv)
{
	const unsigned char *b = (const unsigned char *)bits(cv);
	size_t n = (size_t)kcell_canvas_w(cv) * kcell_canvas_h(cv) * 4;
	uint64_t h = 0xcbf29ce484222325ULL;

	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

static void want_digest(KCellCanvas *cv, uint64_t want, const char *what)
{
	uint64_t got = digest(cv);

	if (print) {
		printf("%-28s 0x%016llxULL\n", what, (unsigned long long)got);
		return;
	}
	if (got != want) {
		printf("FAIL  digest %s: 0x%016llx, want 0x%016llx\n", what,
		       (unsigned long long)got, (unsigned long long)want);
		fails++;
	}
}

/* Premultiplied: no channel may exceed the alpha it was scaled by. */
static int premultiplied(KCellCanvas *cv)
{
	int n = kcell_canvas_w(cv) * kcell_canvas_h(cv);
	const uint32_t *p = bits(cv);

	for (int i = 0; i < n; i++) {
		uint32_t a = p[i] >> 24;

		if (((p[i] >> 16) & 255) > a || ((p[i] >> 8) & 255) > a ||
		    (p[i] & 255) > a)
			return 0;
	}
	return 1;
}

/* Every pixel a sentinel, so a write anywhere shows. */
#define SENTINEL 0x80402010u

static void sentinel(KCellCanvas *cv)
{
	int n = kcell_canvas_w(cv) * kcell_canvas_h(cv);
	uint32_t *p = bits(cv);

	for (int i = 0; i < n; i++)
		p[i] = SENTINEL;
}

/* 1 when every pixel outside (x, y, w, h) is still the sentinel. */
static int untouched_outside(KCellCanvas *cv, int x, int y, int w, int h)
{
	for (int j = 0; j < kcell_canvas_h(cv); j++)
		for (int i = 0; i < kcell_canvas_w(cv); i++) {
			int in = i >= x && i < x + w && j >= y && j < y + h;

			if (!in && px_at(cv, i, j) != SENTINEL)
				return 0;
		}
	return 1;
}

/* ── inputs ───────────────────────────────────────────────────────────── */

#define NS 96
static double wave[NS];

/* A triangle wave with noise and a flat stretch, from integers only: the
 * inputs must be the same doubles on every build, or the digests would be
 * testing the libm. */
static void make_wave(void)
{
	unsigned s = 12345;

	for (int i = 0; i < NS; i++) {
		int tri = i % 32 < 16 ? i % 32 : 32 - i % 32;

		s = s * 1103515245u + 12345u;
		wave[i] = tri * 5.0 + (double)((s >> 16) % 13);
		if (i >= 40 && i < 52)
			wave[i] = 0.0;
	}
}

static KCellSeries series(int mode, double step, double lw)
{
	return (KCellSeries){
		.v = wave,
		.n = NS,
		.vmax = 100.0,
		.step = step,
		.mode = mode,
		.area_alpha = 90,
		.line_alpha = 255,
		.rest_alpha = 90,
		.line_w = lw,
	};
}

/* ── the series ───────────────────────────────────────────────────────── */

static void check_series(void)
{
	KCellCanvas *cv = kcell_canvas_new(20, 4, CW, CH, 1);	/* 160x64 */
	KCellCanvas *cm = kcell_canvas_new(20, 4, CW, CH, 1);
	KCellSeries s;
	int W = kcell_canvas_w(cv), H = kcell_canvas_h(cv);

	ok(cv && cm, "series: canvases");
	if (!cv || !cm)
		return;

	/* The whole canvas, one sample a pixel. */
	s = series(KCELL_SERIES_BOTH, 0, 0);
	kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
	ok(premultiplied(cv), "series: premultiplied");
	want_digest(cv, 0x7cbeaf07ce8f3d82ULL, "series-both-1px");

	/* Mirrored is upright flipped, pixel for pixel. */
	kcell_canvas_clear(cm);
	s.mode = KCELL_SERIES_BOTH | KCELL_SERIES_MIRROR;
	kcell_canvas_series(cm, 0, 0, W, H, &s, KT_ACCENT);
	{
		int same = 1;

		for (int y = 0; y < H && same; y++)
			for (int x = 0; x < W && same; x++)
				same = px_at(cv, x, y) == px_at(cm, x, H - 1 - y);
		ok(same, "series: a mirror is the upright series flipped");
	}

	/* Into a rectangle, with every pixel round it a sentinel. */
	sentinel(cv);
	s = series(KCELL_SERIES_BOTH, 3.5, 1.5);
	kcell_canvas_series(cv, 13, 9, 101, 37, &s, KT_WARN);
	ok(untouched_outside(cv, 13, 9, 101, 37),
	   "series: nothing written outside its rectangle");
	ok(premultiplied(cv), "series: premultiplied over a background");
	want_digest(cv, 0xcb797e36e2475b62ULL, "series-rect-step3.5");

	/* Held and not held: a series shorter than the band. */
	kcell_canvas_clear(cv);
	s = series(KCELL_SERIES_BOTH, 4, 0);
	s.n = 10;
	s.v = wave + 20;
	kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
	{
		int empty = 1;

		for (int y = 0; y < H; y++)
			for (int x = 0; x < W - 10 * 4; x++)
				empty &= px_at(cv, x, y) == 0;
		ok(empty, "series: not held, the band left of the data is empty");
		ok(px_at(cv, W - 10 * 4, H - 1) != 0,
		   "series: not held, the data starts at its own edge");
	}
	kcell_canvas_clear(cv);
	s.mode |= KCELL_SERIES_HOLD;
	kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
	ok(px_at(cv, 0, H - 1) != 0, "series: held, the oldest value reaches x 0");
	want_digest(cv, 0x90c6aad93145ac1fULL, "series-held");

	/* At rest: one row, the bottom one, at the rest weight — and nothing
	 * of the area, which has no height. */
	{
		double zero[8] = { 0 };
		int only = 1;

		kcell_canvas_clear(cv);
		s = series(KCELL_SERIES_BOTH, 0, 0);
		s.v = zero;
		s.n = 8;
		s.mode |= KCELL_SERIES_HOLD;
		kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				uint32_t a = px_at(cv, x, y) >> 24;

				only &= y == H - 1 ? a == 90 : a == 0;
			}
		ok(only, "series: at rest, exactly the bottom row at rest_alpha");
	}

	/* A dribble is a pixel: the lone sample lifts its trace off the rest
	 * row into the one above, and silence beside it stays on the rest row
	 * at the rest weight. */
	{
		double drib[16] = { 0 };

		drib[12] = 0.0001;
		kcell_canvas_clear(cv);
		s = series(KCELL_SERIES_BOTH, 10, 0);
		s.v = drib;
		s.n = 16;
		kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
		/* Sample 12 of 16 at ten pixels each is centred on x 125. */
		ok((px_at(cv, 125, H - 2) >> 24) > 0,
		   "series: a sample above zero is drawn a pixel high");
		ok((px_at(cv, 5, H - 2) >> 24) == 0 &&
			   (px_at(cv, 5, H - 1) >> 24) == 90,
		   "series: silence beside it rests at the rest weight");
	}

	/* The full scale: every row of every column carries the area. */
	{
		double full[4] = { 100, 100, 100, 100 };
		int all = 1;

		kcell_canvas_clear(cv);
		s = series(KCELL_SERIES_AREA, 0, 0);
		s.v = full;
		s.n = 4;
		s.mode |= KCELL_SERIES_HOLD;
		kcell_canvas_series(cv, 0, 0, W, H, &s, KT_ACCENT);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				all &= (px_at(cv, x, y) >> 24) == 90;
		ok(all, "series: at vmax the area fills the band");
	}

	kcell_canvas_free(cv);
	kcell_canvas_free(cm);
}

/* ── the line ─────────────────────────────────────────────────────────── */

static uint64_t alpha_sum(KCellCanvas *cv)
{
	uint64_t s = 0;
	int n = kcell_canvas_w(cv) * kcell_canvas_h(cv);

	for (int i = 0; i < n; i++)
		s += bits(cv)[i] >> 24;
	return s;
}

static void check_line(void)
{
	KCellCanvas *a = kcell_canvas_new(8, 4, CW, CH, 1);	/* 64x64 */
	KCellCanvas *b = kcell_canvas_new(8, 4, CW, CH, 1);
	int same;

	ok(a && b, "line: canvases");
	if (!a || !b)
		return;

	/* A horizontal line centred in a row is exactly that row. */
	kcell_canvas_line(a, 2, 10.5, 20, 10.5, 1, KT_TEXT, 255);
	same = 1;
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 64; x++) {
			uint32_t al = px_at(a, x, y) >> 24;

			same &= (y == 10 && x >= 2 && x < 20) ? al == 255
							       : al == 0;
		}
	ok(same, "line: a horizontal line on a row centre is that row");

	/* Either end first: the same bytes. */
	kcell_canvas_clear(a);
	kcell_canvas_clear(b);
	kcell_canvas_line(a, 3.25, 50.75, 60.5, 7.125, 1.5, KT_ACCENT, 200);
	kcell_canvas_line(b, 60.5, 7.125, 3.25, 50.75, 1.5, KT_ACCENT, 200);
	ok(!memcmp(bits(a), bits(b), 64 * 64 * 4),
	   "line: the same from either end");
	ok(premultiplied(a), "line: premultiplied");
	want_digest(a, 0x649596ec6c2341ffULL, "line-shallow");

	/* Steep is shallow transposed: one code path turned on its side. */
	kcell_canvas_clear(b);
	kcell_canvas_line(b, 50.75, 3.25, 7.125, 60.5, 1.5, KT_ACCENT, 200);
	same = 1;
	for (int y = 0; y < 64 && same; y++)
		for (int x = 0; x < 64 && same; x++)
			same = px_at(a, x, y) == px_at(b, y, x);
	ok(same, "line: a steep line is the shallow one transposed");

	/* Coverage is area: a 40x30 run (length 50), two pixels wide, at full
	 * alpha is 100 pixels' worth, give or take the rounding of each. */
	kcell_canvas_clear(a);
	kcell_canvas_line(a, 10, 10, 50, 40, 2, KT_TEXT, 255);
	{
		uint64_t sum = alpha_sum(a);

		ok(sum > 255 * 95 && sum < 255 * 105,
		   "line: its coverage adds up to its area");
	}

	/* Off the canvas at both ends: clipped, not wrapped. */
	sentinel(a);
	kcell_canvas_line(a, -1e6, 32, 1e6, 33, 3, KT_TEXT, 255);
	same = 1;
	for (int y = 0; y < 64; y++)
		if (y < 29 || y > 36)
			for (int x = 0; x < 64; x++)
				same &= px_at(a, x, y) == SENTINEL;
	ok(same, "line: a segment far past the canvas stays on its rows");

	kcell_canvas_free(a);
	kcell_canvas_free(b);
}

/* ── the clip ─────────────────────────────────────────────────────────── */

static void check_clip(void)
{
	KCellCanvas *cv = kcell_canvas_new(8, 4, CW, CH, 1);
	KCellSeries s = series(KCELL_SERIES_BOTH, 0, 0);
	int all, refused = 0;

	ok(cv != NULL, "clip: canvas");
	if (!cv)
		return;

	/* Fills, marks and a nested clip that tries to widen itself. */
	sentinel(cv);
	ok(kcell_canvas_clip_push(cv, 10, 12, 30, 20) == 0, "clip: push");
	ok(kcell_canvas_clip_push(cv, 0, 0, 64, 64) == 0, "clip: nested push");
	kcell_canvas_fill(cv, 0, 0, 64, 64, KT_DIM, 255);
	kcell_canvas_series(cv, 0, 0, 64, 64, &s, KT_ACCENT);
	kcell_canvas_line(cv, 0, 0, 64, 64, 3, KT_TEXT, 255);
	ok(untouched_outside(cv, 10, 12, 30, 20),
	   "clip: fills, series and lines stay inside, and a nested clip "
	   "cannot widen it");
	kcell_canvas_clip_pop(cv);
	kcell_canvas_clip_pop(cv);
	kcell_canvas_fill(cv, 0, 0, 64, 64, KT_DIM, 255);
	all = 1;
	for (int i = 0; i < 64 * 64; i++)
		all &= bits(cv)[i] == 0xff404448u;
	ok(all, "clip: popped, a fill reaches the whole canvas again");

	/* A full stack refuses; a clear empties it. */
	for (int i = 0; i < 12; i++)
		refused += kcell_canvas_clip_push(cv, i, i, 10, 10) != 0;
	ok(refused == 4, "clip: the ninth push is refused");
	kcell_canvas_clear(cv);
	kcell_canvas_fill(cv, 0, 0, 64, 64, KT_DIM, 255);
	all = 1;
	for (int i = 0; i < 64 * 64; i++)
		all &= bits(cv)[i] == 0xff404448u;
	ok(all, "clip: a clear empties the stack");

	kcell_canvas_free(cv);
}

/* ── the chart ────────────────────────────────────────────────────────── */

static KchPlot chart(void)
{
	return (KchPlot){
		.a = wave,
		.na = NS,
		.b = wave + 30,
		.nb = NS - 30,
		.vmax = 100.0,
		.slot_a = KT_ACCENT,
		.slot_b = KT_WARN,
		.step = 1.0,
		.hold = 1,
		.line_w = 1.0,
		.area_alpha = 90,
		.line_alpha = 255,
		.rest_alpha = 90,
		.seq = 1000,
		.grid = 20,
		.grid_slot = KT_MID,
		.grid_alpha = 60,
		.plate_slot = KT_DIM,
		.plate_alpha = 55,
		.base_slot = KT_DIM,
		.base_alpha = 255,
	};
}

/* The pixels the drawn cell (x, y) resolves to, as the canvas they came
 * from: kch_plot() leaves the picture in the sprite table, not here. */
static const uint32_t *cell_pixels(int x, int y, int *stride)
{
	int gw = 0, gh = 0;
	const KtuiCell *c = ktui_draw_cells(&gw, &gh);
	uint32_t cp = c[y * gw + x].ch;
	const KtuiSprite *s;

	if (!KTUI_IS_SPRITE(cp))
		return NULL;
	s = ktui_sprite_get((int)KTUI_SPRITE_SLOT(cp));
	if (!s || !s->pix)
		return NULL;
	*stride = pixman_image_get_stride((pixman_image_t *)s->pix) / 4;
	return pixman_image_get_data((pixman_image_t *)s->pix);
}

static void check_chart(void)
{
	KCellCanvas *cv = kcell_canvas_new(14, 4, CW, CH, 1);	/* 112x64 */
	KchPlot p = chart();
	uint64_t h0;
	int slot0, st = 0;

	ok(cv != NULL, "chart: canvas");
	if (!cv)
		return;

	/* The pair, into part of a canvas the way the panel's strip does it. */
	sentinel(cv);
	kch_plot_draw(cv, 4, 0, 100, 64, &p);
	ok(untouched_outside(cv, 4, 0, 100, 64),
	   "chart: nothing outside its rectangle");
	ok(premultiplied(cv), "chart: premultiplied");
	/* The midline is the base, hard-edged and full weight, where no
	 * trace crosses it. */
	ok(px_at(cv, 4, 32) == 0xff404448u, "chart: the midline is at h/2");
	want_digest(cv, 0x128f9e7657fef671ULL, "chart-pair");

	/* One series, and the gridline of the newest sample's multiple. */
	p.b = NULL;
	p.nb = 0;
	p.seq = 1000;
	kcell_canvas_clear(cv);
	kch_plot_draw(cv, 0, 0, 112, 64, &p);
	ok((px_at(cv, 111, 0) >> 24) == 60,
	   "chart: sample 1000 of a grid of 20 is a gridline at the right edge");
	ok((px_at(cv, 91, 0) >> 24) == 60 && (px_at(cv, 92, 0) >> 24) == 55,
	   "chart: the next one is twenty samples left, on the plate");
	want_digest(cv, 0xddd9619ac19633dcULL, "chart-single");

	/* The mark: one sample's column, top to bottom, over everything, and
	 * part of what the tile is keyed on. */
	p.mark = 1;
	p.mark_slot = KT_TEXT;
	p.mark_alpha = 255;
	kcell_canvas_clear(cv);
	kch_plot_draw(cv, 0, 0, 112, 64, &p);
	ok((px_at(cv, 111, 0) >> 24) == 255 &&
		   px_at(cv, 111, 0) == px_at(cv, 111, 63) &&
		   px_at(cv, 110, 63) != px_at(cv, 111, 63),
	   "chart: mark 1 is the newest sample's column, top to bottom");
	{
		uint32_t mk = px_at(cv, 111, 0);

		p.mark = 21;
		kcell_canvas_clear(cv);
		kch_plot_draw(cv, 0, 0, 112, 64, &p);
		ok(px_at(cv, 91, 10) == mk && px_at(cv, 91, 63) == mk &&
			   px_at(cv, 111, 10) != mk && px_at(cv, 90, 10) != mk,
		   "chart: mark 21 is twenty samples left, and only there");
	}
	h0 = kch_plot_hash(&p);
	p.mark = 22;
	ok(kch_plot_hash(&p) != h0, "chart: a moved mark is new content");

	/* As a tile: rasterised when the content moves and only then. */
	p = chart();
	p.per_cell = 1;
	p.hold = 0;
	ok(kch_plot(7, krect(2, 3, 30, 5), &p, KT_ACCENT, KT_BG) == 0,
	   "chart: kch_plot draws a tile");
	h0 = kch_plot_hash(&p);
	slot0 = kch_tile_slot(7);
	ktui_draw_flush();
	ok(kch_plot(7, krect(2, 3, 30, 5), &p, KT_ACCENT, KT_BG) == 0 &&
		   kch_tile_slot(7) == slot0,
	   "chart: the same content is the same slot, no raster");
	p.seq++;
	ok(kch_plot_hash(&p) != h0, "chart: a new sample number is new content");
	ok(kch_plot(7, krect(2, 3, 30, 5), &p, KT_ACCENT, KT_BG) == 0 &&
		   kch_tile_slot(7) != slot0,
	   "chart: new content alternates the slot");
	{
		const uint32_t *px = cell_pixels(2, 3, &st);

		ok(px != NULL, "chart: the drawn cells are the tile's sprite");
	}
	kch_tile_drop(7);
	ok(kch_tile_slot(7) < 0, "chart: a dropped tile is gone");

	kcell_canvas_free(cv);
}

int main(int argc, char **argv)
{
	print = argc > 1 && !strcmp(argv[1], "--print");
	ktui_theme = &theme;
	ktui_backend_set(&backend);
	ktui_w = 120;
	ktui_h = 40;
	ktui_draw_resize();
	ktui_draw_clip_none();
	make_wave();

	check_series();
	check_line();
	check_clip();
	check_chart();

	if (print)
		return 0;
	if (fails) {
		printf("plotcheck: %d failed\n", fails);
		return 1;
	}
	printf("plotcheck: ok\n");
	return 0;
}

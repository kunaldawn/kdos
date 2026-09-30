/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   textcheck — display text: the op, its cell form and its pixels
 *
 * kch_px.c is INCLUDED so the recorded list, the key, the differ and the row
 * comparison can be read directly; the libkwl calls it makes are stubbed, and
 * the cell size is the loaded font's. kch_chrome.c is linked for real, since
 * the cell form lives there. A golden is a cell frame with the pixel layer
 * stubbed out, so everything display text does in pixels is checked here:
 *
 *   - with no backdrop, and with no slot cleared for one, it records nothing
 *     and kch_display_text() draws the string on the rectangle's first row;
 *   - with one, the rectangle's cells go blank on the backdrop's slot and one
 *     text op is recorded at `rows` cells of height — two when the band under
 *     it is another slot, whose flat rectangle comes first;
 *   - the key follows the string and not where it sits in the pool, and the
 *     differ answers the text's own rectangle when only the string changed;
 *   - a string or an op that does not fit is refused whole, nothing recorded
 *     and no cell touched;
 *   - replayed, every inked pixel lies in the rectangle (a string too long is
 *     cut, at scale 1 and 2), alignment puts the ink at the side asked for,
 *     and a replay under a clip region is the whole replay inside it and
 *     nothing outside — the premise of re-rasterising only what moved;
 *   - a flat body is the slot's colour, claims opaque, and has the same rows
 *     until an op reaches them.
 * ---------------------------------
 */

#include <stdio.h>

#include "kch_px.c"

/* ── libkwl, as far as kch_px.c asks ─────────────────────────────────── */

static int cell_w = 8, cell_h = 16, opaque_claim = -1;

int kwl_cell_w(void) { return cell_w; }
int kwl_cell_h(void) { return cell_h; }
void kwl_set_opaque(bool on) { opaque_claim = on; }
void kwl_set_backdrop(KDispBackdropFn fn) { (void)fn; }
void kwl_set_backdrop_cache(uint64_t (*key)(void), KwlBackdropBandFn band,
			    KwlBackdropDiffFn diff, KwlBackdropSameFn same)
{
	(void)key; (void)band; (void)diff; (void)same;
}
void kwl_set_pixels_dirty_fn(int (*fn)(void)) { (void)fn; }
void kwl_list_view(int x, int y, int w, int h, int top)
{
	(void)x; (void)y; (void)w; (void)h; (void)top;
}
/* The header band's icon, which nothing here draws. */
int kicon_slot(const char *name, int cw, int ch)
{
	(void)name; (void)cw; (void)ch;
	return -1;
}

static int fails, checks;

#define CHECK(c, ...)                                                        \
	do {                                                                 \
		checks++;                                                    \
		if (!(c)) {                                                  \
			fails++;                                             \
			fprintf(stderr, "textcheck:%d: ", __LINE__);         \
			fprintf(stderr, __VA_ARGS__);                        \
			fputc('\n', stderr);                                 \
		}                                                            \
	} while (0)

enum { COLS = 40, ROWS = 10 };

static const KtuiCell *cell(int x, int y)
{
	int w, h;
	const KtuiCell *c = ktui_draw_cells(&w, &h);

	return &c[y * w + x];
}

static void frame(void)
{
	ktui_draw_clear();
	ktui_draw_fill(krect(0, 0, COLS, ROWS), KT_SURFACE);
	kch_px_reset();
}

/* The ink of `img` outside (x, y, w, h) — any pixel not fully clear — and
 * the extent of the ink inside it. */
static int ink_outside(pixman_image_t *img, int x, int y, int w, int h,
		       int *minx, int *maxx)
{
	int iw = pixman_image_get_width(img), ih = pixman_image_get_height(img);
	int st = pixman_image_get_stride(img) / 4, out = 0;
	const uint32_t *px = pixman_image_get_data(img);

	*minx = iw;
	*maxx = -1;
	for (int j = 0; j < ih; j++)
		for (int i = 0; i < iw; i++) {
			if (!px[j * st + i])
				continue;
			if (i < x || i >= x + w || j < y || j >= y + h) {
				out++;
				continue;
			}
			if (i < *minx)
				*minx = i;
			if (i > *maxx)
				*maxx = i;
		}
	return out;
}

static pixman_image_t *canvas(int scale)
{
	return pixman_image_create_bits(PIXMAN_a8r8g8b8, COLS * cell_w * scale,
					ROWS * cell_h * scale, NULL, 0);
}

int main(void)
{
	char big[PX_POOL + 8];
	pixman_image_t *img, *part;
	int minx, maxx, out;

	if (kcell_font_load("monospace:pixelsize=16") != 0) {
		fprintf(stderr, "textcheck: no usable font\n");
		return 2;
	}
	cell_w = kcell_w();
	cell_h = kcell_h();
	ktui_offscreen_init(COLS, ROWS);
	ktui_draw_init();

	/* No backdrop: refused, and the cell form is the string on row one. */
	frame();
	CHECK(!kch_display_live(), "live with no backdrop");
	CHECK(!kch_px_text(2, 1, 6, 2, "42%", KT_TEXT, KT_ACCENT,
			   KCH_ALIGN_RIGHT) && nops == 0,
	      "recorded with no backdrop (%d ops)", nops);
	CHECK(kch_display_text(krect(2, 1, 6, 2), "42%", KT_TEXT, KT_ACCENT,
			       KCH_ALIGN_RIGHT) == 0,
	      "kch_display_text claimed pixels with no backdrop");
	CHECK(cell(5, 1)->ch == '4' && cell(6, 1)->ch == '2' &&
	      cell(7, 1)->ch == '%' && cell(7, 1)->fg == KT_TEXT,
	      "the cell form is not the string at the right of row one");
	CHECK(cell(2, 1)->ch == ' ' && cell(2, 1)->bg == KT_ACCENT &&
	      cell(7, 2)->ch == ' ' && cell(7, 2)->bg == KT_ACCENT,
	      "the cell form did not fill the rectangle with its band");
	frame();
	kch_display_text(krect(2, 1, 7, 2), "42%", KT_TEXT, KT_ACCENT,
			 KCH_ALIGN_CENTER);
	CHECK(cell(4, 1)->ch == '4' && cell(6, 1)->ch == '%',
	      "the cell form is not centred");

	/* A popup: the backdrop owns KT_SURFACE. */
	kch_px_popup(KT_SURFACE);
	frame();
	CHECK(kch_display_live(), "not live under a popup");
	CHECK(kch_display_text(krect(2, 1, 6, 2), "42%", KT_TEXT, KT_ACCENT,
			       KCH_ALIGN_RIGHT) == 1,
	      "kch_display_text drew cells under a popup");
	CHECK(nops == 2 && ops[0].kind == PXO_RECT &&
	      ops[0].x == 2 * cell_w && ops[0].y == cell_h &&
	      ops[0].w == 6 * cell_w && ops[0].h == 2 * cell_h &&
	      ops[0].rgb == kch_slot_rgb(KT_ACCENT) && ops[0].alpha == 0xFF,
	      "the band under the text is not a flat rectangle of its slot");
	CHECK(nops == 2 && ops[1].kind == PXO_TEXT && ops[1].x == ops[0].x &&
	      ops[1].y == ops[0].y && ops[1].w == ops[0].w &&
	      ops[1].h == ops[0].h && ops[1].size == 2 * cell_h &&
	      ops[1].align == KCH_ALIGN_RIGHT &&
	      ops[1].rgb == kch_slot_rgb(KT_TEXT) &&
	      !strcmp(pool + ops[1].str, "42%"),
	      "the text op is not the rectangle at two rows of the cell");
	CHECK(cell(2, 1)->ch == ' ' && cell(7, 2)->ch == ' ' &&
	      cell(2, 1)->bg == KT_SURFACE && cell(7, 2)->bg == KT_SURFACE &&
	      cell(1, 1)->bg == KT_SURFACE,
	      "the cells over the text are not blank on the backdrop's slot");
	frame();
	kch_px_text(2, 1, 6, 2, "42%", KT_TEXT, KT_SURFACE, KCH_ALIGN_LEFT);
	CHECK(nops == 1 && ops[0].kind == PXO_TEXT,
	      "text on the backdrop's own slot recorded a band (%d ops)", nops);

	/* The key follows the string, not the pool. */
	{
		uint64_t k1, k2;
		struct px_op a[2];
		pixman_region32_t r;
		pixman_box32_t *e;

		frame();
		kch_px_text(2, 1, 6, 2, "42%", KT_TEXT, KT_SURFACE, 0);
		k1 = pic_key();
		frame();
		kch_px_text(2, 1, 6, 2, "42%", KT_TEXT, KT_SURFACE, 0);
		CHECK(pic_key() == k1, "the same string keyed differently");
		frame();
		kch_px_text(2, 1, 6, 2, "43%", KT_TEXT, KT_SURFACE, 0);
		k2 = pic_key();
		CHECK(k2 != k1, "a changed string kept the key");

		frame();
		kch_px_text(2, 1, 6, 2, "7", KT_TEXT, KT_SURFACE, 0);
		kch_px_text(20, 4, 6, 2, "hello", KT_TEXT, KT_SURFACE, 0);
		memcpy(a, ops, sizeof(a));
		k1 = px_key();
		frame();
		kch_px_text(2, 1, 6, 2, "123456", KT_TEXT, KT_SURFACE, 0);
		kch_px_text(20, 4, 6, 2, "hello", KT_TEXT, KT_SURFACE, 0);
		CHECK(ops[1].str != a[1].str && op_eq(&ops[1], &a[1]),
		      "the same text at another pool offset compared unequal");
		pixman_region32_init(&r);
		CHECK(pic_diff(k1, COLS * cell_w, ROWS * cell_h, 1, &r) == 0,
		      "the differ did not know a key it handed out");
		e = pixman_region32_extents(&r);
		CHECK(pixman_region32_n_rects(&r) == 1 && e->x1 == 2 * cell_w &&
		      e->y1 == cell_h && e->x2 == 8 * cell_w &&
		      e->y2 == 3 * cell_h,
		      "the differ answered more or less than the changed "
		      "text's rectangle");
		pixman_region32_fini(&r);
	}

	/* Refused whole. */
	frame();
	ktui_draw_text(3, 1, 1, "x", KT_TEXT, KT_SURFACE, 0);
	memset(big, 'W', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	CHECK(!kch_px_text(2, 1, 6, 2, big, KT_TEXT, KT_ACCENT, 0) &&
	      nops == 0 && cell(3, 1)->ch == 'x',
	      "a string past the pool was recorded or touched a cell");
	frame();
	for (int i = 0; i < PX_MAX - 1; i++)
		kch_px_rect(0, 0, 1, 1, 0, 0xFF);
	CHECK(!kch_px_text(2, 1, 6, 2, "1", KT_TEXT, KT_ACCENT, 0) &&
	      nops == PX_MAX - 1,
	      "text and its band recorded into one free op");
	CHECK(kch_px_text(2, 1, 6, 2, "1", KT_TEXT, KT_SURFACE, 0) &&
	      nops == PX_MAX, "text alone refused its one free op");

	/* The pixels, at both scales. */
	for (int scale = 1; scale <= 2; scale++) {
		int x = 4 * cell_w * scale, y = 2 * cell_h * scale;
		int w = 10 * cell_w * scale, h = 2 * cell_h * scale;
		const uint32_t *d;
		int full = 0, tw;

		frame();
		kch_px_text(4, 2, 10, 2, "88", KT_TEXT, KT_SURFACE,
			    KCH_ALIGN_RIGHT);
		img = canvas(scale);
		kch_px_replay(img, scale);
		out = ink_outside(img, x, y, w, h, &minx, &maxx);
		CHECK(out == 0 && maxx >= 0,
		      "scale %d: %d pixels inked outside the rectangle, "
		      "ink %s", scale, out, maxx >= 0 ? "present" : "absent");
		tw = kcell_canvas_text_width(2 * cell_h * scale, "88");
		CHECK(tw > 0 && maxx >= x + w - cell_w * scale &&
		      minx >= x + w - tw - scale,
		      "scale %d: right-aligned ink spans %d..%d of %d..%d, "
		      "advance %d", scale, minx, maxx, x, x + w - 1, tw);
		d = pixman_image_get_data(img);
		for (int i = 0; i < pixman_image_get_width(img) *
					pixman_image_get_height(img); i++)
			if (d[i] == (0xFF000000u | kch_slot_rgb(KT_TEXT)))
				full++;
		CHECK(full > 0, "scale %d: no pixel is the full ink colour",
		      scale);
		pixman_image_unref(img);

		frame();
		kch_px_text(4, 2, 10, 2, "88", KT_TEXT, KT_SURFACE,
			    KCH_ALIGN_LEFT);
		img = canvas(scale);
		kch_px_replay(img, scale);
		ink_outside(img, x, y, w, h, &minx, &maxx);
		CHECK(minx >= x && minx < x + cell_w * scale && maxx < x + tw,
		      "scale %d: left-aligned ink spans %d..%d", scale, minx,
		      maxx);
		pixman_image_unref(img);

		/* Too long for its rectangle: cut, never spilled. */
		frame();
		kch_px_text(4, 2, 2, 2, "WWWWWWWW", KT_TEXT, KT_ACCENT,
			    KCH_ALIGN_LEFT);
		img = canvas(scale);
		kch_px_replay(img, scale);
		out = ink_outside(img, x, y, 2 * cell_w * scale, h, &minx,
				  &maxx);
		CHECK(out == 0, "scale %d: a long string spilled %d pixels",
		      scale, out);
		pixman_image_unref(img);
	}

	/* Under a clip region: the whole replay inside, nothing outside. */
	{
		pixman_region32_t clip;
		const uint32_t *a, *b;
		int st, iw, ih, bad = 0, cx0 = 5 * cell_w + 3, cy0 = cell_h + 5,
			    cw0 = 3 * cell_w, ch0 = cell_h;

		frame();
		kch_px_text(4, 1, 10, 2, "1987", KT_TEXT, KT_ACCENT,
			    KCH_ALIGN_CENTER);
		img = canvas(1);
		part = canvas(1);
		kch_px_replay(img, 1);
		pixman_region32_init_rect(&clip, cx0, cy0, (unsigned)cw0,
					  (unsigned)ch0);
		pixman_image_set_clip_region32(part, &clip);
		kch_px_replay(part, 1);
		pixman_image_set_clip_region32(part, NULL);
		pixman_region32_fini(&clip);
		a = pixman_image_get_data(img);
		b = pixman_image_get_data(part);
		st = pixman_image_get_stride(img) / 4;
		iw = pixman_image_get_width(img);
		ih = pixman_image_get_height(img);
		for (int j = 0; j < ih; j++)
			for (int i = 0; i < iw; i++) {
				int in = i >= cx0 && i < cx0 + cw0 && j >= cy0 &&
					 j < cy0 + ch0;

				if (in ? a[j * st + i] != b[j * st + i]
				       : b[j * st + i] != 0)
					bad++;
			}
		CHECK(bad == 0, "%d pixels differ between a clipped replay and "
				"the whole one", bad);
		pixman_image_unref(img);
		pixman_image_unref(part);
	}

	CHECK(kch_display_cols("88", 2) ==
		      (kcell_canvas_text_width(2 * cell_h, "88") + cell_w - 1) /
			      cell_w &&
	      kch_display_cols("88", 2) > 0,
	      "kch_display_cols is not the ink's width in whole cells");

	/* A flat body. */
	kch_px_flat(KT_BG);
	frame();
	CHECK(kch_display_live(), "not live over a flat body");
	px_key();
	CHECK(opaque_claim == 1, "a flat body did not claim opaque");
	CHECK(px_rows_same(COLS * cell_w, ROWS * cell_h, 1, 0, cell_h,
			   4 * cell_h),
	      "a flat body with no ops has rows that differ");
	kch_px_text(2, 1, 6, 2, "5", KT_TEXT, KT_BG, 0);
	CHECK(!px_rows_same(COLS * cell_w, ROWS * cell_h, 1, 0, cell_h,
			    4 * cell_h),
	      "rows under display text answered the same");
	img = canvas(1);
	flat_draw(img, COLS * cell_w, ROWS * cell_h, 1);
	CHECK(pixman_image_get_data(img)[0] ==
		      (0xFF000000u | kch_slot_rgb(KT_BG)),
	      "a flat body is not its slot's colour, opaque");
	pixman_image_unref(img);

	fprintf(stderr, "textcheck: %d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}

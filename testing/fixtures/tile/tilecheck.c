/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   tilecheck — kch_tile past one sprite, and the lifetime of its views
 *
 * A tile larger than 16x16 cells is a grid of sprites over views of one
 * canvas. Everything that can go wrong with that goes wrong as PIXELS or as a
 * FREE, never as a return code: a block cut at the wrong origin shows another
 * block's pixels, and a view the table's evictor unrefs one time too many is
 * freed while this file's tile still names it. So the check reads the pixels
 * the drawn cells resolve to, cell by cell, against what was rasterised — and
 * counts every view's destruction through a destroy function hung on it.
 *
 * The evictor is kcell_tile_free, the one the shell's picture path registers
 * and the one that UNREFS. The cases:
 *
 *   - a 40x20 tile (3x2 blocks) publishes, alternates halves, and every cell
 *     shows the right pixel;
 *   - a full table evicts the half that is not drawn: no view is destroyed,
 *     and the next commit publishes it again;
 *   - the drawn half evicted while undrawn comes back on the next begin with
 *     no raster; so does every block after a ktui_sprite_clear();
 *   - a byte budget that fits one half refuses the other ALL OR NOTHING —
 *     including when every put succeeded but made room by evicting a block
 *     the same commit had just put — and the picture that was up stays up;
 *   - a 16x2 tile, the panel meters' shape, is one sprite under the key it
 *     has always had;
 *   - with no evictor a refused put mid-commit takes back the blocks the
 *     same commit had put;
 *   - a re-put of a view the table still names takes no second reference;
 *   - reset destroys every view exactly once, with and without an evictor.
 *
 * Run under ASan by `CC="cc -fsanitize=address,undefined" selftest.sh`, a
 * view unref'd once too often is a use-after-free report as well as a count.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kchrome.h"
#include "kdisp.h"
#include "kicon.h"

#define CW 4
#define CH 8

/* The display and the icon switch are all kch_tile.c asks libkdisp and
 * libkicon, so they are stubbed rather than linked. */
int kicon_enabled(void) { return 1; }
int kdisp_cell_w(void) { return CW; }
int kdisp_cell_h(void) { return CH; }
int kdisp_scale(void) { return 1; }

static int fails;

static void ok(int c, const char *what)
{
	if (!c) {
		printf("FAIL  %s\n", what);
		fails++;
	}
}

static void flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)ff;
	memcpy(prev, cur, (size_t)w * h * sizeof(*prev));
}

static void size(int *w, int *h)
{
	*w = 200;
	*h = 60;
}

static int caps(void) { return 0; }

static int poll_ev(KtuiEvent *ev, int ms)
{
	(void)ev;
	(void)ms;
	return 0;
}

static const KtuiBackend backend = {
	.name = "tilecheck",
	.flush = flush,
	.poll_event = poll_ev,
	.size = size,
	.caps = caps,
};

/* ── views, counted ───────────────────────────────────────────────────── */

#define MAXV 256
static const void *seen[MAXV];
static int nseen, ndestroyed;

static void on_destroy(pixman_image_t *img, void *data)
{
	(void)img;
	(void)data;
	ndestroyed++;
}

/* Hang the counter on every picture the table names for the tile's cells. */
static void note_views(int x0, int y0, int w, int h)
{
	int gw = 0, gh = 0;
	const KtuiCell *c = ktui_draw_cells(&gw, &gh);

	for (int y = y0; y < y0 + h; y++)
		for (int x = x0; x < x0 + w; x++) {
			uint32_t cp = c[y * gw + x].ch;
			const KtuiSprite *s;
			int known = 0;

			if (!KTUI_IS_SPRITE(cp))
				continue;
			s = ktui_sprite_get((int)KTUI_SPRITE_SLOT(cp));
			if (!s)
				continue;
			for (int i = 0; i < nseen; i++)
				known |= seen[i] == s->pix;
			if (!known && nseen < MAXV) {
				seen[nseen++] = s->pix;
				pixman_image_set_destroy_function(
					(pixman_image_t *)s->pix, on_destroy,
					NULL);
			}
		}
}

/* ── the picture ──────────────────────────────────────────────────────── */

static uint32_t pattern(int x, int y, unsigned seed)
{
	return 0xff000000u | ((seed * 0x9e3779b9u) ^ (unsigned)(x * 7919 + y * 104729)) >> 8;
}

static void paint(KCellCanvas *cv, unsigned seed)
{
	pixman_image_t *img = kcell_canvas_image(cv);
	uint32_t *d = pixman_image_get_data(img);
	int stride = pixman_image_get_stride(img) / 4;

	for (int y = 0; y < kcell_canvas_h(cv); y++)
		for (int x = 0; x < kcell_canvas_w(cv); x++)
			d[y * stride + x] = pattern(x, y, seed);
}

/* Every cell of the drawn tile must be a sprite whose picture, at the cell's
 * sub-cell coordinate, is exactly the rasterised pattern at that cell. */
static int verify(int x0, int y0, int w, int h, unsigned seed)
{
	int gw = 0, gh = 0;
	const KtuiCell *c = ktui_draw_cells(&gw, &gh);

	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++) {
			uint32_t cp = c[(y0 + j) * gw + x0 + i].ch;
			const KtuiSprite *s;
			pixman_image_t *pix;
			const uint32_t *d;
			int st;

			if (!KTUI_IS_SPRITE(cp))
				return 0;
			s = ktui_sprite_get((int)KTUI_SPRITE_SLOT(cp));
			if (!s || !s->pix)
				return 0;
			pix = (pixman_image_t *)s->pix;
			d = pixman_image_get_data(pix);
			st = pixman_image_get_stride(pix) / 4;
			for (int py = 0; py < CH; py++)
				for (int px = 0; px < CW; px++) {
					int sx = (int)KTUI_SPRITE_SX(cp) * CW + px;
					int sy = (int)KTUI_SPRITE_SY(cp) * CH + py;

					if (d[sy * st + sx] !=
					    pattern(i * CW + px, j * CH + py, seed))
						return 0;
				}
		}
	return 1;
}

static void frame_clear(void)
{
	ktui_draw_clear();
	ktui_draw_flush();
}

/* Draw the tile, present it, and check it. */
static int show(int id, int x0, int y0, int w, int h, unsigned seed)
{
	ktui_draw_clear();
	if (kch_tile_draw(id, krect(x0, y0, w, h), KT_TEXT, KT_SURFACE) < 0)
		return 0;
	note_views(x0, y0, w, h);
	ktui_draw_flush();
	return verify(x0, y0, w, h, seed);
}

/* Fill the table with pictures the table owns (the evictor frees them), so
 * whatever is not on the screen is evicted to make room. */
static void flood(uint64_t base, int n)
{
	for (int i = 0; i < n; i++) {
		pixman_image_t *p =
			pixman_image_create_bits(PIXMAN_a8r8g8b8, 1, 1, NULL, 0);

		if (ktui_sprite_put(base + (uint64_t)i, p, 1, 1, ' ') < 0)
			pixman_image_unref(p);
	}
}

static uint64_t half_key(int id, int k, int block)
{
	return (((uint64_t)0x71 << 56) | ((uint64_t)(unsigned)id << 8) |
		(uint64_t)k) ^ ((uint64_t)block * KTUI_TILE_STRIDE);
}

/* How many blocks of half k are in the table. */
static int in_table(int id, int k, int nblocks)
{
	int n = 0;

	for (int i = 0; i < nblocks; i++)
		n += ktui_sprite_find(half_key(id, k, i)) >= 0;
	return n;
}

static size_t tile_bytes(int w, int h)
{
	return (size_t)w * CW * h * CH * 4;
}

enum { BIG = 3, SMALL = 4 };

static void big_tile(void)
{
	const int W = 40, H = 20, X = 10, Y = 5, NB = 6;
	KCellCanvas *cv;
	int s1, s2, s3;

	/* ── publish, alternate ────────────────────────────────────────── */
	cv = kch_tile_begin(BIG, W, H, 1);
	ok(cv != NULL, "a 40x20 tile is handed a canvas");
	if (!cv)
		return;
	ok(kcell_canvas_w(cv) == W * CW && kcell_canvas_h(cv) == H * CH,
	   "the canvas is the whole tile");
	paint(cv, 1);
	s1 = kch_tile_commit(BIG);
	ok(s1 >= 0, "the commit publishes every block");
	ok(in_table(BIG, 1, NB) == NB, "six blocks under the half's keys");
	ok(show(BIG, X, Y, W, H, 1), "every cell shows its own pixel");

	cv = kch_tile_begin(BIG, W, H, 2);
	ok(cv != NULL, "a new content is a new raster");
	if (!cv)
		return;
	paint(cv, 2);
	s2 = kch_tile_commit(BIG);
	ok(s2 >= 0 && s2 != s1, "into the other half's slots");
	ok(show(BIG, X, Y, W, H, 2), "and every cell shows the new picture");
	ok(kch_tile_begin(BIG, W, H, 2) == NULL,
	   "the content that is up is not rasterised again");

	/* ── the undrawn half is evicted, the view survives ────────────── */
	flood(0x1000, KTUI_MAX_SPRITES);
	ok(in_table(BIG, 1, NB) == 0, "a full table evicts the undrawn half");
	ok(in_table(BIG, 0, NB) == NB, "and never the half on the screen");
	ok(ndestroyed == 0, "an eviction destroys no view");
	cv = kch_tile_begin(BIG, W, H, 3);
	ok(cv != NULL, "the next content rasterises");
	if (!cv)
		return;
	paint(cv, 3);
	s3 = kch_tile_commit(BIG);
	ok(s3 >= 0, "and publishes the evicted half again");
	ok(show(BIG, X, Y, W, H, 3), "whose views still hold the pixels");

	/* ── the drawn half evicted while undrawn ──────────────────────── */
	frame_clear();
	flood(0x20000, KTUI_MAX_SPRITES);
	ok(in_table(BIG, 1, NB) == 0, "an undrawn tile's published half is "
				      "evictable too");
	ok(kch_tile_slot(BIG) < 0, "and the tile then says it is not up");
	ok(kch_tile_draw(BIG, krect(X, Y, W, H), KT_TEXT, KT_SURFACE) < 0,
	   "nor draws a hole");
	ok(kch_tile_begin(BIG, W, H, 3) == NULL,
	   "the same content is put back without a raster");
	ok(show(BIG, X, Y, W, H, 3), "from the pixels the canvas kept");
	ok(ndestroyed == 0, "still no view destroyed");

	/* ── a cleared table hands every reference back ───────────────── */
	ktui_sprite_clear();
	ok(ndestroyed == 0, "a clear gives back the table's references only");
	/*
	 * The budget is declared on the EMPTY table: a sprite is counted at
	 * the cell size declared when it was put, and one put before any was
	 * declared would be given back at a size it was never charged.
	 *
	 * One half and three blocks. The refused half's first three puts fit,
	 * the fourth makes room by evicting the first — nothing draws that
	 * half yet — and every later one fits: every put succeeds and the
	 * half is still missing a block.
	 */
	ktui_sprite_budget(tile_bytes(W, H) + tile_bytes(16, 16) * 2 +
				   tile_bytes(8, 16),
			   CW, CH);
	ok(kch_tile_begin(BIG, W, H, 3) == NULL, "and the tile comes back");
	ok(show(BIG, X, Y, W, H, 3), "whole");

	/* ── a budget that fits one half: all or nothing ──────────────── */
	s3 = kch_tile_slot(BIG);
	cv = kch_tile_begin(BIG, W, H, 4);
	ok(cv != NULL, "a content the table cannot take rasterises");
	if (!cv)
		return;
	paint(cv, 4);
	ok(kch_tile_commit(BIG) == s3,
	   "the refused commit answers the slot still up");
	ok(in_table(BIG, 0, NB) == 0,
	   "no block of the refused half stays behind — even one whose room "
	   "was made by evicting a block of the same commit");
	ok(show(BIG, X, Y, W, H, 3), "and the picture that was up stays up");
	ok(ndestroyed == 0, "the refusal destroys no view");
	ktui_sprite_budget(0, 0, 0);

	/* ── a resize frees the old views and nothing else ─────────────── */
	cv = kch_tile_begin(BIG, 20, 3, 5);
	ok(cv != NULL, "a resize is a new tile");
	ok(ndestroyed == nseen, "whose predecessor's views are all destroyed");
	ok(in_table(BIG, 0, NB) == 0 && in_table(BIG, 1, NB) == 0,
	   "and whose predecessor's blocks are gone from the table");
	if (!cv)
		return;
	paint(cv, 5);
	ok(kch_tile_commit(BIG) >= 0 && show(BIG, X, Y, 20, 3, 5),
	   "20x3: two blocks side by side");
}

static void small_tile(void)
{
	KCellCanvas *cv = kch_tile_begin(SMALL, 16, 2, 7);
	int s;

	ok(cv != NULL, "a 16x2 tile is handed a canvas");
	if (!cv)
		return;
	paint(cv, 7);
	s = kch_tile_commit(SMALL);
	ok(s >= 0 && ktui_sprite_find(half_key(SMALL, 1, 0)) == s,
	   "one sprite, under the key the half has always had");
	ok(ktui_sprite_get(s) && ktui_sprite_get(s)->w == 16 &&
		   ktui_sprite_get(s)->h == 2,
	   "sized to the whole tile");
	ok(show(SMALL, 150, 50, 16, 2, 7), "and every cell shows its pixel");

	/* Three commits with nothing evicted: the third re-puts views the
	 * table still names, which must take no second reference — reset
	 * below counts every view destroyed. */
	for (unsigned c = 8; c <= 9; c++) {
		cv = kch_tile_begin(SMALL, 16, 2, c);
		ok(cv != NULL, "each new content rasterises");
		if (!cv)
			return;
		paint(cv, c);
		ok(kch_tile_commit(SMALL) >= 0 && show(SMALL, 150, 50, 16, 2, c),
		   "and shows");
	}
	ok(ktui_sprite_find(half_key(SMALL, 0, 0)) >= 0 &&
		   ktui_sprite_find(half_key(SMALL, 1, 0)) >= 0,
	   "both halves stay in the table between commits");
}

static void run(const char *what)
{
	int before = fails;

	nseen = ndestroyed = 0;
	ktui_draw_clear();
	ktui_draw_flush();
	big_tile();
	small_tile();
	kch_tile_reset();
	ok(ndestroyed == nseen, "reset destroys every view, once");
	ok(in_table(BIG, 0, 6) + in_table(BIG, 1, 6) +
			   in_table(SMALL, 0, 1) + in_table(SMALL, 1, 1) == 0,
	   "and leaves nothing of the tiles in the table");
	ok(kch_tile_begin(BIG, 1000, 2, 1) == NULL,
	   "a tile wider than the grid is refused");
	printf("%s  %s (%d views)\n", fails == before ? "ok  " : "FAIL",
	       what, nseen);
}

int main(void)
{
	ktui_backend_set(&backend);
	ktui_w = 200;
	ktui_h = 60;
	ktui_draw_resize();
	ktui_draw_clip_none();

	ktui_sprite_evictor(kcell_tile_free, NULL);
	run("tiles under the picture path's evictor");

	ktui_sprite_clear();
	ktui_sprite_evictor(NULL, NULL);
	nseen = ndestroyed = 0;
	{
		int before = fails;
		KCellCanvas *cv;
		int s;

		/* One half and one 16x16 block: with nothing evictable the
		 * other half's second put is REFUSED, and the first must not
		 * stay behind. */
		ktui_sprite_budget(tile_bytes(40, 20) + tile_bytes(16, 16), CW,
				   CH);
		cv = kch_tile_begin(BIG, 40, 20, 9);
		ok(cv != NULL, "no evictor: a canvas");
		if (cv) {
			paint(cv, 9);
			s = kch_tile_commit(BIG);
			ok(s >= 0 && show(BIG, 0, 0, 40, 20, 9),
			   "no evictor: publishes");
			cv = kch_tile_begin(BIG, 40, 20, 10);
			ok(cv != NULL, "no evictor: the next content rasterises");
			if (cv) {
				paint(cv, 10);
				ok(kch_tile_commit(BIG) == s,
				   "no evictor: a refused put keeps the slot up");
				ok(in_table(BIG, 0, 6) == 0,
				   "no evictor: the blocks put before the "
				   "refusal are taken out again");
				ok(show(BIG, 0, 0, 40, 20, 9),
				   "no evictor: the picture that was up stays");
			}
		}
		ktui_sprite_budget(0, 0, 0);
		kch_tile_reset();
		ok(ndestroyed == nseen && nseen == 6,
		   "no evictor: reset destroys every view");
		printf("%s  tiles with no evictor (%d views)\n",
		       fails == before ? "ok  " : "FAIL", nseen);
	}
	return fails ? 1 : 0;
}

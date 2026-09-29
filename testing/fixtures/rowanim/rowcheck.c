/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   rowcheck — the travelling selection plate (kch_px_row_anim)
 *
 * kch_px.c is INCLUDED so the recorded op list can be read directly, and the
 * handful of libkwl calls it makes are stubbed: the cell size is a variable
 * here, and the backend is a table whose only answer is whether it keeps a
 * frame clock. Time is driven by hand through ktui_anim_set_clock().
 *
 * Every failure of this code is a plate in the wrong place for a few frames,
 * which no golden can show (a golden is a cell frame with the pixel layer
 * stubbed out), so the cases are the rules themselves:
 *
 *   - at rest it records exactly what kch_px_row() records;
 *   - a new item slides from the old row with an ease-out, reaches the new
 *     row at KCH_ROW_ANIM_MS and not before, and keeps the clock live until
 *     then; a move into another column slides x and stretches w as well;
 *   - a second move mid-slide sets out from where the plate stands;
 *   - the same item somewhere else (a scroll) lands at once;
 *   - a plate missing from the list before this one, a changed cell size, a
 *     backdrop installed afresh, a backend with no frame clock and
 *     `motion = no` all land at once;
 *   - four keys are four plates, and a fifth evicts the one asked least
 *     recently and disturbs no other;
 *   - a selection moved to a row the plate already stands on starts nothing.
 * ---------------------------------
 */

#include <stdio.h>

#include "kch_px.c"

/* ── libkwl, as far as kch_px.c asks ─────────────────────────────────── */

static int cell_w = 8, cell_h = 16;

int kwl_cell_w(void) { return cell_w; }
int kwl_cell_h(void) { return cell_h; }
void kwl_set_opaque(bool on) { (void)on; }
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

/* ── the clock, the backend and the motion switch ────────────────────── */

static int64_t now_ms = 1000;
static int64_t fake_now(void) { return now_ms; }
static int clock_on = 1, motion_on = 1;
static int be_animates(void) { return clock_on; }
static int be_motion(void) { return motion_on; }
static const KtuiBackend be = { .animates = be_animates };

static int fails, checks;

#define CHECK(c, ...)                                                        \
	do {                                                                 \
		checks++;                                                    \
		if (!(c)) {                                                  \
			fails++;                                             \
			fprintf(stderr, "rowcheck:%d: ", __LINE__);          \
			fprintf(stderr, __VA_ARGS__);                        \
			fputc('\n', stderr);                                 \
		}                                                            \
	} while (0)

/* One frame of a surface: drop the list, record the selection. Returns the
 * plate's op (the rounded one); the bar is the op after it. */
static const struct px_op *frame(int key, int item, int cx, int cy, int cw)
{
	kch_px_reset();
	kch_px_row_anim(key, item, cx, cy, cw, KCH_T_ACTIVE);
	return nops >= 2 ? &ops[0] : NULL;
}

/* The plate a still row at (cx, cy, cw) records, and the bar with it. */
static int at_rest(const struct px_op *p, int cx, int cy, int cw)
{
	return p && p->kind == PXO_ROUND && p->x == cx * cell_w + 1 &&
	       p->y == cy * cell_h + 1 && p->w == cw * cell_w - 2 &&
	       p->h == cell_h - 2 && p[1].kind == PXO_RECT &&
	       p[1].x == p->x && p[1].y == p->y + 1;
}

static void fresh(void)
{
	kch_px_popup(KT_SURFACE);
	now_ms += 10000;
}

int main(void)
{
	const struct px_op *p;
	struct px_op want[2];

	ktui_anim_set_clock(fake_now);
	ktui_anim_set_motion_fn(be_motion);
	ktui_backend_set(&be);

	/* At rest: byte for byte what kch_px_row() records. */
	fresh();
	kch_px_reset();
	kch_px_row(1, 3, 20, KCH_T_ACTIVE);
	CHECK(nops == 2, "kch_px_row recorded %d ops", nops);
	memcpy(want, ops, sizeof(want));
	p = frame(0, 2, 1, 3, 20);
	CHECK(nops == 2 && op_eq(&ops[0], &want[0]) && op_eq(&ops[1], &want[1]),
	      "a first plate is not kch_px_row's plate");
	CHECK(!ktui_anim_live(), "a first plate started the clock");
	p = frame(0, 2, 1, 3, 20);
	CHECK(at_rest(p, 1, 3, 20), "an unchanged plate moved");

	/* A new item slides: at the start it stands on the old row. */
	p = frame(0, 3, 1, 4, 20);
	CHECK(at_rest(p, 1, 3, 20), "a slide did not start on the old row (y %d)",
	      p ? p->y : -1);
	CHECK(ktui_anim_live(), "a slide left the clock idle");
	{
		int last = p ? p->y : 0, y1 = 3 * cell_h + 1, y2 = 4 * cell_h + 1;
		int half = 0;

		for (int t = 10; t < KCH_ROW_ANIM_MS; t += 10) {
			now_ms += 10;
			p = frame(0, 3, 1, 4, 20);
			CHECK(p && p->y >= last && p->y <= y2,
			      "t=%d: y %d went back or past (last %d)", t,
			      p ? p->y : -1, last);
			CHECK(t > KCH_ROW_ANIM_MS / 2 || (p && p->y < y2),
			      "t=%d: arrived early", t);
			if (t == 40)
				half = p ? p->y : 0;
			last = p ? p->y : last;
			/* The bar travels with the plate. */
			CHECK(p && p[1].y == p->y + 1, "the bar left the plate");
		}
		/* Ease-out: past the middle of the distance before half time. */
		CHECK(half > (y1 + y2) / 2, "not an ease-out: %d at half time "
		      "between %d and %d", half, y1, y2);
		now_ms += 10;
		p = frame(0, 3, 1, 4, 20);
		CHECK(at_rest(p, 1, 4, 20), "not on the new row at the end");
		now_ms += 1;
		CHECK(!ktui_anim_live(), "the clock outlived the slide");
	}

	/* Mid-slide redirect: sets out from where it stands. */
	now_ms += 1000;
	p = frame(0, 5, 1, 8, 20);		/* 4 -> 8 */
	now_ms += KCH_ROW_ANIM_MS / 3;
	p = frame(0, 5, 1, 8, 20);
	{
		int mid = p ? p->y : -1;

		p = frame(0, 6, 1, 2, 20);	/* turn back to row 2 */
		CHECK(p && p->y == mid, "a redirect jumped: %d, stood at %d",
		      p ? p->y : -1, mid);
		now_ms += KCH_ROW_ANIM_MS;
		p = frame(0, 6, 1, 2, 20);
		CHECK(at_rest(p, 1, 2, 20), "a redirect did not land");
	}

	/* Another column: x and w travel too. */
	now_ms += 1000;
	p = frame(0, -2, 30, 2, 12);
	CHECK(at_rest(p, 1, 2, 20), "a column move did not start in place");
	now_ms += KCH_ROW_ANIM_MS / 2;
	p = frame(0, -2, 30, 2, 12);
	CHECK(p && p->x > 1 * cell_w + 1 && p->x < 30 * cell_w + 1 &&
	      p->w < 20 * cell_w - 2 && p->w > 12 * cell_w - 2,
	      "a column move did not slide and stretch (x %d w %d)",
	      p ? p->x : -1, p ? p->w : -1);
	now_ms += KCH_ROW_ANIM_MS;
	p = frame(0, -2, 30, 2, 12);
	CHECK(at_rest(p, 30, 2, 12), "a column move did not land");

	/* The same item carried by a scroll: at once. */
	now_ms += 1000;
	p = frame(0, -2, 30, 5, 12);
	CHECK(at_rest(p, 30, 5, 12), "a scrolled item slid (y %d)",
	      p ? p->y : -1);
	CHECK(!ktui_anim_live(), "a scroll started the clock");

	/* Even mid-slide: a scroll during one lands the plate. */
	p = frame(0, 7, 30, 9, 12);
	now_ms += 20;
	p = frame(0, 7, 30, 6, 12);
	CHECK(at_rest(p, 30, 6, 12), "a scroll mid-slide did not land");

	/* Missing from the list before: lands. */
	now_ms += 1000;
	frame(0, 7, 30, 6, 12);
	kch_px_reset();			/* a frame with the selection off-screen */
	p = frame(0, 8, 30, 10, 12);
	CHECK(at_rest(p, 30, 10, 12), "a plate slid from a row nobody saw");

	/* Several passes of one layout (reset, record, reset, record) still
	 * slide: the key is in the list before each. */
	now_ms += 1000;
	frame(0, 9, 30, 3, 12);
	now_ms += 20;
	p = frame(0, 9, 30, 3, 12);
	CHECK(p && !at_rest(p, 30, 3, 12), "a second pass landed the slide");

	/* A changed cell size: lands. */
	now_ms += 1000;
	frame(0, 9, 30, 3, 12);
	cell_h = 20;
	p = frame(0, 10, 30, 4, 12);
	CHECK(at_rest(p, 30, 4, 12), "a changed cell size slid");
	cell_h = 16;
	frame(0, 10, 30, 4, 12);

	/* A backdrop installed afresh: lands. */
	fresh();
	frame(0, 1, 1, 1, 20);
	fresh();
	p = frame(0, 2, 1, 7, 20);
	CHECK(at_rest(p, 1, 7, 20), "a fresh surface slid from the last one");

	/* No frame clock, and motion off: lands. */
	clock_on = 0;
	p = frame(0, 3, 1, 9, 20);
	CHECK(at_rest(p, 1, 9, 20), "moved with no frame clock");
	clock_on = 1;
	motion_on = 0;
	p = frame(0, 4, 1, 2, 20);
	CHECK(at_rest(p, 1, 2, 20), "moved with motion = no");
	CHECK(!ktui_anim_live(), "motion = no started the clock");
	motion_on = 1;

	/* A new item on the row it already stands on starts nothing. */
	now_ms += 1000;
	p = frame(0, 5, 1, 2, 20);
	CHECK(at_rest(p, 1, 2, 20) && !ktui_anim_live(),
	      "a move onto the same row started the clock");

	/* Four keys are four plates; a fifth evicts the one asked least
	 * recently, whichever slot it sits in, and no other. */
	fresh();
	kch_px_reset();
	for (int k = 0; k < 4; k++)
		kch_px_row_anim(k, 0, 1, k, 20, KCH_T_ACTIVE);
	kch_px_reset();
	for (int i = 0; i < 4; i++) {
		int k = (i + 1) % 4;	/* 1 2 3 0: key 1 is now the stalest */

		kch_px_row_anim(k, 1, 1, k + 4, 20, KCH_T_ACTIVE);
		CHECK(at_rest(&ops[2 * i], 1, k, 20),
		      "key %d did not start its own slide", k);
	}
	CHECK(nops == 8, "four keys recorded %d ops", nops);
	now_ms += 20;
	kch_px_reset();
	kch_px_row_anim(9, 0, 1, 12, 20, KCH_T_ACTIVE);	/* evicts key 1 */
	for (int k = 0; k < 4; k++) {
		if (k == 1)
			continue;
		kch_px_row_anim(k, 1, 1, k + 4, 20, KCH_T_ACTIVE);
		p = &ops[nops - 2];
		CHECK(p->y > k * cell_h + 1 && p->y < (k + 4) * cell_h + 1,
		      "key %d lost its slide to an eviction (y %d)", k, p->y);
	}
	kch_px_row_anim(1, 1, 1, 5, 20, KCH_T_ACTIVE);
	CHECK(at_rest(&ops[nops - 2], 1, 5, 20),
	      "the evicted key did not land (y %d)", ops[nops - 2].y);

	printf("rowcheck: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}

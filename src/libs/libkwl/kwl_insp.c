/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the KDOS_INSPECT overlay — see kwl_insp.h
 *
 * No Wayland in this file: it counts what kwl.c tells it, lays a panel over
 * a copy of the cells and paints into a pixman image, so a fixture can drive
 * it with no compositor (testing/fixtures/kwl/inspcheck.c).
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kwl_insp.h"

#define INSP_FADE_MS 1000	/* a changed row's tint goes out over this */
#define INSP_TINT_A 88		/* its alpha the moment the row changed      */
#define INSP_STEP_MS 100	/* the refresh cadence while a tint fades    */
#define INSP_W 30		/* the panel, in cells                       */
#define INSP_ROWS 8
#define INSP_LABEL 9		/* the label column inside it                */

static struct {
	int on;			/* -1 until asked                            */

	/* THE WINDOW: counts since `win_at`, and what the last whole window
	 * read, per second. A window longer than two seconds read as nothing
	 * happened, not as a slow second. */
	int64_t win_at;
	int n_commit, n_stash;
	int r_commit, r_stash;
	int64_t paint_us, paint_max_win, paint_max;
	int64_t cb_ms, cb_max_win, cb_max;
	/* The last commit was the overlay's own. Its frame callback is not
	 * the surface's latency, and counting it would change the panel,
	 * which would ask for another refresh, for ever. */
	int last_own;

	/* The last commit of the surface's own: what it changed. */
	int d_rows, d_cells, d_full;

	/* Per row: when its cells last changed, and the span that did. */
	int64_t *row_at;
	KCellSpan *row_span;
	int rows_n;

	char shown[INSP_ROWS][INSP_W + 1];	/* the panel last laid down  */
	int shown_rows, shown_w, shown_h;
	int64_t laid_at;			/* and when                  */

	KtuiCell *copy, *scratch;
	size_t copy_n, scratch_n;
} I = { .on = -1 };

int kwl_insp_on(void)
{
	if (I.on < 0) {
		const char *e = getenv("KDOS_INSPECT");

		I.on = e && *e && *e != '0';
	}
	return I.on;
}

int64_t kwl_insp_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void roll(int64_t now)
{
	int64_t span = now - I.win_at;

	if (span < 1000)
		return;
	if (span < 2000) {
		I.r_commit = (int)(I.n_commit * 1000 / span);
		I.r_stash = (int)(I.n_stash * 1000 / span);
		I.paint_max = I.paint_max_win;
		I.cb_max = I.cb_max_win;
	} else {
		I.r_commit = I.r_stash = 0;
		I.paint_max = I.cb_max = 0;
	}
	I.n_commit = I.n_stash = 0;
	I.paint_max_win = I.cb_max_win = 0;
	I.win_at = now;
}

void kwl_insp_stash(int64_t now)
{
	roll(now);
	I.n_stash++;
}

void kwl_insp_committed(int own, int64_t paint_us, int64_t now)
{
	roll(now);
	I.last_own = own;
	if (own)
		return;
	I.n_commit++;
	I.paint_us = paint_us;
	if (paint_us > I.paint_max_win)
		I.paint_max_win = paint_us;
}

void kwl_insp_frame_done(int64_t ms)
{
	if (I.last_own)
		return;
	I.cb_ms = ms;
	if (ms > I.cb_max_win)
		I.cb_max_win = ms;
}

static int rows_fit(int h)
{
	if (I.row_at && I.rows_n == h)
		return 0;

	int64_t *at = calloc((size_t)(h > 0 ? h : 1), sizeof(*at));
	KCellSpan *sp = calloc((size_t)(h > 0 ? h : 1), sizeof(*sp));

	if (!at || !sp) {
		free(at);
		free(sp);
		return -1;
	}
	free(I.row_at);
	free(I.row_span);
	I.row_at = at;
	I.row_span = sp;
	I.rows_n = h;
	return 0;
}

void kwl_insp_damage(const KCellSpan *spans, int w, int h, int full,
		     int64_t now)
{
	int rows = 0, cells = 0;

	if (h <= 0 || rows_fit(h) < 0)
		return;
	for (int y = 0; y < h; y++) {
		KCellSpan s = full ? (KCellSpan){ 0, w } : spans[y];

		if (s.x1 <= s.x0)
			continue;
		I.row_at[y] = now;
		I.row_span[y] = s;
		rows++;
		cells += s.x1 - s.x0;
	}
	I.d_rows = rows;
	I.d_cells = cells;
	I.d_full = full;
}

/* The tint's alpha for a row changed `age` ms ago; 0 once it has gone out.
 * A row never changed has an `at` of 0, which is always long gone. */
static int tint_alpha(int64_t at, int64_t now)
{
	int64_t age = now - at;

	if (!at || age < 0 || age >= INSP_FADE_MS)
		return 0;
	return (int)(INSP_TINT_A * (INSP_FADE_MS - age) / INSP_FADE_MS);
}

static int fading(int64_t now)
{
	for (int y = 0; y < I.rows_n; y++)
		if (tint_alpha(I.row_at[y], now))
			return 1;
	return 0;
}

static void human_bytes(char *out, size_t n, size_t b)
{
	if (b >= 1024 * 1024)
		snprintf(out, n, "%.1f MiB", (double)b / (1024.0 * 1024.0));
	else
		snprintf(out, n, "%zu KiB", b / 1024);
}

/* The panel's lines for a grid `w` x `h`: the full panel, or one compact
 * line on a surface too short for it (a bar, an OSD). Returns the rows. */
static int panel_text(char out[INSP_ROWS][INSP_W + 1], int w, int h,
		      int64_t now)
{
	char spr[24];

	roll(now);
	memset(out, 0, sizeof(char[INSP_ROWS][INSP_W + 1]));
	if (w < INSP_W || h < 1)
		return 0;
	if (h < INSP_ROWS + 1) {
		snprintf(out[0], INSP_W + 1, " %d/s stash %d/s %.1fms",
			 I.r_commit, I.r_stash, (double)I.paint_us / 1000.0);
		return 1;
	}
	human_bytes(spr, sizeof(spr), ktui_sprite_bytes());
	snprintf(out[0], INSP_W + 1, " KDOS_INSPECT %dx%d", w, h);
	snprintf(out[1], INSP_W + 1, " commits  %d/s", I.r_commit);
	snprintf(out[2], INSP_W + 1, " stashed  %d/s", I.r_stash);
	snprintf(out[3], INSP_W + 1, " paint    %.2f ms  max %.2f",
		 (double)I.paint_us / 1000.0, (double)I.paint_max / 1000.0);
	snprintf(out[4], INSP_W + 1, " frame    %d ms  max %d", (int)I.cb_ms,
		 (int)I.cb_max);
	if (I.d_full)
		snprintf(out[5], INSP_W + 1, " changed  all %d rows", I.d_rows);
	else
		snprintf(out[5], INSP_W + 1, " changed  %d rows %d cells",
			 I.d_rows, I.d_cells);
	snprintf(out[6], INSP_W + 1, " sprites  %s", spr);
	snprintf(out[7], INSP_W + 1, " hits     %d", ktui_hit_count());
	return INSP_ROWS;
}

const KtuiCell *kwl_insp_cells(const KtuiCell *cur, int w, int h,
			       int64_t now)
{
	size_t n = (size_t)w * (size_t)h;
	int rows = panel_text(I.shown, w, h, now);
	int x0 = w - INSP_W;

	I.shown_rows = rows;
	I.shown_w = w;
	I.shown_h = h;
	I.laid_at = now;
	if (!rows || !n)
		return cur;
	if (I.copy_n < n) {
		KtuiCell *p = realloc(I.copy, n * sizeof(*p));

		if (!p)
			return cur;
		I.copy = p;
		I.copy_n = n;
	}
	memcpy(I.copy, cur, n * sizeof(*cur));
	for (int y = 0; y < rows; y++) {
		KtuiCell *row = I.copy + (size_t)y * w;

		/* A wide glyph standing across the panel's left edge would draw
		 * its right half under the panel's first cell. */
		if (x0 > 0 && row[x0].ch == KTUI_WIDE_CONT) {
			memset(&row[x0 - 1], 0, sizeof(row[x0 - 1]));
			row[x0 - 1].ch = ' ';
			row[x0 - 1].fg = row[x0].fg;
			row[x0 - 1].bg = row[x0].bg;
		}
		for (int i = 0; i < INSP_W; i++) {
			KtuiCell *c = &row[x0 + i];
			char ch = I.shown[y][i];

			memset(c, 0, sizeof(*c));
			c->ch = ch ? (unsigned char)ch : ' ';
			c->bg = KT_SURFACE;
			c->fg = rows > 1 && y == 0 ? KT_ACCENT
				: rows > 1 && i < INSP_LABEL ? KT_MID
							    : KT_TEXT;
		}
	}
	return I.copy;
}

static uint32_t slot_rgb(int slot)
{
	KRgb c = ktui_theme->slot[slot];

	return (uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b;
}

void kwl_insp_paint(pixman_image_t *grid, int w, int h, int cw, int ch,
		    int64_t now)
{
	uint32_t tint = slot_rgb(KT_ACCENT);
	int t = cw >= 16 ? 2 : 1;	/* an outline's weight, by the scale */
	int focus = ktui_focus_get();

	for (int y = 0; y < h && y < I.rows_n; y++) {
		int a = tint_alpha(I.row_at[y], now);
		KCellSpan s = I.row_span[y];

		if (!a)
			continue;
		if (s.x1 > w)
			s.x1 = w;
		if (s.x1 > s.x0)
			kcell_px_fill(grid, s.x0 * cw, y * ch,
				      (s.x1 - s.x0) * cw, ch, tint,
				      (uint8_t)a);
	}

	for (int i = 0; i < ktui_hit_count(); i++) {
		KRect r;
		int id, slot;

		if (!ktui_hit_at(i, &r, &id) || r.w <= 0 || r.h <= 0)
			continue;
		slot = id == focus ? KT_ACCENT
		     : id >= KTUI_ID_CHROME && id < KTUI_ID_HASHED ? KT_MID
								    : KT_WARN;

		uint32_t c = slot_rgb(slot);
		int px = r.x * cw, py = r.y * ch, pw = r.w * cw, ph = r.h * ch;

		kcell_px_fill(grid, px, py, pw, t, c, 255);
		kcell_px_fill(grid, px, py + ph - t, pw, t, c, 255);
		kcell_px_fill(grid, px, py + t, t, ph - 2 * t, c, 255);
		kcell_px_fill(grid, px + pw - t, py + t, t, ph - 2 * t, c, 255);
	}
}

int kwl_insp_wait(int64_t now)
{
	char next[INSP_ROWS][INSP_W + 1];
	int rows;

	if (fading(now)) {
		int64_t left = I.laid_at + INSP_STEP_MS - now;

		return left > 0 ? (int)left : 0;
	}
	/* The panel is laid only on a grid kwl_insp_cells() was handed, at
	 * the size that call last saw. */
	if (!I.shown_rows)
		return -1;
	rows = panel_text(next, I.shown_w, I.shown_h, now);
	if (rows != I.shown_rows || memcmp(next, I.shown, sizeof(next)))
		return 0;
	/* A rate still inside its window reads differently when it closes. */
	if (I.n_commit || I.n_stash || I.r_commit || I.r_stash) {
		int64_t left = I.win_at + 1000 - now;

		return left > 0 ? (int)left : 0;
	}
	return -1;
}

KtuiCell *kwl_insp_scratch(size_t n)
{
	if (I.scratch_n < n) {
		KtuiCell *p = realloc(I.scratch, n * sizeof(*p));

		if (!p)
			return NULL;
		I.scratch = p;
		I.scratch_n = n;
	}
	return I.scratch;
}

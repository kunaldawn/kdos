/* libkwm — where a new window lands. See kwm.h.
 *
 * Ported from kdos-comp's placement.c, which took it from Openbox. The
 * arithmetic is transcribed rather than reinvented: it is the behaviour
 * testing/fixtures/wm/geometry.txt records, and a "tidier" search would put
 * windows somewhere else.
 */

#include <stdlib.h>

#include "kwm.h"

#define GRID(b, i, j) ((b)->grid[(i) * ((b)->ncols - 1) + (j)])

struct bitmap {
	int nrows, ncols;
	int *rows, *cols, *grid;
};

static int cmp_int(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;

	/* Not a subtraction: two edges far apart on a large layout overflow it,
	 * and qsort then orders them by the sign of the wrap. */
	return x < y ? -1 : x > y;
}

static void bitmap_free(struct bitmap *b)
{
	free(b->rows);
	free(b->cols);
	free(b->grid);
	b->rows = b->cols = b->grid = NULL;
	b->nrows = b->ncols = 0;
}

/* Sort a list of grid lines and drop the duplicates, returning how many are
 * left. Two windows sharing an edge must contribute one line, not two. */
static int order_grid(int *edges, int n)
{
	qsort(edges, (size_t)n, sizeof(int), cmp_int);

	int i = 0, j = 0;

	while (j < n) {
		int last = edges[j++];

		edges[i++] = last;
		while (j < n && edges[j] == last)
			j++;
	}

	return i;
}

/*
 * Divide the usable area by extending every existing window's edges to
 * infinity. Every interval of the result is then either wholly covered by a
 * given window or wholly uncovered — no window ever partially covers one, which
 * is what makes the overlap count below exact rather than approximate.
 */
static void build_grid(struct bitmap *b, KwmRect usable, const KwmBox *ex, int n)
{
	bitmap_free(b);

	if (n < 1)
		return;

	int maxrc = 2 * n + 2;

	b->rows = calloc((size_t)maxrc, sizeof(int));
	b->cols = calloc((size_t)maxrc, sizeof(int));
	if (!b->rows || !b->cols) {
		bitmap_free(b);
		return;
	}

	int right = usable.x + usable.w;
	int bottom = usable.y + usable.h;

	b->cols[0] = usable.x;
	b->rows[0] = usable.y;
	b->cols[1] = right;
	b->rows[1] = bottom;

	int nr = 2, nc = 2;

	for (int i = 0; i < n; i++) {
		/* Only lines that fall INSIDE the usable area are added; one
		 * outside it would make an interval nothing can be placed in. */
		if (ex[i].left > usable.x && ex[i].left < right)
			b->cols[nc++] = ex[i].left;
		if (ex[i].top > usable.y && ex[i].top < bottom)
			b->rows[nr++] = ex[i].top;
		if (ex[i].right > usable.x && ex[i].right < right)
			b->cols[nc++] = ex[i].right;
		if (ex[i].bottom > usable.y && ex[i].bottom < bottom)
			b->rows[nr++] = ex[i].bottom;
	}

	b->nrows = order_grid(b->rows, nr);
	b->ncols = order_grid(b->cols, nc);

	int cells = (b->nrows - 1) * (b->ncols - 1);

	if (cells < 1) {
		bitmap_free(b);
		return;
	}

	b->grid = calloc((size_t)cells, sizeof(int));
	if (!b->grid)
		bitmap_free(b);
}

/*
 * The largest index j with edges[j] <= val. -1 means val is below the first
 * line; nedges - 1 means it is at or above the last.
 */
static int find_interval(const int *edges, int n, double val)
{
	int l = 0, r = n;

	while (l < r) {
		int m = (l + r) / 2;

		if (edges[m] > val)
			r = m;
		else
			l = m + 1;
	}

	return r - 1;
}

/* Count, for every interval, how many windows cover it. */
static void build_overlap(struct bitmap *b, const KwmBox *ex, int n)
{
	if (b->nrows < 1 || b->ncols < 1 || !b->grid)
		return;

	for (int i = 0; i < n; i++) {
		/*
		 * Edges land exactly on grid lines by construction, so the
		 * search is nudged half a unit inward: the leading edges into
		 * the interval they open, the trailing edges out of the one
		 * they close. Without it a window is counted as covering the
		 * intervals merely ADJACENT to it.
		 */
		int fc = find_interval(b->cols, b->ncols, ex[i].left + 0.5);
		int fr = find_interval(b->rows, b->nrows, ex[i].top + 0.5);

		if (fc < 0)
			fc = 0;
		if (fr < 0)
			fr = 0;

		int lc = find_interval(b->cols, b->ncols, ex[i].right - 0.5);
		int lr = find_interval(b->rows, b->nrows, ex[i].bottom - 0.5);

		lc = lc + 1 < b->ncols - 1 ? lc + 1 : b->ncols - 1;
		lr = lr + 1 < b->nrows - 1 ? lr + 1 : b->nrows - 1;

		for (int r = fr; r < lr; r++)
			for (int c = fc; c < lc; c++)
				GRID(b, r, c) += 1;
	}
}

/*
 * Total overlap of a region of the given size starting at interval (i, j) and
 * extending in the prescribed directions. A region that would run off the grid
 * scores INT_MAX, which no candidate ever beats.
 */
static int compute_overlap(struct bitmap *b, int i, int j, int w, int h,
			   int right, int down, int *single)
{
	int nri = b->nrows - 1;
	int nci = b->ncols - 1;
	int istep = down ? 1 : -1;
	int jstep = right ? 1 : -1;
	int overlap = 0;
	int count = 0;

	for (int ii = i; ii >= 0 && ii < nri && h > 0; ii += istep) {
		int rh = b->rows[ii + 1] - b->rows[ii];
		int mh = h < rh ? h : rh;

		if (mh < 0)
			mh = 0;
		h -= rh;

		int ww = w;

		for (int jj = j; jj >= 0 && jj < nci && ww > 0; jj += jstep) {
			int cw = b->cols[jj + 1] - b->cols[jj];
			int mw = ww < cw ? ww : cw;

			if (mw < 0)
				mw = 0;

			overlap += GRID(b, ii, jj) * mh * mw;
			count++;
			ww -= cw;
		}

		if (ww > 0) {
			overlap = INT_MAX;
			break;
		}
	}

	if (h > 0)
		overlap = INT_MAX;

	if (single)
		*single = count == 1;

	return overlap;
}

KwmRect
kwm_place(KwmRect usable, int gap, KwmBorder margin,
	  int want_w, int want_h, const KwmBox *ex, int n)
{
	KwmRect out;

	/* The default, and what an empty output or a failed allocation gets. */
	out.x = usable.x + margin.left + gap;
	out.y = usable.y + margin.top + gap;
	out.w = want_w;
	out.h = want_h;

	struct bitmap b = { 0, 0, NULL, NULL, NULL };

	build_grid(&b, usable, ex, n);
	build_overlap(&b, ex, n);

	if (!b.grid) {
		bitmap_free(&b);
		return out;
	}

	/* The searched region carries the gap on BOTH sides, so two windows
	 * placed by this search are a whole gap apart. */
	int w = want_w + margin.left + margin.right + 2 * gap;
	int h = want_h + margin.top + margin.bottom + 2 * gap;
	int offx = margin.left + gap;
	int offy = margin.top + gap;

	int best = INT_MAX;
	int nri = b.nrows - 1;
	int nci = b.ncols - 1;

	for (int i = 0; i < nri; i++) {
		for (int j = 0; j < nci; j++) {
			/*
			 * A region wider or taller than the interval it starts
			 * in can extend either way, and the four possibilities
			 * overlap differently. A region that fits inside its
			 * interval has one answer, so the other three are
			 * skipped.
			 */
			for (int d = 0; d < 4; d++) {
				int right = (d & 1) == 0;
				int down = (d & 2) == 0;
				int single = 0;
				int ov = compute_overlap(&b, i, j, w, h,
							 right, down, &single);

				if (ov >= best)
					continue;

				best = ov;
				out.x = right ? b.cols[j] + offx
					      : b.cols[j + 1] - w + offx;
				out.y = down ? b.rows[i] + offy
					     : b.rows[i + 1] - h + offy;

				if (best <= 0)
					goto done;
				if (single)
					break;
			}
		}
	}

done:
	bitmap_free(&b);
	return out;
}

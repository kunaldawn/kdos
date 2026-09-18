/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kcell_ascii — pixels to characters, by SHAPE
 *
 * NOT aalib, and the reason is not taste. aalib picks a glyph by LUMINANCE
 * alone — it never asks which part of the cell a character actually covers —
 * which is exactly why chafa and notcurses produce better pictures than the
 * 1990s tools, and its driver model writes cells into a terminal or /dev/vcsa
 * rather than into a framebuffer. aalib stays where it is, powering `bb`.
 *
 * What this implements is the SHAPE-VECTOR method, clean-room from its
 * published description rather than from chafa's source (LGPL, needs glib):
 *
 *   1. measure every candidate glyph once as a 6-component density vector —
 *      mean coverage at six sample discs arranged inside the cell
 *   2. take the same six samples from the image, per cell
 *   3. one contrast pass: normalise by the cell's own maximum, square, scale
 *      back. Without it everything collapses onto the mid-grey glyph
 *   4. nearest neighbour by Euclidean distance over the candidate table
 *
 * ONE ALGORITHM, ONE IMPLEMENTATION — this one. There used to be a GLES2 twin
 * in the pre-fork compositor's ascii.c; the labwc fork did not regraft it, so
 * the CPU half is the whole of it now and `kdos-ascii` (kdos-shell, by
 * basename) is its consumer. The disc positions are still handed out below rather
 * than inlined, for whoever writes the second one next.
 *
 * NO libm, which is the constraint the whole file is written under: libkcell
 * must stay linkable by anything libktui is, and libktui links nothing but
 * musl. The contrast exponent is therefore exactly 2 (a multiply) and the
 * distance comparison is on SQUARED distance (no sqrt). Neither is an
 * approximation — squaring is what the reference does, and comparing squares
 * orders identically to comparing roots.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kcell.h"

/*
 * The candidate set.
 *
 * Blocks first because they carry the ramp, then the ASCII shapes that are
 * genuinely distinct from one another. Anything the loaded font does not have
 * is dropped at init — Terminus's console charset has ░▒▓█ but no half blocks,
 * and a candidate that rasterises to nothing would win every dark cell.
 */
static const uint32_t CANDIDATES[] = {
	' ', 0x2591, 0x2592, 0x2593, 0x2588,		/* ░ ▒ ▓ █ */
	0x2580, 0x2584, 0x258c, 0x2590,			/* ▀ ▄ ▌ ▐ */
	'.', ',', '\'', '`', ':', ';', '"', '^', '~', '-', '_', '=', '+',
	'*', '<', '>', '/', '\\', '|', '(', ')', '[', ']', '{', '}',
	'!', '?', 'i', 'l', 't', 'r', 'c', 'v', 'x', 'z', 'n', 'u',
	'o', 'e', 'a', 's', 'w', 'm', 'q', 'p', 'd', 'b', 'k', 'h',
	'C', 'O', 'Q', '0', 'D', 'U', 'X', 'Z', 'A', 'V', 'Y', 'T',
	'L', 'F', 'E', 'H', 'N', 'M', 'W', '#', '%', '&', '$', '@',
};
#define NCANDIDATES ((int)(sizeof(CANDIDATES) / sizeof(CANDIDATES[0])))

static uint32_t glyphs[NCANDIDATES];
static float vectors[NCANDIDATES][KCELL_ASCII_DIM];
/* |v|^2 per candidate, so the match is a dot product. See
 * kcell_ascii_match(). */
static float norms[NCANDIDATES];
static int nglyphs;
static bool ready;

/*
 * Where the six discs sit, in cell-relative coordinates, and how big they are.
 *
 * Two columns by three rows. A cell is twice as tall as it is wide, so a square
 * arrangement would sample the same horizontal band three times and never
 * notice a vertical stroke — which is the one thing `|` and `-` differ by.
 */
static const float DISC_X[KCELL_ASCII_DIM] = {
	0.28f, 0.72f, 0.28f, 0.72f, 0.28f, 0.72f
};
static const float DISC_Y[KCELL_ASCII_DIM] = {
	0.20f, 0.20f, 0.50f, 0.50f, 0.80f, 0.80f
};
#define DISC_R 0.26f

/*
 * THE DISC MASKS, MEASURED ONCE PER CELL SIZE.
 *
 * Which pixels a disc covers depends on the cell's SHAPE and not on where the
 * cell sits, so the ellipse test is the same for every cell of a picture — and
 * a picture is thousands of cells. Deriving it per cell is two float divisions
 * and a multiply-add per pixel, six times over, for an answer that never
 * changes.
 *
 * An ellipse is convex, so each of its rows is ONE contiguous run: the mask is
 * a list of spans, and sampling a cell is then a walk of sequential loads with
 * no float arithmetic and no division anywhere inside it.
 *
 * The offsets are SIGNED and relative to the cell's own origin, because the
 * top and bottom discs reach a few per cent outside the cell — the whole point
 * of six samples is that they overlap. Whoever walks the spans clips them to
 * the buffer it is reading, which is the cell for a candidate measurement and
 * the picture for an image sample.
 *
 * One size is kept. A picture is tiled at one cell size from end to end.
 */
struct disc_span {
	int dy, x0, x1;
};

static struct disc_span *disc_spans[KCELL_ASCII_DIM];
static int disc_nspans[KCELL_ASCII_DIM];
static int disc_cw, disc_ch;

static void discs_free(void)
{
	for (int d = 0; d < KCELL_ASCII_DIM; d++) {
		free(disc_spans[d]);
		disc_spans[d] = NULL;
		disc_nspans[d] = 0;
	}
	disc_cw = disc_ch = 0;
}

/* Floor, not truncation, and no libm for it: a disc's bounding box starts at a
 * negative offset and truncating towards zero would lose its first row. */
static int ifloor(float f)
{
	int i = (int)f;

	return (f < 0.0f && (float)i != f) ? i - 1 : i;
}

static bool discs_build(int cw, int ch)
{
	if (disc_spans[0] && cw == disc_cw && ch == disc_ch)
		return true;
	discs_free();
	if (cw <= 0 || ch <= 0)
		return false;

	for (int d = 0; d < KCELL_ASCII_DIM; d++) {
		float cx = DISC_X[d] * (float)cw;
		float cy = DISC_Y[d] * (float)ch;
		float rx = DISC_R * (float)cw;
		float ry = DISC_R * (float)ch;
		int bx0 = ifloor(cx - rx), bx1 = ifloor(cx + rx);
		int by0 = ifloor(cy - ry), by1 = ifloor(cy + ry);
		int rows = by1 > by0 ? by1 - by0 : 1;
		struct disc_span *sp = calloc((size_t)rows, sizeof(*sp));

		if (!sp) {
			discs_free();
			return false;
		}
		int n = 0;
		for (int y = by0; y < by1; y++) {
			/* An ellipse in cell space, so the disc is round on
			 * screen rather than round in a stretched coordinate
			 * system nobody looks at. */
			float dy = ((float)y + 0.5f - cy) / ry;
			int first = -1, last = -1;

			for (int x = bx0; x < bx1; x++) {
				float dx = ((float)x + 0.5f - cx) / rx;

				if (dx * dx + dy * dy > 1.0f)
					continue;
				if (first < 0)
					first = x;
				last = x;
			}
			if (first < 0)
				continue;
			sp[n].dy = y;
			sp[n].x0 = first;
			sp[n].x1 = last + 1;
			n++;
		}
		disc_spans[d] = sp;
		disc_nspans[d] = n;
	}
	disc_cw = cw;
	disc_ch = ch;
	return true;
}

/* Mean coverage of one disc over a cell-sized 8-bit alpha bitmap, clipped to
 * that cell. The mask must already be built for it. */
static float disc_mean(const uint8_t *cov, int cw, int ch, int stride, int d)
{
	long sum = 0;
	int n = 0;

	for (int i = 0; i < disc_nspans[d]; i++) {
		const struct disc_span *sp = &disc_spans[d][i];
		const uint8_t *row;
		int x0 = sp->x0, x1 = sp->x1;

		if (sp->dy < 0 || sp->dy >= ch)
			continue;
		if (x0 < 0)
			x0 = 0;
		if (x1 > cw)
			x1 = cw;
		row = cov + (size_t)sp->dy * stride;
		for (int x = x0; x < x1; x++)
			sum += row[x];
		n += x1 > x0 ? x1 - x0 : 0;
	}
	return n ? (float)sum / (float)n / 255.0f : 0.0f;
}

/*
 * The disc positions, for a consumer that has to reproduce the sampling
 * somewhere else — a GPU shader was the one such consumer and went with the
 * pre-fork compositor. Handing them out still beats repeating the literals in
 * GLSL, where a drifted copy would make two halves quietly disagree.
 */
void kcell_ascii_discs(float *out_xy)
{
	for (int d = 0; d < KCELL_ASCII_DIM; d++) {
		out_xy[d * 2 + 0] = DISC_X[d];
		out_xy[d * 2 + 1] = DISC_Y[d];
	}
}

int kcell_ascii_init(void)
{
	if (ready)
		return nglyphs;
	if (kcell_w() <= 0)
		return 0;

	const int cw = kcell_w(), ch = kcell_h();

	if (!discs_build(cw, ch))
		return 0;

	uint8_t *cov = calloc(1, (size_t)cw * ch);
	if (!cov)
		return 0;

	nglyphs = 0;
	for (int i = 0; i < NCANDIDATES; i++) {
		uint32_t cp = CANDIDATES[i];
		if (cp != ' ' && !kcell_has(cp))
			continue;

		memset(cov, 0, (size_t)cw * ch);

		/*
		 * Rasterise into a cell-sized coverage map at the same origin
		 * kcell_paint uses. A glyph measured at its own bearing rather
		 * than in its cell would put every descender in the middle.
		 */
		KCellGlyph g;
		if (cp != ' ' && kcell_glyph_scaled(cp, 1, &g) && g.pix) {
			pixman_format_code_t fmt = pixman_image_get_format(g.pix);
			const uint8_t *src =
				(const uint8_t *)pixman_image_get_data(g.pix);
			int sstride = pixman_image_get_stride(g.pix);
			int bpp = fmt == PIXMAN_a8 ? 1 : 4;

			for (int y = 0; y < g.height; y++) {
				int dy = y + kcell_ascent() - g.y;
				if (dy < 0 || dy >= ch)
					continue;
				for (int x = 0; x < g.width; x++) {
					int dx = x + g.x;
					if (dx < 0 || dx >= cw)
						continue;
					const uint8_t *p = src +
						(size_t)y * sstride + (size_t)x * bpp;
					/* a8 is coverage; a colour glyph's
					 * alpha is its last byte. */
					cov[(size_t)dy * cw + dx] =
						bpp == 1 ? p[0] : p[3];
				}
			}
		}

		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			vectors[nglyphs][d] = disc_mean(cov, cw, ch, cw, d);
		glyphs[nglyphs] = cp;
		nglyphs++;
	}
	free(cov);

	/*
	 * Normalise the whole table by its single largest component.
	 *
	 * Across the SET, not per glyph: dividing each vector by its own maximum
	 * would map `.` and `█` onto the same point, since both are "full where
	 * they are full". The set-wide scale is what spreads the candidates out
	 * along the ramp while keeping their shapes distinguishable.
	 */
	float maxc = 0.0f;
	for (int i = 0; i < nglyphs; i++)
		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			if (vectors[i][d] > maxc)
				maxc = vectors[i][d];
	if (maxc > 0.0f)
		for (int i = 0; i < nglyphs; i++)
			for (int d = 0; d < KCELL_ASCII_DIM; d++)
				vectors[i][d] /= maxc;

	for (int i = 0; i < nglyphs; i++) {
		float sq = 0.0f;

		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			sq += vectors[i][d] * vectors[i][d];
		norms[i] = sq;
	}

	ready = true;
	return nglyphs;
}

/* The table measures ONE face at ONE cell size, so a font change invalidates
 * every vector in it — a candidate dropped because the outgoing font lacked it
 * would stay dropped, and one measured at the old cell would be compared
 * against samples from the new. */
void kcell_ascii_forget(void)
{
	ready = false;
	nglyphs = 0;
	discs_free();
}

int kcell_ascii_count(void)
{
	return nglyphs;
}

uint32_t kcell_ascii_glyph(int i)
{
	return (i >= 0 && i < nglyphs) ? glyphs[i] : ' ';
}

const float *kcell_ascii_vector(int i)
{
	return (i >= 0 && i < nglyphs) ? vectors[i] : NULL;
}

void kcell_ascii_contrast(float *s)
{
	float m = 0.0f;
	for (int d = 0; d < KCELL_ASCII_DIM; d++)
		if (s[d] > m)
			m = s[d];
	if (m <= 0.0f)
		return;
	/*
	 * Normalise, square, denormalise. The exponent is 2 because that is what
	 * the reference uses AND because a multiply needs no libm — the two
	 * happen to agree, which is the only reason this constraint is free.
	 */
	for (int d = 0; d < KCELL_ASCII_DIM; d++) {
		float v = s[d] / m;
		s[d] = v * v * m;
	}
}

int kcell_ascii_match(const float *s)
{
	int best = 0;
	float bestd = 0.0f;

	/*
	 * SQUARED distance: comparing squares orders identically to comparing
	 * roots, and sqrt would mean libm.
	 *
	 * Expanded as |s|^2 - 2 s.v + |v|^2 and with the |s|^2 dropped, since
	 * it is the same for every candidate and cannot change which one wins.
	 * What is left is a dot product against a norm the table already
	 * carries — the inner loop over eighty candidates per cell, for
	 * thousands of cells, is then a multiply-add per component.
	 */
	for (int i = 0; i < nglyphs; i++) {
		float dot = 0.0f;

		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			dot += s[d] * vectors[i][d];

		float score = norms[i] - 2.0f * dot;

		if (i == 0 || score < bestd) {
			bestd = score;
			best = i;
		}
	}
	return best;
}

/* ── the whole-image path ──────────────────────────────────────────────── */

void kcell_ascii_sample(const uint32_t *argb, int w, int h, int stride_px,
			int cx, int cy, int cell_w, int cell_h, float *out,
			uint32_t *tint)
{
	long tr = 0, tg = 0, tb = 0;
	int tn = 0;
	int bx = cx * cell_w, by = cy * cell_h;

	if (!discs_build(cell_w, cell_h)) {
		for (int d = 0; d < KCELL_ASCII_DIM; d++)
			out[d] = 0.0f;
		if (tint)
			*tint = 0xff000000u;
		return;
	}

	for (int d = 0; d < KCELL_ASCII_DIM; d++) {
		long sum = 0;
		int n = 0;

		for (int i = 0; i < disc_nspans[d]; i++) {
			const struct disc_span *sp = &disc_spans[d][i];
			int y = by + sp->dy;
			int x0 = bx + sp->x0, x1 = bx + sp->x1;
			const uint32_t *srow;

			/* Clipped to the PICTURE, not to the cell: the top and
			 * bottom discs overlap their neighbours, so only a
			 * cell on the picture's edge loses anything here. */
			if (y < 0 || y >= h)
				continue;
			if (x0 < 0)
				x0 = 0;
			if (x1 > w)
				x1 = w;
			srow = argb + (size_t)y * stride_px;
			n += x1 > x0 ? x1 - x0 : 0;
			for (int x = x0; x < x1; x++) {
				uint32_t p = srow[x];
				int r = (int)((p >> 16) & 0xff);
				int g = (int)((p >> 8) & 0xff);
				int b = (int)(p & 0xff);

				/* Rec.601 luma, integer — the same weights
				 * every other KDOS tool uses to reduce a colour
				 * to a brightness. Weighted here and divided
				 * once per disc: a divide per pixel buys
				 * nothing but rounding error. */
				sum += r * 30 + g * 59 + b * 11;
				tr += r;
				tg += g;
				tb += b;
			}
		}
		tn += n;
		out[d] = n ? (float)sum / ((float)n * 25500.0f) : 0.0f;
	}

	if (tint)
		*tint = tn ? (0xff000000u |
			      ((uint32_t)(tr / tn) << 16) |
			      ((uint32_t)(tg / tn) << 8) |
			      (uint32_t)(tb / tn))
			   : 0xff000000u;
}

int kcell_ascii_image(const uint32_t *argb, int w, int h, int stride_px,
		      int cell_w, int cell_h, uint32_t *out_cp,
		      uint32_t *out_tint, int *out_cols, int *out_rows)
{
	if (!kcell_ascii_init())
		return -1;
	if (cell_w <= 0 || cell_h <= 0)
		return -1;

	int cols = w / cell_w, rows = h / cell_h;
	if (cols <= 0 || rows <= 0)
		return -1;

	for (int cy = 0; cy < rows; cy++)
		for (int cx = 0; cx < cols; cx++) {
			float s[KCELL_ASCII_DIM];
			uint32_t tint;
			kcell_ascii_sample(argb, w, h, stride_px, cx, cy,
					   cell_w, cell_h, s, &tint);
			kcell_ascii_contrast(s);
			int idx = kcell_ascii_match(s);
			size_t o = (size_t)cy * cols + cx;
			out_cp[o] = kcell_ascii_glyph(idx);
			if (out_tint)
				out_tint[o] = tint;
		}

	*out_cols = cols;
	*out_rows = rows;
	return 0;
}

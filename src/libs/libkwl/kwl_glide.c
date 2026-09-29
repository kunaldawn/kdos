/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   a scrolled list presented gliding — see kwl_glide.h
 *
 * THE LAG. A gliding view is presented `lag` buffer pixels behind the cells:
 * positive when the list moved down its items (the content goes up, so it is
 * drawn lower than where it will land), negative the other way. The pixel
 * shown at row y of the view is the new picture's row y - lag where that is
 * inside the view, and otherwise the OLD picture's row y - lag + from — the
 * old picture being what the screen showed when the glide started, `from`
 * pixels behind the new one. Both halves are the same content where they
 * meet, so the view reads as one list sliding.
 *
 * WHY THE OLD PICTURE IS CAPTURED AND NOT DRAWN. The rows sliding out are
 * rows the cells no longer have: they were scrolled away by the very change
 * being animated. The screen still shows them, so they are copied out of the
 * buffer on the screen at the start, and a later step while one is under way
 * copies the picture in between — which is why a second notch continues the
 * slide from where it is rather than jumping to the first one's end.
 *
 * `lag` only shrinks towards 0 and never passes it, and |from| is under the
 * view's height, so every row the old picture is asked for is inside it.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kwl.h"
#include "kwl_glide.h"

typedef struct {
	int x, y, w, h, top;
} View;

/* Declared by the draw in progress, and by the frame being committed. */
static View decl[KWL_GLIDES], want[KWL_GLIDES];
static int ndecl, nwant;

typedef struct {
	int used;
	int seen;		/* declared in the frame being committed        */
	int retire;		/* no longer declared: one more commit, at rest */
	int x, y, w, h, top;	/* cells                                        */
	int cw, ch;		/* the cell in buffer pixels the lag is in      */
	int64_t t0;
	int from;		/* the lag at t0; 0 is at rest                  */
	int shown;		/* the lag the last commit presented            */
	uint32_t *old;		/* the screen's picture of the view at t0       */
	int opaque;		/* ...shown under an opaque claim               */
	size_t cap;
	int ow, oh;		/* its size in pixels                           */
} Glide;

static Glide G[KWL_GLIDES];

void kwl_list_view(int x, int y, int w, int h, int top)
{
	if (w < 1 || h < 2)
		return;
	/* The same list declared twice in one draw is the last word on it. */
	for (int i = 0; i < ndecl; i++)
		if (decl[i].x == x && decl[i].y == y && decl[i].w == w &&
		    decl[i].h == h) {
			decl[i].top = top;
			return;
		}
	if (ndecl < KWL_GLIDES)
		decl[ndecl++] = (View){ x, y, w, h, top };
}

void kwl_glide_frame(void)
{
	memcpy(want, decl, sizeof(want));
	nwant = ndecl;
	ndecl = 0;
}

static int lag_at(const Glide *g, int64_t now)
{
	int64_t t = now - g->t0;
	float e;

	if (!g->from || t >= KWL_GLIDE_MS)
		return 0;
	if (t < 0)
		t = 0;
	e = ktui_ease(KT_EASE_OUT, (float)t / (float)KWL_GLIDE_MS);
	/* Truncated towards zero: never past the rest position, and never
	 * further from it than it started. */
	return (int)((float)g->from * (1.0f - e));
}

int kwl_glide_owed(void)
{
	for (int i = 0; i < KWL_GLIDES; i++)
		if (G[i].used && (G[i].from || G[i].shown || G[i].retire))
			return 1;
	return 0;
}

/* Rows the two frames share that are the same row moved by `d`, of those
 * they share at all. A list whose items were replaced (a filter typed, a
 * page switched) moved its first row without moving anything. */
static int shifted(const KtuiCell *cur, const KtuiCell *screen, int w,
		   const Glide *g, int d)
{
	int both = 0, same = 0;

	for (int r = 0; r < g->h; r++) {
		int s = r + d;

		if (s < 0 || s >= g->h)
			continue;
		both++;
		same += !memcmp(&cur[(size_t)(g->y + r) * w + g->x],
				&screen[(size_t)(g->y + s) * w + g->x],
				(size_t)g->w * sizeof(KtuiCell));
	}
	return both && same && same * 2 >= both;
}

/* The view's pixel box inside a grid image, clipped to it. */
static int box(const Glide *g, pixman_image_t *img, int *X, int *Y, int *W,
	       int *H)
{
	int iw = pixman_image_get_width(img), ih = pixman_image_get_height(img);

	*X = g->x * g->cw;
	*Y = g->y * g->ch;
	*W = g->w * g->cw;
	*H = g->h * g->ch;
	if (*X + *W > iw)
		*W = iw - *X;
	if (*Y + *H > ih)
		*H = ih - *Y;
	return *W > 0 && *H > 0;
}

static int capture(Glide *g, pixman_image_t *shown)
{
	int X, Y, W, H;
	const uint32_t *src;
	int stride;

	if (!box(g, shown, &X, &Y, &W, &H))
		return 0;
	if ((size_t)W * H > g->cap) {
		uint32_t *p = realloc(g->old, (size_t)W * H * sizeof(*p));

		if (!p)
			return 0;
		g->old = p;
		g->cap = (size_t)W * H;
	}
	src = pixman_image_get_data(shown);
	stride = pixman_image_get_stride(shown) / 4;
	for (int r = 0; r < H; r++)
		memcpy(g->old + (size_t)r * W,
		       src + (size_t)(Y + r) * stride + X,
		       (size_t)W * sizeof(uint32_t));
	g->ow = W;
	g->oh = H;
	return 1;
}

static Glide *slot_for(const View *v, int cw, int ch)
{
	Glide *free_slot = NULL;

	for (int i = 0; i < KWL_GLIDES; i++) {
		Glide *g = &G[i];

		if (g->used && !g->seen && g->x == v->x && g->y == v->y &&
		    g->w == v->w && g->h == v->h && g->cw == cw && g->ch == ch)
			return g;
		if (!g->used && !free_slot)
			free_slot = g;
	}
	if (!free_slot)
		return NULL;
	free_slot->used = 1;
	free_slot->retire = 0;
	free_slot->x = v->x;
	free_slot->y = v->y;
	free_slot->w = v->w;
	free_slot->h = v->h;
	free_slot->top = v->top;
	free_slot->cw = cw;
	free_slot->ch = ch;
	free_slot->from = 0;
	free_slot->shown = 0;
	return free_slot;
}

void kwl_glide_begin(pixman_image_t *shown, int shown_opaque,
		     const KtuiCell *cur, const KtuiCell *screen, int w, int h,
		     int cw, int ch, int64_t now, int allow)
{
	for (int i = 0; i < KWL_GLIDES; i++)
		G[i].seen = 0;
	for (int i = 0; i < nwant; i++) {
		View v = want[i];
		Glide *g;
		int d, from;

		/* Only the part of the list the grid has. */
		if (v.x < 0) {
			v.w += v.x;
			v.x = 0;
		}
		if (v.y < 0) {
			v.h += v.y;
			v.y = 0;
		}
		if (v.x + v.w > w)
			v.w = w - v.x;
		if (v.y + v.h > h)
			v.h = h - v.y;
		if (v.w < 1 || v.h < 2 || cw < 1 || ch < 1)
			continue;
		g = slot_for(&v, cw, ch);
		if (!g)
			continue;
		g->seen = 1;
		if (v.top == g->top)
			continue;
		d = v.top - g->top;
		g->top = v.top;
		from = g->shown + d * ch;
		if (allow && shown && screen && from > -g->h * ch &&
		    from < g->h * ch && shifted(cur, screen, w, g, d) &&
		    capture(g, shown)) {
			g->from = from;
			g->t0 = now;
			g->opaque = shown_opaque;
		} else {
			g->from = 0;
		}
	}
	/* A list that went is at rest from now on; one that is on the screen
	 * in between is owed the commit that puts its cells' pixels back. */
	for (int i = 0; i < KWL_GLIDES; i++) {
		Glide *g = &G[i];

		if (!g->used || g->seen)
			continue;
		g->from = 0;
		if (g->shown)
			g->retire = 1;
		else
			g->used = 0;
	}
}

/* The view at `lag`, in place: the new picture moved by `lag` rows of
 * pixels, and the rows that opens filled from the old one. */
static void compose(Glide *g, pixman_image_t *grid, int lag)
{
	uint32_t *px = pixman_image_get_data(grid);
	int stride = pixman_image_get_stride(grid) / 4;
	int X, Y, W, H;
	size_t bytes;

	if (!box(g, grid, &X, &Y, &W, &H) || W != g->ow || H != g->oh)
		return;
	bytes = (size_t)W * sizeof(uint32_t);
	if (lag > 0) {
		for (int r = H - 1; r >= lag; r--)
			memmove(px + (size_t)(Y + r) * stride + X,
				px + (size_t)(Y + r - lag) * stride + X, bytes);
		for (int r = 0; r < lag && r < H; r++) {
			int o = r - lag + g->from;

			if (o >= 0 && o < H)
				memcpy(px + (size_t)(Y + r) * stride + X,
				       g->old + (size_t)o * W, bytes);
		}
	} else {
		for (int r = 0; r < H + lag; r++)
			memmove(px + (size_t)(Y + r) * stride + X,
				px + (size_t)(Y + r - lag) * stride + X, bytes);
		for (int r = H + lag < 0 ? 0 : H + lag; r < H; r++) {
			int o = r - lag + g->from;

			if (o >= 0 && o < H)
				memcpy(px + (size_t)(Y + r) * stride + X,
				       g->old + (size_t)o * W, bytes);
		}
	}
}

int kwl_glide_apply(pixman_image_t *grid, KtuiCell *shadow, int w, int h,
		    int opaque, int64_t now, int (*rects)[4], int max)
{
	int n = 0;

	for (int i = 0; i < KWL_GLIDES; i++) {
		Glide *g = &G[i];
		int lag, X, Y, W, H;

		if (!g->used)
			continue;
		lag = g->retire ? 0 : lag_at(g, now);
		/* A picture shown translucent is not laid under an opaque
		 * claim: whatever the claim hides would show through it. */
		if (opaque && !g->opaque)
			lag = 0;
		if (lag && box(g, grid, &X, &Y, &W, &H) &&
		    (W != g->ow || H != g->oh))
			lag = 0;
		if (lag) {
			compose(g, grid, lag);
			/* These pixels are not the cells': the next paint of
			 * this buffer repaints the view whole. */
			for (int r = g->y; shadow && r < g->y + g->h && r < h;
			     r++)
				for (int c = g->x; c < g->x + g->w && c < w; c++)
					shadow[(size_t)r * w + c].ch =
						KCELL_STALE;
		}
		if ((lag || g->shown) && n < max &&
		    box(g, grid, &X, &Y, &W, &H)) {
			rects[n][0] = X;
			rects[n][1] = Y;
			rects[n][2] = W;
			rects[n][3] = H;
			n++;
		}
		g->shown = lag;
		if (!lag)
			g->from = 0;
		if (g->retire) {
			g->retire = 0;
			g->used = 0;
		}
	}
	return n;
}

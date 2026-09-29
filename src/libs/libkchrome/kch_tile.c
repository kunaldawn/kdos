/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shell — tiles: a block of cells drawn as pixels
 *
 * A tile is one rectangle of the grid that the shell paints itself, at pixel
 * resolution, instead of filling with glyphs. libkcell rasterises it and
 * libktui's sprite table carries it, so the layout, the damage diff, the
 * eight-colour palette and the text fallback are all unchanged — see
 * kcell_canvas.c for why that is the whole design rather than a second
 * renderer bolted on.
 *
 * TWO SLOTS PER TILE, ALTERNATING, AND THAT IS THE LOAD-BEARING PART.
 *
 * A sprite cell encodes the SLOT and the sub-cell coordinate, nothing about
 * the picture. Redrawing a canvas in place therefore changes no cell, the row
 * diff sees nothing, and the panel goes on presenting the frame it had — a
 * clock tile would freeze at the minute it was first drawn and a CPU graph
 * would never move. The obvious fix, `ktui_draw_invalidate()`, repaints the
 * WHOLE surface once a second for a twelve-cell chart.
 *
 * So each tile owns two canvases and two keys and swaps between them on every
 * content change. The cells covering the tile change slot, the diff repaints
 * exactly those rows, and nothing else on the bar is touched.
 *
 * A TILE IS NEVER REQUIRED. `kch_tile_slot()` answers -1 on a terminal, with
 * `icons = no`, when fcft has no font, when the table is full and when the
 * canvas will not allocate — and every caller draws the glyph layout it had
 * before. That is the same contract libkicon keeps, and it is what keeps
 * `--dump`, tty1 and a golden frame honest.
 *
 * A REFUSED PUT KEEPS THE PICTURE THAT IS UP, and is tried once more.
 *
 * The sprite table refuses on a full table or a spent byte budget, and both
 * clear again. Flashing back to the glyph layout mid-hover for one frame is
 * worse than a frame of the previous picture, so the old slot stays — but the
 * tile then remembers what it PUBLISHED separately from what it last tried,
 * or the "already showing this" answer below would freeze it at a picture it
 * claims is current. Attempts are capped per content hash: an unrelenting
 * refusal must not turn every frame into a full canvas raster, which is the
 * memory pressure being survived — and the cap is rearmed on a timer, or a
 * tile whose content never changes would sit on the glyph layout for the rest
 * of the session after two refusals that have long since cleared.
 *
 * A TILE OF ANY SIZE, AS A GRID OF VIEWS.
 *
 * One sprite covers at most 16x16 cells: a sprite cell carries its sub-cell
 * coordinate in four bits each way. A larger tile is published as a grid of
 * sprites, one per 16x16 block, under libktui's tiled-key scheme (the half's
 * key XOR block index x KTUI_TILE_STRIDE, the keys ktui_sprite_tile_at()
 * answers for). Each block's picture is a VIEW onto the canvas — the canvas's
 * own pixels at the block's origin, at the canvas's stride — made once when
 * the tile is made, so a commit is table inserts and never a pixel copy. A
 * tile that fits one sprite is one view of the whole canvas and publishes the
 * key it always had, so the cells it draws are the same codepoints.
 *
 * THE TABLE HOLDS A REFERENCE OF ITS OWN ON EVERY VIEW IT NAMES, taken after
 * it accepts the put and given back by whoever makes it stop naming the view:
 * this file on a drop, the evictor on an eviction. The evictor is one per
 * process and belongs to whichever owner registered it — the shell's picture
 * path registers kcell_tile_free, which UNREFS — so a view the table held
 * without a reference would be freed under this file by the first eviction of
 * a half that is not on the screen, and the next commit would publish freed
 * memory. That is also why a commit is this file's own loop over
 * ktui_sprite_put() and not ktui_sprite_put_tiled(): the latter hands a
 * refused picture to the evictor even when the table goes on naming it, which
 * for a picture its owner keeps is one unref too many.
 *
 * AN EVICTED HALF IS PUBLISHED AGAIN, NOT TRUSTED. The half on the screen is
 * referenced by the cells and cannot be evicted while it is drawn; one that
 * stopped being drawn for a frame can, and its slot can be handed to someone
 * else's picture. "Already showing this" therefore asks the table whether
 * every block of the published half is still there under its key and view,
 * and puts back what is not — the canvas still holds the pixels, so that is
 * inserts, not a raster.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcell.h"
#include "kdisp.h"
#include "kicon.h"
#include "kchrome.h"

/* How many tiles one program holds at once. Past it kch_tile_begin() answers
 * NULL and the caller draws its cells, so this is a bound on memory and on
 * sprite slots rather than on what can be drawn: each tile holds two canvases,
 * and a large one is megabytes. */
#define TILE_MAX 24

/* Cells per sprite, each way — the four bits of sub-cell coordinate. */
#define TILE_SPR 16

struct tile {
	int used;
	int id;			/* the caller's, stable for the tile's life  */
	int cw, ch;		/* cells                                     */
	int pw, ph, pscale;	/* the pixel cell the canvases were cut to   */
	int cols, rows;		/* 16x16 blocks, one sprite each             */
	KCellCanvas *cv[2];
	pixman_image_t **view[2];	/* cols*rows views onto each canvas  */
	uint64_t key[2];
	int flip;		/* which half is currently published         */
	int slot;		/* block 0's published slot, or -1           */
	uint64_t content;	/* the last content a canvas was handed out for */
	uint64_t shown;		/* the content of the published picture      */
	int tries;		/* puts attempted for `content`              */
	uint64_t fail_at;	/* when the last put was refused, ms         */
	int have;
};

/*
 * Two, so a refusal that clears on the next frame still publishes and one
 * that does not costs one extra raster rather than one per frame.
 *
 * And a second before the budget comes back, because what refuses a put is
 * transient — a full table or a spent byte budget, both of which clear as
 * other pictures stop being drawn — while a tile's content hash need never
 * change again. A tile is redrawn about once a second anyway, so the worst
 * case is one extra raster per second rather than one per frame.
 */
#define TILE_TRIES 2
#define TILE_RETRY_MS 1000

static uint64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

static struct tile tiles[TILE_MAX];
static int tiles_on = 1;

/*
 * THE PIXEL SIZE OF A CELL A TILE IS RASTERISED AT.
 *
 * The display's, because the caller laid the tile's contents out in the same
 * one — a canvas cut to anything else clips the text or mis-centres the mark,
 * and the picture is then rescaled by whatever is presenting it.
 *
 * A display with no pixels of its own answers a cell of one, and a sprite it
 * is handed is rescaled by whatever presents it — so such a tile is rasterised
 * at the nominal cell instead. Four pixels is the floor a cell has to clear to
 * be a pixel cell at all, the same one kicon_init() refuses at.
 */
#define TILE_NOMINAL_CW 10
#define TILE_NOMINAL_CH 20

static void tile_cell(int *pw, int *ph, int *pscale)
{
	int w = kdisp_cell_w(), h = kdisp_cell_h(), s = kdisp_scale();

	if (w < 4 || h < 4) {
		w = TILE_NOMINAL_CW;
		h = TILE_NOMINAL_CH;
		s = 1;
	}
	*pw = w;
	*ph = h;
	*pscale = s > 0 ? s : 1;
}

void kch_tile_enable(int on)
{
	tiles_on = on;
	if (!on)
		kch_tile_reset();
}

static struct tile *find(int id)
{
	for (int i = 0; i < TILE_MAX; i++)
		if (tiles[i].used && tiles[i].id == id)
			return &tiles[i];
	return NULL;
}

static int blocks(const struct tile *t)
{
	return t->cols * t->rows;
}

static uint64_t block_key(const struct tile *t, int k, int i)
{
	return t->key[k] ^ ((uint64_t)i * KTUI_TILE_STRIDE);
}

/* Block i's cell rectangle inside the tile. */
static void block_rect(const struct tile *t, int i, int *x, int *y, int *w,
		       int *h)
{
	*x = (i % t->cols) * TILE_SPR;
	*y = (i / t->cols) * TILE_SPR;
	*w = t->cw - *x < TILE_SPR ? t->cw - *x : TILE_SPR;
	*h = t->ch - *y < TILE_SPR ? t->ch - *y : TILE_SPR;
}

/* The slot naming block i of half k, or -1 when the table no longer names
 * THIS view under that key — dropped, evicted, or the slot given away. */
static int block_slot(const struct tile *t, int k, int i)
{
	int slot = ktui_sprite_find(block_key(t, k, i));
	const KtuiSprite *s = ktui_sprite_get(slot);

	return s && s->pix == t->view[k][i] ? slot : -1;
}

/*
 * Take half k out of the table, giving back the reference the table held on
 * each view it still named. A block the table already evicted gave its
 * reference back through the evictor, which is why the answer is asked per
 * block rather than assumed.
 */
static void unpublish(struct tile *t, int k)
{
	if (!t->view[k])
		return;
	for (int i = 0; i < blocks(t); i++)
		if (block_slot(t, k, i) >= 0) {
			ktui_sprite_drop(block_key(t, k, i));
			pixman_image_unref(t->view[k][i]);
		}
}

/*
 * Put every block of half k. Returns block 0's slot, or -1 with the half
 * taken out again: all or nothing, because a half missing a block draws a
 * hole where that block's cells are.
 *
 * The reference is taken only for a put the table ACCEPTED and only when it
 * did not already name this view — a re-put of a view it holds replaces
 * nothing and hands nothing back. And the half is checked whole AFTER the
 * loop: making room under the byte budget can evict a block this same loop
 * put a moment ago, since nothing draws the half yet.
 */
static int publish(struct tile *t, int k)
{
	int first = -1;

	for (int i = 0; i < blocks(t); i++) {
		int bx, by, bw, bh;
		int had = block_slot(t, k, i) >= 0;

		block_rect(t, i, &bx, &by, &bw, &bh);
		int slot = ktui_sprite_put(block_key(t, k, i), t->view[k][i],
					   bw, bh, ' ');
		if (slot < 0) {
			unpublish(t, k);
			return -1;
		}
		if (!had)
			pixman_image_ref(t->view[k][i]);
		if (i == 0)
			first = slot;
	}
	for (int i = 0; i < blocks(t); i++)
		if (block_slot(t, k, i) < 0) {
			unpublish(t, k);
			return -1;
		}
	return first;
}

/*
 * The slots must go BEFORE the pixels: a view has no pixels of its own, so
 * one the table still names after its canvas is freed composites out of freed
 * memory.
 */
static void tile_free(struct tile *t)
{
	for (int k = 0; k < 2; k++) {
		unpublish(t, k);
		if (t->view[k]) {
			for (int i = 0; i < blocks(t); i++)
				if (t->view[k][i])
					pixman_image_unref(t->view[k][i]);
			free(t->view[k]);
		}
		kcell_canvas_free(t->cv[k]);
	}
	memset(t, 0, sizeof(*t));
}

/*
 * Drop everything. `kdos theme <accent>` retints the palette, and every tile
 * was rasterised in the old one — the same reason kicon_retint() exists, and
 * the same fix.
 */
void kch_tile_reset(void)
{
	for (int i = 0; i < TILE_MAX; i++)
		if (tiles[i].used)
			tile_free(&tiles[i]);
}

/* Both canvases, and a view per block of each. 0 when any of it will not
 * allocate, with whatever did left for tile_free(). */
static int tile_make(struct tile *t)
{
	for (int k = 0; k < 2; k++) {
		t->cv[k] = kcell_canvas_new(t->cw, t->ch, t->pw, t->ph,
					    t->pscale);
		/* The key carries the tile and the half, so the two halves
		 * are two sets of slots and swapping between them is what the
		 * row diff notices. */
		t->key[k] = ((uint64_t)0x71 << 56) |
			    ((uint64_t)(unsigned)t->id << 8) | (uint64_t)k;
		if (!t->cv[k])
			return 0;
		t->view[k] = calloc((size_t)blocks(t), sizeof(*t->view[k]));
		if (!t->view[k])
			return 0;
		for (int i = 0; i < blocks(t); i++) {
			int bx, by, bw, bh;

			block_rect(t, i, &bx, &by, &bw, &bh);
			t->view[k][i] =
				kcell_canvas_view(t->cv[k], bx, by, bw, bh);
			if (!t->view[k][i])
				return 0;
		}
	}
	return 1;
}

/*
 * The canvas to draw into, or NULL when this tile already SHOWS exactly this
 * content — which is the ordinary case, once a second at most being the
 * exception. `content` is the caller's own hash of everything it is about to
 * draw; getting it wrong in the direction of "too specific" costs a raster,
 * and in the direction of "too loose" freezes the tile.
 *
 * NULL is also the answer while the attempts for one content hash are spent:
 * the sprite table is refusing, the picture that is up stays up, and the tile
 * rasterises nothing a put will only refuse again. The budget comes back a
 * second after the refusal, so a tile whose content never changes is not left
 * on its caller's glyph layout once the table has room.
 */
KCellCanvas *kch_tile_begin(int id, int cw, int ch, uint64_t content)
{
	struct tile *t = find(id);
	int pw, ph, pscale;

	if (!tiles_on || !kicon_enabled())
		return NULL;
	/* No larger than the grid it is drawn into: past that the cells that
	 * would show it do not exist, and the canvases would be pixels nobody
	 * sees. The canvas refuses what its fills cannot address. */
	if (cw < 1 || ch < 1 || cw > ktui_w || ch > ktui_h)
		return NULL;
	tile_cell(&pw, &ph, &pscale);

	if (t && (t->cw != cw || t->ch != ch || t->pw != pw || t->ph != ph ||
		  t->pscale != pscale)) {
		/* A resize is a different picture in every cell, and so is the
		 * same cells at a different pixel size; there is nothing worth
		 * keeping either way. */
		tile_free(t);
		t = NULL;
	}
	if (!t) {
		for (int i = 0; i < TILE_MAX && !t; i++)
			if (!tiles[i].used)
				t = &tiles[i];
		if (!t)
			return NULL;
		memset(t, 0, sizeof(*t));
		t->used = 1;
		t->id = id;
		t->cw = cw;
		t->ch = ch;
		t->pw = pw;
		t->ph = ph;
		t->pscale = pscale;
		t->cols = (cw + TILE_SPR - 1) / TILE_SPR;
		t->rows = (ch + TILE_SPR - 1) / TILE_SPR;
		t->slot = -1;
		if (!tile_make(t)) {
			tile_free(t);
			return NULL;
		}
	}

	if (t->have && t->shown == content && t->slot >= 0) {
		int up = 1;

		/* Still in the table, block for block — see the file's
		 * header. What was evicted goes back from the pixels the
		 * canvas still holds; what cannot go back is a tile that is
		 * no longer up, and it rasterises like one. */
		for (int i = 0; i < blocks(t) && up; i++)
			if (block_slot(t, t->flip, i) < 0)
				up = 0;
		if (!up) {
			t->slot = publish(t, t->flip);
			up = t->slot >= 0;
			if (!up)
				t->have = 0;
		}
		if (up) {
			/* Already up. The memo follows, so a content that
			 * comes back after a refused one is not rasterised
			 * again for nothing. */
			t->content = content;
			t->tries = 0;
			return NULL;
		}
	}
	if (t->content != content)
		t->tries = 0;			/* a new picture, a new budget */
	else if (t->tries >= TILE_TRIES) {
		if (now_ms() - t->fail_at < TILE_RETRY_MS)
			return NULL;		/* the table is still refusing */
		t->tries = 0;			/* it has had time to clear */
	}

	int next = t->flip ^ 1;
	kcell_canvas_clear(t->cv[next]);
	t->content = content;
	t->tries++;
	return t->cv[next];
}

/*
 * Publish what kch_tile_begin() handed out. Returns the slot, or -1.
 *
 * A refused put leaves the previous picture published and says so, which is
 * why the slot it answers is the one that is still up rather than -1: the
 * caller draws the tile it has, not a one-frame hole.
 */
int kch_tile_commit(int id)
{
	struct tile *t = find(id);
	int next;

	if (!t)
		return -1;
	next = t->flip ^ 1;
	int slot = publish(t, next);
	if (slot < 0) {
		t->fail_at = now_ms();
		return t->slot;			/* keep whatever was up */
	}
	t->flip = next;
	t->slot = slot;
	t->shown = t->content;
	t->have = 1;
	return slot;
}

/* What the last commit published — block 0's slot — or -1 if this tile has
 * never drawn or a block of it has left the table. Asked whole, so a caller
 * that lays out around a slot it was given can count on kch_tile_draw(). */
int kch_tile_slot(int id)
{
	struct tile *t = find(id);

	if (!t || !t->have || t->slot < 0)
		return -1;
	for (int i = 0; i < blocks(t); i++)
		if (block_slot(t, t->flip, i) < 0)
			return -1;
	return t->slot;
}

/*
 * Write the published tile's cells at `r`, block by block, clipped to `r`.
 * -1, and nothing written, when the tile is not up or a block of it has left
 * the table since it was published: a tile drawn with a block missing shows
 * whatever those cells held, so the caller draws its cells instead.
 */
int kch_tile_draw(int id, KRect r, int fg, int bg)
{
	struct tile *t = find(id);

	if (kch_tile_slot(id) < 0)
		return -1;
	for (int i = 0; i < blocks(t); i++) {
		int bx, by, bw, bh;

		block_rect(t, i, &bx, &by, &bw, &bh);
		if (bw > r.w - bx)
			bw = r.w - bx;
		if (bh > r.h - by)
			bh = r.h - by;
		if (bw > 0 && bh > 0)
			ktui_draw_sprite(krect(r.x + bx, r.y + by, bw, bh),
					 block_slot(t, t->flip, i), fg, bg);
	}
	return 0;
}

/*
 * One tile, gone: its slots out of the table, then its canvases. The slots go
 * first for tile_free()'s reason. The frame being composed must not still
 * draw it — its cells would name slots the table has given back.
 */
void kch_tile_drop(int id)
{
	struct tile *t = find(id);

	if (t)
		tile_free(t);
}

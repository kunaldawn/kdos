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
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kicon.h"
#include "kwl.h"
#include "kchrome.h"

/* Small on purpose: the Start button and the meters strip are the two that
 * exist, and a bar with a dozen pixel-rendered blocks on it would be a bar
 * that had stopped being a character grid. */
#define TILE_MAX 8

struct tile {
	int used;
	int id;			/* the caller's, stable for the tile's life  */
	int cw, ch;		/* cells                                     */
	KCellCanvas *cv[2];
	uint64_t key[2];
	int flip;		/* which half is currently published         */
	int slot;		/* the published slot, or -1                 */
	uint64_t content;	/* what was rasterised into it               */
	int have;
};

static struct tile tiles[TILE_MAX];
static int tiles_on = 1;

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

/*
 * Drop everything. `kdos theme <accent>` retints the palette, and every tile
 * was rasterised in the old one — the same reason kicon_retint() exists, and
 * the same fix.
 */
void kch_tile_reset(void)
{
	for (int i = 0; i < TILE_MAX; i++) {
		struct tile *t = &tiles[i];
		if (!t->used)
			continue;
		for (int k = 0; k < 2; k++) {
			/* The slot must go BEFORE the pixels: the table holds
			 * a borrowed pointer and nothing reference-counts it. */
			ktui_sprite_drop(t->key[k]);
			kcell_canvas_free(t->cv[k]);
		}
		memset(t, 0, sizeof(*t));
	}
}

/*
 * The canvas to draw into, or NULL when this tile is already showing exactly
 * this content — which is the ordinary case, once a second at most being the
 * exception. `content` is the caller's own hash of everything it is about to
 * draw; getting it wrong in the direction of "too specific" costs a raster,
 * and in the direction of "too loose" freezes the tile.
 */
KCellCanvas *kch_tile_begin(int id, int cw, int ch, uint64_t content)
{
	struct tile *t = find(id);

	if (!tiles_on || !kicon_enabled())
		return NULL;
	if (cw < 1 || ch < 1 || cw > 16 || ch > 16)
		return NULL;

	if (t && (t->cw != cw || t->ch != ch)) {
		/* A resize is a different picture in every cell; there is
		 * nothing worth keeping. */
		for (int k = 0; k < 2; k++) {
			ktui_sprite_drop(t->key[k]);
			kcell_canvas_free(t->cv[k]);
		}
		memset(t, 0, sizeof(*t));
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
		t->slot = -1;
		for (int k = 0; k < 2; k++) {
			t->cv[k] = kcell_canvas_new(cw, ch, kwl_cell_w(),
						    kwl_cell_h(), kwl_scale());
			/* The key carries the tile and the half, so the two
			 * halves are two slots and swapping between them is
			 * what the row diff notices. */
			t->key[k] = ((uint64_t)0x71 << 56) |
				    ((uint64_t)(unsigned)id << 8) |
				    (uint64_t)k;
			if (!t->cv[k]) {
				for (int j = 0; j <= k; j++)
					kcell_canvas_free(t->cv[j]);
				memset(t, 0, sizeof(*t));
				return NULL;
			}
		}
	}

	if (t->have && t->content == content && t->slot >= 0)
		return NULL;			/* nothing changed */

	int next = t->flip ^ 1;
	kcell_canvas_clear(t->cv[next]);
	t->content = content;
	return t->cv[next];
}

/* Publish what kch_tile_begin() handed out. Returns the slot, or -1. */
int kch_tile_commit(int id)
{
	struct tile *t = find(id);
	int next;

	if (!t)
		return -1;
	next = t->flip ^ 1;
	int slot = ktui_sprite_put(t->key[next], kcell_canvas_image(t->cv[next]),
				   t->cw, t->ch, ' ');
	if (slot < 0)
		return t->slot;			/* keep whatever was up */
	t->flip = next;
	t->slot = slot;
	t->have = 1;
	return slot;
}

/* What the last commit published, or -1 if this tile has never drawn. */
int kch_tile_slot(int id)
{
	struct tile *t = find(id);

	return t && t->have ? t->slot : -1;
}

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
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kcell.h"
#include "kdisp.h"
#include "kicon.h"
#include "kchrome.h"

/* Small on purpose: the Start button and the meters strip are the two that
 * exist, and a bar with a dozen pixel-rendered blocks on it would be a bar
 * that had stopped being a character grid. */
#define TILE_MAX 8

struct tile {
	int used;
	int id;			/* the caller's, stable for the tile's life  */
	int cw, ch;		/* cells                                     */
	int pw, ph, pscale;	/* the pixel cell the canvases were cut to   */
	KCellCanvas *cv[2];
	uint64_t key[2];
	int flip;		/* which half is currently published         */
	int slot;		/* the published slot, or -1                 */
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
	if (cw < 1 || ch < 1 || cw > 16 || ch > 16)
		return NULL;
	tile_cell(&pw, &ph, &pscale);

	if (t && (t->cw != cw || t->ch != ch || t->pw != pw || t->ph != ph ||
		  t->pscale != pscale)) {
		/* A resize is a different picture in every cell, and so is the
		 * same cells at a different pixel size; there is nothing worth
		 * keeping either way. */
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
		t->pw = pw;
		t->ph = ph;
		t->pscale = pscale;
		t->slot = -1;
		for (int k = 0; k < 2; k++) {
			t->cv[k] = kcell_canvas_new(cw, ch, pw, ph, pscale);
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

	if (t->have && t->shown == content && t->slot >= 0) {
		/* Already up. The memo follows, so a content that comes back
		 * after a refused one is not rasterised again for nothing. */
		t->content = content;
		t->tries = 0;
		return NULL;
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
	int slot = ktui_sprite_put(t->key[next], kcell_canvas_image(t->cv[next]),
				   t->cw, t->ch, ' ');
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

/* What the last commit published, or -1 if this tile has never drawn. */
int kch_tile_slot(int id)
{
	struct tile *t = find(id);

	return t && t->have ? t->slot : -1;
}

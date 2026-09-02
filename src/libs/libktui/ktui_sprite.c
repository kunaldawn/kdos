/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   sprites — pixels in the cell grid
 *
 * A sprite is a picture that occupies WHOLE CELLS. The table here holds an
 * opaque pointer per slot and nothing else; the cells themselves carry the
 * slot and the sub-cell coordinate inside their codepoint, so a sprite needs
 * no second damage mechanism at all — the ordinary row diff already notices
 * when the icon at a cell becomes a different icon.
 *
 * THAT IS THE WHOLE DESIGN, and it is why the slot is keyed by CONTENT rather
 * than allocated per frame. Two frames drawing the same icon at the same size
 * must produce byte-identical cells, or every frame is a full repaint of every
 * row an icon sits in. The key is the caller's (libkicon hashes name + pixel
 * size + accent); slots persist until somebody drops one.
 *
 * libktui still links nothing but musl. The pointer is `const void *` and this
 * file never dereferences it — libkcell, which already links pixman, is what
 * turns it into pixels, and a backend that cannot draw pixels renders the
 * fallback codepoint instead. Nothing in this toolkit may REQUIRE a sprite:
 * every consumer draws its glyph tier when ktui_sprite_put() answers -1, which
 * is what a full table, a missing icon theme and a tty all look like.
 * ---------------------------------
 */

#include <string.h>

#include "ktui.h"

static KtuiSprite sprites[KTUI_MAX_SPRITES];
static int nsprites;

/*
 * The LRU stamp lives beside the table rather than inside KtuiSprite: the
 * struct is public, every backend reads it, and a bookkeeping field there is a
 * field a backend can be tempted to act on.
 */
static unsigned long used[KTUI_MAX_SPRITES];
static unsigned long clock_;

static KtuiSpriteFree evict_fn;
static void *evict_user;
static size_t byte_cap;
static size_t byte_used;
static int cell_px_w, cell_px_h;

void ktui_sprite_evictor(KtuiSpriteFree fn, void *user)
{
	evict_fn = fn;
	evict_user = user;
}

void ktui_sprite_budget(size_t max_bytes, int cw_px, int ch_px)
{
	byte_cap = max_bytes;
	cell_px_w = cw_px;
	cell_px_h = ch_px;
}

size_t ktui_sprite_bytes(void)
{
	return byte_used;
}

/* What one sprite costs, at the cell size the caller declared. Zero when it
 * declared none, which turns the byte budget off rather than guessing. */
static size_t sprite_bytes(int cw, int ch)
{
	if (cell_px_w <= 0 || cell_px_h <= 0)
		return 0;
	return (size_t)cw * cell_px_w * (size_t)ch * cell_px_h * 4;
}

/*
 * IS ANYTHING STILL DRAWING THIS? The cell buffer is this library's, so the
 * question is answerable here and nowhere else — which is the whole reason
 * eviction lives in the sprite table rather than in the program that decodes
 * pictures.
 */
static int on_screen(int slot)
{
	int w = 0, h = 0;
	const KtuiCell *cells = ktui_cells(&w, &h);
	long n = (long)w * h;

	if (!cells)
		return 1;	/* no buffer to check: assume the worst */
	for (long i = 0; i < n; i++)
		if (KTUI_IS_SPRITE(cells[i].ch) &&
		    (int)KTUI_SPRITE_SLOT(cells[i].ch) == slot)
			return 1;
	return 0;
}

static void release(int slot)
{
	if (sprites[slot].pix && evict_fn)
		evict_fn(sprites[slot].key, sprites[slot].pix, evict_user);
	byte_used -= sprite_bytes(sprites[slot].w, sprites[slot].h);
	memset(&sprites[slot], 0, sizeof(sprites[slot]));
	used[slot] = 0;
}

/*
 * Take back the least recently used slot that nothing is drawing. Returns its
 * index, or -1 when every slot is either on the screen or there is no evictor
 * to hand the picture back to — in which case the caller answers -1 and the
 * consumer draws its glyph, which is what it does for a tty anyway.
 */
static int evict_one(void)
{
	int best = -1;

	if (!evict_fn)
		return -1;
	for (int i = 0; i < nsprites; i++) {
		if (!sprites[i].pix || on_screen(i))
			continue;
		if (best < 0 || used[i] < used[best])
			best = i;
	}
	if (best < 0)
		return -1;
	release(best);
	return best;
}

int ktui_sprite_slots(void)
{
	return nsprites;
}

const KtuiSprite *ktui_sprite_get(int slot)
{
	if (slot < 0 || slot >= nsprites || !sprites[slot].pix)
		return NULL;
	return &sprites[slot];
}

int ktui_sprite_find(uint64_t key)
{
	for (int i = 0; i < nsprites; i++)
		if (sprites[i].pix && sprites[i].key == key) {
			used[i] = ++clock_;
			return i;
		}
	return -1;
}

int ktui_sprite_put(uint64_t key, const void *pix, int cw, int ch,
		    uint32_t fallback)
{
	if (!pix || cw < 1 || ch < 1 || cw > 16 || ch > 16)
		return -1;

	int slot = ktui_sprite_find(key);
	if (slot < 0) {
		/* A freed slot before the end of the table is reused, so a
		 * theme switch that drops every icon does not push the table
		 * to its cap on the second accent. */
		for (int i = 0; i < nsprites; i++)
			if (!sprites[i].pix) {
				slot = i;
				break;
			}
	}
	if (slot < 0) {
		if (nsprites >= KTUI_MAX_SPRITES) {
			slot = evict_one();
			if (slot < 0)
				return -1;	/* the caller draws its glyph */
		} else {
			slot = nsprites++;
		}
	}

	/* The byte budget is made room for BEFORE the slot is written, and a
	 * picture that cannot be made room for is refused rather than allowed
	 * to push the total past the cap. */
	size_t want = sprite_bytes(cw, ch);

	if (byte_cap && want) {
		while (byte_used + want > byte_cap) {
			int freed = evict_one();

			if (freed < 0) {
				if (slot == nsprites - 1)
					nsprites--;
				return -1;
			}
		}
	}

	/*
	 * A SLOT BEING REUSED FOR A DIFFERENT PICTURE HANDS THE OLD ONE BACK.
	 * Icons re-register the same pointer under the same key and nothing is
	 * released; a picture re-registers a new one, and without this the
	 * table would simply forget the old pointer and its owner would have
	 * nothing left to free it with.
	 */
	if (sprites[slot].pix && sprites[slot].pix != pix && evict_fn)
		evict_fn(sprites[slot].key, sprites[slot].pix, evict_user);

	byte_used -= sprite_bytes(sprites[slot].w, sprites[slot].h);
	sprites[slot].key = key;
	sprites[slot].pix = pix;
	sprites[slot].w = cw;
	sprites[slot].h = ch;
	sprites[slot].fallback = fallback;
	byte_used += want;
	used[slot] = ++clock_;
	return slot;
}

/*
 * Dropping is the OWNER's job and it must happen BEFORE the picture is freed:
 * the table holds a borrowed pointer and this library does no pixel work, so
 * it cannot free anything on its own. The evictor exists for exactly that gap
 * — an owner that registers one is telling the table how to hand a picture
 * back, and until one does, a full table refuses rather than guesses.
 *
 * Dropping does NOT call the evictor. The owner is already here and about to
 * free the picture itself; calling back into it would be a free from inside
 * its own call.
 */
void ktui_sprite_drop(uint64_t key)
{
	int slot = ktui_sprite_find(key);

	if (slot >= 0) {
		byte_used -= sprite_bytes(sprites[slot].w, sprites[slot].h);
		memset(&sprites[slot], 0, sizeof(sprites[slot]));
		used[slot] = 0;
	}
}

void ktui_sprite_clear(void)
{
	/* Hand every picture back before forgetting it, or a clear is a leak
	 * of everything the table was holding. */
	if (evict_fn)
		for (int i = 0; i < nsprites; i++)
			if (sprites[i].pix)
				evict_fn(sprites[i].key, sprites[i].pix,
					 evict_user);
	memset(sprites, 0, sizeof(sprites));
	memset(used, 0, sizeof(used));
	nsprites = 0;
	byte_used = 0;
}

/*
 * Write the cells. `bg` is the colour underneath — a sprite is composited OVER
 * it, so a transparent icon on a selected row takes the selection's fill for
 * free. `fg` is only ever used by a backend that cannot draw pixels.
 */
void ktui_draw_sprite(KRect r, int slot, int fg, int bg)
{
	const KtuiSprite *s = ktui_sprite_get(slot);
	if (!s)
		return;
	for (int j = 0; j < r.h && j < s->h; j++)
		for (int i = 0; i < r.w && i < s->w; i++)
			ktui_draw_cell(r.x + i, r.y + j,
				       KTUI_SPRITE_BASE |
					       ((uint32_t)slot << 8) |
					       ((uint32_t)j << 4) | (uint32_t)i,
				       fg, bg, KT_A_NONE);
}

/*
 * What a TEXT backend puts there. The fallback goes in the sprite's top-left
 * cell and the rest are blank: a 2x2 icon rendered as four identical blocks
 * merges with the icon beside it, and the one thing a dump has to stay is
 * readable. `ktui_draw_dump()` and the tty flush both route through this.
 */
uint32_t ktui_sprite_text_cell(uint32_t ch)
{
	const KtuiSprite *s;

	if (!KTUI_IS_SPRITE(ch))
		return ch;
	if (KTUI_SPRITE_SX(ch) || KTUI_SPRITE_SY(ch))
		return ' ';
	s = ktui_sprite_get((int)KTUI_SPRITE_SLOT(ch));
	return s && s->fallback ? s->fallback : ' ';
}

/*
 * A PICTURE LARGER THAN ONE SPRITE.
 *
 * A slot holds at most 16x16 cells, because the codepoint encoding gives four
 * bits to each of sx and sy. Anything bigger is a grid of slots sharing a key
 * prefix, so the whole picture evicts and re-registers as a unit rather than
 * leaving a screen with three quarters of a photograph on it.
 *
 * The tile key is `key ^ (tile_index * KTUI_TILE_STRIDE)`. XOR with a stride
 * rather than addition, so two pictures whose keys are adjacent cannot produce
 * the same tile key — adjacent keys are exactly what a caller hashing a file
 * path and a frame number produces.
 *
 * The caller supplies the sub-picture for each tile, because scaling and
 * cropping are pixel work and this library does none. It is called back once
 * per tile with the cell rectangle that tile covers.
 */
int ktui_sprite_put_tiled(uint64_t key, int cw, int ch, uint32_t fallback,
			  KtuiSpriteTile tile, void *user)
{
	int cols, rows, n = 0;

	if (cw < 1 || ch < 1 || !tile)
		return -1;

	cols = (cw + 15) / 16;
	rows = (ch + 15) / 16;
	if (cols * rows > KTUI_MAX_SPRITES)
		return -1;

	for (int ty = 0; ty < rows; ty++) {
		for (int tx = 0; tx < cols; tx++) {
			int tw = cw - tx * 16 > 16 ? 16 : cw - tx * 16;
			int th = ch - ty * 16 > 16 ? 16 : ch - ty * 16;
			uint64_t tk = key ^ ((uint64_t)(ty * cols + tx) *
					     KTUI_TILE_STRIDE);
			const void *pix = tile(user, tx * 16, ty * 16, tw, th);
			int slot;

			if (!pix)
				goto unwind;
			slot = ktui_sprite_put(tk, pix, tw, th, fallback);
			if (slot < 0) {
				/* The table did not take it, so the owner is
				 * still holding a picture nothing will ever
				 * name. Handing it back here is the same
				 * message an eviction carries: this table is
				 * not keeping it. */
				if (evict_fn)
					evict_fn(tk, pix, evict_user);
				goto unwind;
			}
			n++;
		}
	}
	return n;

unwind:
	/*
	 * ALL OR NOTHING. A picture that registered two thirds of its tiles
	 * would draw two thirds of itself, and the third that is missing shows
	 * whatever the cells under it held. Refusing entirely is what makes
	 * the consumer draw its fallback, which is a decision it already knows
	 * how to make.
	 */
	for (int i = 0; i < n; i++) {
		uint64_t tk = key ^ ((uint64_t)i * KTUI_TILE_STRIDE);
		int slot = ktui_sprite_find(tk);

		/* Handed back, not just forgotten: these tiles were made by
		 * the tile callback and given straight to the table, so the
		 * owner has no other pointer to free them with. */
		if (slot >= 0 && evict_fn)
			evict_fn(tk, sprites[slot].pix, evict_user);
		ktui_sprite_drop(tk);
	}
	return -1;
}

void ktui_sprite_drop_tiled(uint64_t key, int cw, int ch)
{
	int cols = (cw + 15) / 16, rows = (ch + 15) / 16;

	for (int i = 0; i < cols * rows; i++)
		ktui_sprite_drop(key ^ ((uint64_t)i * KTUI_TILE_STRIDE));
}

/* Which tile of a tiled picture covers a cell, and where inside it. */
int ktui_sprite_tile_at(uint64_t key, int cw, int cell_x, int cell_y,
			int *sx, int *sy)
{
	int cols = (cw + 15) / 16;
	int tx = cell_x / 16, ty = cell_y / 16;

	if (sx)
		*sx = cell_x % 16;
	if (sy)
		*sy = cell_y % 16;
	return ktui_sprite_find(key ^ ((uint64_t)(ty * cols + tx) *
				       KTUI_TILE_STRIDE));
}

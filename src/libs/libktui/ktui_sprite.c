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
		if (sprites[i].pix && sprites[i].key == key)
			return i;
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
		if (nsprites >= KTUI_MAX_SPRITES)
			return -1;	/* the caller draws its glyph */
		slot = nsprites++;
	}

	sprites[slot].key = key;
	sprites[slot].pix = pix;
	sprites[slot].w = cw;
	sprites[slot].h = ch;
	sprites[slot].fallback = fallback;
	return slot;
}

/*
 * Dropping is the OWNER's job and it must happen BEFORE the picture is freed.
 * There is no reference count and no eviction policy here on purpose: a table
 * that could evict a slot somebody is still drawing would be a dangling
 * pointer handed to pixman, and the only code that knows when an image dies is
 * the code that allocated it.
 */
void ktui_sprite_drop(uint64_t key)
{
	int slot = ktui_sprite_find(key);
	if (slot >= 0)
		memset(&sprites[slot], 0, sizeof(sprites[slot]));
}

void ktui_sprite_clear(void)
{
	memset(sprites, 0, sizeof(sprites));
	nsprites = 0;
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

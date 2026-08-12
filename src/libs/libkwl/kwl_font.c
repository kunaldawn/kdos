/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   glyphs — one cell, one codepoint, cached
 *
 * A cell grid asks for the same few hundred codepoints thousands of times a
 * second, so every rasterized glyph is kept. fcft has its own cache, but this
 * one also holds the ALPHA SPAN we actually blit, which is the part that costs.
 *
 * The grid is monospaced by construction, not by hope: the cell is the font's
 * widest advance, and a glyph wider than one cell is drawn clipped rather than
 * allowed to run into its neighbour. That matters more here than in a terminal
 * emulator, because libktui's box-drawing and block characters must tile
 * exactly — a half-pixel of overhang turns a border into a dashed line.
 */

#include <stdlib.h>
#include <string.h>

#include "kwl_priv.h"

#define CACHE_BUCKETS 512

struct glyph_slot {
	struct glyph_slot *next;
	uint32_t cp;
	const struct fcft_glyph *g;	/* NULL = known-missing, cached too */
};

static struct fcft_font *font;
static struct glyph_slot *cache[CACHE_BUCKETS];
static int cell_w, cell_h, ascent;

int kwl_font_load(const char *name)
{
	/*
	 * DejaVu Sans Mono by default, and the choice is load-bearing rather
	 * than aesthetic: libktui's rich tier uses eighth blocks and the full
	 * box-drawing set, and a font missing them renders a chart as blanks.
	 * The console's ter-kdos32n has neither, which is exactly why the vt
	 * tier exists — see ktui_ramp_init().
	 */
	const char *names[1] = { name && *name ? name : "monospace:size=11" };

	if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_ERROR))
		return -1;
	font = fcft_from_name(1, names, NULL);
	if (!font)
		return -1;

	cell_w = font->max_advance.x;
	cell_h = font->height;
	ascent = font->ascent;
	if (cell_w <= 0 || cell_h <= 0) {
		fcft_destroy(font);
		font = NULL;
		return -1;
	}
	return 0;
}

void kwl_font_free(void)
{
	for (int i = 0; i < CACHE_BUCKETS; i++) {
		struct glyph_slot *s = cache[i];
		while (s) {
			struct glyph_slot *next = s->next;
			free(s);
			s = next;
		}
		cache[i] = NULL;
	}
	if (font) {
		fcft_destroy(font);
		font = NULL;
	}
	fcft_fini();
}

int kwl_font_cell_w(void) { return cell_w; }
int kwl_font_cell_h(void) { return cell_h; }
int kwl_font_ascent(void) { return ascent; }

const struct fcft_glyph *kwl_glyph(uint32_t cp)
{
	if (!font)
		return NULL;

	unsigned h = (cp * 2654435761u) % CACHE_BUCKETS;
	for (struct glyph_slot *s = cache[h]; s; s = s->next)
		if (s->cp == cp)
			return s->g;

	const struct fcft_glyph *g =
		fcft_rasterize_char_utf32(font, cp, FCFT_SUBPIXEL_NONE);

	/*
	 * A miss is cached as NULL. Without that, every frame re-runs the whole
	 * fontconfig fallback chain for a codepoint no installed font has — and
	 * a cell grid draws the same missing glyph on every single frame, so it
	 * is the miss, not the hit, that decides the frame time.
	 */
	struct glyph_slot *s = calloc(1, sizeof(*s));
	if (!s)
		return g;
	s->cp = cp;
	s->g = g;
	s->next = cache[h];
	cache[h] = s;
	return g;
}

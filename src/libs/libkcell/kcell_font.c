/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   glyphs — one cell, one codepoint, cached, at every scale in use
 *
 * A cell grid asks for the same few hundred codepoints thousands of times a
 * second, so every rasterized glyph is kept. fcft has its own cache, but this
 * one also holds the ALPHA SPAN we actually blit, which is the part that costs —
 * and now the UPSCALED span as well.
 *
 * The grid is monospaced by construction, not by hope: the cell is the font's
 * widest advance, and a glyph wider than one cell is drawn clipped rather than
 * allowed to run into its neighbour. That matters more here than in a terminal
 * emulator, because libktui's box-drawing and block characters must tile
 * exactly — a half-pixel of overhang turns a border into a dashed line.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"

#define CACHE_BUCKETS 512
/*
 * The cap exists because notifyd renders attacker-influenced strings: without
 * one, a notification body sweeping the codepoint space grows a slot per
 * distinct codepoint ever seen, misses and upscaled masks included, for the
 * life of the session. fcft re-rasterises cheaply, so eviction is cheap to be
 * wrong about.
 */
#define CACHE_MAX_SLOTS 4096
/*
 * And a budget in BYTES, because a slot is not a fixed cost: it also owns an
 * upscaled mask per scale above 1, and those are the memory. A colour glyph at
 * a 32-pixel cell is 64x64x4 at scale 2 and 128x128x4 at scale 4, so the slot
 * cap alone bounds the cache at hundreds of megabytes — on a 4 GB live ISO
 * that is still the OOM the cap was added to prevent.
 */
#define CACHE_MAX_BYTES (8u << 20)

struct glyph_slot {
	struct glyph_slot *next;
	uint32_t cp;
	const struct fcft_glyph *g;	/* NULL = known-missing, cached too */
	/*
	 * Upscaled masks, indexed by scale. [0] and [1] are never allocated:
	 * scale 1 is g->pix, which fcft owns. Everything above it is ours and is
	 * unref'd in kcell_font_free().
	 */
	pixman_image_t *up[KCELL_MAX_SCALE + 1];
};

static struct fcft_font *font;
static struct glyph_slot *cache[CACHE_BUCKETS];
static unsigned cache_count;
static size_t cache_bytes;	/* the upscaled masks only; fcft owns the rest */
static unsigned evict_cursor;
static int cell_w, cell_h, ascent;

/* What an upscaled mask costs, from the image itself so that the accounting
 * cannot drift from the allocation. */
static size_t mask_bytes(pixman_image_t *img)
{
	return (size_t)pixman_image_get_stride(img) *
	       (size_t)pixman_image_get_height(img);
}

static void slot_free(struct glyph_slot *s)
{
	for (int n = 2; n <= KCELL_MAX_SCALE; n++)
		if (s->up[n]) {
			cache_bytes -= mask_bytes(s->up[n]);
			pixman_image_unref(s->up[n]);
		}
	free(s);
}

/*
 * Clock over the buckets: the next non-empty one loses its TAIL entry — the
 * oldest insertion in that chain, so the glyph just asked for is never the
 * victim. The fcft glyph itself is fcft's and survives; only our slot and its
 * upscaled masks go.
 */
static void cache_evict_one(void)
{
	for (unsigned i = 0; i < CACHE_BUCKETS; i++) {
		unsigned b = (evict_cursor + i) % CACHE_BUCKETS;
		struct glyph_slot **pp = &cache[b];
		if (!*pp)
			continue;
		while ((*pp)->next)
			pp = &(*pp)->next;
		slot_free(*pp);
		*pp = NULL;
		cache_count--;
		evict_cursor = (b + 1) % CACHE_BUCKETS;
		return;
	}
}

/*
 * Is what fontconfig handed back actually monospaced?
 *
 * This is not paranoia, it is a measured failure. fontconfig NEVER fails a
 * match — asked for a font that is not installed it returns its best guess, and
 * on a machine without Terminus that guess was a PROPORTIONAL face whose
 * `max_advance.x` was 91 pixels against a 44-pixel line height. Every cell in
 * the desktop would have been twice as wide as it was tall, and nothing in the
 * pipeline would have said so — a cell grid does not notice that its cells are
 * the wrong shape.
 *
 * The test is the advance of a wide glyph against a narrow one. In a monospaced
 * face they are equal by definition; in a proportional one 'M' is roughly twice
 * 'i'. A charcell bitmap font like Terminus passes exactly.
 */
static bool looks_monospaced(struct fcft_font *f)
{
	const struct fcft_glyph *m = fcft_rasterize_char_utf32(f, 'M',
							       FCFT_SUBPIXEL_NONE);
	const struct fcft_glyph *i = fcft_rasterize_char_utf32(f, 'i',
							       FCFT_SUBPIXEL_NONE);
	if (!m || !i)
		return true;		/* cannot tell; do not second-guess */
	return m->advance.x == i->advance.x;
}

int kcell_font_load(const char *name)
{
	/*
	 * The desktop asks for Terminus by name — see docs/KDOS-TEXTMODE.md —
	 * because it is the same rasterisation tty1 and the boot splash use.
	 * The choice is load-bearing either way: libktui's rich tier uses
	 * eighth blocks and the full box-drawing set, and a font missing them
	 * renders a chart as blanks. The console's ter-kdos32n has neither,
	 * which is exactly why the vt tier exists — see ktui_ramp_init().
	 */
	const char *names[1] = { name && *name ? name : "monospace:size=11" };

	if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_ERROR))
		return -1;
	font = fcft_from_name(1, names, NULL);
	if (!font)
		return -1;

	if (!looks_monospaced(font)) {
		/*
		 * Retry at the same pixel size through `monospace`, which
		 * fontconfig is obliged to resolve to something fixed-width.
		 * Keeping the size means the layout the caller planned for
		 * still holds; only the shape of the glyphs changes.
		 */
		int px = font->height > 0 ? font->height : 16;
		char alt[64];
		snprintf(alt, sizeof(alt), "monospace:pixelsize=%d", px);
		const char *fallback[1] = { alt };

		struct fcft_font *mono = fcft_from_name(1, fallback, NULL);
		if (mono) {
			fcft_destroy(font);
			font = mono;
		}
		/* If even that is proportional there is nothing further to try,
		 * and a wrong-shaped grid is still better than no desktop. */
	}

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

void kcell_font_free(void)
{
	for (int i = 0; i < CACHE_BUCKETS; i++) {
		struct glyph_slot *s = cache[i];
		while (s) {
			struct glyph_slot *next = s->next;
			slot_free(s);
			s = next;
		}
		cache[i] = NULL;
	}
	cache_count = 0;
	cache_bytes = 0;
	evict_cursor = 0;
	if (font) {
		fcft_destroy(font);
		font = NULL;
	}
	fcft_fini();
}

int kcell_w(void) { return cell_w; }
int kcell_h(void) { return cell_h; }
int kcell_ascent(void) { return ascent; }

static struct glyph_slot *slot_for(uint32_t cp)
{
	if (!font)
		return NULL;

	unsigned h = (cp * 2654435761u) % CACHE_BUCKETS;
	for (struct glyph_slot *s = cache[h]; s; s = s->next)
		if (s->cp == cp)
			return s;

	/* Here and nowhere else: the slot about to be inserted is not in the
	 * cache yet, so it can never be its own victim, and every consumer
	 * finishes with the glyph it was handed before asking for another. */
	while (cache_count &&
	       (cache_count >= CACHE_MAX_SLOTS || cache_bytes > CACHE_MAX_BYTES))
		cache_evict_one();

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
		return NULL;
	s->cp = cp;
	s->g = g;
	s->next = cache[h];
	cache[h] = s;
	cache_count++;
	return s;
}

const struct fcft_glyph *kcell_glyph(uint32_t cp)
{
	struct glyph_slot *s = slot_for(cp);
	return s ? s->g : NULL;
}

bool kcell_has(uint32_t cp)
{
	const struct fcft_glyph *g = kcell_glyph(cp);
	/*
	 * A glyph with no pixels is a real answer for a space and a wrong one
	 * for anything else, so emptiness alone cannot be the test — but a font
	 * that has no such codepoint returns NULL, and that is what is being
	 * asked about.
	 */
	return g != NULL;
}

static void free_bits(pixman_image_t *img, void *data)
{
	(void)img;
	free(data);
}

/*
 * Nearest-neighbour expansion of the glyph's own coverage.
 *
 * NOT a pixman transform on fcft's image: that would mutate a cached object
 * this library does not own, and with two outputs at different scales the
 * mutation would be read back by whichever painted second. And not a filtered
 * scale either — SCALE THE COVERAGE, NOT A COMPOSITED COLOUR. Blending first
 * and scaling afterwards puts a grey fringe on every edge of what is supposed
 * to be an unantialiased bitmap font.
 */
static pixman_image_t *upscale(pixman_image_t *src, int scale)
{
	pixman_format_code_t fmt = pixman_image_get_format(src);
	int bpp;

	switch (fmt) {
	case PIXMAN_a8:
		bpp = 1;
		break;
	case PIXMAN_a8r8g8b8:
	case PIXMAN_x8r8g8b8:
		bpp = 4;		/* a colour font; fcft hands these back */
		break;
	default:
		return NULL;
	}

	int sw = pixman_image_get_width(src);
	int sh = pixman_image_get_height(src);
	int sstride = pixman_image_get_stride(src);
	const uint8_t *sdata = (const uint8_t *)pixman_image_get_data(src);
	if (sw <= 0 || sh <= 0 || !sdata)
		return NULL;

	int dw = sw * scale, dh = sh * scale;
	/* pixman wants a stride that is a multiple of four, and data aligned to
	 * the same; calloc gives the alignment and this gives the stride. */
	int dstride = ((dw * bpp) + 3) & ~3;
	uint8_t *ddata = calloc(1, (size_t)dstride * (size_t)dh);
	if (!ddata)
		return NULL;

	for (int y = 0; y < sh; y++) {
		const uint8_t *srow = sdata + (size_t)y * (size_t)sstride;
		for (int r = 0; r < scale; r++) {
			uint8_t *drow = ddata +
				(size_t)(y * scale + r) * (size_t)dstride;
			for (int x = 0; x < sw; x++) {
				const uint8_t *sp = srow + (size_t)x * bpp;
				for (int c = 0; c < scale; c++)
					memcpy(drow + (size_t)(x * scale + c) * bpp,
					       sp, (size_t)bpp);
			}
		}
	}

	pixman_image_t *out = pixman_image_create_bits(fmt, dw, dh,
						       (uint32_t *)ddata,
						       dstride);
	if (!out) {
		free(ddata);
		return NULL;
	}
	pixman_image_set_destroy_function(out, free_bits, ddata);
	return out;
}

bool kcell_glyph_scaled(uint32_t cp, int scale, KCellGlyph *out)
{
	if (scale < 1)
		scale = 1;
	if (scale > KCELL_MAX_SCALE)
		scale = KCELL_MAX_SCALE;

	struct glyph_slot *s = slot_for(cp);
	if (!s || !s->g || !s->g->pix)
		return false;

	pixman_image_t *pix = s->g->pix;
	if (scale > 1) {
		if (!s->up[scale]) {
			s->up[scale] = upscale(s->g->pix, scale);
			if (!s->up[scale])
				return false;
			cache_bytes += mask_bytes(s->up[scale]);
		}
		pix = s->up[scale];
	}

	out->pix = pix;
	out->x = s->g->x * scale;
	out->y = s->g->y * scale;
	out->width = s->g->width * scale;
	out->height = s->g->height * scale;
	return true;
}

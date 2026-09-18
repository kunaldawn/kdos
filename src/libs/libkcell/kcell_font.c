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
 * The grid is monospaced by construction, not by hope: the cell is the advance
 * of one CHARACTER — see cell_advance() for why that is not the face's maximum
 * — and a glyph wider than one cell is drawn clipped rather than allowed to
 * run into its neighbour. A fallback face answers with whatever metric it has,
 * and a bitmap exceeding the cell would otherwise overwrite the character
 * beside it, which that character has no reason to repaint. The box-drawing
 * and block characters do not depend on the clip: kcell_paint synthesises
 * them, so their tiling is arithmetic rather than a property of the face.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "kcell_priv.h"

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
	uint8_t style;			/* which face this was rasterized from */
	const struct fcft_glyph *g;	/* NULL = known-missing, cached too */
	/*
	 * Upscaled masks, indexed by scale. [0] and [1] are never allocated:
	 * scale 1 is g->pix, which fcft owns. Everything above it is ours and is
	 * unref'd when the cache is flushed.
	 */
	pixman_image_t *up[KCELL_MAX_SCALE + 1];
};

/*
 * THE FOUR FACES, INDEXED BY THE STYLE BITS. `face[0]` is the upright one and
 * is the only one that must exist: the cell geometry is its, and the whole
 * toolkit divides by that.
 *
 * A COMPANION IS ONLY TAKEN IF IT FITS THE CELL. fontconfig never fails a
 * match, so asking for a bold or an italic Terminus returns SOMETHING — and on
 * this image that something can be a different family at a different size. A
 * companion whose advance or height differs would draw a row out of step with
 * the one above it, so it is refused. Italic then falls back to upright, which
 * is a style lost rather than a grid broken; bold falls back to striking the
 * upright mask twice, which is a style approximated.
 */
static struct fcft_font *face[KCELL_NSTYLE];
static struct glyph_slot *cache[CACHE_BUCKETS];
static unsigned cache_count;
static size_t cache_bytes;	/* the upscaled masks only; fcft owns the rest */
static unsigned evict_cursor;
static int cell_w, cell_h, ascent;

/*
 * THE ONE OWNER OF FCFT INSIDE libkcell, AND IT COUNTS ITS HOLDERS. The pair
 * is declared in kcell_priv.h, which both holders include.
 *
 * fcft_init() is not idempotent and fcft_fini() is not refcounted: the init
 * builds a fresh FreeType handle over whatever one is already standing, and
 * the fini destroys FreeType, fontconfig's state and fcft's own mutexes
 * whether or not anything still wants them. Two files here need the library
 * up and they start and stop independently — this one for the cell faces,
 * kcell_canvas.c for text at an arbitrary pixel size — so a flag saying only
 * whether fcft is up cannot express the case the count exists for: ONE HOLDER
 * RELEASING WHILE THE OTHER IS STILL DRAWING. Under a single flag that
 * release tears FreeType down beneath the other's live faces and the next
 * frame composites out of freed glyph images; under a flag per file, neither
 * of which reads the other, whichever initialises second leaks the first FT
 * handle and orphans every face resolved through it.
 *
 * So each holder takes ONE reference and releases it once: the library comes
 * up on the first reference and goes down on the last, either entry point may
 * come first, and a reference asked for while it is already up costs nothing.
 *
 * A refused init is latched by the CALLER and not here — a bar drawing sixty
 * times a second must not retry the whole library init on every frame.
 */
static unsigned fcft_refs;

int kcell_fcft_ref(void)
{
	if (fcft_refs > 0) {
		fcft_refs++;
		return 1;
	}
	if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_ERROR))
		return 0;
	fcft_refs = 1;
	return 1;
}

void kcell_fcft_unref(void)
{
	/* An unmatched release is dropped rather than obeyed: taking the count
	 * past zero would fini the library under the holder that still has it. */
	if (fcft_refs == 0)
		return;
	if (--fcft_refs == 0)
		fcft_fini();
}

/*
 * AND THIS FILE HOLDS AT MOST ONE OF THOSE REFERENCES, EVER. A load is
 * repeatable and a free is not paired with any particular load, so counting
 * one per load would leave a count that never reaches zero and an fcft that
 * is never torn down.
 */
static bool fcft_held;

/*
 * THE MISSING-GLYPH SENTINEL.
 *
 * fcft cannot report a codepoint as absent. Its fallback search ends by
 * rasterising glyph index 0 out of the primary face, so a codepoint no
 * installed font carries comes back as a perfectly valid glyph — a tofu box
 * for a TrueType face, the default character for a PCF — and NULL means only
 * that FreeType failed to load something.
 *
 * The only way to recognise that answer is to hold one of them for comparison:
 * a permanent Unicode noncharacter can never be in any font, so whatever the
 * face renders for it IS its .notdef, and anything that rasterises to the same
 * picture at the same metrics is the same .notdef. Measured once per load,
 * against the face finally chosen.
 */
#define NOTDEF_PROBE 0xfdd0u

static bool notdef_known;
static int notdef_x, notdef_y, notdef_w, notdef_h, notdef_adv;
static bool notdef_haspix;
static pixman_format_code_t notdef_fmt;
static int notdef_stride, notdef_rows, notdef_pw;
static uint8_t *notdef_bits;

/* The meaningful bytes of one row. A pixman image's stride is rounded up to a
 * four-byte boundary and fcft writes only the row's own bytes into a plain
 * malloc, so the padding beyond this length is uninitialised and comparing it
 * makes two identical pictures differ. */
static size_t row_bytes(pixman_image_t *img)
{
	return ((size_t)PIXMAN_FORMAT_BPP(pixman_image_get_format(img)) *
		(size_t)pixman_image_get_width(img) + 7) / 8;
}

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

/*
 * HOW WIDE ONE CHARACTER IS, which is NOT the face's maximum advance.
 *
 * FreeType's max advance is the widest glyph in the FACE, and a monospaced
 * face may still carry glyphs two and three cells wide: Noto Sans Mono has 243
 * at two and nine at three, so its maximum is three times the width of a
 * letter. A grid cut to that is a grid with two blank columns after every
 * character and a box-drawing rule reaching a third of the way across its
 * cell, which is a dashed border on every window in the desktop.
 *
 * `M` IS THE MEASURE, and looks_monospaced() has already established that it
 * is the same advance as `i`. A face that will not rasterise it falls back to
 * the maximum, which is the same number on a charcell bitmap font and on any
 * face whose advances are uniform.
 */
static int cell_advance(struct fcft_font *f)
{
	const struct fcft_glyph *m = fcft_rasterize_char_utf32(f, 'M',
							       FCFT_SUBPIXEL_NONE);

	return m && m->advance.x > 0 ? m->advance.x : f->max_advance.x;
}

static void notdef_forget(void)
{
	free(notdef_bits);
	notdef_bits = NULL;
	notdef_haspix = false;
	notdef_known = false;
}

/* Rasterise the probe straight off the face rather than through the cache:
 * the sentinel is what the cache's own misses are recognised BY, and a
 * codepoint that cannot exist has no business occupying a slot. */
static void notdef_measure(void)
{
	const struct fcft_glyph *g;

	notdef_forget();
	g = fcft_rasterize_char_utf32(face[0], NOTDEF_PROBE,
				      FCFT_SUBPIXEL_NONE);
	if (!g)
		return;			/* the face refuses it; nothing to match */

	notdef_x = g->x;
	notdef_y = g->y;
	notdef_w = g->width;
	notdef_h = g->height;
	notdef_adv = g->advance.x;
	if (g->pix) {
		const uint8_t *src = (const uint8_t *)pixman_image_get_data(g->pix);
		size_t rb;
		int r;

		notdef_fmt = pixman_image_get_format(g->pix);
		notdef_stride = pixman_image_get_stride(g->pix);
		notdef_rows = pixman_image_get_height(g->pix);
		notdef_pw = pixman_image_get_width(g->pix);
		if (notdef_stride <= 0 || notdef_rows <= 0)
			return;
		rb = row_bytes(g->pix);
		/* calloc, and row-wise: the padding of the stored copy stays
		 * zero so the probe itself never holds an uninitialised byte. */
		notdef_bits = calloc((size_t)notdef_stride,
				     (size_t)notdef_rows);
		if (!notdef_bits)
			return;		/* unmeasured is better than mismeasured */
		for (r = 0; r < notdef_rows; r++)
			memcpy(notdef_bits + (size_t)r * (size_t)notdef_stride,
			       src + (size_t)r * (size_t)notdef_stride, rb);
		notdef_haspix = true;
	}
	notdef_known = true;
}

/*
 * A COMPANION FACE, OR NOTHING. The pattern is the caller's own name with the
 * style appended, so a companion is always the same family at the same size —
 * and it is refused outright unless its cell matches the upright face's, since
 * a row drawn on a different advance steps out of line with the row above it.
 */
static struct fcft_font *companion(const char *base, const char *attrs)
{
	char spec[224];
	const char *names[1] = { spec };
	struct fcft_font *f;

	snprintf(spec, sizeof(spec), "%s%s", base, attrs);
	f = fcft_from_name(1, names, NULL);
	if (f && (cell_advance(f) != cell_w || f->height != cell_h)) {
		fcft_destroy(f);
		f = NULL;
	}
	return f;
}

/*
 * Every face, and everything measured against one.
 *
 * A glyph slot holds an fcft glyph pointer and masks sized for the current
 * cell; kcell_ascii's candidate table is a measurement of these faces at this
 * cell size; the tiling scratch is sized in cells. All of it names faces that
 * are about to stop existing, so all of it goes together — keeping any of it
 * would hand the next frame a pointer into a destroyed font.
 */
static void drop_faces(void)
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
	notdef_forget();
	for (int i = 0; i < KCELL_NSTYLE; i++)
		if (face[i]) {
			fcft_destroy(face[i]);
			face[i] = NULL;
		}
	cell_w = cell_h = ascent = 0;
	kcell_ascii_forget();
	kcell_tile_forget();
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

	/* A load REPLACES whatever stood before it, and replacing it is the
	 * first thing it does: every cached glyph and every measurement names
	 * the outgoing faces. */
	drop_faces();

	if (!fcft_held) {
		if (!kcell_fcft_ref())
			return -1;
		fcft_held = true;
	}
	face[0] = fcft_from_name(1, names, NULL);
	if (!face[0]) {
		/*
		 * THE REFERENCE GOES BACK ON THE WAY OUT. A caller reading -1
		 * as "there is no cell font" has nothing left to call
		 * kcell_font_free() on, so a reference still held here is one
		 * nothing will ever release and FreeType stands for the life
		 * of the process — and on a machine where no name resolves,
		 * that is every process that tried.
		 */
		kcell_fcft_unref();
		fcft_held = false;
		return -1;
	}

	if (!looks_monospaced(face[0])) {
		/*
		 * Retry at the same pixel size through `monospace`, which
		 * fontconfig is obliged to resolve to something fixed-width.
		 * Keeping the size means the layout the caller planned for
		 * still holds; only the shape of the glyphs changes.
		 */
		int px = face[0]->height > 0 ? face[0]->height : 16;
		char alt[64];
		snprintf(alt, sizeof(alt), "monospace:pixelsize=%d", px);
		const char *fallback[1] = { alt };

		struct fcft_font *mono = fcft_from_name(1, fallback, NULL);
		if (mono) {
			fcft_destroy(face[0]);
			face[0] = mono;
		}
		/* If even that is proportional there is nothing further to try,
		 * and a wrong-shaped grid is still better than no desktop. */
	}

	cell_w = cell_advance(face[0]);
	cell_h = face[0]->height;
	ascent = face[0]->ascent;
	if (cell_w <= 0 || cell_h <= 0) {
		/*
		 * AND BACK ON THIS WAY OUT TOO. Every path that answers -1
		 * answers it to a caller holding no font to free, so a
		 * reference kept on any one of them is a reference nothing
		 * will ever release.
		 */
		fcft_destroy(face[0]);
		face[0] = NULL;
		cell_w = cell_h = ascent = 0;
		kcell_fcft_unref();
		fcft_held = false;
		return -1;
	}

	face[KCELL_ST_ITALIC] = companion(names[0], ":slant=italic");
	face[KCELL_ST_BOLD] = companion(names[0], ":weight=bold");
	face[KCELL_ST_BOLD | KCELL_ST_ITALIC] =
		companion(names[0], ":weight=bold:slant=italic");

	notdef_measure();
	return 0;
}

void kcell_font_free(void)
{
	drop_faces();
	if (fcft_held) {
		kcell_fcft_unref();
		fcft_held = false;
	}
}

int kcell_w(void) { return cell_w; }
int kcell_h(void) { return cell_h; }
int kcell_ascent(void) { return ascent; }

/*
 * WHICH FACE ACTUALLY ANSWERS A STYLE. A companion the load refused leaves its
 * slot empty and the request falls back to one that exists — the slant is kept
 * in preference to the weight, because a missing bold is recovered by striking
 * the mask twice and a missing slant is not recoverable at all.
 */
static int face_style(int style)
{
	style &= KCELL_ST_ITALIC | KCELL_ST_BOLD;
	if (face[style])
		return style;
	if ((style & KCELL_ST_ITALIC) && face[KCELL_ST_ITALIC])
		return KCELL_ST_ITALIC;
	if ((style & KCELL_ST_BOLD) && face[KCELL_ST_BOLD])
		return KCELL_ST_BOLD;
	return 0;
}

static struct glyph_slot *slot_for(uint32_t cp, int style)
{
	style = face_style(style);
	if (!face[style])
		return NULL;

	/* The face is part of the KEY, not of the answer: the same codepoint
	 * rasterized from two faces is two glyphs, and a cache that held only
	 * one of them would draw whichever a frame asked for first. */
	unsigned h = ((cp + (unsigned)style * 0x9e3779b9u) * 2654435761u) %
		     CACHE_BUCKETS;
	for (struct glyph_slot *s = cache[h]; s; s = s->next)
		if (s->cp == cp && s->style == (uint8_t)style)
			return s;

	/* Here and nowhere else: the slot about to be inserted is not in the
	 * cache yet, so it can never be its own victim, and every consumer
	 * finishes with the glyph it was handed before asking for another. */
	while (cache_count &&
	       (cache_count >= CACHE_MAX_SLOTS || cache_bytes > CACHE_MAX_BYTES))
		cache_evict_one();

	const struct fcft_glyph *g =
		fcft_rasterize_char_utf32(face[style], cp,
					  FCFT_SUBPIXEL_NONE);

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
	s->style = (uint8_t)style;
	s->g = g;
	s->next = cache[h];
	cache[h] = s;
	cache_count++;
	return s;
}

const struct fcft_glyph *kcell_glyph(uint32_t cp)
{
	struct glyph_slot *s = slot_for(cp, 0);
	return s ? s->g : NULL;
}

bool kcell_has(uint32_t cp)
{
	const struct fcft_glyph *g = kcell_glyph(cp);
	const uint8_t *bits;
	size_t rb;
	int r;

	/*
	 * NULL is a rasterisation failure, not an absence — see the sentinel
	 * above. A codepoint is missing when what came back is the face's own
	 * .notdef, and that is recognised by being the same picture at the same
	 * metrics as the probe. Emptiness cannot be the test on its own: a
	 * glyph with no pixels is the right answer for a space.
	 *
	 * With no sentinel measured — a face that refuses even the probe —
	 * every rasterised glyph counts as present, which errs towards drawing
	 * a tofu box rather than towards dropping a character the font has.
	 */
	if (!g)
		return false;
	if (!notdef_known)
		return true;
	if (g->x != notdef_x || g->y != notdef_y || g->width != notdef_w ||
	    g->height != notdef_h || g->advance.x != notdef_adv)
		return true;
	if (!g->pix)
		return notdef_haspix;
	if (!notdef_haspix)
		return true;
	if (pixman_image_get_format(g->pix) != notdef_fmt ||
	    pixman_image_get_stride(g->pix) != notdef_stride ||
	    pixman_image_get_height(g->pix) != notdef_rows ||
	    pixman_image_get_width(g->pix) != notdef_pw)
		return true;
	bits = (const uint8_t *)pixman_image_get_data(g->pix);
	rb = row_bytes(g->pix);
	/* Row by row over the meaningful bytes only: the stride padding is
	 * never written, so a whole-buffer compare reads uninitialised heap
	 * and reports two copies of the same .notdef as different pictures. */
	for (r = 0; r < notdef_rows; r++)
		if (memcmp(bits + (size_t)r * (size_t)notdef_stride,
			   notdef_bits + (size_t)r * (size_t)notdef_stride,
			   rb) != 0)
			return true;
	return false;
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
	return kcell_glyph_face(cp, scale, 0, out);
}

bool kcell_glyph_styled(uint32_t cp, int scale, int italic, KCellGlyph *out)
{
	return kcell_glyph_face(cp, scale, italic ? KCELL_ST_ITALIC : 0, out);
}

bool kcell_glyph_face(uint32_t cp, int scale, int style, KCellGlyph *out)
{
	if (scale < 1)
		scale = 1;
	if (scale > KCELL_MAX_SCALE)
		scale = KCELL_MAX_SCALE;

	struct glyph_slot *s = slot_for(cp, style);
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
	/* Bold that no face answered is the caller's to strike twice; the
	 * cache holds one mask per face, never a pre-emboldened copy. */
	out->synth_bold = (style & KCELL_ST_BOLD) &&
			  !(face_style(style) & KCELL_ST_BOLD);
	return true;
}

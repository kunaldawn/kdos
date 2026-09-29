/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the pixel display list — record while drawing, replay under the cells
 *
 * PIXELS FOR PAINT, CELLS FOR LAYOUT. Everything about where a control goes —
 * the degradation passes, every hit map, every `--dump` — stays in cells and
 * is untouched. This is only what a surface is painted WITH, one layer down.
 *
 * It has to be record-then-replay because the two happen at different times:
 * libkwl calls the backdrop at PAINT time, before the cells go down, while the
 * layout that decides where a plate belongs runs in the surface's own draw.
 * Recording also makes a multi-pass layout free — each pass begins by dropping
 * the list, so only the pass that is finally kept has drawn anything.
 *
 * One list for the whole process. A surface draws one thing at a time and
 * there is exactly one backdrop; two lists would be two answers to what is
 * under the grid.
 * ---------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include "kchrome.h"
#include "kcell.h"
#include "kwl.h"

enum { PXO_RECT = 0, PXO_ROUND, PXO_TEXT };

/*
 * A TEXT op is its rectangle, its colour in `rgb`, its pixel size and
 * alignment, and its string twice: `str` is where it sits in this list's pool
 * (replay draws it), `txt` a hash of it (comparison uses that). A remembered
 * list keeps only the hash — the pool is refilled by every recording — and
 * two ops with the same hash, size, place and colour draw the same pixels.
 */
struct px_op {
	uint8_t kind, alpha, radius, align;
	int16_t x, y, w, h;
	uint32_t rgb, rgb2;		/* equal for a flat fill */
	uint16_t size, str;
	uint64_t txt;
};

/*
 * Bounded rather than grown: a surface wanting more plates than this has
 * stopped being a cell grid. Overflow drops the op, which loses a plate and
 * can never corrupt anything.
 */
#define PX_MAX 256
static struct px_op ops[PX_MAX];
static int nops;

/*
 * The strings the list's text ops draw, NUL-terminated end to end and emptied
 * with the list. Display text is headings and figures, a few dozen bytes a
 * surface; a string that does not fit is refused and drawn in cells instead.
 */
#define PX_POOL 2048
static char pool[PX_POOL];
static int pool_len;

/* Set by whichever backdrop this surface installed — see kch_px_live(). */
static int backdrop_on;
/* Which one, and what the installed picture is drawn with. */
enum { BD_BARE = 0, BD_POPUP, BD_CUSTOM, BD_FLAT };
static int backdrop_kind;
static KchPxBackdrop custom;
/* The slot the surface handed its backdrop, -1 for a custom one — see
 * own_slot(). */
static int handed_slot = -1;

/*
 * THE PLATES IN FLIGHT, one per selection a surface draws. A slot remembers
 * where its plate is going and where it set out from, in pixels at scale 1,
 * and which list it was last recorded in: `rec_gen` counts kch_px_reset(), so
 * a slot whose `seen` is neither this list nor the one before it was not on
 * the screen the last time a frame was painted, and a slide from there would
 * start at a place nobody saw it.
 *
 * Four, because a surface draws one selection per list and none draws more
 * than two lists; a fifth key takes the slot asked least recently, whose
 * plate then lands without a slide the next time it is drawn.
 */
#define ROW_ANIM_N 4
static struct row_anim {
	int used, key, item;
	int x, y, w;		/* where it is going                  */
	int fx, fy, fw;		/* where it set out from               */
	int cell_w, cell_h;	/* the cell both were measured in      */
	unsigned seen, stamp;
	KtuiAnim a;		/* 0 -> 1 from the start to the target */
} row_anim[ROW_ANIM_N];
static unsigned rec_gen, row_clock;

/*
 * THE PICTURE THE LAST PAINT PRODUCED, kept so an unchanged one is a copy
 * rather than a re-rasterisation — and so a buffer that already wears it can
 * be handed back just the cells that changed (libkwl's band restore).
 *
 * Every full paint lays the whole backdrop, so without it a pointer moving
 * over the taskbar would re-run the body gradient and every recorded plate, at
 * full surface size, for each motion event. Nothing about that picture
 * depends on the frame: it is the ops, the salt, the palette, the size and the
 * scale, and all five are cheap to compare.
 */
static pixman_image_t *cache_img;
static int cache_w, cache_h, cache_scale;
static uint64_t cache_key;

#define FNV_PRIME 1099511628211ULL

static uint64_t ops_key(void)
{
	uint64_t k = 1469598103934665603ULL;
	const unsigned char *b = (const unsigned char *)ops;

	for (size_t i = 0; i < (size_t)nops * sizeof(*ops); i++)
		k = (k ^ b[i]) * FNV_PRIME;
	return k ^ ((uint64_t)nops << 32);
}

/*
 * THE PALETTE BY VALUE, NOT BY POINTER. Night light re-projects the chosen
 * scheme into the SAME warmed table, so a scheme switched while it is on moves
 * every slot and leaves `ktui_theme` where it was — and the body gradient reads
 * its tones at draw time, not from the op list. Keyed on the pointer, that
 * picture would stay in the old colours.
 */
static uint64_t theme_key(void)
{
	uint64_t k = 1469598103934665603ULL;

	for (int i = 0; i < KT_NCOLOR; i++) {
		KRgb c = ktui_theme->slot[i];

		k = (k ^ c.r) * FNV_PRIME;
		k = (k ^ c.g) * FNV_PRIME;
		k = (k ^ c.b) * FNV_PRIME;
	}
	return k;
}

/*
 * What the picture depends on beyond the op list and the palette. One
 * definition per backdrop, because a key that disagreed with the picture would
 * either pin a stale backdrop on the screen or make every frame a repaint.
 */
static uint64_t cur_salt(void)
{
	switch (backdrop_kind) {
	case BD_POPUP:
		return (uint64_t)kch_popup_alpha() << 8 | 1u;
	case BD_CUSTOM:
		return (custom.salt ? custom.salt() : 0) << 2 | 3u;
	case BD_FLAT:
		/* The slot's colour is in the palette key; which slot is not. */
		return (uint64_t)handed_slot << 8 | 4u;
	default:
		return 2u;
	}
}

/* The whole identity of the picture the next paint would lay down, at a
 * given size and scale — which the cache compares separately. */
static uint64_t pic_key(void)
{
	uint64_t k = ops_key();

	k = (k ^ cur_salt()) * FNV_PRIME;
	return (k ^ theme_key()) * FNV_PRIME;
}

/*
 * THE LAST FEW PICTURES BY THEIR OP LISTS, so a picture can be compared with
 * one painted a frame or two ago and only the part that moved re-rasterised
 * and repainted.
 *
 * Kept per key because the question is always "what differs from the picture
 * THAT buffer wears": two buffers alternate, the one being painted holds the
 * frame before last, and the compositor shows a third answer again. Every key
 * libkwl is handed is noted here as it is handed out, so each of those three is
 * normally present; one that has been evicted answers "unknown" and the
 * caller paints in full. Least recently asked goes first.
 */
#define HIST_N 6
static struct {
	struct px_op ops[PX_MAX];
	int nops;
	uint64_t key, salt, theme;
	unsigned stamp;
	int used;
} hist[HIST_N];
static unsigned hist_clock;

static void hist_note(uint64_t key)
{
	int slot = 0;

	for (int i = 0; i < HIST_N; i++) {
		if (hist[i].used && hist[i].key == key) {
			hist[i].stamp = ++hist_clock;
			return;
		}
		if (hist[slot].used &&
		    (!hist[i].used || hist[i].stamp < hist[slot].stamp))
			slot = i;
	}
	hist[slot].used = 1;
	hist[slot].key = key;
	hist[slot].salt = cur_salt();
	hist[slot].theme = theme_key();
	hist[slot].nops = nops;
	memcpy(hist[slot].ops, ops, (size_t)nops * sizeof(*ops));
	hist[slot].stamp = ++hist_clock;
}

static void hist_drop(void)
{
	for (int i = 0; i < HIST_N; i++)
		hist[i].used = 0;
}

/* Field by field: the struct has a padding byte, and two ops that paint the
 * same pixels must compare equal whatever it holds. */
static int op_eq(const struct px_op *a, const struct px_op *b)
{
	return a->kind == b->kind && a->alpha == b->alpha &&
	       a->radius == b->radius && a->align == b->align &&
	       a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h &&
	       a->rgb == b->rgb && a->rgb2 == b->rgb2 && a->size == b->size &&
	       a->txt == b->txt;
}

/* The pixels one op can write: every primitive replay uses stays inside the
 * rectangle it is handed — text included, which is cut to its rectangle glyph
 * by glyph. */
static void op_touch(pixman_region32_t *r, const struct px_op *p, int scale)
{
	pixman_region32_union_rect(r, r, p->x * scale, p->y * scale,
				   (unsigned)(p->w * scale),
				   (unsigned)(p->h * scale));
}

/*
 * WHERE TWO OP LISTS PAINT DIFFERENT PIXELS, over the same body.
 *
 * The ops both lists share IN THE SAME ORDER (their longest common
 * subsequence) paint the same pixels in the same sequence; every other op is
 * one that appeared, vanished or moved, and outside all of their rectangles
 * each pixel is covered by the same ops composited in the same order, so it
 * is the same pixel. Order matters because the ops blend: a plate that moves
 * from under another to above it changes their overlap and nothing else, and
 * a set comparison would miss exactly that.
 *
 * The common prefix and suffix are stripped first, which is all of it for a
 * hover moving between rows; the table is only built for what is left.
 * Without the memory for it, every remaining op is taken as changed — larger,
 * never wrong.
 */
static void ops_changed(const struct px_op *a, int na, const struct px_op *b,
			int nb, int scale, pixman_region32_t *out)
{
	while (na && nb && op_eq(a, b)) {
		a++;
		b++;
		na--;
		nb--;
	}
	while (na && nb && op_eq(&a[na - 1], &b[nb - 1])) {
		na--;
		nb--;
	}

	uint16_t *t = na && nb ? malloc((size_t)(na + 1) * (nb + 1) *
					sizeof(*t))
			       : NULL;
	int i = 0, j = 0, st = nb + 1;

	if (t) {
		/* t[i][j]: the common subsequence of a[i..] and b[j..]. */
		for (int x = na; x >= 0; x--)
			for (int y = nb; y >= 0; y--)
				t[x * st + y] =
					x == na || y == nb ? 0
					: op_eq(&a[x], &b[y])
						? (uint16_t)(t[(x + 1) * st +
							       y + 1] + 1)
					: t[(x + 1) * st + y] >
							t[x * st + y + 1]
						? t[(x + 1) * st + y]
						: t[x * st + y + 1];
		while (i < na && j < nb) {
			if (op_eq(&a[i], &b[j])) {
				i++;
				j++;
			} else if (t[(i + 1) * st + j] >= t[i * st + j + 1]) {
				op_touch(out, &a[i++], scale);
			} else {
				op_touch(out, &b[j++], scale);
			}
		}
		free(t);
	}
	for (; i < na; i++)
		op_touch(out, &a[i], scale);
	for (; j < nb; j++)
		op_touch(out, &b[j], scale);
}

/*
 * The pixels of a w x h picture at `scale` where the one keyed `from` and the
 * one the current state would paint can differ; -1 when that is not known —
 * a key not among those remembered, or a salt or a palette that moved, which moves
 * the body under every pixel.
 *
 * It is also libkwl's "what moved" (kwl_set_backdrop_cache): a buffer that
 * wears an older picture restores and repaints only these pixels, and the
 * compositor is told only about these.
 */
static int pic_diff(uint64_t from, int w, int h, int scale,
		    pixman_region32_t *out)
{
	for (int i = 0; i < HIST_N; i++) {
		if (!hist[i].used || hist[i].key != from)
			continue;
		if (hist[i].salt != cur_salt() || hist[i].theme != theme_key())
			return -1;
		hist[i].stamp = ++hist_clock;
		pixman_region32_clear(out);
		ops_changed(hist[i].ops, hist[i].nops, ops, nops, scale, out);
		pixman_region32_intersect_rect(out, out, 0, 0, (unsigned)w,
					       (unsigned)h);
		return 0;
	}
	return -1;
}

static void cache_drop(void)
{
	if (cache_img) {
		pixman_image_unref(cache_img);
		cache_img = NULL;
	}
	cache_w = cache_h = cache_scale = 0;
}

static void popup_draw(pixman_image_t *dst, int w, int h, int scale);
static void bare_draw(pixman_image_t *dst, int w, int h, int scale);
static void flat_draw(pixman_image_t *dst, int w, int h, int scale);

static void pic_draw(pixman_image_t *dst, int w, int h, int scale)
{
	switch (backdrop_kind) {
	case BD_POPUP:
		popup_draw(dst, w, h, scale);
		break;
	case BD_CUSTOM:
		if (custom.draw)
			custom.draw(dst, w, h, scale);
		break;
	case BD_FLAT:
		flat_draw(dst, w, h, scale);
		break;
	default:
		bare_draw(dst, w, h, scale);
		break;
	}
}

/*
 * The cached picture, brought up to date: re-rasterised only when something
 * it depends on moved since the last time, and then only where it moved. NULL
 * when there is no memory for a copy, and the caller then draws straight into
 * its own destination.
 *
 * THE PART IS DRAWN UNDER A CLIP, by the same draw that paints the whole: the
 * body and every op go down again with pixman discarding everything outside
 * the changed region. Every primitive a picture is made of is a pixman fill
 * that honours the destination's clip and places each pixel by its own
 * coordinates, so a pixel drawn under the clip is the pixel the whole draw
 * would have put there — which is what lets a moved plate cost its own
 * rectangles and not the surface.
 */
static pixman_image_t *cache_get(int w, int h, int scale)
{
	uint64_t key = pic_key();

	hist_note(key);
	if (cache_img && cache_w == w && cache_h == h &&
	    cache_scale == scale && cache_key == key)
		return cache_img;
	if (cache_img && cache_w == w && cache_h == h &&
	    cache_scale == scale) {
		pixman_region32_t r;
		int ok;

		pixman_region32_init(&r);
		ok = pic_diff(cache_key, w, h, scale, &r) == 0;
		if (ok) {
			pixman_image_set_clip_region32(cache_img, &r);
			pic_draw(cache_img, w, h, scale);
			pixman_image_set_clip_region32(cache_img, NULL);
			cache_key = key;
		}
		pixman_region32_fini(&r);
		if (ok)
			return cache_img;
	}
	if (!cache_img || cache_w != w || cache_h != h) {
		cache_drop();
		cache_img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						     NULL, 0);
		if (!cache_img)
			return NULL;
	}
	pic_draw(cache_img, w, h, scale);
	cache_w = w;
	cache_h = h;
	cache_scale = scale;
	cache_key = key;
	return cache_img;
}

/* libkwl's full paint: the whole picture, through the cache. */
static void px_backdrop(pixman_image_t *dst, int w, int h, int scale)
{
	pixman_image_t *img = cache_get(w, h, scale);

	if (!img) {
		/* No memory for a copy is not a reason to draw nothing. */
		pic_draw(dst, w, h, scale);
		return;
	}
	pixman_image_composite32(PIXMAN_OP_SRC, img, NULL, dst, 0, 0, 0, 0, 0,
				 0, w, h);
}

/*
 * libkwl's partial paint: one rectangle of the SAME picture, laid back under
 * cells about to be repainted. A composite rather than a clipped redraw,
 * because the cell painter keeps its own clip on this image and a draw that
 * set one would take it away. Refusing without a cache is what sends libkwl to
 * the full paint instead.
 */
static int px_band(pixman_image_t *dst, int w, int h, int scale, int x, int y,
		   int bw, int bh)
{
	pixman_image_t *img = cache_get(w, h, scale);

	if (!img)
		return -1;
	pixman_image_composite32(PIXMAN_OP_SRC, img, NULL, dst, x, y, 0, 0, x,
				 y, bw, bh);
	return 0;
}

/*
 * libkwl's scroll: are two runs of pixel rows of the current picture the same
 * pixels? A grid band moved from one to the other carries its picture along,
 * and is right only where the answer is yes.
 *
 * ANSWERED FROM THE OP LIST, NOT THE PIXELS. Comparing the cached rows reads
 * the whole band twice, and measured at 1080p that costs more than the move
 * saves. The bare body is clear everywhere and the flat one is one colour
 * everywhere, so their rows are all the same until an op touches one: an op
 * that reaches either run makes the answer no, unless it is a flat rectangle
 * covering both runs top to bottom, which is the same colour on every row it
 * covers. The popup and custom bodies are graded top to bottom — no two rows
 * a cell apart are the same pixels — so they answer no without looking, and a
 * list scrolled over one is repainted.
 */
static int px_rows_same(int w, int h, int scale, int y, int from, int n)
{
	int lo = y < from ? y : from, hi = (y > from ? y : from) + n;

	(void)w;
	if ((backdrop_kind != BD_BARE && backdrop_kind != BD_FLAT) || n <= 0 ||
	    lo < 0 || hi > h)
		return 0;
	for (int i = 0; i < nops; i++) {
		const struct px_op *p = &ops[i];
		int t = p->y * scale, b = (p->y + p->h) * scale;
		int hit = (t < y + n && b > y) || (t < from + n && b > from);

		if (hit && !(p->kind == PXO_RECT && t <= lo && b >= hi))
			return 0;
	}
	return 1;
}

/*
 * THE KEY libkwl asks at paint time, and the opaque claim beside it. libkwl
 * asks this once per commit, before it paints, so the claim made here is about
 * exactly the picture that commit carries: a body at alpha 255 covers every
 * pixel, and anything under a surface that says so is culled and never
 * blended. Only a backdrop that can say so claims it — a bare one is mostly
 * holes.
 *
 * Every key handed out is remembered with its op list, because libkwl will
 * later ask what differs from it (pic_diff).
 */
static uint64_t px_key(void)
{
	int opaque = 0;
	uint64_t key = pic_key();

	if (backdrop_kind == BD_POPUP)
		opaque = kch_popup_alpha() == 0xFF;
	else if (backdrop_kind == BD_FLAT)
		opaque = 1;
	else if (backdrop_kind == BD_CUSTOM && custom.opaque)
		opaque = custom.opaque();
	kwl_set_opaque(opaque);
	hist_note(key);
	return key;
}


/*
 * THE DISPLAY ASKS WHETHER THE PLATE LIST MOVED, at flush time.
 *
 * A frame is committed on a CELL diff, and a plate is not a cell: a menu row
 * highlighted by one changes no text, so without this a highlight following
 * the pointer down a list produces no frame at all and stays where it was.
 *
 * It has to be a question rather than an announcement. A surface re-records
 * its whole list on every draw, so a flag set while recording cannot tell a
 * change from a redescription and would make every draw of a backdrop
 * surface a commit. Asked once the list is complete, the answer is the key of
 * what is recorded now against the key of the picture last painted — and that
 * is a change exactly when the two disagree.
 */
static int px_dirty(void)
{
	if (!backdrop_on)
		return 0;
	if (!cache_img)
		return 1;		/* nothing painted yet */
	return cache_key != pic_key();
}

/* Every backdrop goes in through here, so the cache, the dirty question and
 * the band restore are always installed together. */
static void px_install(int kind)
{
	backdrop_on = 1;
	backdrop_kind = kind;
	handed_slot = -1;
	cache_drop();
	hist_drop();
	/* A surface put up afresh has no plate on the screen to slide from. */
	memset(row_anim, 0, sizeof(row_anim));
	kwl_set_backdrop(px_backdrop);
	kwl_set_backdrop_cache(px_key, px_band, pic_diff, px_rows_same);
	kwl_set_pixels_dirty_fn(px_dirty);
}

void kch_px_reset(void)
{
	nops = 0;
	pool_len = 0;
	rec_gen++;
}

/* The op just added, or NULL when it was refused. Zeroed whole first: the key
 * is a hash of the list's bytes, padding and a kind's unused fields included,
 * so two ops that draw the same thing must be the same bytes. */
static struct px_op *add(int kind, int x, int y, int w, int h, int r,
			 uint32_t top, uint32_t bot, uint8_t a)
{
	struct px_op *p;

	if (nops >= PX_MAX || w <= 0 || h <= 0 || a == 0)
		return NULL;
	p = &ops[nops++];
	memset(p, 0, sizeof(*p));
	p->kind = (uint8_t)kind;
	p->alpha = a;
	p->radius = (uint8_t)r;
	p->x = (int16_t)x;
	p->y = (int16_t)y;
	p->w = (int16_t)w;
	p->h = (int16_t)h;
	p->rgb = top;
	p->rgb2 = bot;
	return p;
}

void kch_px_rect(int x, int y, int w, int h, uint32_t rgb, uint8_t a)
{
	add(PXO_RECT, x, y, w, h, 0, rgb, rgb, a);
}

void kch_px_round(int x, int y, int w, int h, int r, uint32_t rgb, uint8_t a)
{
	add(PXO_ROUND, x, y, w, h, r, rgb, rgb, a);
}

void kch_px_grad(int x, int y, int w, int h, int r, uint32_t top, uint32_t bot,
		 uint8_t a)
{
	add(PXO_ROUND, x, y, w, h, r, top, bot, a);
}

/*
 * A plate under a run of CELLS.
 *
 * The inset is what keeps a plate off its neighbours, so a row of buttons
 * reads as buttons rather than as one long bar — and it is why this takes cell
 * coordinates: every caller has them, and converting in one place means a
 * plate and the glyphs on it cannot disagree about where the button is.
 */
void kch_px_plate(int cx, int cy, int cw, int ch, KchTone tone, int inset)
{
	int w = kwl_cell_w(), h = kwl_cell_h();

	add(PXO_ROUND, cx * w + inset, cy * h + inset, cw * w - 2 * inset,
	    ch * h - 2 * inset, KCH_PLATE_RADIUS, kch_tone(tone),
	    kch_tone(tone), kch_tone_alpha(tone));
}

/*
 * WHETHER A RECORDED OP CAN EVER REACH A SCREEN, asked in one place.
 *
 * The list is replayed by a backdrop, and a backdrop is painted by libkwl — so
 * it reaches a screen only where one is installed, which a `--dump` run never
 * does.
 *
 * A control whose only state cue is a plate is a control with no state at all
 * where this is false, which is why every caller of `kch_px_row()` and
 * `kch_px_row_anim()` also draws the cell form of the same fact.
 */
int kch_px_live(void)
{
	return backdrop_on;
}

/*
 * THE SLOT WHOSE CELLS SHOW THE BACKDROP, or -1: the one the surface handed
 * its backdrop when it is still clear, else any slot cleared to alpha 0 (the
 * taskbar clears its own). Display text lies under cells of this slot, and
 * with none there is nowhere for it to show through.
 */
static int own_slot(void)
{
	if (!backdrop_on)
		return -1;
	if (handed_slot >= 0 && kcell_slot_alpha(handed_slot) == 0)
		return handed_slot;
	for (int i = 0; i < 8; i++)
		if (kcell_slot_alpha(i) == 0)
			return i;
	return -1;
}

int kch_display_live(void)
{
	return own_slot() >= 0;
}

static uint64_t str_hash(const char *s)
{
	uint64_t k = 1469598103934665603ULL;

	for (const unsigned char *b = (const unsigned char *)s; *b; b++)
		k = (k ^ *b) * FNV_PRIME;
	return k;
}

int kch_display_cols(const char *s, int rows)
{
	int cw = kwl_cell_w(), px = rows * kwl_cell_h();
	int tw;

	if (!s || !*s || cw <= 0 || px <= 0)
		return 0;
	tw = kcell_canvas_text_width(px, s);
	return tw > 0 ? (tw + cw - 1) / cw : 0;
}

/*
 * RECORDED, OR REFUSED WHOLE. Every way this can fail — no backdrop, no slot
 * of the backdrop's to lie under, no face at the size, a full list or pool —
 * answers 0 before anything is recorded or any cell is touched, so a caller's
 * fallback draws on a frame this call left alone.
 */
int kch_px_text(int cx, int cy, int cw, int ch, const char *s, int fg, int bg,
		int align)
{
	int own = own_slot(), pw = kwl_cell_w(), ph = kwl_cell_h();
	int px = ch * ph, plate;
	size_t n;
	struct px_op *p;

	if (own < 0 || !s || !*s || cw <= 0 || ch <= 0 || pw <= 0 || px <= 0 ||
	    px > 0xFFFF)
		return 0;
	n = strlen(s);
	plate = (bg & 7) != own;
	if (n + 1 > (size_t)(PX_POOL - pool_len) || nops + plate + 1 > PX_MAX)
		return 0;
	if (kcell_canvas_text_width(px, s) <= 0)
		return 0;

	/* The cells' own background, laid as pixels under the text: the
	 * cells over it become the backdrop's, so whatever they were filled
	 * with has to be in the picture instead. */
	if (plate)
		add(PXO_RECT, cx * pw, cy * ph, cw * pw, px, 0,
		    kch_slot_rgb(bg), kch_slot_rgb(bg), 0xFF);
	p = add(PXO_TEXT, cx * pw, cy * ph, cw * pw, px, 0, kch_slot_rgb(fg),
		kch_slot_rgb(fg), 0xFF);
	if (!p)
		return 0;
	p->align = (uint8_t)align;
	p->size = (uint16_t)px;
	p->str = (uint16_t)pool_len;
	p->txt = str_hash(s);
	memcpy(pool + pool_len, s, n + 1);
	pool_len += (int)n + 1;
	ktui_draw_fill(krect(cx, cy, cw, ch), own);
	return 1;
}

/* See kchrome.h: the declaration is libkwl's to act on. */
void kch_list_view(int x, int y, int w, int h, int top)
{
	kwl_list_view(x, y, w, h, top);
}

/* A selected row by its pixel rectangle: one cell high, `w` wide, with its top
 * left corner at (x, y). kch_px_row() is this at a cell's corner. */
static void row_px(int x, int y, int w, int h, KchTone tone)
{
	add(PXO_ROUND, x + 1, y + 1, w - 2, h - 2, KCH_PLATE_RADIUS,
	    kch_tone(tone), kch_tone(tone), kch_tone_alpha(tone));
	/* Two pixels, inset from the plate's own rounded corner: a bar that
	 * started at the plate's edge would be cut by the radius at both ends
	 * and read as two dots. */
	add(PXO_RECT, x + 1, y + 2, 2, h - 4, 0, kch_slot_rgb(KT_ACCENT),
	    kch_slot_rgb(KT_ACCENT), 0xFF);
}

void kch_px_row(int cx, int cy, int cw, KchTone tone)
{
	int w = kwl_cell_w(), h = kwl_cell_h();

	row_px(cx * w, cy * h, cw * w, h, tone);
}

static int lerp_px(int from, int to, float v)
{
	float d = (float)(to - from) * v;

	return from + (int)(d < 0.0f ? d - 0.5f : d + 0.5f);
}

void kch_px_row_anim(int key, int item, int cx, int cy, int cw, KchTone tone)
{
	int w = kwl_cell_w(), h = kwl_cell_h();
	int tx = cx * w, ty = cy * h, tw = cw * w;
	struct row_anim *r = NULL;
	float v;

	for (int i = 0; i < ROW_ANIM_N; i++)
		if (row_anim[i].used && row_anim[i].key == key)
			r = &row_anim[i];
	if (!r) {
		r = &row_anim[0];
		for (int i = 1; i < ROW_ANIM_N && r->used; i++)
			if (!row_anim[i].used || row_anim[i].stamp < r->stamp)
				r = &row_anim[i];
		memset(r, 0, sizeof(*r));
	}

	if (!r->used || r->cell_w != w || r->cell_h != h ||
	    rec_gen - r->seen > 1 ||
	    (r->item == item && (r->x != tx || r->y != ty || r->w != tw))) {
		/* Put it where it lands: nothing on the screen to slide from,
		 * or the same item carried by the page rather than moved. */
		ktui_anim_stop(&r->a);
		r->fx = tx;
		r->fy = ty;
		r->fw = tw;
	} else if (r->item != item) {
		/* From where it stands NOW, so a selection moved again
		 * mid-slide turns in the air rather than jumping back. */
		v = ktui_anim_value(&r->a);
		r->fx = lerp_px(r->fx, r->x, v);
		r->fy = lerp_px(r->fy, r->y, v);
		r->fw = lerp_px(r->fw, r->w, v);
		if (r->fx != tx || r->fy != ty || r->fw != tw)
			ktui_anim_start(&r->a, 0.0f, 1.0f, KCH_ROW_ANIM_MS,
					KT_EASE_OUT, 0);
		else
			ktui_anim_stop(&r->a);
	}
	r->used = 1;
	r->key = key;
	r->item = item;
	r->x = tx;
	r->y = ty;
	r->w = tw;
	r->cell_w = w;
	r->cell_h = h;
	r->seen = rec_gen;
	r->stamp = ++row_clock;

	/* A plate that set out from its target is on it, whatever a stopped
	 * animation's end value happens to be. */
	v = r->fx == tx && r->fy == ty && r->fw == tw ? 1.0f
						       : ktui_anim_value(&r->a);
	row_px(lerp_px(r->fx, tx, v), lerp_px(r->fy, ty, v),
	       lerp_px(r->fw, tw, v), h, tone);
}

/*
 * A one-pixel vertical rule at a cell boundary — a segment separator.
 *
 * It costs no columns at all, which the glyph it replaces did: a `║` in the
 * fill colour spent a whole cell and still read as three or four strokes once
 * the fills either side of it were counted. Drawn short of the full height so
 * it reads as a divider rather than as a wall.
 */
void kch_px_vrule(int cx, int y0, int rows)
{
	int w = kwl_cell_w(), h = kwl_cell_h();

	add(PXO_RECT, cx * w + w / 2, y0 * h + h / 4, 1, rows * h - h / 2, 0,
	    kch_tone(KCH_T_EDGE), kch_tone(KCH_T_EDGE),
	    kch_tone_alpha(KCH_T_EDGE));
}

/*
 * DISPLAY TEXT: a string at a pixel size of its own, in its rectangle.
 *
 * The size is recorded at scale 1 and asked for at `scale`, so a HiDPI output
 * gets the face at the size it needs rather than a stretched one — and the
 * size policy makes the common case exact: a rectangle two rows of a 32-pixel
 * Terminus cell high asks for 64, which is the 32 strike doubled, and at
 * scale 2 for 128. The line is centred in the rectangle and the advance is
 * measured at the size drawn, so alignment is exact at every scale.
 */
static void text_replay(pixman_image_t *dst, const struct px_op *p, int scale)
{
	int ps = p->size * scale;
	int x = p->x * scale, y = p->y * scale, w = p->w * scale,
	    h = p->h * scale;
	const char *str = pool + p->str;
	int tw = kcell_canvas_text_width(ps, str);
	int th = kcell_canvas_text_height(ps);
	int tx = p->align == KCH_ALIGN_RIGHT    ? x + w - tw
		 : p->align == KCH_ALIGN_CENTER ? x + (w - tw) / 2
						: x;

	kcell_text_draw(dst, tx, y + (h - th) / 2 + kcell_canvas_text_ascent(ps),
			ps, str, p->rgb, x, y, w, h);
}

void kch_px_replay(pixman_image_t *dst, int scale)
{
	for (int i = 0; i < nops; i++) {
		const struct px_op *p = &ops[i];

		if (p->kind == PXO_TEXT)
			text_replay(dst, p, scale);
		else if (p->kind == PXO_ROUND)
			kcell_px_round_grad(dst, p->x * scale, p->y * scale,
					    p->w * scale, p->h * scale,
					    p->radius * scale, p->rgb,
					    p->rgb2, p->alpha);
		else
			kcell_px_fill(dst, p->x * scale, p->y * scale,
				      p->w * scale, p->h * scale, p->rgb,
				      p->alpha);
	}
}

/*
 * The body every KDOS surface is painted on: a gradient, an edge on the side
 * the desktop is, and a highlight just inside it.
 *
 * Shared because the taskbar and the Start menu wearing two different bodies
 * is exactly the drift this library exists to prevent — and because the edge
 * is doing load-bearing work. The fill cannot say where a surface starts: a
 * translucent dark body over this distro's near-black wallpaper is within
 * 1.07:1 of it in every accent. The edge is 2.2-2.8:1 and one pixel.
 */
void kch_px_body(pixman_image_t *dst, int w, int h, int scale, uint8_t alpha,
		 int edge)
{
	int e = scale;

	kcell_px_clear(dst, 0, 0, w, h);
	kcell_px_vgrad(dst, 0, 0, w, h, kch_tone(KCH_T_BODY_TOP),
		       kch_tone(KCH_T_BODY_BOT), alpha);
	if (edge == KCH_EDGE_BOTTOM) {
		kcell_px_fill(dst, 0, h - e, w, e, kch_tone(KCH_T_EDGE),
			      kch_tone_alpha(KCH_T_EDGE));
		kcell_px_fill(dst, 0, h - 2 * e, w, e, kch_tone(KCH_T_LIP),
			      kch_tone_alpha(KCH_T_LIP));
	} else if (edge == KCH_EDGE_TOP) {
		kcell_px_fill(dst, 0, 0, w, e, kch_tone(KCH_T_EDGE),
			      kch_tone_alpha(KCH_T_EDGE));
		kcell_px_fill(dst, 0, e, w, e, kch_tone(KCH_T_LIP),
			      kch_tone_alpha(KCH_T_LIP));
	}
}

/*
 * Every popup this desktop opens wears the taskbar's body.
 *
 * The two halves of the tree disagree about which slot is "the background" —
 * some surfaces fill with KT_BG and some with KT_SURFACE — and normalising
 * that would be twenty files of slot renaming for no visible gain. Taking the
 * slot as an argument costs one word at each call site and leaves each surface
 * its own vocabulary; what has to agree is the PICTURE, and that is this file.
 *
 * Clearing the slot to alpha 0 is what hands those cells to the backdrop:
 * kwl_set_backdrop() flips the sense of a zero alpha from "punch a hole" to
 * "leave it alone, something under you owns it". Order matters — the backdrop
 * has to be installed before the slot is cleared, or the first frame goes out
 * with a hole where the surface should be.
 */
static void popup_draw(pixman_image_t *dst, int w, int h, int scale)
{
	kch_px_body(dst, w, h, scale, kch_popup_alpha(), KCH_EDGE_NONE);
	kch_px_replay(dst, scale);
}

static int body_slot_v = KT_BG;

static void bare_draw(pixman_image_t *dst, int w, int h, int scale)
{
	kcell_px_clear(dst, 0, 0, w, h);
	kch_px_replay(dst, scale);
}

/* Opaque, so every pixel is the slot's or an op's: nothing under the surface
 * shows through, which is what the opaque claim in px_key() says. */
static void flat_draw(pixman_image_t *dst, int w, int h, int scale)
{
	kcell_px_fill(dst, 0, 0, w, h, kch_slot_rgb(handed_slot), 0xFF);
	kch_px_replay(dst, scale);
}

void kch_px_bare(int body_slot)
{
	px_install(BD_BARE);
	handed_slot = body_slot & 7;
	kcell_set_slot_alpha(handed_slot, 0);
}

void kch_px_popup(int body_slot)
{
	body_slot_v = body_slot & 7;
	px_install(BD_POPUP);
	handed_slot = body_slot_v;
	kcell_set_slot_alpha(body_slot_v, 0);
}

void kch_px_flat(int body_slot)
{
	px_install(BD_FLAT);
	handed_slot = body_slot & 7;
	kcell_set_slot_alpha(handed_slot, 0);
}

void kch_px_custom(const KchPxBackdrop *bd)
{
	custom = *bd;
	px_install(BD_CUSTOM);
}

/*
 * Which slot the surface behind this chrome is drawn in.
 *
 * The shared chrome has to CLEAR things — the strip a button bar owns, the
 * background a heading sits on — and "clear" means "back to the page", not
 * "back to KT_BG". Half this desktop's surfaces call their page KT_SURFACE,
 * and with a translucent body a hardcoded KT_BG paints an opaque band across
 * the one row a button bar owns. KT_BG until told otherwise, which is what
 * every surface that never installs a backdrop wants.
 */
int kch_body_slot(void)
{
	return body_slot_v;
}

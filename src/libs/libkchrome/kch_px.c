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

#include "kchrome.h"
#include "kcell.h"
#include "kwl.h"

enum { PXO_RECT = 0, PXO_ROUND };

struct px_op {
	uint8_t kind, alpha, radius;
	int16_t x, y, w, h;
	uint32_t rgb, rgb2;		/* equal for a flat fill */
};

/*
 * Bounded rather than grown: a surface wanting more plates than this has
 * stopped being a cell grid. Overflow drops the op, which loses a plate and
 * can never corrupt anything.
 */
#define PX_MAX 256
static struct px_op ops[PX_MAX];
static int nops;

void kch_px_reset(void)
{
	nops = 0;
}

static void add(int kind, int x, int y, int w, int h, int r, uint32_t top,
		uint32_t bot, uint8_t a)
{
	struct px_op *p;

	if (nops >= PX_MAX || w <= 0 || h <= 0 || a == 0)
		return;
	p = &ops[nops++];
	p->kind = (uint8_t)kind;
	p->alpha = a;
	p->radius = (uint8_t)r;
	p->x = (int16_t)x;
	p->y = (int16_t)y;
	p->w = (int16_t)w;
	p->h = (int16_t)h;
	p->rgb = top;
	p->rgb2 = bot;
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

void kch_px_row(int cx, int cy, int cw, KchTone tone)
{
	int w = kwl_cell_w(), h = kwl_cell_h();

	kch_px_plate(cx, cy, cw, 1, tone, 1);
	/* Two pixels, inset from the plate's own rounded corner: a bar that
	 * started at the plate's edge would be cut by the radius at both ends
	 * and read as two dots. */
	add(PXO_RECT, cx * w + 1, cy * h + 2, 2, h - 4, 0,
	    kch_slot_rgb(KT_ACCENT), kch_slot_rgb(KT_ACCENT), 0xFF);
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

void kch_px_replay(pixman_image_t *dst, int scale)
{
	for (int i = 0; i < nops; i++) {
		const struct px_op *p = &ops[i];

		if (p->kind == PXO_ROUND)
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
static void popup_backdrop(pixman_image_t *dst, int w, int h, int scale)
{
	kch_px_body(dst, w, h, scale, kch_popup_alpha(), KCH_EDGE_NONE);
	kch_px_replay(dst, scale);
}

static int body_slot_v = KT_BG;

static void bare_backdrop(pixman_image_t *dst, int w, int h, int scale)
{
	kcell_px_clear(dst, 0, 0, w, h);
	kch_px_replay(dst, scale);
}

void kch_px_bare(int body_slot)
{
	kwl_set_backdrop(bare_backdrop);
	kcell_set_slot_alpha(body_slot & 7, 0);
}

void kch_px_popup(int body_slot)
{
	body_slot_v = body_slot & 7;
	kwl_set_backdrop(popup_backdrop);
	kcell_set_slot_alpha(body_slot_v, 0);
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

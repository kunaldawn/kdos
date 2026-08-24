/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   the tone ladder — what a surface is made of, above the eight slots
 *
 * The eight VT slots say what a CELL is. They cannot say what a raised button
 * is, because the palette's dark end is compressed to nothing: measured,
 * `variant` against `backdrop` is 1.00:1 in phosphor and no better than 1.05:1
 * in any accent, so a panel painted in its own background colour is the same
 * colour as the desktop behind it. That is why every control on the bar used
 * to be drawn at full accent — 14:1 — with nothing in between, and why six
 * different things were shouting at once.
 *
 * These are DERIVED from the same nine scheme colours, mixed, and they are the
 * missing middle. One table, read by the taskbar and the Start menu both, so
 * that a plate means the same thing on each.
 *
 * Do not add a literal here. Every value below is a kcol_mix() of two scheme
 * colours, which is what makes `kdos theme amber` retint the whole of it.
 * ---------------------------------
 */

#include "kchrome.h"

/*
 * The alphas. A plate is translucent over the bar's own body, so a tone is a
 * colour AND the strength it is laid on at — separating them would let a
 * caller pair one with the wrong other.
 */
#define A_BODY   0xCC		/* 80% — comp.conf's panel_opacity default */
/*
 * A POPUP IS READ; THE BAR IS GLANCED AT, and that is the whole reason these
 * are two numbers.
 *
 * At the bar's 80% the Start menu over a file manager had the panes, the file
 * listing and the status line of the window BEHIND it legible through every
 * row of it — photographed, and the menu was the harder of the two to read.
 * The bar survives that because a taskbar is a row of pictures and it has the
 * adaptive-opacity switch besides; a menu is thirty rows of text and a person
 * is looking straight at it. 95% still shows the desktop moving underneath and
 * costs nothing that matters.
 */
#define A_POPUP  0xF2
#define A_EDGE   0xFF
#define A_LIP    0x59
#define A_REST   0x8C
#define A_HOVER  0xCC
#define A_ACTIVE 0xEB

static const uint8_t tone_alpha[KCH_T_N] = {
	[KCH_T_BODY_TOP] = A_BODY,  [KCH_T_BODY_BOT] = A_BODY,
	[KCH_T_EDGE]     = A_EDGE,  [KCH_T_LIP]      = A_LIP,
	[KCH_T_REST]     = A_REST,  [KCH_T_HOVER]    = A_HOVER,
	[KCH_T_ACTIVE]   = A_ACTIVE,
};

static uint32_t tone[KCH_T_N];
static const void *tone_for;		/* the theme the cache was built from */

/* Straight (not premultiplied) composite of `fg` at `a` over `bg`. Used only
 * to work out what a tone will LOOK like so the solve below can measure it;
 * the painting is pixman's and premultiplied. */
static uint32_t over(uint32_t fg, uint32_t bg, uint8_t a)
{
	uint32_t out = 0;

	for (int sh = 16; sh >= 0; sh -= 8) {
		uint32_t f = (fg >> sh) & 0xff, b = (bg >> sh) & 0xff;

		out |= ((f * a + b * (255 - a)) / 255) << sh;
	}
	return out;
}

/*
 * THE FOCUSED PLATE IS SOLVED, NOT CHOSEN.
 *
 * It has two jobs that pull against each other: stand far enough off the bar
 * to say which window you are in, and stay dark enough that the label on it
 * reads. Mixing further toward `pdark` does the first and undoes the second,
 * and no single percentage serves all four accents — measured, 68% gives
 * 3.95:1 for text on bone and 50% still only gives 4.83:1.
 *
 * So walk down from 68% and stop at the first mix whose label clears 7:1,
 * measured against the plate as it will actually be composited: over the bar's
 * body, over the desktop backdrop. Bone reaches the 30% floor without ever
 * clearing it and lands at 6.17:1 — which is why the 2px accent underline is
 * not decoration. On bone it carries more of the focused state than the plate
 * does.
 */
static uint32_t solve_active(const KcolScheme *s, uint32_t body_ref)
{
	uint32_t best = kcol_mix(s->dim, s->pdark, 30);

	for (int x = 68; x >= 30; x--) {
		uint32_t plate = kcol_mix(s->dim, s->pdark, x);
		uint32_t seen = over(plate, body_ref, A_ACTIVE);

		if (kcol_contrast(s->text, seen) >= 700)
			return plate;
		best = plate;
	}
	return best;
}

static void build(void)
{
	const KcolScheme *s = kcol_find(ktui_theme->name);
	uint32_t body_ref;

	if (!s)
		s = &kcol_schemes[0];

	/*
	 * The body is a gradient between these two, both close to `variant`
	 * and both dark: the bar's shape comes from its top edge and from the
	 * plates on it, never from the fill. Over a black wallpaper a
	 * translucent dark body IS the wallpaper, and no amount of mixing
	 * toward `deep` changes that — it only makes it darker.
	 */
	tone[KCH_T_BODY_TOP] = kcol_mix(s->deep, s->variant, 45);
	tone[KCH_T_BODY_BOT] = kcol_mix(s->deep, s->variant, 75);

	/* The edge does the work the fill cannot: 2.2-2.8:1 against the
	 * desktop in every accent, which is a line you can see and not a line
	 * that shouts. */
	tone[KCH_T_EDGE] = kcol_mix(s->dim, s->pdark, 45);
	tone[KCH_T_LIP]  = kcol_mix(s->pdark, s->text, 20);

	tone[KCH_T_REST]  = s->dim;
	tone[KCH_T_HOVER] = kcol_mix(s->dim, s->pdark, 40);

	body_ref = over(kcol_mix(s->deep, s->variant, 60), s->backdrop, A_BODY);
	tone[KCH_T_ACTIVE] = solve_active(s, body_ref);

	tone_for = ktui_theme;
}

/*
 * Keyed on the theme POINTER as well as on the explicit reset. A live
 * `kdos theme amber` swaps ktui_theme under a running panel, and a cache that
 * only a signal handler could invalidate is a cache that is stale in every
 * program that forgot to call the reset.
 */
uint32_t kch_tone(KchTone t)
{
	if (tone_for != ktui_theme)
		build();
	return tone[t < 0 || t >= KCH_T_N ? 0 : t];
}

uint8_t kch_tone_alpha(KchTone t)
{
	return tone_alpha[t < 0 || t >= KCH_T_N ? 0 : t];
}

uint8_t kch_popup_alpha(void)
{
	return A_POPUP;
}

/* One of libktui's eight slots as the packed value the pixel painters take.
 * Here rather than beside them because it is a palette question, and this is
 * the file that answers those — it must give a real answer even where the
 * painters themselves are stubbed out. */
uint32_t kch_slot_rgb(int slot)
{
	KRgb c = ktui_theme->slot[slot & 7];

	return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

void kch_tone_reset(void)
{
	tone_for = NULL;
}

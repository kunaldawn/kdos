/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the KDE role mapping, and the floors it is held to
 *
 * A KColorScheme set is two backgrounds and ten roles drawn on them, and
 * every role is SOLVED against both backgrounds rather than picked: the
 * palette's dark end is compressed enough (deep, backdrop and variant are
 * within 1.19:1 of each other) that a mapping judged by eye passes on one
 * background and fails on the other, and the two backgrounds of a set are
 * exactly where KDE puts alternating rows.
 *
 * The mapping, per set:
 *
 *   backgrounds   View variant / variant+5% text, Window deep / variant,
 *                 Button 9% / 11% text over deep, Tooltip 13%, Header and
 *                 Complementary 7%, the inactive header on deep.
 *   ForegroundNormal   text; deep on a Selection.
 *   ForegroundInactive the set's background walked towards its ink from
 *                      56% until it clears 4.5:1 on both backgrounds: the
 *                      kcol_muted() walk, re-solved per set because View and
 *                      Window are different grounds.
 *   Active, Link, Positive   the accent; deep on a Selection.
 *   Visited       pdark; deep 30% into the accent on a Selection.
 *   Negative, Neutral   urgent and secondary.
 *   DecorationFocus     the accent, held to 3:1.
 *   DecorationHover     35% of the way from the focus colour to deep (to
 *                       the accent on a Selection), backed off towards the
 *                       focus colour until it holds 3:1.
 *
 * Every accent-coloured role that misses its floor is walked towards the
 * set's ink one percent at a time, so it keeps as much of its hue as the
 * floor allows and never more.
 *
 * THE SELECTION IS THE ACCENT UNDER DEEP, never pdark under text. pdark under
 * text measures 2.42:1 on bone and 2.43:1 on paper; primary under deep
 * measures 6.02:1 at its worst, on paper. A selected row is the one row a
 * person is reading, so it is the last place a scheme may fall below AA.
 *
 * [General] AccentColor is never written. Plasma's colour applicator
 * recolours Selection, Link and Decoration whenever an accent is set, and
 * the colours it would write are not the solved ones.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

const char *const kth_set_group[KTH_NSET] = {
	"Colors:View",
	"Colors:Window",
	"Colors:Button",
	"Colors:Selection",
	"Colors:Tooltip",
	"Colors:Complementary",
	"Colors:Header",
	"Colors:Header][Inactive",
};

const char *const kth_role_key[KTH_NROLE] = {
	"BackgroundNormal",
	"BackgroundAlternate",
	"DecorationFocus",
	"DecorationHover",
	"ForegroundNormal",
	"ForegroundInactive",
	"ForegroundActive",
	"ForegroundLink",
	"ForegroundVisited",
	"ForegroundNegative",
	"ForegroundNeutral",
	"ForegroundPositive",
};

const char *const kth_wm_key[KTH_NWM] = {
	"activeBackground",
	"activeBlend",
	"activeForeground",
	"inactiveBackground",
	"inactiveBlend",
	"inactiveForeground",
};

#define FLOOR_TEXT	450
#define FLOOR_DECO	300

/* Where the muted walk starts, kcol_muted()'s own starting mix: every dark
 * scheme clears the floor there with the mix unchanged, so a walk that began
 * lower would hand them a dimmer tone than libkcolor gives the console. */
#define MUTED_START	56

/* How far towards deep the hover decoration starts. */
#define HOVER_MIX	35

/* The Selection's alternate background starts this far from the accent
 * towards deep: far enough to tell two selected rows apart, and walked back
 * whenever deep on it would fall under the text floor. */
#define SELALT_MIX	10

int kth_role_floor(int role)
{
	switch (role) {
	case KTH_BG:
	case KTH_BG_ALT:
		return 0;
	case KTH_DECO_FOCUS:
	case KTH_DECO_HOVER:
		return FLOOR_DECO;
	default:
		return FLOOR_TEXT;
	}
}

/* ── names ─────────────────────────────────────────────────────────────── */

const char *kth_title(const KcolScheme *sc)
{
	static const char pfx[] = "KDOS-";

	if (strncmp(sc->theme_name, pfx, sizeof(pfx) - 1) ||
	    !sc->theme_name[sizeof(pfx) - 1])
		kb_die("scheme %s: theme name '%s' does not start with %s",
		       sc->name, sc->theme_name, pfx);
	return sc->theme_name + sizeof(pfx) - 1;
}

int kth_is_light(const KcolScheme *sc)
{
	return kcol_luma(sc->deep) >= 0.5;
}

const KcolScheme *kth_dark_partner(const KcolScheme *sc)
{
	if (!kth_is_light(sc))
		return sc;
	if (!kth_is_light(kcol_default()))
		return kcol_default();
	for (int i = 0; i < kcol_nscheme; i++)
		if (!kth_is_light(&kcol_schemes[i]))
			return &kcol_schemes[i];
	kb_die("the palette has no dark scheme");
}

const KcolScheme *kth_light_scheme(void)
{
	for (int i = 0; i < kcol_nscheme; i++)
		if (kth_is_light(&kcol_schemes[i]))
			return &kcol_schemes[i];
	kb_die("the palette has no light scheme");
}

int kth_pick(const char *name, const KcolScheme **out, int cap)
{
	if (name) {
		const KcolScheme *sc = kcol_find(name);

		if (!sc)
			kb_die("unknown accent '%s' (try: kdos-theme accents)",
			       name);
		out[0] = sc;
		return 1;
	}
	if (kcol_nscheme > cap)
		kb_die("%d schemes and room for %d", kcol_nscheme, cap);
	for (int i = 0; i < kcol_nscheme; i++)
		out[i] = &kcol_schemes[i];
	return kcol_nscheme;
}

void kth_rgb(KbBuf *b, uint32_t c)
{
	KcolRgb v = kcol_rgb(c);

	kb_buf_printf(b, "%u,%u,%u", v.r, v.g, v.b);
}

/* ── the solvers ──────────────────────────────────────────────────────── */

static int holds(uint32_t c, uint32_t bg, uint32_t alt, int floor)
{
	return kcol_contrast(c, bg) >= floor && kcol_contrast(c, alt) >= floor;
}

/* `c` as it is when it holds, else walked towards `ink` until it does. The
 * ink always holds — the text pair is the first thing checked — so the walk
 * ends on the ink at worst. */
static uint32_t legible(uint32_t c, uint32_t ink, uint32_t bg, uint32_t alt,
			int floor)
{
	for (int pct = 0; pct <= 100; pct++) {
		uint32_t m = pct ? kcol_mix(c, ink, pct) : c;

		if (holds(m, bg, alt, floor))
			return m;
	}
	return ink;
}

static uint32_t muted(uint32_t ground, uint32_t ink, uint32_t bg,
		      uint32_t alt)
{
	for (int pct = MUTED_START; pct <= 100; pct++) {
		uint32_t m = kcol_mix(ground, ink, pct);

		if (holds(m, bg, alt, FLOOR_TEXT))
			return m;
	}
	return ink;
}

static uint32_t hover(uint32_t focus, uint32_t toward, uint32_t bg,
		      uint32_t alt)
{
	for (int pct = HOVER_MIX; pct > 0; pct--) {
		uint32_t m = kcol_mix(focus, toward, pct);

		if (holds(m, bg, alt, FLOOR_DECO))
			return m;
	}
	return focus;
}

/* ── one set ──────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t bg, alt;
	uint32_t ink;		/* what is read on it                   */
	uint32_t accent;	/* what an emphasised role starts from  */
	uint32_t visited;
	uint32_t toward;	/* where the hover decoration moves to  */
} SetDef;

static void fill(uint32_t *r, const KcolScheme *sc, const SetDef *d)
{
	r[KTH_BG] = d->bg;
	r[KTH_BG_ALT] = d->alt;
	r[KTH_FG] = legible(d->ink, d->ink, d->bg, d->alt, FLOOR_TEXT);
	r[KTH_FG_INACTIVE] = muted(d->bg, d->ink, d->bg, d->alt);
	r[KTH_FG_ACTIVE] = legible(d->accent, d->ink, d->bg, d->alt,
				   FLOOR_TEXT);
	r[KTH_FG_LINK] = r[KTH_FG_ACTIVE];
	r[KTH_FG_POSITIVE] = r[KTH_FG_ACTIVE];
	r[KTH_FG_VISITED] = legible(d->visited, d->ink, d->bg, d->alt,
				    FLOOR_TEXT);
	r[KTH_FG_NEGATIVE] = legible(sc->urgent, d->ink, d->bg, d->alt,
				     FLOOR_TEXT);
	r[KTH_FG_NEUTRAL] = legible(sc->secondary, d->ink, d->bg, d->alt,
				    FLOOR_TEXT);
	r[KTH_DECO_FOCUS] = legible(d->accent, d->ink, d->bg, d->alt,
				    FLOOR_DECO);
	r[KTH_DECO_HOVER] = hover(r[KTH_DECO_FOCUS], d->toward, d->bg,
				  d->alt);
}

void kth_palette(const KcolScheme *sc, KthPalette *p)
{
	uint32_t view_alt = kcol_mix(sc->variant, sc->text, 5);
	uint32_t button = kcol_mix(sc->deep, sc->text, 9);
	uint32_t button_alt = kcol_mix(sc->deep, sc->text, 11);
	uint32_t tooltip = kcol_mix(sc->deep, sc->text, 13);
	uint32_t header = kcol_mix(sc->deep, sc->text, 7);

	int pct = SELALT_MIX;
	uint32_t sel_alt = kcol_mix(sc->primary, sc->deep, pct);

	while (pct > 0 && kcol_contrast(sc->deep, sel_alt) < FLOOR_TEXT)
		sel_alt = kcol_mix(sc->primary, sc->deep, --pct);

	const SetDef def[KTH_NSET] = {
		[KTH_VIEW] = { sc->variant, view_alt, sc->text, sc->primary,
			       sc->pdark, sc->deep },
		[KTH_WINDOW] = { sc->deep, sc->variant, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_BUTTON] = { button, button_alt, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_SELECTION] = { sc->primary, sel_alt, sc->deep, sc->deep,
				    kcol_mix(sc->deep, sc->primary, 30),
				    sc->primary },
		[KTH_TOOLTIP] = { tooltip, tooltip, sc->text, sc->primary,
				  sc->pdark, sc->deep },
		[KTH_COMPLEMENTARY] = { header, header, sc->text, sc->primary,
					sc->pdark, sc->deep },
		[KTH_HEADER] = { header, header, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_HEADER_INACTIVE] = { sc->deep, header, sc->text,
					  sc->primary, sc->pdark, sc->deep },
	};

	for (int s = 0; s < KTH_NSET; s++)
		fill(p->set[s], sc, &def[s]);

	/* The title bar is the Header set: Breeze draws it from Header, and
	 * [WM] is what every other decoration and the window switcher read. */
	const uint32_t *h = p->set[KTH_HEADER];
	const uint32_t *hi = p->set[KTH_HEADER_INACTIVE];

	p->wm[KTH_WM_ACTIVE_BG] = h[KTH_BG];
	p->wm[KTH_WM_ACTIVE_FG] = h[KTH_FG];
	p->wm[KTH_WM_ACTIVE_BLEND] = h[KTH_DECO_FOCUS];
	p->wm[KTH_WM_INACTIVE_BG] = hi[KTH_BG];
	p->wm[KTH_WM_INACTIVE_FG] = muted(hi[KTH_BG], sc->text, hi[KTH_BG],
					  hi[KTH_BG]);
	p->wm[KTH_WM_INACTIVE_BLEND] = p->wm[KTH_WM_INACTIVE_FG];

	/* Both effect colours are inert at the amounts written beside them;
	 * they are the scheme's own so that an amount raised later blends into
	 * this scheme and not into Breeze's grey. */
	p->disabled = sc->deep;
	p->inactive = sc->variant;
}

/* ── the floors ───────────────────────────────────────────────────────── */

static int one(KthRow row, void *ctx, const char *group, const char *role,
	       uint32_t c, uint32_t bg, uint32_t alt, int floor)
{
	if ((c | bg | alt) & KTH_MISSING) {
		row(ctx, group, role, -1, -1, floor, 0);
		return 1;
	}

	int a = kcol_contrast(c, bg), b = kcol_contrast(c, alt);
	int ok = a >= floor && b >= floor;

	row(ctx, group, role, a, b, floor, ok);
	return !ok;
}

int kth_check(const KthPalette *p, KthRow row, void *ctx)
{
	int bad = 0;

	for (int s = 0; s < KTH_NSET; s++) {
		const uint32_t *r = p->set[s];

		for (int k = 0; k < KTH_NROLE; k++) {
			int floor = kth_role_floor(k);

			if (!floor) {
				if (r[k] & KTH_MISSING) {
					row(ctx, kth_set_group[s],
					    kth_role_key[k], -1, -1, 0, 0);
					bad++;
				}
				continue;
			}
			bad += one(row, ctx, kth_set_group[s], kth_role_key[k],
				   r[k], r[KTH_BG], r[KTH_BG_ALT], floor);
		}
	}

	bad += one(row, ctx, "WM", kth_wm_key[KTH_WM_ACTIVE_FG],
		   p->wm[KTH_WM_ACTIVE_FG], p->wm[KTH_WM_ACTIVE_BG],
		   p->wm[KTH_WM_ACTIVE_BG], FLOOR_TEXT);
	bad += one(row, ctx, "WM", kth_wm_key[KTH_WM_INACTIVE_FG],
		   p->wm[KTH_WM_INACTIVE_FG], p->wm[KTH_WM_INACTIVE_BG],
		   p->wm[KTH_WM_INACTIVE_BG], FLOOR_TEXT);
	return bad;
}

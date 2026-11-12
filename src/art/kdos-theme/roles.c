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
 *   backgrounds   View variant / variant+5% text, Window the ground (below)
 *                 / variant, Button 9% / 11% text over deep, Tooltip 13%,
 *                 Header and Complementary 7%, the inactive header on deep.
 *   ForegroundNormal   text; the selection's ink on a Selection.
 *   ForegroundInactive the set's background walked towards its ink from
 *                      56% until it clears 4.5:1 on both backgrounds: the
 *                      kcol_muted() walk, re-solved per set because View and
 *                      Window are different grounds.
 *   Active, Link, Positive   the accent; the ink on a Selection.
 *   Visited       pdark; the ink 30% into the selection on a Selection.
 *   Negative, Neutral   urgent and secondary.
 *   DecorationFocus     the accent, held to 3:1.
 *   DecorationHover     35% of the way from the focus colour to deep,
 *                       backed off towards the focus colour until it holds
 *                       3:1. On a Selection it is a fill instead: the
 *                       selection 35% further from its ink (below).
 *
 * Every accent-coloured role that misses its floor is walked towards the
 * set's ink one percent at a time, so it keeps as much of its hue as the
 * floor allows and never more.
 *
 * THE SELECTION CARRIES THE VIEW'S OWN TEXT on a dark scheme: the accent
 * walked towards deep until `text` holds 4.5:1 on it, with `text` as its
 * ink. KDE draws ForegroundNormal over Highlight as often as it draws
 * HighlightedText — Dolphin's and Breeze's translucent item selection lays
 * the Selection over the view and keeps the view's text, and so do Haruna,
 * KSudoku, the keyboard page's layout list and GTK through Plasma's bridge —
 * so a selection only `deep` reads on leaves that text at under 2:1. The
 * light scheme keeps the accent under `deep`, where both cases hold. Both
 * are measured: the solid set, and the view's text over the Selection laid
 * over both view backgrounds at SEL_BLEND, Dolphin's measured fill.
 *
 * A SELECTION'S HOVER IS A FILL UNDER THE SELECTION'S INK. Plasma's GTK
 * bridge writes Selection DecorationHover as GTK's hovered-selected
 * background, and GTK paints a hovered selected row in it with the
 * selection's text on top; a hover moved towards the ink would leave that
 * text near 1.5:1. Moving it away from the ink only raises the contrast.
 *
 * THE WINDOW GROUND IS `deep` WALKED TOWARDS GREY until the five shades
 * KColorScheme derives from it (shade.c) are no more than KTH_SHADE_SLACK
 * more colourful than the ground, under the scheme's own [KDE] contrast,
 * which every kdeglobals KDOS writes carries too. From a ground with a
 * channel at zero, as most `deep` rows have, KColorScheme makes Mid and its
 * kin the most saturated colour of the hue at a mid-grey luma, and
 * LibreOffice paints its workspace in Mid. The walk keeps the ground's
 * lightness and moves it a distance the eye barely reads at that depth.
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

/* The Selection's alternate background starts this far from the selection
 * towards deep: far enough to tell two selected rows apart, and walked back
 * whenever the selection's ink on it would fall under the text floor. */
#define SELALT_MIX	10

/* How much of the Selection a translucent selection lays over a view:
 * Dolphin's selected and hovered item, measured on screen. */
#define SEL_BLEND	64

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

/* The ink always holds — the text pair is the first thing checked — so the
 * walk ends on the ink at worst. */
uint32_t kth_legible(uint32_t c, uint32_t ink, uint32_t bg, uint32_t alt,
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
	r[KTH_FG] = kth_legible(d->ink, d->ink, d->bg, d->alt, FLOOR_TEXT);
	r[KTH_FG_INACTIVE] = muted(d->bg, d->ink, d->bg, d->alt);
	r[KTH_FG_ACTIVE] = kth_legible(d->accent, d->ink, d->bg, d->alt,
				   FLOOR_TEXT);
	r[KTH_FG_LINK] = r[KTH_FG_ACTIVE];
	r[KTH_FG_POSITIVE] = r[KTH_FG_ACTIVE];
	r[KTH_FG_VISITED] = kth_legible(d->visited, d->ink, d->bg, d->alt,
				    FLOOR_TEXT);
	r[KTH_FG_NEGATIVE] = kth_legible(sc->urgent, d->ink, d->bg, d->alt,
				     FLOOR_TEXT);
	r[KTH_FG_NEUTRAL] = kth_legible(sc->secondary, d->ink, d->bg, d->alt,
				    FLOOR_TEXT);
	r[KTH_DECO_FOCUS] = kth_legible(d->accent, d->ink, d->bg, d->alt,
				    FLOOR_DECO);
	r[KTH_DECO_HOVER] = hover(r[KTH_DECO_FOCUS], d->toward, d->bg,
				  d->alt);
}

static int shades_hold(uint32_t ground, int contrast)
{
	int bound = kth_chroma(ground) + KTH_SHADE_SLACK;

	for (int r = 0; r < KTH_NSHADE; r++)
		if (kth_chroma(kth_shade(ground, r, contrast)) > bound)
			return 0;
	return 1;
}

/* `deep` walked towards the grey of its brightest channel; the grey itself
 * makes grey shades, so the walk always ends. */
static uint32_t window_ground(const KcolScheme *sc)
{
	KcolRgb v = kcol_rgb(sc->deep);
	unsigned m = v.r > v.g ? v.r : v.g;

	m = m > v.b ? m : v.b;
	for (int pct = 0; pct <= 100; pct++) {
		uint32_t g = kcol_mix(sc->deep, m * 0x010101u, pct);

		if (shades_hold(g, KTH_CONTRAST))
			return g;
	}
	return m * 0x010101u;
}

static int blend_holds(uint32_t text, uint32_t sel, uint32_t view,
		       uint32_t view_alt)
{
	return kcol_contrast(text, kcol_mix(view, sel, SEL_BLEND)) >= FLOOR_TEXT &&
	       kcol_contrast(text, kcol_mix(view_alt, sel, SEL_BLEND)) >= FLOOR_TEXT;
}

static uint32_t selection(const KcolScheme *sc, uint32_t view,
			  uint32_t view_alt)
{
	if (kth_is_light(sc))
		return sc->primary;
	for (int pct = 0; pct <= 100; pct++) {
		uint32_t s = pct ? kcol_mix(sc->primary, sc->deep, pct)
				 : sc->primary;

		if (kcol_contrast(sc->text, s) >= FLOOR_TEXT &&
		    blend_holds(sc->text, s, view, view_alt))
			return s;
	}
	return sc->deep;
}

void kth_palette(const KcolScheme *sc, KthPalette *p)
{
	kth_palette_titled(sc, KTH_HEADER_MIX, 0, p);
}

void kth_palette_titled(const KcolScheme *sc, int header_pct, int inactive_pct,
			KthPalette *p)
{
	uint32_t view_alt = kcol_mix(sc->variant, sc->text, 5);
	uint32_t ground = window_ground(sc);
	uint32_t button = kcol_mix(sc->deep, sc->text, 9);
	uint32_t button_alt = kcol_mix(sc->deep, sc->text, 11);
	uint32_t tooltip = kcol_mix(sc->deep, sc->text, 13);
	uint32_t comp = kcol_mix(sc->deep, sc->text, KTH_HEADER_MIX);
	uint32_t header = kcol_mix(sc->deep, sc->text, header_pct);
	uint32_t header_off = inactive_pct ?
		kcol_mix(sc->deep, sc->text, inactive_pct) : sc->deep;

	uint32_t sel = selection(sc, sc->variant, view_alt);
	uint32_t ink = kth_is_light(sc) ? sc->deep : sc->text;
	int pct = SELALT_MIX;
	uint32_t sel_alt = kcol_mix(sel, sc->deep, pct);

	while (pct > 0 && kcol_contrast(ink, sel_alt) < FLOOR_TEXT)
		sel_alt = kcol_mix(sel, sc->deep, --pct);

	const SetDef def[KTH_NSET] = {
		[KTH_VIEW] = { sc->variant, view_alt, sc->text, sc->primary,
			       sc->pdark, sc->deep },
		[KTH_WINDOW] = { ground, sc->variant, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_BUTTON] = { button, button_alt, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_SELECTION] = { sel, sel_alt, ink, ink,
				    kcol_mix(ink, sel, 30), sel },
		[KTH_TOOLTIP] = { tooltip, tooltip, sc->text, sc->primary,
				  sc->pdark, sc->deep },
		[KTH_COMPLEMENTARY] = { comp, comp, sc->text, sc->primary,
					sc->pdark, sc->deep },
		[KTH_HEADER] = { header, header, sc->text, sc->primary,
				 sc->pdark, sc->deep },
		[KTH_HEADER_INACTIVE] = { header_off, header, sc->text,
					  sc->primary, sc->pdark, sc->deep },
	};

	for (int s = 0; s < KTH_NSET; s++)
		fill(p->set[s], sc, &def[s]);
	p->set[KTH_SELECTION][KTH_DECO_HOVER] =
		kcol_mix(sel, kth_is_light(sc) ? sc->text : sc->deep, HOVER_MIX);

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
	p->contrast = KTH_CONTRAST;
}

/* ── the floors ───────────────────────────────────────────────────────── */

static int one(KthRow row, void *ctx, const char *group, const char *role,
	       uint32_t c, uint32_t bg, uint32_t alt, int floor)
{
	if ((c | bg | alt) & KTH_MISSING) {
		row(ctx, group, role, KTH_ROW_CONTRAST, -1, -1, floor, 0);
		return 1;
	}

	int a = kcol_contrast(c, bg), b = kcol_contrast(c, alt);
	int ok = a >= floor && b >= floor;

	row(ctx, group, role, KTH_ROW_CONTRAST, a, b, floor, ok);
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
					    kth_role_key[k], KTH_ROW_CONTRAST,
					    -1, -1, 0, 0);
					bad++;
				}
				continue;
			}
			if (s == KTH_SELECTION && k == KTH_DECO_HOVER)
				bad += one(row, ctx, kth_set_group[s],
					   kth_role_key[k], r[KTH_FG], r[k],
					   r[k], FLOOR_TEXT);
			else
				bad += one(row, ctx, kth_set_group[s],
					   kth_role_key[k], r[k], r[KTH_BG],
					   r[KTH_BG_ALT], floor);
		}
	}

	/* The view's text over the Selection laid over the view. */
	const uint32_t *v = p->set[KTH_VIEW];
	uint32_t sel = p->set[KTH_SELECTION][KTH_BG];

	if ((v[KTH_BG] | v[KTH_BG_ALT] | sel) & KTH_MISSING)
		bad += one(row, ctx, "Colors:View", "SelectionBlend", v[KTH_FG],
			   KTH_MISSING, KTH_MISSING, FLOOR_TEXT);
	else
		bad += one(row, ctx, "Colors:View", "SelectionBlend", v[KTH_FG],
			   kcol_mix(v[KTH_BG], sel, SEL_BLEND),
			   kcol_mix(v[KTH_BG_ALT], sel, SEL_BLEND), FLOOR_TEXT);

	/* The shades Qt derives from the Window ground. */
	uint32_t ground = p->set[KTH_WINDOW][KTH_BG];

	for (int k = 0; k < KTH_NSHADE; k++) {
		if (ground & KTH_MISSING) {
			row(ctx, "Colors:Window", kth_shade_key[k],
			    KTH_ROW_CHROMA, -1, -1, 0, 0);
			bad++;
			continue;
		}
		int floor = kth_chroma(ground) + KTH_SHADE_SLACK;
		int c = kth_chroma(kth_shade(ground, k, p->contrast));
		int ok = c <= floor;

		row(ctx, "Colors:Window", kth_shade_key[k], KTH_ROW_CHROMA, c,
		    c, floor, ok);
		bad += !ok;
	}

	bad += one(row, ctx, "WM", kth_wm_key[KTH_WM_ACTIVE_FG],
		   p->wm[KTH_WM_ACTIVE_FG], p->wm[KTH_WM_ACTIVE_BG],
		   p->wm[KTH_WM_ACTIVE_BG], FLOOR_TEXT);
	bad += one(row, ctx, "WM", kth_wm_key[KTH_WM_INACTIVE_FG],
		   p->wm[KTH_WM_INACTIVE_FG], p->wm[KTH_WM_INACTIVE_BG],
		   p->wm[KTH_WM_INACTIVE_BG], FLOOR_TEXT);
	return bad;
}

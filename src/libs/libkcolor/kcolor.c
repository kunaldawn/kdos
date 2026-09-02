/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libkcolor — palette table and colour arithmetic
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"

#include "kcolor.h"

#define SCHEME(id, lbl, tname, p, dm, sec, urg, dp, txt, var, pd, bd)         \
	{ #id, lbl, tname, KCOL_HEX(p), KCOL_HEX(dm), KCOL_HEX(sec),          \
	  KCOL_HEX(urg), KCOL_HEX(dp), KCOL_HEX(txt), KCOL_HEX(var),          \
	  KCOL_HEX(pd), KCOL_HEX(bd) },

const KcolScheme kcol_schemes[] = { KCOL_SCHEMES(SCHEME) };

#undef SCHEME

const int kcol_nscheme = (int)(sizeof(kcol_schemes) / sizeof(kcol_schemes[0]));

const KcolScheme *kcol_find(const char *name)
{
	for (int i = 0; i < kcol_nscheme; i++)
		if (!strcmp(kcol_schemes[i].name, name))
			return &kcol_schemes[i];
	return NULL;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int kcol_parse(const char *s, uint32_t *out)
{
	if (*s == '#')
		s++;
	int d[6], n = 0;
	while (n < 6 && s[n]) {
		int v = hexval(s[n]);
		if (v < 0)
			break;
		d[n] = v;
		n++;
	}
	if (n == 3) {
		/* #39f expands to #3399ff, the same way CSS does. */
		*out = (uint32_t)((d[0] * 17) << 16 | (d[1] * 17) << 8 | (d[2] * 17));
		return 0;
	}
	if (n == 6) {
		*out = (uint32_t)(d[0] << 20 | d[1] << 16 | d[2] << 12 | d[3] << 8 |
				  d[4] << 4 | d[5]);
		return 0;
	}
	return -1;
}

void kcol_format(uint32_t rgb, char *buf)
{
	static const char hex[] = "0123456789abcdef";
	for (int i = 0; i < 6; i++)
		buf[i] = hex[(rgb >> (20 - i * 4)) & 0xf];
	buf[6] = 0;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* python's colorsys, transcribed. The odd-looking `2.0 - maxc - minc` rather
 * than `2.0 - sumc`, and the modulo on a possibly-negative hue, are both
 * exactly what CPython does; matching it is what keeps the regenerated theme
 * byte-identical to the artwork already in git. */

#define ONE_THIRD (1.0 / 3.0)
#define ONE_SIXTH (1.0 / 6.0)
#define TWO_THIRD (2.0 / 3.0)

/* Hue only ever arrives within a turn or two of range, so subtracting is both
 * exact and enough — and it keeps libkcolor off libm, which is what lets the
 * phase-1 consumers link it. */
static double pymod1(double x)
{
	while (x >= 1.0)
		x -= 1.0;
	while (x < 0.0)
		x += 1.0;
	return x;
}

void kcol_to_hls(uint32_t rgb, double *h, double *l, double *s)
{
	double r = (double)((rgb >> 16) & 0xff) / 255.0;
	double g = (double)((rgb >> 8) & 0xff) / 255.0;
	double b = (double)(rgb & 0xff) / 255.0;

	double maxc = r > g ? (r > b ? r : b) : (g > b ? g : b);
	double minc = r < g ? (r < b ? r : b) : (g < b ? g : b);
	double sumc = maxc + minc;
	double rangec = maxc - minc;

	*l = sumc / 2.0;
	if (minc == maxc) {
		*h = 0.0;
		*s = 0.0;
		return;
	}
	*s = (*l <= 0.5) ? rangec / sumc : rangec / (2.0 - maxc - minc);

	double rc = (maxc - r) / rangec;
	double gc = (maxc - g) / rangec;
	double bc = (maxc - b) / rangec;
	double hh;
	if (r == maxc)
		hh = bc - gc;
	else if (g == maxc)
		hh = 2.0 + rc - bc;
	else
		hh = 4.0 + gc - rc;
	*h = pymod1(hh / 6.0);
}

static double hls_v(double m1, double m2, double hue)
{
	hue = pymod1(hue);
	if (hue < ONE_SIXTH)
		return m1 + (m2 - m1) * hue * 6.0;
	if (hue < 0.5)
		return m2;
	if (hue < TWO_THIRD)
		return m1 + (m2 - m1) * (TWO_THIRD - hue) * 6.0;
	return m1;
}

/* python's round(): a half goes to EVEN, not away from zero. A plain +0.5
 * floor shifts one channel in a handful of the vendored icons, which is a
 * diff against artwork that is committed to git. */
static uint8_t chan(double v)
{
	double x = v * 255.0;
	if (x <= 0.0)
		return 0;
	if (x >= 255.0)
		return 255;
	long long i = (long long)x;
	double frac = x - (double)i;
	if (frac > 0.5 || (frac == 0.5 && (i & 1)))
		i++;
	return (uint8_t)(i > 255 ? 255 : i);
}

uint32_t kcol_from_hls(double h, double l, double s)
{
	double r, g, b;
	if (s == 0.0) {
		r = g = b = l;
	} else {
		double m2 = (l <= 0.5) ? l * (1.0 + s) : l + s - (l * s);
		double m1 = 2.0 * l - m2;
		r = hls_v(m1, m2, h + ONE_THIRD);
		g = hls_v(m1, m2, h);
		b = hls_v(m1, m2, h - ONE_THIRD);
	}
	return (uint32_t)chan(r) << 16 | (uint32_t)chan(g) << 8 | chan(b);
}

uint32_t kcol_mix(uint32_t a, uint32_t b, int pct)
{
	uint32_t out = 0;
	for (int i = 16; i >= 0; i -= 8) {
		unsigned x = (a >> i) & 0xff, y = (b >> i) & 0xff;
		out |= ((x * (100 - (unsigned)pct) + y * (unsigned)pct) / 100) << i;
	}
	return out;
}

uint32_t kcol_muted(const KcolScheme *sc)
{
	return kcol_mix(sc->deep, sc->text, 56);
}

/*
 * sRGB channel -> linear light, x65535, as a TABLE.
 *
 * The transfer function is a 2.4 power and this library does not link libm —
 * the phase-1 rule libktui keeps and libkcolor keeps with it. There are only
 * 256 possible inputs, so the honest answer is to have measured all of them:
 * generated as round(((c/255 + 0.055) / 1.055) ^ 2.4 * 65535), with the linear
 * segment c/12.92 below 0.04045, and checked against CPython to two decimals
 * of the contrast ratios it feeds.
 */
static const uint32_t srgb_lin[256] = {
	    0,    20,    40,    60,    80,    99,   119,   139,
	  159,   179,   199,   219,   241,   264,   288,   313,
	  340,   367,   396,   427,   458,   491,   526,   562,
	  599,   637,   677,   718,   761,   805,   851,   898,
	  947,   997,  1048,  1101,  1156,  1212,  1270,  1330,
	 1391,  1453,  1517,  1583,  1651,  1720,  1790,  1863,
	 1937,  2013,  2090,  2170,  2250,  2333,  2418,  2504,
	 2592,  2681,  2773,  2866,  2961,  3058,  3157,  3258,
	 3360,  3464,  3570,  3678,  3788,  3900,  4014,  4129,
	 4247,  4366,  4488,  4611,  4736,  4864,  4993,  5124,
	 5257,  5392,  5530,  5669,  5810,  5953,  6099,  6246,
	 6395,  6547,  6700,  6856,  7014,  7174,  7335,  7500,
	 7666,  7834,  8004,  8177,  8352,  8528,  8708,  8889,
	 9072,  9258,  9445,  9635,  9828, 10022, 10219, 10417,
	10619, 10822, 11028, 11235, 11446, 11658, 11873, 12090,
	12309, 12530, 12754, 12980, 13209, 13440, 13673, 13909,
	14146, 14387, 14629, 14874, 15122, 15371, 15623, 15878,
	16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001,
	18277, 18556, 18837, 19121, 19407, 19696, 19987, 20281,
	20577, 20876, 21177, 21481, 21787, 22096, 22407, 22721,
	23038, 23357, 23678, 24002, 24329, 24658, 24990, 25325,
	25662, 26001, 26344, 26688, 27036, 27386, 27739, 28094,
	28452, 28813, 29176, 29542, 29911, 30282, 30656, 31033,
	31412, 31794, 32179, 32567, 32957, 33350, 33745, 34143,
	34544, 34948, 35355, 35764, 36176, 36591, 37008, 37429,
	37852, 38278, 38706, 39138, 39572, 40009, 40449, 40891,
	41337, 41785, 42236, 42690, 43147, 43606, 44069, 44534,
	45002, 45473, 45947, 46423, 46903, 47385, 47871, 48359,
	48850, 49344, 49841, 50341, 50844, 51349, 51858, 52369,
	52884, 53401, 53921, 54445, 54971, 55500, 56032, 56567,
	57105, 57646, 58190, 58737, 59287, 59840, 60396, 60955,
	61517, 62082, 62650, 63221, 63795, 64372, 64952, 65535
};

/*
 * WCAG relative luminance, x65535.
 *
 * Integer throughout: the weights are 2126/7152/722 per ten thousand, which is
 * the same 0.2126/0.7152/0.0722 the specification gives and avoids a double on
 * a path the tone solve runs in a loop.
 */
uint32_t kcol_lum(uint32_t rgb)
{
	uint32_t r = srgb_lin[(rgb >> 16) & 0xff];
	uint32_t g = srgb_lin[(rgb >> 8) & 0xff];
	uint32_t b = srgb_lin[rgb & 0xff];

	return (2126u * r + 7152u * g + 722u * b) / 10000u;
}

/*
 * The contrast ratio between two colours, x100 — 4.83:1 comes back as 483.
 *
 * Scaled rather than a double because every consumer compares it against a
 * floor (7.0 for text on a plate, 4.5 for the AA minimum) and a fixed point
 * with two decimals is finer than any of those thresholds is meaningful.
 *
 * 3277 is the specification's 0.05 offset at this scale. It is what stops two
 * near-blacks reporting an enormous ratio: without it the KDOS palette's
 * `deep` against `backdrop` would come out around 4:1 instead of the 1.00:1
 * that is the truth about them.
 */
int kcol_contrast(uint32_t a, uint32_t b)
{
	uint32_t la = kcol_lum(a) + 3277, lb = kcol_lum(b) + 3277;
	uint32_t hi = la > lb ? la : lb, lo = la > lb ? lb : la;

	return (int)((hi * 100u) / lo);
}

void kcol_sem(const KcolScheme *sc, KcolSem *out)
{
	uint32_t d = sc->deep, t = sc->text, urg = sc->urgent;

	out->on_accent = kcol_mixf(0xffffff, t, 0.35);
	out->warning_bg = kcol_mixf(sc->secondary, d, 0.25);
	out->warning_fg = d;
	out->destructive_bg = kcol_mixf(urg, d, 0.2);
	out->destructive_fg = kcol_mixf(0xffffff, urg, 0.15);
	out->headerbar_border = t;
	out->header = kcol_mixf(d, t, 0.07);
	out->side_backdrop = kcol_mixf(d, t, 0.04);
	out->dialog = kcol_mixf(d, t, 0.11);
	out->thumb = kcol_mixf(d, t, 0.13);
	out->border = kcol_mixf(sc->variant, t, 0.22);
	out->border_unfocused = kcol_mixf(sc->variant, t, 0.14);
}

double kcol_luma(uint32_t rgb)
{
	double r = (double)((rgb >> 16) & 0xff);
	double g = (double)((rgb >> 8) & 0xff);
	double b = (double)(rgb & 0xff);
	return (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
}

/* ──────────────────────────────────────────────────────────────────────── */

int kcol_family(uint32_t rgb)
{
	double h, l, s;
	kcol_to_hls(rgb, &h, &l, &s);
	if (s < 0.08)
		return KCOL_FAM_NEUTRAL;
	double deg = h * 360.0;
	if (deg < 20.0 || deg >= 330.0)
		return KCOL_FAM_URGENT;
	if (deg < 70.0)
		return KCOL_FAM_SECONDARY;
	return KCOL_FAM_ACCENT;
}

uint32_t kcol_remap(const KcolScheme *sc, uint32_t rgb)
{
	double h, l, s;
	kcol_to_hls(rgb, &h, &l, &s);

	/* Pure black and pure white are structure, not colour: outlines,
	 * highlights and the whites of an eye. Tinting them is what turns a
	 * recoloured icon set into mush. */
	if (l <= 0.01 || l >= 0.99)
		return rgb;

	double dummy, nh, ns = s;
	double deg = h * 360.0;

	if (s < 0.08) {
		kcol_to_hls(sc->variant, &nh, &dummy, &dummy);
		ns = s * 1.4 + 0.05;
		if (ns > 0.18)
			ns = 0.18;
	} else if (deg < 20.0 || deg >= 330.0) {
		kcol_to_hls(sc->urgent, &nh, &dummy, &dummy);
	} else if (deg < 70.0) {
		kcol_to_hls(sc->secondary, &nh, &dummy, &dummy);
	} else if (deg < 180.0 || (deg >= 200.0 && deg < 340.0)) {
		kcol_to_hls(sc->primary, &nh, &dummy, &dummy);
	} else {
		/* 180..200 is cyan, the one band that lands on the accent at
		 * full saturation and reads as a second accent rather than a
		 * shade of it. */
		kcol_to_hls(sc->primary, &nh, &dummy, &dummy);
		ns = s * 0.75;
	}

	return kcol_from_hls(nh, l, ns);
}

uint32_t kcol_mixf(uint32_t a, uint32_t b, double t)
{
	uint32_t out = 0;
	for (int i = 16; i >= 0; i -= 8) {
		double x = (double)((a >> i) & 0xff) / 255.0;
		double y = (double)((b >> i) & 0xff) / 255.0;
		out |= (uint32_t)chan(x + (y - x) * t) << i;
	}
	return out;
}

/* ──────────────────────────────────────────────────────────────────────── */

static int is_hexd(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
	       (c >= 'A' && c <= 'F');
}

static int is_wordc(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z') || c == '_';
}

char *kcol_retint_text(const char *in, size_t len, const KcolScheme *sc,
		       size_t *outlen)
{
	/* `#39f` comes out as `#3399ff`, so a token can GROW by three. Doubling
	 * is the cheap safe bound. */
	char *out = kb_calloc(1, len * 2 + 2);
	size_t o = 0;

	for (size_t i = 0; i < len; i++) {
		if (in[i] != '#') {
			out[o++] = in[i];
			continue;
		}

		size_t run = 0;
		while (i + 1 + run < len && is_hexd(in[i + 1 + run]))
			run++;

		int take = 0;
		if (run >= 6 && (i + 7 >= len || !is_wordc(in[i + 7])))
			take = 6;
		else if (run >= 3 && (i + 4 >= len || !is_wordc(in[i + 4])))
			take = 3;

		if (!take) {
			out[o++] = in[i];
			continue;
		}

		char tok[8];
		memcpy(tok, in + i + 1, (size_t)take);
		tok[take] = 0;

		uint32_t v;
		if (kcol_parse(tok, &v) < 0) {
			out[o++] = in[i];
			continue;
		}

		/* A colour kcol_remap DECLINES keeps its original spelling,
		 * case and digit count, because that is what returning the
		 * whole regex match does. A colour it remaps comes back as six
		 * lowercase digits even if the result is the same value — so
		 * this cannot be a `nv == v` test. */
		double h, l, s;
		kcol_to_hls(v, &h, &l, &s);
		if (l <= 0.01 || l >= 0.99) {
			out[o++] = '#';
			memcpy(out + o, in + i + 1, (size_t)take);
			o += (size_t)take;
			i += (size_t)take;
			continue;
		}

		char hex[7];
		kcol_format(kcol_remap(sc, v), hex);
		out[o++] = '#';
		memcpy(out + o, hex, 6);
		o += 6;
		i += (size_t)take;
	}

	out[o] = 0;
	if (outlen)
		*outlen = o;
	return out;
}

/*
 * The accent in force, from the one word `kdos theme` writes to
 * $XDG_CACHE_HOME/kdos/theme.
 *
 * READING IS SHARED, APPLYING IS NOT. Every front end resolves the same two
 * paths in the same order and every one of them got the same fallback wrong at
 * least once; what it then does with the name differs — a cell surface calls
 * ktui_theme_set, the compositor rebuilds its own tables — so only the read is
 * here. No colours are read: the palette is compiled in, and this file names
 * which of its schemes is in force.
 *
 * Returns 0 and leaves `out` empty when there is no state file, which means
 * the default scheme and is not an error.
 */
int kcol_theme_name(char *out, size_t cap)
{
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	char path[512];
	FILE *f;
	size_t n;

	if (!out || cap == 0)
		return 0;
	out[0] = '\0';

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return 0;

	f = fopen(path, "re");
	if (!f)
		return 0;
	if (!fgets(out, (int)cap, f)) {
		fclose(f);
		out[0] = '\0';
		return 0;
	}
	fclose(f);

	/* One word: the file is a name and a newline, and a name with a
	 * newline in it matches no scheme. */
	n = strcspn(out, "\r\n \t");
	out[n] = '\0';
	return out[0] != '\0';
}

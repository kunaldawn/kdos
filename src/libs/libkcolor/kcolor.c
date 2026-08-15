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

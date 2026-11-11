/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the shades KColorScheme derives, and how colourful they are
 *
 * A scheme file carries no Light, Midlight, Mid, Dark or Shadow: KColorScheme
 * derives all five from the Window background when it builds an application
 * palette (KColorScheme::shade, over KColorUtils::shade and the KHCY colour
 * space of KGuiAddons), and LibreOffice paints its workspace in Mid. This is
 * that derivation, step for step, so the generator can measure what Qt will
 * paint instead of what the file says.
 *
 * KHCY's chroma of a colour with one channel at zero is 1 at ANY luma, and
 * below a luma of 0.006 KColorScheme lifts every shade to a fixed luma while
 * keeping the chroma. A near-black ground with a zero channel — most of the
 * palette's `deep` row — therefore yields five shades as saturated as the
 * hue allows, mid-grey bright. kth_chroma is how far that has gone.
 *
 * Float arithmetic and libm, unlike libkcolor: KHCY is defined on a 2.2
 * power curve and CIELAB on a cube root, and a table would be a second copy
 * of a curve that is not ours.
 * ---------------------------------
 */

#include <math.h>

#include "kdos-theme.h"

const char *const kth_shade_key[KTH_NSHADE] = {
	"ShadeLight",
	"ShadeMidlight",
	"ShadeMid",
	"ShadeDark",
	"ShadeShadow",
};

/* Rec. 709, KHCY's HCY_REC. */
static const double YC[3] = { 0.2126, 0.7152, 0.0722 };

typedef struct {
	double h, c, y;
} Hcy;

static double norm(double a)
{
	return a < 1.0 ? (a > 0.0 ? a : 0.0) : 1.0;
}

static double gam(double n)
{
	return pow(norm(n), 2.2);
}

static double igam(double n)
{
	return pow(norm(n), 1.0 / 2.2);
}

/* A QColor holds 16 bits a channel; what red() returns is that rounded the
 * way qt_div_257 rounds. Both steps are kept, so a shade that lands on a
 * half-way value lands where Qt puts it. */
static unsigned q16(double f)
{
	return (unsigned)lround(norm(f) * 65535.0);
}

static unsigned q8(unsigned v16)
{
	return (v16 + 0x80u - ((v16 + 0x80u) >> 8)) >> 8;
}

static Hcy from_rgbf(double rf, double gf, double bf)
{
	double r = gam(rf), g = gam(gf), b = gam(bf);
	Hcy o;

	o.y = r * YC[0] + g * YC[1] + b * YC[2];

	double p = fmax(fmax(r, g), b), n = fmin(fmin(r, g), b);
	double d = 6.0 * (p - n);

	if (n == p)
		o.h = 0.0;
	else if (r == p)
		o.h = (g - b) / d;
	else if (g == p)
		o.h = (b - r) / d + 1.0 / 3.0;
	else
		o.h = (r - g) / d + 2.0 / 3.0;

	if (r == g && g == b)
		o.c = 0.0;
	else
		o.c = fmax((o.y - n) / o.y, (p - o.y) / (1.0 - o.y));
	return o;
}

static Hcy from_rgb(uint32_t c)
{
	KcolRgb v = kcol_rgb(c);

	return from_rgbf(v.r / 255.0, v.g / 255.0, v.b / 255.0);
}

/* KHCY::qColor, as the three 16-bit channels a QColor stores. */
static void to_rgb16(Hcy k, unsigned out[3])
{
	double h = fmod(k.h, 1.0);

	h = h < 0.0 ? 1.0 + h : (h > 0.0 ? h : 0.0);

	double c = norm(k.c), y = norm(k.y), hs = h * 6.0, th, tm;

	if (hs < 1.0) {
		th = hs;
		tm = YC[0] + YC[1] * th;
	} else if (hs < 2.0) {
		th = 2.0 - hs;
		tm = YC[1] + YC[0] * th;
	} else if (hs < 3.0) {
		th = hs - 2.0;
		tm = YC[1] + YC[2] * th;
	} else if (hs < 4.0) {
		th = 4.0 - hs;
		tm = YC[2] + YC[1] * th;
	} else if (hs < 5.0) {
		th = hs - 4.0;
		tm = YC[2] + YC[0] * th;
	} else {
		th = 6.0 - hs;
		tm = YC[0] + YC[2] * th;
	}

	double tp, to, tn;

	if (tm >= y) {
		tp = y + y * c * (1.0 - tm) / tm;
		to = y + y * c * (th - tm) / tm;
		tn = y - y * c;
	} else {
		tp = y + (1.0 - y) * c;
		to = y + (1.0 - y) * c * (th - tm) / (1.0 - tm);
		tn = y - (1.0 - y) * c * tm / (1.0 - tm);
	}

	double r, g, b;

	if (hs < 1.0) {
		r = tp; g = to; b = tn;
	} else if (hs < 2.0) {
		r = to; g = tp; b = tn;
	} else if (hs < 3.0) {
		r = tn; g = tp; b = to;
	} else if (hs < 4.0) {
		r = tn; g = to; b = tp;
	} else if (hs < 5.0) {
		r = to; g = tn; b = tp;
	} else {
		r = tp; g = tn; b = to;
	}
	out[0] = q16(igam(r));
	out[1] = q16(igam(g));
	out[2] = q16(igam(b));
}

static uint32_t pack(const unsigned v16[3])
{
	return q8(v16[0]) << 16 | q8(v16[1]) << 8 | q8(v16[2]);
}

/* KColorUtils::shade with no chroma adjustment, from a 16-bit colour. */
static void shade16(const unsigned in[3], double ky, unsigned out[3])
{
	Hcy k = from_rgbf(in[0] / 65535.0, in[1] / 65535.0, in[2] / 65535.0);

	k.y = norm(k.y + ky);
	to_rgb16(k, out);
}

uint32_t kth_shade(uint32_t c, int role, int contrast)
{
	KcolRgb v = kcol_rgb(c);
	unsigned in[3] = { v.r * 257u, v.g * 257u, v.b * 257u }, o[3];
	double k = contrast / 10.0;
	double y = from_rgb(c).y, yi = 1.0 - y, ky;

	k = k < 1.0 ? (k > -1.0 ? k : -1.0) : 1.0;

	if (y < 0.006) {
		switch (role) {
		case KTH_SHADE_LIGHT:
			ky = 0.05 + 0.95 * k;
			break;
		case KTH_SHADE_MID:
			ky = 0.01 + 0.20 * k;
			break;
		case KTH_SHADE_DARK:
			ky = 0.02 + 0.40 * k;
			break;
		default:
			ky = 0.03 + 0.60 * k;
			break;
		}
		shade16(in, ky, o);
		return pack(o);
	}
	if (y > 0.93) {
		switch (role) {
		case KTH_SHADE_MIDLIGHT:
			ky = -0.02 - 0.20 * k;
			break;
		case KTH_SHADE_DARK:
			ky = -0.06 - 0.60 * k;
			break;
		case KTH_SHADE_SHADOW:
			ky = -0.10 - 0.90 * k;
			break;
		default:
			ky = -0.04 - 0.40 * k;
			break;
		}
		shade16(in, ky, o);
		return pack(o);
	}

	double light = (0.05 + y * 0.55) * (0.25 + k * 0.75);
	double dark = -y * (0.55 + k * 0.35);

	switch (role) {
	case KTH_SHADE_LIGHT:
		shade16(in, light, o);
		break;
	case KTH_SHADE_MIDLIGHT:
		shade16(in, (0.15 + 0.35 * yi) * light, o);
		break;
	case KTH_SHADE_MID:
		shade16(in, (0.35 + 0.15 * y) * dark, o);
		break;
	case KTH_SHADE_DARK:
		shade16(in, dark, o);
		break;
	default: {
		/* KColorUtils::darken over the dark shade, chroma kept. */
		unsigned d[3];

		shade16(in, dark, d);
		Hcy h = from_rgbf(d[0] / 65535.0, d[1] / 65535.0, d[2] / 65535.0);

		h.y = norm(h.y * (1.0 - (0.5 + 0.3 * y)));
		to_rgb16(h, o);
		break;
	}
	}
	return pack(o);
}

/* sRGB to CIE L*a*b* under D65, and the chroma sqrt(a² + b²): the measure of
 * how colourful a colour looks that holds at every lightness, which KHCY's
 * own chroma does not. */
static double lin(unsigned v)
{
	double x = v / 255.0;

	return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

static double labf(double t)
{
	return t > 216.0 / 24389.0 ? cbrt(t) : (24389.0 / 27.0 * t + 16.0) / 116.0;
}

int kth_chroma(uint32_t c)
{
	KcolRgb v = kcol_rgb(c);
	double r = lin(v.r), g = lin(v.g), b = lin(v.b);
	double x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047;
	double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
	double z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
	double fy = labf(y);
	double a = 500.0 * (labf(x) - fy), bb = 200.0 * (fy - labf(z));

	return (int)lround(sqrt(a * a + bb * bb) * 100.0);
}

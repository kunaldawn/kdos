/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — the distro palette, projected onto eight VT slots
 * ---------------------------------
 */

#include <string.h>

#include "kcolor.h"
#include "ktui.h"

/* The numbers live in libkcolor and nowhere else. This file only decides
 * which of the nine scheme colours each of the eight slots takes — `deep` is
 * the one the terminal has no use for. */

#define RGB(x) { (uint8_t)(0x##x >> 16), (uint8_t)((0x##x >> 8) & 0xff), (uint8_t)(0x##x & 0xff) }

#define KT_SCHEME(id, lbl, tname, p, dm, sec, urg, dp, txt, var, pd, bd)      \
	{ #id, lbl, {                                                         \
		[KT_BG] = RGB(bd),      [KT_ERR] = RGB(urg),                  \
		[KT_ACCENT] = RGB(p),   [KT_WARN] = RGB(sec),                 \
		[KT_DIM] = RGB(dm),     [KT_MID] = RGB(pd),                   \
		[KT_SURFACE] = RGB(var), [KT_TEXT] = RGB(txt) } },

const KtuiTheme ktui_themes[] = { KCOL_SCHEMES(KT_SCHEME) };

#undef KT_SCHEME
#undef RGB

int ktui_ntheme = (int)(sizeof(ktui_themes) / sizeof(ktui_themes[0]));
const KtuiTheme *ktui_theme = &ktui_themes[0];

int ktui_theme_set(const char *name)
{
	for (int i = 0; i < ktui_ntheme; i++) {
		if (!strcmp(ktui_themes[i].name, name)) {
			ktui_theme = &ktui_themes[i];
			return 0;
		}
	}
	return -1;
}

/* No sqrt: comparing squares orders identically to comparing roots, and this
 * library links no maths library. */
int ktui_theme_nearest(uint32_t rgb)
{
	int best = KT_TEXT;
	long bd = -1;
	int r = (int)((rgb >> 16) & 0xff);
	int g = (int)((rgb >> 8) & 0xff);
	int b = (int)(rgb & 0xff);

	for (int i = 0; i < KT_NCOLOR; i++) {
		KRgb s = ktui_theme->slot[i];
		long dr = r - (long)s.r;
		long dg = g - (long)s.g;
		long db = b - (long)s.b;
		long d = dr * dr + dg * dg + db * db;

		if (bd < 0 || d < bd) {
			bd = d;
			best = i;
		}
	}

	return best;
}

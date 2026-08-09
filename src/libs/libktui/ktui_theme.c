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

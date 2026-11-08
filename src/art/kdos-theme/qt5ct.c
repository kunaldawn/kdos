/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the Qt 5 palettes
 *
 *   qt5ct-colors <outdir> [accent]   KDOS-<Title>.conf, per accent
 *
 * Qt 5 has no plasma-integration build, so a Qt 5 application under the
 * `kde` platform theme is painted by the qt5ct plugin answering that key,
 * from one of these files. They are the KColorScheme palette above put into
 * QPalette's roles the way KColorScheme fills a Qt 6 palette — Window from
 * Window, Base and Text from View, Highlight from Selection — so a selected
 * row in KeePassXC and one in Dolphin are the same colours.
 *
 * 21 VALUES PER GROUP, in QPalette::ColorRole order, PlaceholderText last.
 * qt5ct rejects a shorter list WHOLE — but for exactly 20, its format from
 * before PlaceholderText, which loses the placeholder colour — and the
 * application falls back to Qt's built-in light palette, which is why the
 * count is asserted at compile time.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kdos-theme.h"

enum { QTCT_ROLES = 21 };

static void list(KbBuf *b, const char *key, const uint32_t *c)
{
	kb_buf_printf(b, "%s=", key);
	for (int i = 0; i < QTCT_ROLES; i++) {
		char hex[8];

		kcol_format(c[i], hex);
		kb_buf_printf(b, "%s#ff%s", i ? ", " : "", hex);
	}
	kb_buf_str(b, "\n");
}

int kth_qt5ct(const char *outdir, const char *accent)
{
	const KcolScheme *pick[32];
	int n = kth_pick(accent, pick, 32);

	for (int i = 0; i < n; i++) {
		const KcolScheme *sc = pick[i];
		KthPalette p;

		kth_palette(sc, &p);
		const uint32_t *win = p.set[KTH_WINDOW];
		const uint32_t *view = p.set[KTH_VIEW];
		const uint32_t *btn = p.set[KTH_BUTTON];
		const uint32_t *sel = p.set[KTH_SELECTION];
		const uint32_t *tip = p.set[KTH_TOOLTIP];
		uint32_t b = btn[KTH_BG];

		/* Light, Midlight, Mid, Dark and Shadow are the bevels Fusion
		 * draws a frame with, so they move towards white and black from
		 * the button, never towards text and deep — which would put the
		 * dark bevel on the light side on `paper`. */
		const uint32_t active[] = {
			win[KTH_FG],			/* WindowText      */
			b,				/* Button          */
			kcol_mix(b, 0xffffff, 15),	/* Light           */
			kcol_mix(b, 0xffffff, 8),	/* Midlight        */
			kcol_mix(b, 0x000000, 35),	/* Dark            */
			kcol_mix(b, 0x000000, 20),	/* Mid             */
			view[KTH_FG],			/* Text            */
			view[KTH_FG_ACTIVE],		/* BrightText      */
			btn[KTH_FG],			/* ButtonText      */
			view[KTH_BG],			/* Base            */
			win[KTH_BG],			/* Window          */
			0x000000,			/* Shadow          */
			sel[KTH_BG],			/* Highlight       */
			sel[KTH_FG],			/* HighlightedText */
			view[KTH_FG_LINK],		/* Link            */
			view[KTH_FG_VISITED],		/* LinkVisited     */
			view[KTH_BG_ALT],		/* AlternateBase   */
			win[KTH_BG],			/* NoRole          */
			tip[KTH_BG],			/* ToolTipBase     */
			tip[KTH_FG],			/* ToolTipText     */
			view[KTH_FG_INACTIVE],		/* PlaceholderText */
		};
		_Static_assert(sizeof(active) / sizeof(active[0]) == QTCT_ROLES,
			       "qt5ct reads exactly QPalette's 21 roles");

		/* Disabled: every text role takes the set's inactive tone. */
		uint32_t disabled[QTCT_ROLES];

		memcpy(disabled, active, sizeof(disabled));
		disabled[0] = win[KTH_FG_INACTIVE];
		disabled[6] = view[KTH_FG_INACTIVE];
		disabled[8] = btn[KTH_FG_INACTIVE];
		disabled[13] = sel[KTH_FG_INACTIVE];

		KbBuf out = {0};
		char file[80];

		kb_buf_str(&out, "[ColorScheme]\n");
		list(&out, "active_colors", active);
		list(&out, "disabled_colors", disabled);
		list(&out, "inactive_colors", active);

		snprintf(file, sizeof(file), "%s.conf", sc->theme_name);
		kth_emit(outdir, file, &out);
		kb_buf_free(&out);
	}
	return 0;
}

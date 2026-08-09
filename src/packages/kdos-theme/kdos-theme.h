/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the generators, in C
 *
 * These were gengtk.py, genicons.py and gencursors.py. They ran at build time
 * AND at runtime from `kdos theme`, which is what made python3 a dependency of
 * the shipped system for exactly two files.
 *
 * Every one of them is the same shape: read vendored artwork, rewrite the
 * colours through libkcolor, write a theme. None of them parses SVG, CSS or
 * PNG — the icon and stylesheet passes are hex-token substitution over text,
 * and the cursor pass is a binary Xcursor rewrite.
 * ---------------------------------
 */

#ifndef KDOS_THEME_H
#define KDOS_THEME_H

#include "kbase.h"
#include "kcolor.h"

/* Where the vendored artwork lives once installed. Each generator takes an
 * explicit source directory so the kpkgbuild can point at $PORT_SRC and the
 * runtime can point at /usr/share/kdos. */
#define GTK_SRC_DEFAULT     "/usr/share/kdos/gtk-theme/theme"
#define ICON_ART_DEFAULT    "/usr/share/kdos/icons/art"
#define ICON_MARKS_DEFAULT  "/usr/share/kdos/icons/marks"
#define CURSOR_ART_DEFAULT  "/usr/share/kdos/cursors/art"

int gen_gtk(const char *src, const char *out, const KcolScheme *sc);
int gen_icons(const char *art, const char *marks, const char *out,
	      const KcolScheme *sc);
int gen_cursors(const char *art, const char *out, const KcolScheme *sc);

#endif /* KDOS_THEME_H */

#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# EVERY ONE OF THESE IS A `feature`, so the value is enabled/disabled/auto and
# never true/false — meson refuses a boolean here rather than coercing it. Every
# loader is named rather than left at `auto`, because auto answers a missing
# dependency by disabling the feature: a sixel encoder that cannot read a JPEG
# or a PNG is a library with no input. gdk-pixbuf2 and gd widen what img2sixel
# and every linking program can open — TIFF, WebP, GIF, BMP and the rest of the
# pixbuf loader set, and gd's own formats — at the cost of glib in this
# library's closure.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Djpeg=enabled -Dpng=enabled -Dgdk-pixbuf2=enabled -Dgd=enabled \
	-Dlibcurl=disabled -Dpython=disabled -Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
install -Dm644 src/sixel.5 -t "$PKG/usr/share/man/man5"

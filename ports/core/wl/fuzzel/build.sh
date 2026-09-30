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

# SVG icons go through librsvg, which needs cairo: the bundled nanosvg draws
# no text, filters or masks. Both are named rather than left on auto, which
# would build without them and say nothing.
# wrap-mode=nodownload: fcft and tllist are ports, and a fallback would fetch.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release --wrap-mode=nodownload \
	-Denable-cairo=enabled \
	-Dpng-backend=libpng \
	-Dsvg-backend=librsvg \
	-Dsystem-nanosvg=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

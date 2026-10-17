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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Djpeg=enabled -Dpng=enabled -Dgdk-pixbuf2=enabled -Dgd=enabled \
	-Dlibcurl=disabled -Dpython=disabled -Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
install -Dm644 src/sixel.5 -t "$PKG/usr/share/man/man5"

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

# man_html renders the page again as HTML with mandoc when it is on the path;
# the package carries the manual page and no HTML copy.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Ddocs=disabled -Dtests=disabled \
	-Dtools=enabled -Dman=enabled -Dman_html=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# The GTK 4 series, installed under the gtksourceview-5 API name. Font
# fallback for the space-drawing glyphs goes through fontconfig and pangoft2;
# without them it is compiled out and nothing says so.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dintrospection=enabled \
	-Dvapi=true \
	-Ddocumentation=false \
	-Dsysprof=false \
	-Dbuild-testsuite=false \
	-Dinstall-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

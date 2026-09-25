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

# introspection stays off: pango's GIR includes HarfBuzz-0.0.gir, and harfbuzz
# is built with introspection disabled. libthai is where a line of Thai may
# break: the script has no spaces between words, and without it a paragraph
# wraps in the middle of one.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=disabled \
	-Dxft=disabled \
	-Dlibthai=enabled \
	-Dcairo=enabled \
	-Dfontconfig=enabled \
	-Dfreetype=enabled \
	-Dsysprof=disabled \
	-Dbuild-testsuite=false \
	-Dbuild-examples=false \
	-Dman-pages=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# introspection is on: Gtk-3.0.gir and Gtk-4.0.gir include Pango-1.0.gir, and
# PyGObject programs load Pango and PangoCairo through it. Pango-1.0.gir
# includes HarfBuzz-0.0.gir, so harfbuzz must be built with introspection as
# well. xft builds pangoxft, without which FLTK's hybrid Wayland/X11 build
# stops at configure. libthai is where a line of Thai may break: the
# script has no spaces between words, and without it a paragraph wraps in the
# middle of one.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=enabled \
	-Dxft=enabled \
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

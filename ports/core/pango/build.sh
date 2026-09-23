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

# introspection stays off: pango's GIR includes the glib GIRs, and glib is
# built with introspection disabled. libthai is not a port.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=disabled \
	-Dxft=disabled \
	-Dlibthai=disabled \
	-Dcairo=enabled \
	-Dfontconfig=enabled \
	-Dfreetype=enabled \
	-Dsysprof=disabled \
	-Dbuild-testsuite=false \
	-Dbuild-examples=false \
	-Dman-pages=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

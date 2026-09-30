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

export XML_CATALOG_FILES=/etc/xml/catalog

# Every input method GTK 3 carries is compiled into libgtk: a loadable module
# is found only through immodules.cache, which no install step regenerates, so
# a module left outside would never load and text-input-v3 would be dead.
# The X11 backend is what GTK 3 applications under Xwayland use, and the only
# backend that starts the ATK bridge, so without it no GTK 3 window is visible
# to a screen reader.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dwayland_backend=true \
	-Dx11_backend=true \
	-Dbroadway_backend=false \
	-Dxinerama=yes \
	-Dcloudproviders=false \
	-Dprofiler=false \
	-Dtracker3=false \
	-Dprint_backends=file,cups \
	-Dcolord=no \
	-Dintrospection=true \
	-Dbuiltin_immodules=all \
	-Dgtk_doc=false \
	-Dman=true \
	-Ddemos=false \
	-Dexamples=false \
	-Dtests=false \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

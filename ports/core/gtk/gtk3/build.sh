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

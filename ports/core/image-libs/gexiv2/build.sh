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

# The 0.14 API is the one GIMP 3.2 accepts: its meson.build takes gexiv2 below
# 0.15, so a 0.16 build here would leave GIMP unconfigurable. python3 installs
# the GExiv2 override into PyGObject's overrides directory, which GIMP's
# Python plug-ins import.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dintrospection=true \
	-Dvapi=true \
	-Dpython3=true \
	-Dtools=true \
	-Dgtk_doc=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

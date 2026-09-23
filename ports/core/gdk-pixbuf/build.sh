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

# The in-process loaders are named rather than probed, and --wrap-mode keeps a
# missing libpng or libjpeg from being answered by the wraps the tarball ships.
#
# introspection=disabled: the typelib is generated against glib's own GIRs,
# and the glib port is built without them. glycin is not a port.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--wrap-mode=nodownload \
	-D png=enabled \
	-D jpeg=enabled \
	-D tiff=enabled \
	-D gif=enabled \
	-D glycin=disabled \
	-D introspection=disabled \
	-D documentation=false \
	-D man=true \
	-D tests=false \
	-D installed_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

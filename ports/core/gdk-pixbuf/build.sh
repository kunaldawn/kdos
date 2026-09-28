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
# others defaults to disabled and carries the BMP, ICO, ANI, PNM, TGA, XPM,
# XBM, ICNS and QTIF loaders; without it no GTK program opens those formats.
#
# introspection is on: GdkPixbuf-2.0.gir is included by GTK's, libnotify's and
# librsvg's GIRs. It is generated against the GLib GIRs that
# glib-introspection installs. glycin is not a port.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--wrap-mode=nodownload \
	-D png=enabled \
	-D jpeg=enabled \
	-D tiff=enabled \
	-D gif=enabled \
	-D others=enabled \
	-D glycin=disabled \
	-D introspection=enabled \
	-D documentation=false \
	-D man=true \
	-D tests=false \
	-D installed_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

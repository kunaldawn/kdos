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

# Built with meson directly rather than through pip, so pygobject-3.0.pc and
# the pygobject headers are installed for the C programs that embed it.
#
# nofallback makes a missing system glib or libffi a setup error; the wraps
# would otherwise try to clone them. pythoncapi-compat is carried inside the
# tarball and is used as it is.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback \
	-Dpython=python3 \
	-Dpycairo=enabled \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gi -maxdepth 1 -name '_gi.*.so')"
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/gi -maxdepth 1 -name '_gi_cairo.*.so')"

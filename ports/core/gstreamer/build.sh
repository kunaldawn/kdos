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

# libdw only adds source lines to a libunwind backtrace, so it is off with
# libunwind: the libunwind port is LLVM's, which installs no libunwind.pc for
# meson to find, and libdw alone would be linked and never called.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dintrospection=disabled \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dptp-helper=disabled \
	-Dlibdw=disabled \
	-Dlibunwind=disabled \
	-Dbash-completion=enabled \
	-Dnls=disabled \
	-Dgst_debug=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# The test suite asks for gtest with a wrap fallback, and gtest is not a port:
# --wrap-mode=nofallback keeps meson from reaching for the network to fetch it,
# so the tests are simply not built.
meson setup build \
	--prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback \
	-Dwerror=false -Ddoc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

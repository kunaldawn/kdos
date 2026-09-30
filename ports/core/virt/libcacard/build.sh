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

# -Dpcsc=enabled passes a physical reader through pcsc-lite; without it only
# the NSS-backed software card exists.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpcsc=enabled \
	-Ddisable_tests=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

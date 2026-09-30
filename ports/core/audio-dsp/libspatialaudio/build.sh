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

# The MIT KEMAR set is compiled in as the binaural HRTF, so headphone
# rendering needs no data file; SOFA HRTF files load through libmysofa. Both
# switches are named, so neither follows what the build root holds.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dlibmysofa=enabled \
	-Dmit_hrtf=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

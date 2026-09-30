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

# build_lib: the parser is compiled once into libtomlplusplus rather than into
# every consumer, and the pkg-config file then names the library.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dbuild_lib=true \
	-Dbuild_examples=false \
	-Dbuild_tests=false \
	-Dbuild_tt=false \
	-Dgenerate_cmake_config=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

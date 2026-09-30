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

# HarfBuzz shaping is on, so a script that needs shaping (Arabic, Devanagari)
# is laid out by the system HarfBuzz instead of drawn glyph by glyph; both it
# and FreeType are the ports', never the copies in the source's external/.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DSDL2TTF_VENDORED=OFF \
	-DSDL2TTF_HARFBUZZ=ON \
	-DSDL2TTF_SAMPLES=OFF
ninja
DESTDIR=$PKG ninja install

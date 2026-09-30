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
#
# PlutoSVG renders OT-SVG glyphs, so a colour emoji font in that format draws
# in colour instead of as its monochrome fallback outlines. It is the port's,
# found through its CMake config: SDLTTF_VENDORED=OFF also keeps the copy in
# external/plutosvg out. STRICT keeps every switch that is on a hard
# requirement.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DSDLTTF_VENDORED=OFF \
	-DSDLTTF_STRICT=ON \
	-DSDLTTF_HARFBUZZ=ON \
	-DSDLTTF_PLUTOSVG=ON \
	-DSDLTTF_SAMPLES=OFF \
	-DSDLTTF_INSTALL_MAN=ON
ninja
DESTDIR=$PKG ninja install

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

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DSDLTTF_VENDORED=OFF \
	-DSDLTTF_STRICT=ON \
	-DSDLTTF_HARFBUZZ=ON \
	-DSDLTTF_PLUTOSVG=ON \
	-DSDLTTF_SAMPLES=OFF \
	-DSDLTTF_INSTALL_MAN=ON
ninja
DESTDIR=$PKG ninja install

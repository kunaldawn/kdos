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
	-DSDL2IMAGE_VENDORED=OFF \
	-DSDL2IMAGE_STRICT=ON \
	-DSDL2IMAGE_DEPS_SHARED=OFF \
	-DSDL2IMAGE_BACKEND_STB=OFF \
	-DSDL2IMAGE_SAMPLES=OFF \
	-DSDL2IMAGE_TESTS=OFF \
	-DSDL2IMAGE_AVIF=ON \
	-DSDL2IMAGE_JPG=ON \
	-DSDL2IMAGE_JXL=ON \
	-DSDL2IMAGE_PNG=ON \
	-DSDL2IMAGE_TIF=ON \
	-DSDL2IMAGE_WEBP=ON \
	-DSDL2IMAGE_SVG=ON
ninja
DESTDIR=$PKG ninja install

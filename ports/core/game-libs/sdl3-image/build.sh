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

# EVERY FORMAT LINKS ITS SYSTEM LIBRARY, NOT stb OR A dlopen. With the stb
# backend on, JPEG goes through a private decoder no libjpeg-turbo update
# reaches; with DEPS_SHARED on, a missing codec library is a format that
# silently stops loading at run time instead of a link the solver can see.
# STRICT makes a codec whose library is absent fail the configure.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DSDLIMAGE_VENDORED=OFF \
	-DSDLIMAGE_STRICT=ON \
	-DSDLIMAGE_DEPS_SHARED=OFF \
	-DSDLIMAGE_BACKEND_STB=OFF \
	-DSDLIMAGE_SAMPLES=OFF \
	-DSDLIMAGE_TESTS=OFF \
	-DSDLIMAGE_INSTALL_MAN=ON \
	-DSDLIMAGE_AVIF=ON \
	-DSDLIMAGE_JPG=ON \
	-DSDLIMAGE_JXL=ON \
	-DSDLIMAGE_PNG=ON \
	-DSDLIMAGE_PNG_LIBPNG=ON \
	-DSDLIMAGE_TIF=ON \
	-DSDLIMAGE_WEBP=ON \
	-DSDLIMAGE_SVG=ON
ninja
DESTDIR=$PKG ninja install

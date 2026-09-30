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

# SDL2 HERE IS sdl2-compat, whose SDL2Config.cmake is what find_package(SDL2)
# reaches; the library links libSDL2-2.0.so.0 and inherits SDL3's drivers.
#
# EVERY FORMAT LINKS ITS SYSTEM LIBRARY, NOT stb OR A dlopen. With the stb
# backend on, JPEG and PNG go through a private decoder no libjpeg-turbo or
# libpng update reaches; with DEPS_SHARED on, a missing codec library is a
# format that silently stops loading at run time instead of a link the solver
# can see. STRICT makes a codec whose library is absent fail the configure.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
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

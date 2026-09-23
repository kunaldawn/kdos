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

# THIS IS THE SDL2 EVERY SDL2 CONSUMER HERE LINKS: libSDL2-2.0.so.0, the SDL2
# headers, SDL2Config.cmake for `find_package(SDL2)`, and sdl2-compat.pc, whose
# `Provides: sdl2` is how pkgconf answers a lookup for `sdl2`.
# It carries no drivers of its own — it dlopens libSDL3.so.0 at SDL_Init, so a
# program built against it gets exactly the backends the sdl3 port chose.
#
# SDL2COMPAT_X11 IS OFF: on, it is `find_package(X11 REQUIRED)` for the X11
# window hooks, and this image carries no X client libraries.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSDL2COMPAT_TESTS=OFF \
	-DSDL2COMPAT_STATIC=OFF \
	-DSDL2COMPAT_X11=OFF
ninja
DESTDIR=$PKG ninja install

# THE CONFIG SCRIPT GOES AND THE pkg-config FILE STAYS. `sdl2-config` hardcodes
# the prefix it was built with and is what a consumer reaches for when
# pkg-config cannot answer — so leaving it means a build that finds the wrong
# flags silently rather than failing at the point the answer was missing.
rm -f "$PKG/usr/bin/sdl2-config"

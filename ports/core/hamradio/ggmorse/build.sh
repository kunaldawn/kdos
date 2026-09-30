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

# The cmake4 branch is upstream's library with a CMake floor the tree's
# CMake accepts. The examples and their SDL2 and ImGui front end need
# submodules a forge archive carries empty; only the library is built.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DGGMORSE_SUPPORT_SDL2=OFF \
	-DGGMORSE_BUILD_TESTS=OFF \
	-DGGMORSE_BUILD_EXAMPLES=OFF \
	-DGGMORSE_SANITIZE_THREAD=OFF \
	-DGGMORSE_SANITIZE_ADDRESS=OFF \
	-DGGMORSE_SANITIZE_UNDEFINED=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libggmorse.so"
test -f "$PKG/usr/include/ggmorse/ggmorse.h"

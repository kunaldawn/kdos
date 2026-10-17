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

export CXXFLAGS="$CXXFLAGS -I/usr/include/minizip"
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DASSIMP_BUILD_ZLIB=OFF \
	-DASSIMP_BUILD_TESTS=OFF \
	-DASSIMP_BUILD_SAMPLES=OFF \
	-DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
	-DASSIMP_BUILD_DOCS=OFF \
	-DASSIMP_BUILD_DRACO=OFF \
	-DASSIMP_BUILD_USE_CCACHE=OFF \
	-DASSIMP_WARNINGS_AS_ERRORS=OFF \
	-DASSIMP_INJECT_DEBUG_POSTFIX=OFF \
	-DASSIMP_IGNORE_GIT_HASH=ON \
	-DASSIMP_HUNTER_ENABLED=OFF
ninja
DESTDIR=$PKG ninja install

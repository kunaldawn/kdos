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

OPTS="-G Ninja \
  -S . \
  -D CMAKE_INSTALL_PREFIX=/usr \
  -D CMAKE_INSTALL_LIBDIR=lib \
  -D CMAKE_BUILD_TYPE=Release \
  -D SPIRV_WERROR=OFF \
  -D SPIRV_SKIP_TESTS=ON \
  -D SPIRV-Headers_SOURCE_DIR=/usr \
  -D SPIRV_TOOLS_BUILD_STATIC=ON \
  -Wno-dev "


cmake -B build $OPTS \
    -D CMAKE_C_FLAGS_RELEASE="${CFLAGS}" \
    -D CMAKE_CXX_FLAGS_RELEASE="${CXXFLAGS}" \
    -D BUILD_SHARED_LIBS=ON
  cmake --build build
DESTDIR=$PKG cmake --install build

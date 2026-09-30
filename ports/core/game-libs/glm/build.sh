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

# HEADER-ONLY: GLM_BUILD_LIBRARY off installs the headers and glmConfig.cmake
# and compiles nothing. Upstream ships no pkg-config file, and a meson or
# autotools consumer that asks pkg-config for `glm` finds nothing without one,
# so the port writes it.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DGLM_BUILD_LIBRARY=OFF \
	-DGLM_BUILD_TESTS=OFF \
	-DGLM_BUILD_INSTALL=ON
ninja
DESTDIR=$PKG ninja install

install -Dm644 /dev/stdin "$PKG/usr/lib/pkgconfig/glm.pc" <<PC
prefix=/usr
includedir=\${prefix}/include

Name: GLM
Description: OpenGL Mathematics
Version: $version
Cflags: -I\${includedir}
PC

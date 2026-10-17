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

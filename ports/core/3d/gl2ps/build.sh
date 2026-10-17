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

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_ZLIB=ON \
	-DENABLE_PNG=ON \
	-DOpenGL_GL_PREFERENCE=GLVND \
	-DCMAKE_DISABLE_FIND_PACKAGE_GLUT=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LATEX=ON
cmake --build build
DESTDIR=$PKG cmake --install build
rm -f "$PKG"/usr/lib/libgl2ps.a
test -e "$PKG"/usr/lib/libgl2ps.so

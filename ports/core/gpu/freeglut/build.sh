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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFREEGLUT_BUILD_SHARED_LIBS=ON \
	-DFREEGLUT_BUILD_STATIC_LIBS=OFF \
	-DFREEGLUT_BUILD_DEMOS=OFF \
	-DFREEGLUT_WAYLAND=OFF \
	-DFREEGLUT_GLES=OFF \
	-DFREEGLUT_REPLACE_GLUT=ON \
	-DFREEGLUT_INSTALL_MAN_PAGES=ON \
	-DOpenGL_GL_PREFERENCE=GLVND
ninja -C build
DESTDIR=$PKG ninja -C build install

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

# THE LIBRARY IS GLX OR EGL, NEVER BOTH. GLEW_EGL is a compile-time switch that
# replaces glXGetProcAddress with eglGetProcAddress and glxewInit with
# eglewInit under the same soname, so one build serves one of them. This is
# the GLX build every consumer's upstream is written against. Function
# lookup goes through libglvnd's dispatch and resolves under an EGL context
# too, but glewInit() itself returns GLEW_ERROR_NO_GLX_DISPLAY when no GLX
# display is current: a program that treats that as fatal runs under
# Xwayland.
#
# The CMake build is used rather than the top-level Makefile: it installs
# into lib rather than lib64 and writes a glew.pc with the real libdir.
cd build/cmake
cmake . -G Ninja -B _build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_UTILS=ON \
	-DGLEW_X11=ON \
	-DGLEW_EGL=OFF \
	-DGLEW_OSMESA=OFF \
	-DGLEW_REGAL=OFF \
	-DOpenGL_GL_PREFERENCE=GLVND
ninja -C _build
DESTDIR=$PKG ninja -C _build install

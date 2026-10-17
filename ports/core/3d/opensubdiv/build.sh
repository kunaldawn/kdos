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

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_LIBDIR_BASE=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DNO_LIB=OFF \
	-DNO_EXAMPLES=ON \
	-DNO_TUTORIALS=ON \
	-DNO_REGRESSION=ON \
	-DNO_TESTS=ON \
	-DNO_GLTESTS=ON \
	-DNO_DOC=ON \
	-DNO_PTEX=ON \
	-DNO_OMP=ON \
	-DNO_TBB=OFF \
	-DNO_CUDA=ON \
	-DNO_OPENCL=ON \
	-DNO_CLEW=ON \
	-DNO_OPENGL=OFF \
	-DNO_METAL=ON \
	-DNO_DX=ON \
	-DNO_GLEW=ON \
	-DNO_GLFW=ON \
	-DNO_GLFW_X11=ON
ninja
DESTDIR=$PKG ninja install

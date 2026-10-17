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
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DOCIO_INSTALL_EXT_PACKAGES=NONE \
	-DOCIO_BUILD_APPS=ON \
	-DOCIO_USE_OIIO_FOR_APPS=OFF \
	-DOCIO_BUILD_OPENFX=OFF \
	-DOCIO_BUILD_NUKE=OFF \
	-DOCIO_BUILD_TESTS=OFF \
	-DOCIO_BUILD_GPU_TESTS=OFF \
	-DOCIO_USE_HEADLESS=OFF \
	-DOCIO_BUILD_DOCS=OFF \
	-DOCIO_BUILD_PYTHON=OFF \
	-DOCIO_BUILD_JAVA=OFF \
	-DOCIO_USE_SIMD=ON \
	-DOCIO_WARNING_AS_ERROR=OFF \
	-Dexpat_ROOT=/usr
ninja
DESTDIR=$PKG ninja install

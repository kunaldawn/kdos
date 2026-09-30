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

# OCIO_INSTALL_EXT_PACKAGES=NONE makes a missing dependency an error instead
# of a download: expat, yaml-cpp, pystring, Imath, zlib and minizip-ng are all
# ports. The command-line tools are built; ocioconvert, ociolutimage and
# ociodisplay read images through OpenEXR, and ociodisplay draws with GLUT and
# GLEW. The SIMD kernels are chosen at run time from the processor's flags.
# The Python bindings are off: Krita and Blender use the C++ library.
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

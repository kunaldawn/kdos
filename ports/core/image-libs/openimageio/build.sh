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

# robin-map is a header-only hash table OIIO compiles in and installs nothing
# of. It is a second source rather than a port, and ROBINMAP_INCLUDE_DIR
# points at it: missing, OIIO's CMake clones it from GitHub.
# OpenImageIO_BUILD_MISSING_DEPS= (empty) makes any other missing dependency a
# configure error instead of a download.
#
# USE_QT=OFF: no iv viewer, which would put Qt under a library everything
# image-shaped links. Ptex, OpenCV, DCMTK, libuhdr, OpenJPH, R3D and Nuke are
# not ports. INSTALL_FONTS=OFF: the bundled Droid faces duplicate fonts the
# image already has. txt2man is not a port, so no manual pages are generated.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DOIIO_BUILD_TESTS=OFF \
	-DOIIO_BUILD_TOOLS=ON \
	-DSTOP_ON_WARNING=OFF \
	-DUSE_CCACHE=OFF \
	-DINSTALL_FONTS=OFF \
	-DINSTALL_DOCS=ON \
	-DOpenImageIO_BUILD_MISSING_DEPS= \
	-DROBINMAP_INCLUDE_DIR="$SRC_ROOT/robin-map-$_robinmap/include" \
	-DUSE_EXTERNAL_PUGIXML=ON \
	-DUSE_PYTHON=ON \
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DUSE_QT=OFF \
	-DUSE_OPENGL=OFF \
	-DUSE_FFMPEG=ON \
	-DUSE_FREETYPE=ON \
	-DUSE_GIF=ON \
	-DUSE_JXL=ON \
	-DUSE_LIBHEIF=ON \
	-DUSE_LIBRAW=ON \
	-DUSE_OPENCOLORIO=ON \
	-DUSE_OPENJPEG=ON \
	-DUSE_OPENVDB=ON \
	-DUSE_TBB=ON \
	-DUSE_WEBP=ON \
	-DUSE_PTEX=OFF \
	-DUSE_OPENCV=OFF \
	-DUSE_DCMTK=OFF \
	-DUSE_LIBUHDR=OFF \
	-DUSE_OPENJPH=OFF \
	-DUSE_R3DSDK=OFF \
	-DUSE_NUKE=OFF
ninja
DESTDIR=$PKG ninja install

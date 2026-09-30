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

# The CMake package goes under lib/cmake, where find_package() looks, not
# share/cmake. The HDF5 patch replaces a check for exactly 1.12.x with a
# floor of 1.12.1 and moves the internal-model guards past 1.14; the hdf5
# port is 2.x, whose minor number is below both guards.
patch -p1 -i "$PORT_SRC/cmake-config-dir.patch"
patch -p1 -i "$PORT_SRC/hdf5-1.14.patch"

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_RPATH=ON \
	-DMEDFILE_BUILD_SHARED_LIBS=ON \
	-DMEDFILE_BUILD_STATIC_LIBS=OFF \
	-DMEDFILE_BUILD_TESTS=OFF \
	-DMEDFILE_BUILD_PYTHON=OFF \
	-DMEDFILE_BUILD_DOC=OFF \
	-DMEDFILE_INSTALL_DOC=OFF
ninja
DESTDIR=$PKG ninja install

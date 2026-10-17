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

cd "$name-$version"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DHDF5_INSTALL_DATA_DIR=share/hdf5 \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DHDF5_BUILD_CPP_LIB=ON \
	-DHDF5_BUILD_FORTRAN=ON \
	-DHDF5_BUILD_HL_LIB=ON \
	-DHDF5_BUILD_TOOLS=ON \
	-DHDF5_BUILD_EXAMPLES=OFF \
	-DHDF5_ENABLE_ZLIB_SUPPORT=ON \
	-DHDF5_ENABLE_SZIP_SUPPORT=ON
ninja
DESTDIR=$PKG ninja install

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

# Fortran is ON because gfortran is here and because the codes that write HDF5
# in the first place are overwhelmingly Fortran — a reader without it can open
# the file and not the bindings somebody needs to write one.
# SZIP is on through libaec's libsz: satellite and climate files are written
# with the szip filter, and a library without it cannot read their datasets.
# THIS TARBALL'S MEMBERS ARE PREFIXED `./`, so kpkg's --strip-components=1
# removes the DOT and the tree lands one level down at $SRC/hdf5-$version
# rather than at $SRC. cmake then reports "does not appear to contain
# CMakeLists.txt", which points at the wrong thing entirely.
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

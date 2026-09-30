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

# qrupdate-ng, the maintained CMake continuation of qrupdate 1.1: the same
# library name and Fortran ABI, so Octave's configure finds -lqrupdate. The
# API reference is off; Doxygen would otherwise be picked up whenever another
# port has put it in the build root.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBLA_VENDOR=OpenBLAS \
	-DINTEGER8=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON
cmake --build build
DESTDIR=$PKG cmake --install build

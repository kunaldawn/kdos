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
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DNLOPT_CXX=ON \
	-DNLOPT_FORTRAN=OFF \
	-DNLOPT_PYTHON=OFF \
	-DNLOPT_OCTAVE=OFF \
	-DNLOPT_MATLAB=OFF \
	-DNLOPT_GUILE=OFF \
	-DNLOPT_JAVA=OFF \
	-DNLOPT_SWIG=OFF \
	-DNLOPT_TESTS=OFF
make
make DESTDIR=$PKG install

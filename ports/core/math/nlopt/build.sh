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

# The C and C++ library only. The Python, Octave, Guile and Java bindings are
# SWIG-generated and each would find whatever interpreter happens to be
# installed, so they are off by name. The Luksan solvers stay: the library is
# LGPL either way.
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
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

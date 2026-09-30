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

# CVODE, IDA, ARKODE, KINSOL and their sensitivity variants in double
# precision with 64-bit indices. KLU (from suitesparse) is the sparse direct
# solver IDA uses under Octave's ode15i and ode15s; Octave's configure drops
# both functions without the sunlinsol_klu library. LAPACK is OpenBLAS's.
# MPI and every GPU backend are off; examples are neither built nor installed.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DSUNDIALS_PRECISION=double \
	-DSUNDIALS_INDEX_SIZE=64 \
	-DSUNDIALS_ENABLE_KLU=ON \
	-DKLU_INCLUDE_DIR=/usr/include/suitesparse \
	-DKLU_LIBRARY_DIR=/usr/lib \
	-DSUNDIALS_ENABLE_LAPACK=ON \
	-DLAPACK_LIBRARIES=/usr/lib/libopenblas.so \
	-DSUNDIALS_ENABLE_OPENMP=ON \
	-DSUNDIALS_ENABLE_PTHREAD=ON \
	-DSUNDIALS_ENABLE_MPI=OFF \
	-DSUNDIALS_ENABLE_CUDA=OFF \
	-DSUNDIALS_ENABLE_FORTRAN=OFF \
	-DSUNDIALS_ENABLE_PYTHON=OFF \
	-DSUNDIALS_ENABLE_C_EXAMPLES=OFF \
	-DSUNDIALS_ENABLE_CXX_EXAMPLES=OFF \
	-DSUNDIALS_ENABLE_EXAMPLES_INSTALL=OFF \
	-DSUNDIALS_TEST_ENABLE_GTEST=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
test -e "$PKG"/usr/lib/libsundials_sunlinsolklu.so
test -e "$PKG"/usr/lib/libsundials_ida.so

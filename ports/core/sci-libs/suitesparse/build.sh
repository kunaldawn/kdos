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

# Every project but GraphBLAS and LAGraph: Octave's sparse algebra links
# AMD, CAMD, COLAMD, CCOLAMD, CHOLMOD, CXSparse, KLU, SPQR and UMFPACK, and
# GraphBLAS is a run-time JIT compiler that nothing here links.
# BLA_VENDOR pins OpenBLAS with 32-bit integers, the ABI the openblas port
# builds and Octave's default indexing expects; a mismatched integer width
# corrupts every BLAS call rather than failing the link. SUITESPARSE_USE_STRICT
# turns a missing OpenMP or Fortran into a configure error instead of a
# slower library.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DSUITESPARSE_ENABLE_PROJECTS="suitesparse_config;mongoose;amd;btf;camd;ccolamd;colamd;cholmod;cxsparse;ldl;klu;umfpack;paru;rbio;spqr;spex" \
	-DBLA_VENDOR=OpenBLAS \
	-DSUITESPARSE_USE_64BIT_BLAS=OFF \
	-DSUITESPARSE_USE_CUDA=OFF \
	-DSUITESPARSE_USE_OPENMP=ON \
	-DSUITESPARSE_USE_FORTRAN=ON \
	-DSUITESPARSE_USE_PYTHON=OFF \
	-DSUITESPARSE_USE_STRICT=ON \
	-DSUITESPARSE_DEMOS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

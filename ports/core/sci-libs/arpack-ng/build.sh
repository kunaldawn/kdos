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

# ICB builds the ISO_C_BINDING entry points (arpack.h, *upd_c) beside the
# Fortran ones; without it only callers that mangle Fortran names by hand can
# link. LP64 integers and the OpenBLAS vendor match the openblas port's ABI.
# MPI is off: PARPACK needs an MPI stack this tree does not carry.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBLA_VENDOR=OpenBLAS \
	-DINTERFACE64=OFF \
	-DMPI=OFF \
	-DICB=ON \
	-DEIGEN=OFF \
	-DPYTHON3=OFF \
	-DEXAMPLES=OFF \
	-DTESTS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

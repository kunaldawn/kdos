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

mkdir -p build && cd build
# glpk is already here and is a decade behind on mixed-integer problems; HiGHS
# is the one an open toolchain can put against a commercial solver. The python
# and fortran interfaces are off — neither has a consumer in this tree.
# HIPO, the interior-point solver, links the system openblas; BUILD_OPENBLAS
# must stay OFF, because ON fetches OpenBLAS over the network at configure.
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DFAST_BUILD=ON \
	-DZLIB=ON -DHIPO=ON -DBUILD_OPENBLAS=OFF \
	-DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
	-DPYTHON_BUILD_SETUP=OFF -DFORTRAN=OFF
ninja
DESTDIR=$PKG ninja install
# HIPO and its BLAS live in libhighs_extras.so, which libhighs dlopens by
# name at solve time and upstream installs only when it is linked statically;
# without it `--solver hipo` reports that the extras library is missing.
install -Dm755 lib/libhighs_extras.so $PKG/usr/lib/libhighs_extras.so

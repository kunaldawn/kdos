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

# The commit is the one Blender 5.2's dependency builder pins. The 2.2.0
# release asks CMake for Eigen 3.3 exactly, which the eigen port (5.x) refuses
# as incompatible; this commit accepts 3.3.4 to 5. It logs through abseil,
# whose submodule the archive carries empty, so the system abseil-cpp is used.
#
# SuiteSparse and METIS are not ports and CUDA is not built: the sparse
# solvers are Eigen's. LAPACK=OFF keeps the dense solvers on Eigen too, so the
# library does not take whichever BLAS the chroot happens to hold.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DPROVIDE_UNINSTALL_TARGET=OFF \
	-DUSE_CUDA=OFF \
	-DSUITESPARSE=OFF \
	-DLAPACK=OFF \
	-DEIGENSPARSE=ON \
	-DEIGENMETIS=OFF \
	-DSCHUR_SPECIALIZATIONS=ON
ninja
DESTDIR=$PKG ninja install

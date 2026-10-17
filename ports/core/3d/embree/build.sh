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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DEMBREE_STATIC_LIB=OFF \
	-DEMBREE_ZIP_MODE=OFF \
	-DEMBREE_INSTALL_DEPENDENCIES=OFF \
	-DEMBREE_TUTORIALS=OFF \
	-DEMBREE_TASKING_SYSTEM=TBB \
	-DEMBREE_ISPC_SUPPORT=ON \
	-DEMBREE_ISPC_EXECUTABLE=/usr/bin/ispc \
	-DEMBREE_SYCL_SUPPORT=OFF \
	-DEMBREE_ISA_SSE2=ON \
	-DEMBREE_ISA_SSE42=ON \
	-DEMBREE_ISA_AVX=ON \
	-DEMBREE_ISA_AVX2=ON \
	-DEMBREE_ISA_AVX512=ON \
	-DEMBREE_STACK_PROTECTOR=ON \
	-DEMBREE_RAY_MASK=ON \
	-DEMBREE_BACKFACE_CULLING=OFF \
	-DEMBREE_FILTER_FUNCTION=ON \
	-DEMBREE_GEOMETRY_TRIANGLE=ON \
	-DEMBREE_GEOMETRY_QUAD=ON \
	-DEMBREE_GEOMETRY_CURVE=ON \
	-DEMBREE_GEOMETRY_SUBDIVISION=ON \
	-DEMBREE_GEOMETRY_USER=ON \
	-DEMBREE_GEOMETRY_INSTANCE=ON \
	-DEMBREE_GEOMETRY_INSTANCE_ARRAY=ON \
	-DEMBREE_GEOMETRY_GRID=ON \
	-DEMBREE_GEOMETRY_POINT=ON
cmake --build build
DESTDIR=$PKG cmake --install build

test -f "$PKG/usr/lib/libembree4.so"

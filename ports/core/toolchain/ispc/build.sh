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
	-DX86_ENABLED=ON \
	-DARM_ENABLED=OFF \
	-DRISCV_ENABLED=OFF \
	-DPPC64_ENABLED=OFF \
	-DWASM_ENABLED=OFF \
	-DXE_ENABLED=OFF \
	-DISPC_CROSS=OFF \
	-DISPC_STATIC_LINK=OFF \
	-DISPC_SLIM_BINARY=ON \
	-DISPC_LIBRARY=OFF \
	-DISPC_LIBRARY_JIT=OFF \
	-DISPC_INCLUDE_RT=ON \
	-DISPCRT_BUILD_CPU=ON \
	-DISPCRT_BUILD_GPU=OFF \
	-DISPCRT_BUILD_TASKING=ON \
	-DISPCRT_BUILD_TASK_MODEL=Threads \
	-DISPC_INCLUDE_UTILS=ON \
	-DISPC_INCLUDE_EXAMPLES=OFF \
	-DISPC_INCLUDE_DPCPP_EXAMPLES=OFF \
	-DISPC_INCLUDE_TESTS=OFF \
	-DISPC_INCLUDE_BENCHMARKS=OFF \
	-DISPC_PREPARE_PACKAGE=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

test -x "$PKG/usr/bin/ispc"

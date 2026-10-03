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

# The version is the one Blender 5.2 pins: Cycles compiles against the NanoVDB
# headers, whose API moves between minor releases.
#
# OPENVDB_USE_DELAYED_LOADING=OFF drops Boost, which only the memory-mapped
# lazy grid loader needs; Blender does not use it. CONCURRENT_MALLOC=Tbbmalloc:
# jemalloc is not a port, and "Auto" would take one if a chroot ever held it.
# The Python module needs nanobind, which is not a port. NanoVDB is installed
# as headers only; its tools convert through OpenVDB grids nothing here needs.
#
# thread/Threading.h, which is installed, reaches TBB's version macros through
# tbb/blocked_range.h, and oneTBB 2023 no longer includes version.h there:
# TBB_INTERFACE_VERSION is undefined, the pre-2021 tbb::task::self() branch is
# taken, and neither OpenVDB nor anything including the header compiles. The
# patch is upstream's later form of the include, which names tbb/version.h.
patch -p1 -i "$PORT_SRC/tbb-version.patch"

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DOPENVDB_ENABLE_RPATH=OFF \
	-DOPENVDB_BUILD_CORE=ON \
	-DOPENVDB_CORE_SHARED=ON \
	-DOPENVDB_CORE_STATIC=OFF \
	-DOPENVDB_BUILD_BINARIES=ON \
	-DOPENVDB_BUILD_PYTHON_MODULE=OFF \
	-DOPENVDB_BUILD_UNITTESTS=OFF \
	-DOPENVDB_BUILD_DOCS=OFF \
	-DOPENVDB_BUILD_AX=OFF \
	-DOPENVDB_BUILD_NANOVDB=ON \
	-DOPENVDB_USE_DELAYED_LOADING=OFF \
	-DOPENVDB_ENABLE_UNINSTALL=OFF \
	-DUSE_NANOVDB=ON \
	-DUSE_BLOSC=ON \
	-DUSE_ZLIB=ON \
	-DUSE_TBB=ON \
	-DUSE_EXR=OFF \
	-DUSE_PNG=OFF \
	-DUSE_LOG4CPLUS=OFF \
	-DUSE_CCACHE=OFF \
	-DCONCURRENT_MALLOC=Tbbmalloc \
	-DNANOVDB_BUILD_TOOLS=OFF \
	-DNANOVDB_USE_CUDA=OFF \
	-DNANOVDB_USE_OPENVDB=OFF \
	-DNANOVDB_ALLOW_FETCHCONTENT=OFF
ninja
DESTDIR=$PKG ninja install

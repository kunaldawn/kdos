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

patch -p1 -i "$PORT_SRC/tbb-version.patch"

_nanobind=$(python3 -m nanobind --cmake_dir)
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
	-DOPENVDB_BUILD_PYTHON_MODULE=ON \
	-Dnanobind_DIR="$_nanobind" \
	-DPython_EXECUTABLE=/usr/bin/python3 \
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
test -e "$PKG"/usr/lib/python3*/site-packages/openvdb*.so

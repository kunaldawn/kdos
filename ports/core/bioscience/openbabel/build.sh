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

export CXXFLAGS="$CXXFLAGS -I/usr/include/tirpc"
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED=ON \
	-DBUILD_GUI=OFF \
	-DENABLE_TESTS=OFF \
	-DOPTIMIZE_NATIVE=OFF \
	-DENABLE_OPENMP=OFF \
	-DWITH_INCHI=ON \
	-DOPENBABEL_USE_SYSTEM_INCHI=OFF \
	-DWITH_JSON=ON \
	-DOPENBABEL_USE_SYSTEM_RAPIDJSON=ON \
	-DWITH_MAEPARSER=OFF \
	-DWITH_COORDGEN=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON \
	-DPYTHON_BINDINGS=OFF \
	-DRUN_SWIG=OFF \
	-DCMAKE_REQUIRED_INCLUDES=/usr/include/tirpc \
	-DCMAKE_REQUIRED_LIBRARIES=tirpc \
	-DCMAKE_CXX_STANDARD_LIBRARIES=-ltirpc
ninja
DESTDIR=$PKG ninja install
find "$PKG/usr/lib/openbabel" -name xtcformat.so | grep -q .

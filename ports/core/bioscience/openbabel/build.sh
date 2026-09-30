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

# With Boost found, the Maestro and Coordgen formats download their parsers at
# configure time, and a rapidjson not found is downloaded the same way: Boost
# is kept out and both formats off, and the system rapidjson is required.
# InChI is the copy in the tarball; the wxWidgets GUI is not built.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
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
	-DCMAKE_DISABLE_FIND_PACKAGE_wxWidgets=ON \
	-DPYTHON_BINDINGS=OFF \
	-DRUN_SWIG=OFF
ninja
DESTDIR=$PKG ninja install

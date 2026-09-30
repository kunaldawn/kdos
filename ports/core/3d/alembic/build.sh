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

# USE_HDF5=OFF: the HDF5 backend reads only the legacy container every
# current writer has replaced with Ogawa, and it is written to the 1.8 HDF5
# API. Without it there is no abcconvert (HDF5 to Ogawa); the other tools
# (abcls, abctree, abcecho, abcdiff, abcstitcher) are built.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DALEMBIC_SHARED_LIBS=ON \
	-DALEMBIC_BUILD_LIBS=ON \
	-DUSE_HDF5=OFF \
	-DUSE_BINARIES=ON \
	-DUSE_EXAMPLES=OFF \
	-DUSE_TESTS=OFF \
	-DUSE_PYALEMBIC=OFF \
	-DUSE_ARNOLD=OFF \
	-DUSE_MAYA=OFF \
	-DUSE_PRMAN=OFF
ninja
DESTDIR=$PKG ninja install

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
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DEXIV2_ENABLE_XMP=ON \
	-DEXIV2_ENABLE_EXTERNAL_XMP=OFF \
	-DEXIV2_ENABLE_PNG=ON \
	-DEXIV2_ENABLE_BMFF=ON \
	-DEXIV2_ENABLE_BROTLI=ON \
	-DEXIV2_ENABLE_VIDEO=ON \
	-DEXIV2_ENABLE_LENSDATA=ON \
	-DEXIV2_ENABLE_INIH=ON \
	-DEXIV2_ENABLE_NLS=OFF \
	-DEXIV2_ENABLE_WEBREADY=OFF \
	-DEXIV2_ENABLE_CURL=OFF \
	-DEXIV2_BUILD_DOC=OFF \
	-DEXIV2_BUILD_SAMPLES=OFF \
	-DEXIV2_BUILD_UNIT_TESTS=OFF \
	-DEXIV2_BUILD_EXIV2_COMMAND=ON
ninja
DESTDIR=$PKG ninja install

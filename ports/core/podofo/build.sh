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

# The library alone: the tests and examples are not installed and the tools
# are marked unsupported upstream. JPEG, TIFF and PNG are looked up without
# REQUIRED and each silently drops an image codec, so they are required here.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D PODOFO_BUILD_LIB_ONLY=ON \
	-D PODOFO_BUILD_STATIC=OFF \
	-D PODOFO_WITH_FONTMANAGER=ON \
	-D PODOFO_WITH_LCMS2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LCMS2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PNG=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

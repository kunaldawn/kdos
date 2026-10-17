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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KIMAGEFORMATS_DDS=ON \
	-D KIMAGEFORMATS_HEIF=ON \
	-D KIMAGEFORMATS_JXL=ON \
	-D KIMAGEFORMATS_JP2=ON \
	-D KIMAGEFORMATS_WITH_KNOWN_CRASHES_JXR=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Archive=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_libavif=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenJPEG=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibRaw=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

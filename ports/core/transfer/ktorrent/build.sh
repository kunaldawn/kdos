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
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Plotting=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Syndication=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Core5Compat=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DNSSD=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Completion=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Multimedia=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6MultimediaWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Taglib=ON \
	-D BUILD_WITH_GEOIP=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

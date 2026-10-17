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
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DOC=ON \
	-D BUILD_KCMS=OFF \
	-D BUILD_ACTIVITIES=OFF \
	-D BUILD_FUZZERS=OFF \
	-D USE_DBUS=ON \
	-D WITH_LIBPROXY=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Samba=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_libssh=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_TIRPC=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_libappimage=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libmtp=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_IMobileDevice=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PList=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Gperf=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Taglib=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON \
	-D WITHOUT_X11=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

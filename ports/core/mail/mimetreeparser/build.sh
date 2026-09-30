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

# The widget half only: Kleopatra's message viewer links it. Qt Quick is kept
# out of the search, because the QML half serves Merkuro alone and, found,
# makes KIO and KService required as well; an installed qtdeclarative would
# otherwise change what this package contains. KF_SKIP_PO_PROCESSING leaves
# every translation out: bundled data is English only. Widgets and
# KWidgetsAddons are looked up as optional and the widget half links both,
# so both are required here, where a missing one fails at configure.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D USE_UNITY_CMAKE_SUPPORT=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Qt6Quick=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Widgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6WidgetsAddons=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

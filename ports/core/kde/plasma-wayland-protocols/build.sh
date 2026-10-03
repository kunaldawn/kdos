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

# XML and a cmake config file, nothing compiled. Here only because fcitx5's
# waylandim links a PlasmaWindowManagement client — kdos-comp does not
# implement that protocol, so the addon falls back to the wlr one it does.
# qt6-qtbase is a build dependency all the same: ECM's KDEInstallDirs asks
# Qt's qtpaths for QT_INSTALL_PREFIX and stops when there is none.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-Wno-dev
DESTDIR=$PKG cmake --install build

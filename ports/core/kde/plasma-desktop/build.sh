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

patch -p1 -i "$PORT_SRC/sandbox-badge.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_SDL2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_XdgUserDir=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_AccountsQt6=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/lib/systemd"

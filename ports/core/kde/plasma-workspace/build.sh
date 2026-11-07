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

patch -p1 -i "$PORT_SRC/locales.patch"
patch -p1 -i "$PORT_SRC/kdos-bootctl-entry.patch"
patch -p1 -i "$PORT_SRC/sandbox-badge.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D WITH_X11=ON \
	-D WITH_X11_SESSION=OFF \
	-D INSTALL_SDDM_WAYLAND_SESSION=ON \
	-D GLIBC_LOCALE_GEN=OFF \
	-D BUILD_GEOTIMEZONED=OFF \
	-D BUILD_CAMERAINDICATOR=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_AccountsService=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_AppStreamQt=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Fontconfig=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Freetype=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON \
	-Wno-dev
cmake --build build --target shell/qwayland-plasma-shell.h shell/wayland-plasma-shell-client-protocol.h
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/lib/systemd"
ln -s plasma-applications.menu "$PKG/etc/xdg/menus/applications.menu"

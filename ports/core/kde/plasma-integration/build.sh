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

# Qt 6 only: the Qt 5 platform theme links KDE Frameworks 5, which is not
# built here, so Qt 5 applications are themed through qt5ct instead.
# QT_QPA_PLATFORMTHEME=kde loads KDEPlasmaPlatformTheme6 from this port.
#
# The four font and portal packages are run-time recommendations CMake cannot
# detect; disabling their lookups keeps the summary from naming them as
# missing. The KDOS portal is xdg-desktop-portal-kdos, not the KDE one.
#
# X11 is required and linked: an application forced onto xcb under Xwayland gets
# its cursor theme and style hints through it.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_QT5=OFF \
	-D BUILD_QT6=ON \
	-D DEFAULT_UNION_STYLE=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontNotoSans=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontNotoColorEmoji=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontHack=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_XDGDesktopPortalKDE=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

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

patch -p1 -i "$PORT_SRC/app-id.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_TESTING_ONLINE=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_KEYRING=OFF \
	-D BUILD_PYTHON_SUPPORT=ON \
	-D NO_X11=OFF \
	-D DISABLE_DBUS=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Wallet=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KIO=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6CoreAddons=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6JobWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PySide6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Shiboken6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Shiboken6Tools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

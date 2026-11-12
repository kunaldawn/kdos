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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/kcm-no-systemd.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_EXAMPLES=OFF \
	-D BUILD_PLASMA_SESSION=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Systemd=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/lib/systemd"

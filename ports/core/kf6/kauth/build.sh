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
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_TESTING=OFF \
	-D KAUTH_BACKEND_NAME=PolkitQt6-1 \
	-D KAUTH_HELPER_BACKEND_NAME=DBus \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

[ -n "$(find "$PKG" -name kauth_backend_plugin.so)" ] || {
	echo 'kauth: the polkit backend was not built' >&2
	exit 1
}

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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.

# Without polkit-qt6 the polkit backend falls back to the Fake one with only a
# warning, and every privileged action then fails at run time; the backend
# plugin is checked for after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D KAUTH_BACKEND_NAME=PolkitQt6-1 \
	-D KAUTH_HELPER_BACKEND_NAME=DBus \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

[ -n "$(find "$PKG" -name kauth_backend_plugin.so)" ] || {
	echo 'kauth: the polkit backend was not built' >&2
	exit 1
}

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
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D BUILD_MAN_DOCS=ON \
	-D BUILD_HTML_DOCS=OFF \
	-D BUILD_QTHELP_DOCS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

[ -f "$PKG/usr/share/man/man7/ecm.7" ] || {
	echo 'extra-cmake-modules: ecm(7) was not built' >&2
	exit 1
}

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

# The per-application network plugin is off: its helper sniffs packets and is
# installed with CAP_NET_RAW set by setcap, a privilege grant a package does
# not make on this system. The KAuth helper is off for the same reason: it is
# a root D-Bus service for killing and renicing other users' processes, and
# nothing here needs more than the caller's own. KF_SKIP_PO_PROCESSING leaves
# the translation catalogues out: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D ENABLE_KAUTH_HELPER=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_libpcap=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Libcap=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

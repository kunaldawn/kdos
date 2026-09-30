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
# sudo is the privilege path, granted to wheel; left on su, every kdesu prompt
# would ask for root's password instead of the user's.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D KDESU_USE_SUDO_DEFAULT=ON \
	-D KDESU_USE_DOAS_DEFAULT=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

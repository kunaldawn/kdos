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

# A build tool, not a library KDevelop links: it generates the parsers of
# KDevelop's QMake manager at KDevelop's build time. flex and bison build its
# own grammar reader.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D BUILD_EXAMPLES=OFF \
	-D BUILD_COMPAT_CMAKECONFIG=ON \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

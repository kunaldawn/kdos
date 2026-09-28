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

# Both front ends are named: left blank, the list is autodetected and a
# missing Qt Quick drops the second without a word. spdlog is not a port and
# only adds debug logging, so it is not searched for.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDDockWidgets_QT6=ON \
	-D KDDockWidgets_FRONTENDS="qtwidgets;qtquick" \
	-D KDDockWidgets_STATIC=OFF \
	-D KDDockWidgets_TESTS=OFF \
	-D KDDockWidgets_EXAMPLES=OFF \
	-D KDDockWidgets_DOCS=OFF \
	-D KDDockWidgets_PYTHON_BINDINGS=OFF \
	-D KDDockWidgets_NO_SPDLOG=ON \
	-D KDDockWidgets_XLib=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

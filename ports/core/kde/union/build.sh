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
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_TOOLS=OFF \
	-D BUILD_EXAMPLES=OFF \
	-D BUILD_WITH_KDEFRAMEWORKS=ON \
	-D BUILD_PLATFORM_PLASMA=ON \
	-D BUILD_INPUT_CSS=ON \
	-D BUILD_INPUT_PLASMASVG=OFF \
	-D BUILD_OUTPUT_QTQUICK=ON \
	-D BUILD_OUTPUT_KIRIGAMI=ON \
	-D BUILD_OUTPUT_QTWIDGETS=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

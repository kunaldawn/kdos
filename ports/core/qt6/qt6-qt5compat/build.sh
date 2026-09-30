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

# QTextCodec converts through ICU, as qt6-qtbase is built with it; iconv is the
# fallback for a Qt without ICU and stays off. The Qt 5 graphical effects are
# QML and compile their shaders with qsb, so qtdeclarative and qtshadertools
# come first.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DFEATURE_textcodec=ON \
	-DFEATURE_codecs=ON \
	-DFEATURE_big_codecs=ON \
	-DFEATURE_iconv=OFF
ninja
DESTDIR=$PKG ninja install

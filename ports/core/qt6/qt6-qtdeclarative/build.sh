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

# QtQml's build generates sources with Python, and configure stops without it.
# quick_vectorimage is named so a missing Qt SVG stops configure instead of
# dropping the VectorImage item; qml_ssl likewise for qt6-qtbase's OpenSSL.
# The install paths, and the /usr/bin links for qml, qmlls and the rest, come
# from qt6-qtbase's installed build internals.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DFEATURE_qml_python=ON \
	-DFEATURE_qml_jit=ON \
	-DFEATURE_qml_network=ON \
	-DFEATURE_qml_ssl=ON \
	-DFEATURE_quick_vectorimage=ON
ninja
DESTDIR=$PKG ninja install

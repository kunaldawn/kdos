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

# The install layout (bin, lib/qt6, include/qt6, mkspecs) is inherited from
# qt6-qtbase through Qt6BuildInternals, so no INSTALL_* path is passed here: a
# path set only in this module would scatter it away from every other one.
#
# qt6-qtdeclarative is a dependency so the QML WebSocket types are built; the
# probe is optional and a missing Quick would drop them without a word.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DFEATURE_websockets_qml=ON
ninja
DESTDIR=$PKG ninja install

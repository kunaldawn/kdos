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
# Assimp is the system library: INPUT_quick3d_assimp=system fails the configure
# when the assimp port is missing instead of compiling Qt's bundled copy.
# OpenXR is off: no OpenXR runtime exists on this host to load. QuickTimeline is
# not ported, and is refused rather than probed so balsam never changes shape
# when it appears.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DINPUT_quick3d_assimp=system \
	-DINPUT_openxr=no \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt6QuickTimeline=ON
ninja
DESTDIR=$PKG ninja install

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
# GeoClue is reached over D-Bus by the geoclue2 plugin; NMEA reads a serial GPS
# through qt6-qtserialport. Gypsy is a dead GPS daemon and stays off.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DFEATURE_gypsy=OFF
ninja
DESTDIR=$PKG ninja install

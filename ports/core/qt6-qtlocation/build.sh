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
# OpenStreetMap is the only geoservice built: ESRI, Mapbox and HERE need an
# account key and are hard-wired off upstream. Offline, the OSM plugin draws
# only tiles already in its cache or in osm.mapping.offline.directory, and a
# program that does not set osm.mapping.providersrepository.disabled first asks
# maps-redirect.qt.io for its tile servers and falls back to the built-in list.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DFEATURE_geoservices_osm=ON
ninja
DESTDIR=$PKG ninja install

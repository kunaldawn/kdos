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

mkdir -p build && cd build
# ENABLE_CURL=OFF so the network path CANNOT EXIST rather than merely not being
# used: with it on, a missing grid is fetched from cdn.proj.org at RUN time,
# which on this distro is a silent dependency on somebody else's server for a
# coordinate transform. The bundled proj.db carries the transformations that
# need no grid; `proj-data` is the offline answer for the rest.
#
# BUILD_PROJSYNC=OFF follows, and cmake makes it an error rather than inferring
# it: projsync IS the grid downloader, so "projsync requires Curl" is the build
# telling you that the one tool you just removed the network from is the one
# tool whose whole purpose was the network.
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_CURL=OFF -DENABLE_TIFF=ON -DBUILD_PROJSYNC=OFF \
	-DBUILD_TESTING=OFF -DBUILD_APPS=ON
ninja
DESTDIR=$PKG ninja install

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

# The CDDB client K3b and audio-CD rippers link. Lookups go to a freedb/gnudb
# server over the network, so offline they fail and the tracks keep their
# numbers; the local cache under ~/.cddb still answers a disc seen before.
# MusicBrainz5 is optional upstream and is not a port, so its lookup is named
# off rather than left to a probe. KF_SKIP_PO_PROCESSING: bundled data is
# English only, and kdoctools builds every translated handbook, which the
# find below removes.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_DOC=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_MusicBrainz5=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi

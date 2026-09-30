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

# The C++ library is under CPP/; the rest of the tarball is the C#, Delphi and
# DLL editions. Clipper2Z, the build with a Z value per vertex, is built beside
# Clipper2 because consumers link one or the other by name.
mkdir -p CPP/build && cd CPP/build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DCLIPPER2_USINGZ=ON \
	-DCLIPPER2_UTILS=OFF \
	-DCLIPPER2_EXAMPLES=OFF \
	-DCLIPPER2_TESTS=OFF
make
make DESTDIR=$PKG install

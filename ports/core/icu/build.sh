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

# icu tries to use clang by default
export CC=gcc CXX=g++
cd icu4c/source
./configure --prefix=/usr --disable-samples --disable-tests
make
make DESTDIR=$PKG install

# Temporal in V8 reads the time-zone rules as a compiled zoneinfo64 resource.
# It is built here from this release's source with this release's genrb, so
# node's Temporal and ICU's Intl agree on one tz database version.
LD_LIBRARY_PATH=lib bin/genrb -q -d "$PKG/usr/share/icu/$version" data/misc/zoneinfo64.txt

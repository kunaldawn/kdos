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

# ENABLE_EXTERNAL_LIBS is what makes this library read more than WAV.
#
# With it OFF, libsndfile compiles and links and handles uncompressed PCM only
# — FLAC, Ogg, Vorbis and Opus are all compiled out, whether or not their
# libraries are installed. Every program on the host that opens a sound file
# goes through here, so this flag decides what formats the whole system can
# read. ENABLE_MPEG adds MP3 in both directions.
#
# The four codec ports must stay in `depends`: cmake disables what it cannot
# find without failing, so losing one produces a library that is silently
# narrower than this recipe claims.

mkdir -p build
cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_PROGRAMS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_TESTING=OFF \
	-DENABLE_EXTERNAL_LIBS=ON \
	-DENABLE_MPEG=ON
make
make DESTDIR=$PKG install

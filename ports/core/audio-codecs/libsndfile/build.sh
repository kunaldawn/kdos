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

mkdir -p build
cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_PROGRAMS=ON \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_TESTING=OFF \
	-DENABLE_EXTERNAL_LIBS=ON \
	-DENABLE_MPEG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Ogg=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Vorbis=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_FLAC=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Opus=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_mp3lame=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_mpg123=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ALSA=ON
make
make DESTDIR=$PKG install

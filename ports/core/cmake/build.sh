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

export CXXFLAGS="$CXXFLAGS -include cstdint"
./bootstrap \
	--prefix=/usr \
	--datadir=/share/$name \
	--no-system-jsoncpp  \
	--no-system-cppdap   \
	--no-system-librhash \
	--system-libs \
	-- -D Cmake_BUILD_LTO=ON \
	-D BUILD_TESTING=OFF
make
make DESTDIR=$PKG install

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

# --sphinx-man and no other Sphinx format: the pages are generated from Help/
# and installed under --mandir, which defaults to PREFIX/man and so is named.
./bootstrap \
	--prefix=/usr \
	--datadir=/share/$name \
	--mandir=/share/man \
	--sphinx-man \
	--sphinx-build=/usr/bin/sphinx-build \
	--no-system-jsoncpp  \
	--no-system-cppdap   \
	--no-system-librhash \
	--system-libs \
	-- -D Cmake_BUILD_LTO=ON \
	-D BUILD_TESTING=OFF
make
make DESTDIR=$PKG install

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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/gnuinstalldirs-lib.patch"

export CXXFLAGS="$CXXFLAGS -include cstdint"

./bootstrap \
	--prefix=/usr \
	--datadir=/share/$name \
	--docdir=/share/doc/$name \
	--mandir=/share/man \
	--sphinx-man \
	--sphinx-build=/usr/bin/sphinx-build \
	--no-system-jsoncpp  \
	--no-system-cppdap   \
	--no-system-librhash \
	--no-system-curl \
	--system-libs \
	-- -D CMAKE_BUILD_TYPE=Release \
	-D CMake_BUILD_LTO=ON \
	-D BUILD_CursesDialog=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON \
	-D BUILD_TESTING=OFF
make
make DESTDIR=$PKG install

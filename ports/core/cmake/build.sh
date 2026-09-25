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
#
# ccmake is BUILD_CursesDialog. Left undefined it follows a curses probe, and
# named ON it still drops ccmake with only a warning when FindCurses fails, so
# CMAKE_REQUIRE_FIND_PACKAGE_Curses makes a missing ncurses stop the build.
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
	-- -D CMake_BUILD_LTO=ON \
	-D BUILD_CursesDialog=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON \
	-D BUILD_TESTING=OFF
make
make DESTDIR=$PKG install

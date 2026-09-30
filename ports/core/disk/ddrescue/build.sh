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

# GNU ships this as .tar.lz and nothing else. kpkg unpacks the extensions it
# knows and COPIES anything else into $SRC, so the recipe does the extract —
# which is why lzip is a dependency of a program that has none of its own.
lzip -dc $name-$version.tar.lz | tar -x --strip-components=1

./configure --prefix=/usr CXX="${CXX:-c++}" CXXFLAGS="$CXXFLAGS"
make
make DESTDIR=$PKG install

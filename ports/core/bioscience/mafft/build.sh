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

make -C core PREFIX=/usr ENABLE_ATOMIC=-Denableatomic CFLAGS="${CFLAGS/-O2/-O3} -std=gnu17"
make -C core PREFIX=/usr ENABLE_ATOMIC=-Denableatomic DESTDIR=$PKG install
make -C extensions PREFIX=/usr CXXFLAGS="${CXXFLAGS/-O2/-O3}" LIBS="$LDFLAGS"
make -C extensions PREFIX=/usr DESTDIR=$PKG install

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

# --disable-examples is not optional on musl: examples/testd.c includes
# <sys/unistd.h>, which is a glibc-ism musl does not provide, so the library
# builds fine and then the build dies on a demo nothing installs.
# --disable-lynx drops the doc build, the only step that wants a browser.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-lynx --disable-examples
make
make DESTDIR=$PKG install

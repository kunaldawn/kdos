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

# The library is built static-only by default and the tools link it that way;
# --enable-libpst-shared installs libpst.so and its .pc file. pst2dii is the
# Summation DII exporter and wants gd; the Python module wants Boost.Python.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-libpst-shared \
	--disable-dii \
	--disable-python \
	--disable-nls
make
make DESTDIR=$PKG install

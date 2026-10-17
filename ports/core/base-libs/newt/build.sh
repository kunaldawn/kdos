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

_py=python$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-python=$_py \
	--without-tcl \
	--without-gpm-support \
	--disable-nls
make -j1
make DESTDIR=$PKG install

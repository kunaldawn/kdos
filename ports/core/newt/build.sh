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

# --with-python takes the interpreter's versioned name: the Makefile runs
# <name>-config for the flags and <name> for site-packages. Left at "yes", it
# globs /usr/include/python*/Python.h and skips the snack module in silence
# when that finds nothing. snack is what byobu-config draws its menus with.
_py=python$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-python=$_py \
	--without-tcl \
	--without-gpm-support \
	--enable-nls
make -j1
make DESTDIR=$PKG install

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

# v7.3 MAT files are HDF5 containers, so without --enable-mat73 every file a
# current MATLAB writes by default is unreadable. MCOS lets class objects be
# read instead of skipped.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-zlib=/usr \
	--with-hdf5=/usr \
	--enable-mat73=yes \
	--enable-extended-sparse=yes \
	--enable-mcos=yes \
	--with-default-file-ver=5
make
make DESTDIR=$PKG install

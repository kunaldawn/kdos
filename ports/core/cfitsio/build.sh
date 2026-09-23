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

# 4.6 IS AUTOMAKE. The old hand-written Makefile had `shared` and `install`
# targets and no `all`; this one is generated, so `make shared` is
# "No rule to make target" after configure has just reported success.
# --enable-curl is what lets a FITS file be opened over a URL, which is a
# network client inside an image reader and is off; every consumer here reads
# from disk.
# drvrnet.c calls gethostbyname(), which musl declares in <netdb.h> only under
# _GNU_SOURCE or _BSD_SOURCE — `-std=gnu11` defines neither. The file is
# compiled whatever --disable-curl says, so the define is not optional.
export CFLAGS="$CFLAGS -D_GNU_SOURCE"

# The Fortran wrappers are C, compiled only when configure finds a Fortran
# compiler; with none it drops them silently and wcslib and gnuastro lose the
# f77 entry points. F77 is named so a missing gfortran fails the probe instead.
# --with-bzip2 lets a .fits.bz2 open directly; it defaults off, and when named
# a missing libbz2 stops configure.
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-reentrant --enable-shared --disable-static --disable-curl \
	--with-bzip2 F77=gfortran
make
make DESTDIR=$PKG install

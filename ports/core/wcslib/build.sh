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

# No Fortran wrappers and no PGPLOT, named rather than probed: configure turns
# on whatever it finds, and gfortran is part of the gcc port. The wrappers are
# a binding nothing here calls — astropy and gnuastro both use the C interface
# — and PGPLOT is not a port. X is only ever searched for PGPLOT's sake.
./configure --prefix=/usr --libdir=/usr/lib \
	--without-pgplot --without-x --disable-fortran --with-cfitsio
make
make DESTDIR=$PKG install

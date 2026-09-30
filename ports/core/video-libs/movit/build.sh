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

# configure refuses to run without sdl2.pc, which only the unit tests and the
# demo link; the library itself uses epoxy, Eigen and FFTW. `make` alone
# builds the unit tests against a Google Test source tree, so only the
# library target is built. The shaders are compiled into the library.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static
make libmovit.la
make DESTDIR=$PKG install

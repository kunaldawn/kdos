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

# configure.py is Botan's own build generator. Only the shared library and the
# botan command are built: the static library and the test binary are not
# shipped, and the Python module is left out because nothing here imports it.
# The documentation needs Sphinx, which is not a port. getrandom is the entropy
# source; zlib, bzip2 and lzma are the compression filters.
export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
python3 ./configure.py \
	--prefix=/usr \
	--libdir=lib \
	--mandir=/usr/share/man \
	--cc=gcc \
	--build-targets=shared,cli \
	--disable-static-library \
	--no-install-python-module \
	--without-documentation \
	--with-zlib \
	--with-bzip2 \
	--with-lzma \
	--with-os-features=getrandom
make
make DESTDIR=$PKG install

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

# xfsprogs hard-requires liburcu and inih: there is no --disable-urcu, which
# is why liburcu is a port rather than an optional dependency.
#
# DEBUG= is not a style choice — the default build defines -DDEBUG and ships
# assertion-heavy binaries.
export DEBUG=-DNDEBUG
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--enable-editline=no \
	--enable-scrub=no \
	--enable-lto=no

make
make DESTDIR=$PKG install install-dev

# The install puts the shared libs in /usr/lib but leaves .la files behind;
# they name build-tree paths and confuse anything that reads them later.
find "$PKG" -name '*.la' -delete

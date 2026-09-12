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

# -j1 FOR THE INSTALL ONLY. The phase exports MAKEFLAGS=-j12, and xfsprogs'
# install targets regenerate their dependency files while other jobs are
# reading them: a half-written .dep leaves a bare line continuation, and make
# reports `No rule to make target '\'` against an object it was building
# happily a moment earlier. It is a race, so it passes as often as it fails.
# A command-line -j overrides the one in MAKEFLAGS.
make -j1 DESTDIR=$PKG install install-dev

# The install puts the shared libs in /usr/lib but leaves .la files behind;
# they name build-tree paths and confuse anything that reads them later.
find "$PKG" -name '*.la' -delete

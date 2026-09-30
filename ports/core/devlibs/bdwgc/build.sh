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

# --enable-threads=posix with --enable-thread-local-alloc: a collector that
# does not know about the threads in the process scans the wrong stacks and
# frees live objects, which presents as a random crash rather than as an error.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-threads=posix --enable-thread-local-alloc \
	--enable-cplusplus --with-libatomic-ops=none
make
make DESTDIR=$PKG install

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

_cflags="$CFLAGS -O3 -ffast-math -pthread -DUSE_PTHREADS -DUSE_BZIP2 -DUSE_GZIP -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L"
_ldflags="$LDFLAGS -lm -pthread -lbz2 -lz"
make prefix=/usr docdir=/usr/share/doc/routino LDFLAGS_LDSO= HAVE_SWIG= \
	CFLAGS="$_cflags" LDFLAGS="$_ldflags"
make prefix=/usr docdir=/usr/share/doc/routino LDFLAGS_LDSO= HAVE_SWIG= \
	CFLAGS="$_cflags" LDFLAGS="$_ldflags" DESTDIR=$PKG install

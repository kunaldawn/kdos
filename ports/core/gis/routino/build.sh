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

# Routino has no configure: the Makefile reads a config file, and every path is
# a make variable. The router is what runs on a booted machine; planetsplitter
# is what turns an .osm extract into the database it reads, and both ship
# because an extract with no preprocessing step is a file nothing can use.
#
# HAVE_SWIG= keeps the Python binding out. python/Makefile builds it whenever
# swig and python3 answer, its typemap calls the Python 2 PyString_FromString,
# and its install rule installs nothing, so the build could only fail on it.
#
# Makefile.conf assigns CFLAGS and LDFLAGS outright, with -g, so both are passed
# here: the exported flags, then every define and library the file would add.
_cflags="$CFLAGS -O3 -pthread -DUSE_PTHREADS -DUSE_BZIP2 -DUSE_GZIP -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L"
_ldflags="$LDFLAGS -lm -pthread -lbz2 -lz"
make prefix=/usr docdir=/usr/share/doc/routino LDFLAGS_LDSO= HAVE_SWIG= \
	CFLAGS="$_cflags" LDFLAGS="$_ldflags"
make prefix=/usr docdir=/usr/share/doc/routino LDFLAGS_LDSO= HAVE_SWIG= \
	CFLAGS="$_cflags" LDFLAGS="$_ldflags" DESTDIR=$PKG install

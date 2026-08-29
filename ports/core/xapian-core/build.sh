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

# `sed` IN depends IS GNU sed AND IT IS LOAD-BEARING. configure generates the
# public include/xapian/version.h by preprocessing version_h.cc and filtering
# the result with `0,/const char \* dummy/d` — a GNU address form toybox's sed
# does not implement. Toybox emits nothing, configure checks nothing, and the
# header is written ZERO BYTES long; the build then reaches the compiler and
# fails with "XAPIAN_DOCID_BASE_TYPE does not name a type", which says nothing
# about sed at all.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

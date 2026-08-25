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

# Not autotools: this configure probes for strlcpy and a few kqueue pieces and
# then copies the matching Makefile.<platform> into place. musl has strlcpy, so
# it lands on Makefile.linux and the compat shims stay out.
./configure

# RELEASE is hardcoded in Makefile.bsd and upstream did not bump it for this
# tarball, so `entr -h` would report the previous version. It is passed here
# rather than patched because it is already a make variable.
make RELEASE=$version
make install DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man

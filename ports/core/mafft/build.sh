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

# PREFIX is baked into the wrapper script at BUILD time, not read at run time,
# so it must be the installed path while DESTDIR carries the staging root.
# ONE RULE IN THE MAKEFILE OMITS -std=c99 — dndblast.c is compiled bare — and
# under GCC 15's default C23 an old-style `extern void cpmx_calc();` declares
# `(void)`, so mltaln.h and functions.h disagree about every function and the
# file fails on thirty "conflicting types". -std=gnu17 restores the meaning of
# an empty parameter list.
#
# CFLAGS IS SAFE TO OVERRIDE HERE, which is not usually true: this Makefile's
# `CFLAGS = -O3` is the whole of it, carrying no -D the source reads, so
# nothing is lost by replacing it.
make -C core PREFIX=/usr CFLAGS="-O3 -std=gnu17"
make -C core PREFIX=/usr DESTDIR=$PKG install

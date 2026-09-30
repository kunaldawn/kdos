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

# The colour-name table is generated with gperf.

# Every NetSurf library's makefile adds -Werror, and the environment's CFLAGS
# come after it, so -Wno-error keeps a warning a newer compiler adds from
# failing the build.
export CFLAGS="$CFLAGS -Wno-error"
export NSSHARED=/usr/share/netsurf-buildsystem
make COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib
# The build system's INSTALL is `install -C`, which toybox's install rejects;
# -C only skips identical files, and $PKG starts empty.
make install INSTALL=install COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib DESTDIR=$PKG

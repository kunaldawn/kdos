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

export CFLAGS="$CFLAGS -Wno-error"
export NSSHARED=/usr/share/netsurf-buildsystem
make COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib WITH_HUBBUB_BINDING=yes WITH_EXPAT_BINDING=yes WITH_LIBXML_BINDING=no
make install COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib WITH_HUBBUB_BINDING=yes WITH_EXPAT_BINDING=yes WITH_LIBXML_BINDING=no DESTDIR=$PKG

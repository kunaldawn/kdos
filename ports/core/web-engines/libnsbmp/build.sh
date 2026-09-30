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

export NSSHARED=/usr/share/netsurf-buildsystem
make COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib
# The build system's INSTALL is `install -C`, which toybox's install rejects;
# -C only skips identical files, and $PKG starts empty.
make install INSTALL=install COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib DESTDIR=$PKG

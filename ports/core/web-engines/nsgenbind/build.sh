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

# The WebIDL and binding-file parsers are generated with flex and bison.
# -Werror is added unless VARIANT is release; release is named so the choice
# does not rest on buildsystem's default.
export NSSHARED=/usr/share/netsurf-buildsystem
make PREFIX=/usr VARIANT=release
# The build system's INSTALL is `install -C`, which toybox's install rejects;
# -C only skips identical files, and $PKG starts empty.
make install INSTALL=install PREFIX=/usr VARIANT=release DESTDIR=$PKG

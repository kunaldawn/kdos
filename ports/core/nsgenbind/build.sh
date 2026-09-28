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
make install PREFIX=/usr VARIANT=release DESTDIR=$PKG

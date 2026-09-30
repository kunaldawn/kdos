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

export CFLAGS="$CFLAGS -O2"
make PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr install

# NO DESKTOP ENTRY. `ii` is a connection and not a program with a window: it
# writes a directory of FIFOs and reading it is `tail -f` in one terminal and
# `echo` into another. A launcher for it with no server, no nick and no channel
# opens a program that exits.

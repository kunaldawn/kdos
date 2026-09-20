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

# A VIDEO HERE IS A STREAM OF FRAMES AND EVERY PROGRAM IS A FILTER over it, so
# blind is farbfeld's argument carried into time: the same bet that a format
# worth having is one a pipe can carry. It needs no codec of its own — reading
# and writing real containers is ffmpeg's job at either end.
make PREFIX=/usr MANPREFIX=/usr/share/man
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man install

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

# THE RELEASE TARBALL, NOT THE TAG ARCHIVE. It ships the generated
# `configure`, so the build needs no autoreconf and no gettext autopoint.
# --with-ncurses cannot make a missing ncurses an error: the library is found
# by probe and linked when present, so it is a depend to keep the terminal
# colour detection always built in.
./configure --prefix=/usr --disable-static --with-ncurses --disable-nls
make
make DESTDIR=$PKG install

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

# HOST-SIDE m4 ONLY — nothing here is compiled and nothing runs on the target.
# An AX_* macro that is missing does not fail at aclocal time: it survives into
# the generated configure as a literal word, and the shell reports
# `AX_WITH_CURSES: command not found` followed by whatever the macro was
# guarding. calcurse is the worked example.
./configure --prefix=/usr
make
make DESTDIR=$PKG install

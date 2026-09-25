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

# COLOURED DIAGNOSTICS COME FROM gettext's libtextstyle, which configure probes
# with no switch to force it either way. gettext is in `depends` so the probe
# finds it in every phase; undeclared, 03_phase3 lists bison before gettext and
# the bootstrap bison would print in monochrome.
./configure --prefix=/usr --disable-nls
make
make DESTDIR=$PKG install

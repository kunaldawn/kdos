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

# ghostscript IS A BUILD DEPENDENCY, not a runtime one. configure only defines
# PATH_GHOSTSCRIPT when it finds `gs` in PATH, and pdf.c references the macro
# UNCONDITIONALLY — so a missing gs is a configure WARNING followed by
# `'PATH_GHOSTSCRIPT' undeclared` a thousand files later.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

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

gcc $CFLAGS -O2 -include string.h -o aa3d aa3d.c $LDFLAGS

install -Dm755 aa3d "$PKG/usr/bin/aa3d"
install -Dm644 logo "$PKG/usr/share/aa3d/logo"
install -Dm644 pyramid "$PKG/usr/share/aa3d/pyramid"

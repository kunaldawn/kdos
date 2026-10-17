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

export LDFLAGS="$LDFLAGS -largp"
f3_cflags="$CFLAGS -std=c17 -Wall -Wextra -pedantic -MMD"

make CFLAGS="$f3_cflags"
make DESTDIR=$PKG PREFIX=/usr CFLAGS="$f3_cflags" install

make extra CFLAGS="$f3_cflags"
make DESTDIR=$PKG PREFIX=/usr CFLAGS="$f3_cflags" install-extra

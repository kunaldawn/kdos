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

# Fix for GCC 15 strictness (C23 default)
# Pass CFLAGS to configure to ensure it's used
# guile has no port and is pinned off, so configure's probe for it cannot
# change what is built.
./configure --host=$TARGET --prefix=/usr --without-guile CFLAGS="$CFLAGS -std=gnu99"
make
make DESTDIR=$PKG install

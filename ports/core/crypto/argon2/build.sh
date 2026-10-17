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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
make OPTTARGET=x86-64 ARGON2_VERSION=$version LIBRARY_REL=lib
make OPTTARGET=x86-64 ARGON2_VERSION=$version LIBRARY_REL=lib PREFIX=/usr DESTDIR=$PKG install
rm -f "$PKG/usr/lib/libargon2.a"
install -Dm644 man/argon2.1 "$PKG/usr/share/man/man1/argon2.1"

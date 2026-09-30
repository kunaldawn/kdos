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

# OPTTARGET=none builds the portable reference code rather than tuning for the
# builder's own processor, so the library runs on every x86-64 machine the
# image boots on. ARGON2_VERSION is what libargon2.pc reports; without it the
# file says ZERO. LIBRARY_REL=lib puts the library where the linker searches
# instead of lib/<triplet>.
make OPTTARGET=none ARGON2_VERSION=$version LIBRARY_REL=lib
make OPTTARGET=none ARGON2_VERSION=$version LIBRARY_REL=lib PREFIX=/usr DESTDIR=$PKG install
rm -f "$PKG/usr/lib/libargon2.a"
install -Dm644 man/argon2.1 "$PKG/usr/share/man/man1/argon2.1"

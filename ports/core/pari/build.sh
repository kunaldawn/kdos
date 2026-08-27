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

# --with-gmp IS NOT OPTIONAL IN PRACTICE. PARI ships its own bignum kernel and
# will build without gmp; the result is several times slower on exactly the
# operations people run PARI for. gmp is already a port.
#
# Its `Configure` is hand-written, not autoconf — it takes --prefix and writes
# an Oxxx directory whose makefile is the real build. `gp` is the interactive
# calculator and the reason this is here beside maxima: PARI answers exact
# integer and number-theoretic questions, maxima answers symbolic ones, and
# neither is a substitute for the other.
./Configure --prefix=/usr --with-gmp --with-readline
make all
make DESTDIR=$PKG install

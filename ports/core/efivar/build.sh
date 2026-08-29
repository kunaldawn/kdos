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

# -Wno-* IS NOT COSMETIC HERE. efivar is built with -Werror and a large warning
# set upstream tunes against the compiler of the day; GCC 15 adds diagnostics
# this 2023 release predates, so the build fails on warnings in code that is
# correct. Suppressing the family is what keeps the recipe free of patches
# against somebody else's source.
export CFLAGS="$CFLAGS -Wno-error -Wno-address -Wno-free-nonheap-object -Wno-stringop-overflow -Wno-array-bounds"

# ERRORFILE is upstream's own escape hatch for exactly that; libdir must be
# passed to both stages or the .pc lands beside a library that is elsewhere.
make ERRORFILE= libdir=/usr/lib bindir=/usr/bin mandir=/usr/share/man
make ERRORFILE= libdir=/usr/lib bindir=/usr/bin mandir=/usr/share/man DESTDIR=$PKG install

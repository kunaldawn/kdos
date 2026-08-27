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

# IT HAS A RUNTIME DISPATCHER AND THE MAKEFILE STILL NEEDS TELLING. minimap2
# builds an SSE4.1 and an AVX2 kernel and picks between them by cpuid at run
# time — the OpenBLAS property — but its default target adds `-msse4.1` to
# EVERYTHING, so a build on a machine that has it produces a binary that
# SIGILLs on one that does not. The `sse2only` target is the portable baseline
# and the dispatch still selects the wide kernel where the CPU has it.
make sse2only=1
install -Dm755 minimap2 $PKG/usr/bin/minimap2
install -Dm644 minimap2.1 $PKG/usr/share/man/man1/minimap2.1
install -Dm755 misc/paftools.js $PKG/usr/share/minimap2/paftools.js

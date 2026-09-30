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

# THE DEFAULT TARGET IS THE PORTABLE ONE. It builds each ksw2 kernel twice,
# once with -msse4.1 and once with -msse2 -mno-sse4.1, plus ksw2_dispatch.o,
# which picks between them by cpuid at run time. -msse4.1 reaches only the
# SSE4.1 kernels and the dispatcher. `sse2only=1` drops the SSE4.1 kernels
# and the dispatcher, leaving every CPU on the slow path.
make
install -Dm755 minimap2 $PKG/usr/bin/minimap2
install -Dm644 minimap2.1 $PKG/usr/share/man/man1/minimap2.1
install -Dm755 misc/paftools.js $PKG/usr/share/minimap2/paftools.js

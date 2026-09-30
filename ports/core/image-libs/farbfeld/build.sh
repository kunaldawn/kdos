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

# THE CONVERTERS ARE THE POINT, not the format. png2ff and jpg2ff read into a
# stream every other tool here can filter, and ff2png/ff2jpg write it back out;
# ff2pam and ff2ppm hand it to anything that speaks netpbm. Nothing is linked
# into a library, so a missing codec is a missing pair of programs rather than
# a build failure.
make PREFIX=/usr MANPREFIX=/usr/share/man
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man install

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

# THE SHIPPED config.mk IS THE BUILD CONFIGURATION and ./configure only exists
# to rewrite it for a cross build. A native build overrides the two prefixes on
# the command line and leaves the file alone, which is why nothing here edits
# a source file.
#
# MANPREFIX IS NOT \$PREFIX/share/man BY ACCIDENT: upstream already spells it
# that way, but naming it keeps the pages where \`man\` looks if the default
# ever changes.
# LDCONFIG IS UNSET BECAUSE MUSL HAS NO ldconfig. Upstream's install target
# runs $(LDCONFIG) after copying the library and exits 127 when it is not
# there, so every file lands correctly and the build fails on the last line.
# config.mk says to unset it for exactly this case; there is no cache to
# refresh on this system because the dynamic linker reads the path directly.
make PREFIX=/usr MANPREFIX=/usr/share/man
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man LDCONFIG= install

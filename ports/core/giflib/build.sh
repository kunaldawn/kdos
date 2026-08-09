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

# Skip the doc/ subdir: it tries to rebuild a logo with `convert`,
# which would require ImageMagick built with GIF support — a
# circular dep. The pre-rendered .1/.7 man pages already ship in
# the tarball, so install those by hand.
sed -i '/\$(MAKE) -C doc/d' Makefile

make
make PREFIX=$PKG/usr install-bin install-include install-lib
install -Dm644 -t $PKG/usr/share/man/man1 doc/*.1
install -Dm644 -t $PKG/usr/share/man/man7 doc/giflib.7

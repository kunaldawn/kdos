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

# The man pages and the info manual ship prebuilt in the tarball and install
# as they are; nothing regenerates them unless their sources are newer. The
# gtk-doc HTML reference installs beside them and is removed.
./configure --prefix=/usr --disable-nls --disable-valgrind-tests
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/gtk-doc"

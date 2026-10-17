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

patch -p1 -i "$PORT_SRC/fix-sysmacros-header.patch"
patch -p1 -i "$PORT_SRC/transport.hxx.patch"

make

install -Dm755 growisofs dvd+rw-format dvd+rw-booktype dvd+rw-mediainfo \
	dvd-ram-control -t "$PKG/usr/bin"
install -Dm644 growisofs.1 -t "$PKG/usr/share/man/man1"

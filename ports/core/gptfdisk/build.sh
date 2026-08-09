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

export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
make gdisk sgdisk fixparts
install -d $PKG/usr/bin $PKG/usr/share/man/man8
install -t $PKG/usr/bin gdisk sgdisk fixparts
install -m644 -t $PKG/usr/share/man/man8 gdisk.8 sgdisk.8 fixparts.8

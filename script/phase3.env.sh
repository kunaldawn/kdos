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

# Environment configuration for KDOS build

export CHROOT=1

export LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:/usr/lib64:/usr/local/lib64"${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export PKG_CONFIG_PATH="/usr/local/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig"${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
export CFLAGS="-O2 -pipe -std=gnu11 -fPIC"
export CXXFLAGS="-O2 -pipe -fPIC"
export LDFLAGS=""
export MAKEFLAGS="-j12"
export TERM=dumb

rm -rf /var/cache/kpkg/work

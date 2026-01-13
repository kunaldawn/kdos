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

export PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig":${PKG_CONFIG_PATH}
export CFLAGS="-O2 -pipe -std=gnu11 -fPIC"
export CXXFLAGS="-O2 -pipe -fPIC"
export LDFLAGS=""
export MAKEFLAGS="-j12"
export TERM=dumb

rm -rf /var/cache/kpkg/work

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

# --libbpf_force on turns a missing libbpf into a configure failure; left on
# auto, `ip ... xdp obj` and `tc ... bpf` are built without the loader whenever
# libbpf is not installed yet.
./configure --prefix=/usr --libbpf_force on

# Fix missing UINT_MAX by adding limits.h via compiler flags
export CFLAGS="$CFLAGS -include limits.h"    
make
make DESTDIR=$PKG SBINDIR=/usr/sbin install

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

# The release tarball carries no configure script. The binary is linked
# -all-static by upstream's Makefile.am, and has to be: podman bind-mounts it
# into containers whose root has no musl. podman looks for it in its helper
# directories, /usr/lib/podman among them, so bindir is that directory and
# /usr/bin never holds it.
autoreconf -fi
./configure --prefix=/usr --bindir=/usr/lib/podman
make
make DESTDIR=$PKG install

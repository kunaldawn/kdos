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

# Buildah's Makefile has no install.bin target. The manual pages are rendered
# by the go-md2man vendored under tests/tools, which 'make docs' builds first.
export BUILDTAGS="seccomp exclude_graphdriver_btrfs"

make BUILDTAGS="$BUILDTAGS" PREFIX=/usr buildah
install -Dm755 bin/buildah $PKG/usr/bin/buildah

make docs
make -C docs DESTDIR=$PKG PREFIX=/usr install

make BUILDTAGS="$BUILDTAGS" DESTDIR=$PKG PREFIX=/usr install.completions

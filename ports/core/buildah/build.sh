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

# Buildah's Makefile has no install.bin target; install chains docs which
# would need go-md2man. Build the binary, then install it by hand.
export BUILDTAGS="seccomp exclude_graphdriver_btrfs exclude_graphdriver_devicemapper"

make BUILDTAGS="$BUILDTAGS" PREFIX=/usr buildah
install -Dm755 bin/buildah $PKG/usr/bin/buildah

make BUILDTAGS="$BUILDTAGS" DESTDIR=$PKG PREFIX=/usr install.completions

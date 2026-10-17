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

export BUILDTAGS="seccomp libsqlite3"

make BUILDTAGS="$BUILDTAGS" EXTRA_LDFLAGS="-s -w" PREFIX=/usr buildah
install -Dm755 bin/buildah $PKG/usr/bin/buildah

make docs
make -C docs DESTDIR=$PKG PREFIX=/usr install

make BUILDTAGS="$BUILDTAGS" DESTDIR=$PKG PREFIX=/usr install.completions

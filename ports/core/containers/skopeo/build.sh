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

export BUILDTAGS="libsubid libsqlite3 exclude_graphdriver_btrfs"

make BUILDTAGS="$BUILDTAGS" EXTRA_LDFLAGS="-s -w" PREFIX=/usr bin/skopeo
install -Dm755 bin/skopeo $PKG/usr/bin/skopeo
make PREFIX=/usr DESTDIR=$PKG install-docs
make PREFIX=/usr DESTDIR=$PKG install-completions

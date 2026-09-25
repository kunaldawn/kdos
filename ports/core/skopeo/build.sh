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

# BUILDTAGS on the command line replaces the Makefile's header probes, so the
# tag set is fixed rather than decided by what the build root happens to hold:
# libsubid links shadow's library for /etc/subuid ranges, libsqlite3 links the
# sqlite port instead of the copy bundled with the Go binding, and the btrfs
# graph driver is excluded as in podman. Signing stays on gpgme; the Sequoia
# backend needs podman-sequoia, which is not a port.
export BUILDTAGS="libsubid libsqlite3 exclude_graphdriver_btrfs"

make BUILDTAGS="$BUILDTAGS" PREFIX=/usr bin/skopeo
install -Dm755 bin/skopeo $PKG/usr/bin/skopeo
make PREFIX=/usr DESTDIR=$PKG install-docs
make PREFIX=/usr DESTDIR=$PKG install-completions

# policy.json and the rest of /etc/containers are containers-common's, which
# podman and buildah read as well.

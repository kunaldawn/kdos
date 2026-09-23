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
# BUILDTAGS on the command line replaces the Makefile's probed tag list
# (btrfs headers, libsubid, systemd, sqlite), so this list is the whole set:
# the btrfs graphdriver builds against btrfs-progs' headers, and libsqlite3
# links the sqlite port instead of go-sqlite3's bundled amalgamation.
export BUILDTAGS="seccomp libsqlite3"

make BUILDTAGS="$BUILDTAGS" PREFIX=/usr buildah
install -Dm755 bin/buildah $PKG/usr/bin/buildah

make docs
make -C docs DESTDIR=$PKG PREFIX=/usr install

make BUILDTAGS="$BUILDTAGS" DESTDIR=$PKG PREFIX=/usr install.completions

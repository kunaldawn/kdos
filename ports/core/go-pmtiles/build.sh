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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=0
# `pmtiles extract` against the hosted planet is a NETWORK operation and is not
# the reason this is here: the offline path is tilemaker → pmtiles at bake, and
# what this gives a booted machine is `show`, `serve` and `convert` over a file
# that is already on the disk.
mkdir -p out
go build -mod=vendor -o out/pmtiles .
install -Dm755 out/pmtiles $PKG/usr/bin/pmtiles

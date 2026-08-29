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
# -o INTO a directory of our own: the command lives in ./seqkit, a package
# directory of the same name as the binary, and `go build -o seqkit` writes it
# INSIDE that directory rather than over it. install then reports
# "Skipped dir" on a path that looks exactly like the binary it wanted.
mkdir -p out
go build -mod=vendor -o out/seqkit ./seqkit
install -Dm755 out/seqkit $PKG/usr/bin/seqkit

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

cd src
export GOROOT_BOOTSTRAP="$SRC_ROOT/go"
export GOROOT_FINAL=/usr/lib/go
export GOOS=linux GOARCH=amd64
export CGO_ENABLED=1

./make.bash

cd ..
install -d $PKG/usr/lib/go
cp -a bin pkg src lib api $PKG/usr/lib/go/
[ -d misc ] && cp -a misc $PKG/usr/lib/go/
install -d $PKG/usr/bin
ln -s /usr/lib/go/bin/go $PKG/usr/bin/go
ln -s /usr/lib/go/bin/gofmt $PKG/usr/bin/gofmt

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

# The source tarball carries upstream-compiled objects: the race detector's
# runtime (src/runtime/race) and the BoringCrypto module. Neither is built here,
# so neither is installed — `go build -race` and GOEXPERIMENT=boringcrypto fail
# to link on this system. pkg/obj is make.bash's scratch, not part of the
# toolchain.
find $PKG/usr/lib/go/src/runtime/race $PKG/usr/lib/go/src/crypto/internal/boring/syso \
	-name '*.syso' -delete
rm -rf $PKG/usr/lib/go/pkg/obj
install -d $PKG/usr/bin
ln -s /usr/lib/go/bin/go $PKG/usr/bin/go
ln -s /usr/lib/go/bin/gofmt $PKG/usr/bin/gofmt

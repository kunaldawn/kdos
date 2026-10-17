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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/go-env-toolchain-local.patch"

cd src
export GOROOT_BOOTSTRAP="$SRC_ROOT/go"
export GOROOT_FINAL=/usr/lib/go
export GOOS=linux GOARCH=amd64
export CGO_ENABLED=1
export GO_LDFLAGS=-buildmode=pie

./make.bash

cd ..
install -d $PKG/usr/lib/go
cp -a bin pkg src lib api $PKG/usr/lib/go/
install -m644 go.env VERSION -t $PKG/usr/lib/go/
[ -d misc ] && cp -a misc $PKG/usr/lib/go/

find $PKG/usr/lib/go/src/runtime/race $PKG/usr/lib/go/src/crypto/internal/boring/syso \
	-name '*.syso' -delete
rm -rf $PKG/usr/lib/go/pkg/obj
find $PKG/usr/lib/go/src -type d -name testdata -prune -exec rm -rf {} +
install -d $PKG/usr/bin
ln -s /usr/lib/go/bin/go $PKG/usr/bin/go
ln -s /usr/lib/go/bin/gofmt $PKG/usr/bin/gofmt

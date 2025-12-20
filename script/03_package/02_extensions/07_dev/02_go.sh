#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/go" ]; then
    exit 0
fi

echo ">>> Building Go $GO_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/go$GO_VER.src.tar.gz
cd go/src

# Go Bootstrap logic:
# 1. We are cross-compiling Go itself.
# 2. To build Go, we need a Go compiler (GOROOT_BOOTSTRAP). We use the host's Go (from Dockerfile).
# 3. The build process compiles tools that run on the HOST (dist, compile, link, etc.).
#    For these, we MUST use the HOST C compiler (gcc).
# 4. The resulting Go compiler will produce binaries for the TARGET.
#    For these target binaries (and for CGO support in the standard library),
#    we MUST use the TARGET C compiler ($TARGET-gcc).

# Host compiler for bootstrapping tools
export CC=gcc
export CXX=g++

# Target compiler for the final Go binary and CGO
export CC_FOR_TARGET=$TARGET-gcc
export CXX_FOR_TARGET=$TARGET-g++
export CGO_ENABLED=1

export GOOS=linux
export GOARCH=amd64
export GOROOT_BOOTSTRAP=$(go env GOROOT)

# Fix permission denied on default /.cache
export GOCACHE=$BUILD_DIR/tmp/go-cache
export GOMODCACHE=$BUILD_DIR/tmp/go-mod-cache
mkdir -p $GOCACHE $GOMODCACHE

./make.bash

cd ../..

# Install to /usr/local/go (standard location)
rm -rf $SYSROOT/usr/local/go
mkdir -p $SYSROOT/usr/local
cp -r go $SYSROOT/usr/local/go

# Symlink binaries to /usr/bin
ln -sf ../local/go/bin/go $SYSROOT/usr/bin/go
ln -sf ../local/go/bin/gofmt $SYSROOT/usr/bin/gofmt

rm -rf go

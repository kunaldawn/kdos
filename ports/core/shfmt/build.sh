#!/bin/bash


tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# THIS TREE IS ~470 build.sh FILES AND preflight ALREADY RUNS `bash -n` OVER
# EVERY ONE. That catches a syntax error and nothing else; shfmt is the same
# gate one level up — it PARSES rather than sources, so it reports the unquoted
# expansion and the inconsistent indentation a syntax check passes. shellcheck
# would be the other half and is Haskell: a GHC bootstrap for one tool is not a
# price this distro pays.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.version=v$version" -o shfmt ./cmd/shfmt
install -Dm755 shfmt $PKG/usr/bin/shfmt

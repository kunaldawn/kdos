#!/bin/bash


tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# ONE KEY FORMAT, NO OPTIONS, NO CONFIGURATION FILE — which is the whole
# argument for it beside gpg. There is no cipher to choose wrong, no key server
# to be unreachable and no web of trust to bootstrap, and an age recipient is a
# short line somebody can read over a phone. It also takes an SSH key directly,
# so a machine that already has one needs no new key material at all.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.Version=v$version" -o age ./cmd/age
go build -mod=vendor -ldflags "-s -w -X main.Version=v$version" -o age-keygen ./cmd/age-keygen
install -Dm755 age        $PKG/usr/bin/age
install -Dm755 age-keygen $PKG/usr/bin/age-keygen
install -Dm644 doc/age.1        $PKG/usr/share/man/man1/age.1
install -Dm644 doc/age-keygen.1 $PKG/usr/share/man/man1/age-keygen.1

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
for cmd in age age-keygen age-inspect age-plugin-batchpass; do
	go build -mod=vendor -ldflags "-s -w -X main.Version=v$version" -o $cmd ./cmd/$cmd
	install -Dm755 $cmd $PKG/usr/bin/$cmd
	install -Dm644 doc/$cmd.1 $PKG/usr/share/man/man1/$cmd.1
done

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

# ONE KEY FORMAT, NO OPTIONS, NO CONFIGURATION FILE — which is the whole
# argument for it beside gpg. There is no cipher to choose wrong, no key server
# to be unreachable and no web of trust to bootstrap, and an age recipient is a
# short line somebody can read over a phone. It also takes an SSH key directly,
# so a machine that already has one needs no new key material at all.
export CGO_ENABLED=0
for cmd in age age-keygen age-inspect age-plugin-batchpass; do
	go build -mod=vendor -ldflags "-s -w -X main.Version=v$version" -o $cmd ./cmd/$cmd
	install -Dm755 $cmd $PKG/usr/bin/$cmd
	install -Dm644 doc/$cmd.1 $PKG/usr/share/man/man1/$cmd.1
done

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
go build -mod=vendor -ldflags "-s -w -X main.version=$version" -o restic ./cmd/restic
install -Dm755 restic $PKG/usr/bin/restic

install -dm755 $PKG/usr/share/man/man1
install -dm755 $PKG/usr/share/bash-completion/completions
install -dm755 $PKG/usr/share/zsh/site-functions
install -dm755 $PKG/usr/share/fish/vendor_completions.d

./restic generate --man $PKG/usr/share/man/man1
./restic generate --bash-completion $PKG/usr/share/bash-completion/completions/restic
./restic generate --zsh-completion $PKG/usr/share/zsh/site-functions/_restic
./restic generate --fish-completion $PKG/usr/share/fish/vendor_completions.d/restic.fish

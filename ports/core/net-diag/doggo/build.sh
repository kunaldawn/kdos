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
go build -mod=vendor -ldflags "-s -w -X main.buildVersion=v$version" -o doggo ./cmd/doggo
install -Dm755 doggo $PKG/usr/bin/doggo
./doggo completions bash > doggo.bash
./doggo completions zsh  > _doggo
./doggo completions fish > doggo.fish
install -Dm644 doggo.bash $PKG/usr/share/bash-completion/completions/doggo
install -Dm644 _doggo     $PKG/usr/share/zsh/site-functions/_doggo
install -Dm644 doggo.fish $PKG/usr/share/fish/vendor_completions.d/doggo.fish

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
go build -ldflags "-s -w -X main.version=$version" -o fzf
install -Dm755 fzf $PKG/usr/bin/fzf
install -Dm644 man/man1/fzf.1 $PKG/usr/share/man/man1/fzf.1
install -Dm644 shell/key-bindings.bash $PKG/usr/share/fzf/key-bindings.bash
install -Dm644 shell/completion.bash $PKG/usr/share/fzf/completion.bash

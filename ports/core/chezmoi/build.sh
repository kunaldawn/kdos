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

# noupgrade drops `chezmoi upgrade`, which replaces its own binary from GitHub
# and would leave /usr/bin/chezmoi different from the file kpkg installed.
export CGO_ENABLED=0
go build -mod=vendor -tags noupgrade \
	-ldflags "-s -w -X main.version=$version -X main.builtBy=kdos" \
	-o chezmoi
install -Dm755 chezmoi $PKG/usr/bin/chezmoi

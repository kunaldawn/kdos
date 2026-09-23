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
go build -ldflags "-s -w" -o yq
install -Dm755 yq $PKG/usr/bin/yq
bash scripts/generate-man-page-md.sh
lowdown -s -Tman -M title=YQ -M section=1 -M "author=Mike Farah" -o yq.1 man.md
install -Dm644 yq.1 $PKG/usr/share/man/man1/yq.1

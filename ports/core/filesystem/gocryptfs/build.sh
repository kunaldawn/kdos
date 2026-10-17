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

export CGO_ENABLED=1
mkdir -p out
go build -mod=vendor -ldflags "-s -w -X main.GitVersion=v$version -X main.GitVersionFuse=vendored -X main.BuildDate=offline" -o out/gocryptfs
go build -mod=vendor -ldflags "-s -w" -o out/gocryptfs-xray ./gocryptfs-xray
install -Dm755 out/gocryptfs       $PKG/usr/bin/gocryptfs
install -Dm755 out/gocryptfs-xray  $PKG/usr/bin/gocryptfs-xray
lowdown -s -Tman -o out/gocryptfs.1      Documentation/MANPAGE.md
lowdown -s -Tman -o out/gocryptfs-xray.1 Documentation/MANPAGE-XRAY.md
install -Dm644 out/gocryptfs.1      $PKG/usr/share/man/man1/gocryptfs.1
install -Dm644 out/gocryptfs-xray.1 $PKG/usr/share/man/man1/gocryptfs-xray.1

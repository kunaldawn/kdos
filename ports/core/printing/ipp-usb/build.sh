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
go build -mod=vendor -ldflags "-s -w" -o out/ipp-usb .
install -Dm755 out/ipp-usb $PKG/usr/sbin/ipp-usb
install -Dm644 ipp-usb.conf $PKG/etc/ipp-usb/ipp-usb.conf
install -Dm644 ipp-usb-quirks/*.conf -t $PKG/usr/share/ipp-usb/quirks
install -Dm644 ipp-usb.8 -t $PKG/usr/share/man/man8

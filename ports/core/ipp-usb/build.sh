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
# IPP-over-USB IS WHY A MODERN PRINTER NEEDS NO DRIVER ON A CABLE EITHER. The
# device exposes the same IPP service it would over the network, on a USB
# interface; this bridges it to localhost so cups sees a driverless printer.
# libusb is a cgo binding, hence CGO_ENABLED.
mkdir -p out
go build -mod=vendor -o out/ipp-usb .
install -Dm755 out/ipp-usb $PKG/usr/sbin/ipp-usb
install -Dm644 ipp-usb.conf $PKG/etc/ipp-usb/ipp-usb.conf

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

# WHAT THIS SOLVES IS THE BROWSER WARNING, and on an island network there is no
# other answer. Every service worth running here — kiwix, a git forge, a
# printer's admin page — wants https, and the two usual routes are both closed:
# Let's Encrypt needs the internet, and a self-signed certificate per host
# means a warning per host that people learn to click through. step-ca issues
# from ONE root that gets trusted once, and it speaks ACME, so caddy renews
# against it automatically with the same config it would use publicly.
#
# caddy's `tls internal` is the smaller answer for one machine; this is the one
# for a network with several.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step-ca ./cmd/step-ca
install -Dm755 step-ca $PKG/usr/bin/step-ca

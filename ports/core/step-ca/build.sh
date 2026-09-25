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
# for a network with several. The server only runs a ca.json it is given: the
# `step` command (step-cli) creates the CA with `step ca init` and is the
# client for everything after.
#
# cgo is what builds the two hardware key stores, so the CA's root key can
# live off the disk. The PKCS#11 one dlopens whatever module the config names
# and links nothing; the YubiKey PIV one links libpcsclite through pkg-config
# and talks to the card through pcscd. With cgo off both compile out silently
# and a `kms` block naming either fails only when the CA starts.
export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step-ca ./cmd/step-ca
install -Dm755 step-ca $PKG/usr/bin/step-ca

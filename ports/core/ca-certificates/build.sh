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

install -Dm644 cacert-$_version.pem $PKG/etc/ssl/cert.pem

install -d $PKG/etc/ssl/certs
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/certs/ca-certificates.crt
ln -s /etc/ssl/cert.pem $PKG/etc/ssl/ca-bundle.crt

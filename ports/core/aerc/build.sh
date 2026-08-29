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
# notmuch IS the reason this is here rather than mutt: aerc queries a notmuch
# database directly, so mail that is already indexed on the machine is
# searchable from the client with no second index. That binding is cgo, which
# is why CGO_ENABLED is on for this port and off for every other Go one.
make PREFIX=/usr GOFLAGS="-mod=vendor -tags=notmuch"
make PREFIX=/usr DESTDIR=$PKG install

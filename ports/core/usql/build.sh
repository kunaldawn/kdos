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

# BUILT WITH `no_base` PLUS ONLY THE DRIVERS THIS MACHINE CAN REACH. usql
# supports about fifty databases and the default build links every one of
# them — most through cgo — which is fifty drivers for two engines that are
# actually here. postgres and sqlite3 are the ports; mysql costs nothing and
# is what a borrowed dump most often is.
#
# sqlite3's driver is cgo, so CGO_ENABLED stays on for this one — the
# alternative is a pure-Go sqlite that reimplements the engine, which is a
# different set of bugs from the sqlite the rest of this machine uses.
export CGO_ENABLED=1
go build -mod=vendor -tags "no_base postgres sqlite3 mysql" -ldflags "-s -w" -o usql
install -Dm755 usql $PKG/usr/bin/usql

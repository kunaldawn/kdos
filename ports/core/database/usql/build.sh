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
# them — most through cgo — which is fifty drivers for the few engines that are
# actually here. postgres, sqlite3 and duckdb are the ports; mysql costs
# nothing and is what a borrowed dump most often is; csvq is pure Go and runs
# SQL over the CSV files on this disk.
#
# `libsqlite3` links the sqlite port's libsqlite3. Without it the cgo driver
# compiles its own bundled amalgamation, a second sqlite of a different version
# from the one the rest of this machine uses. `duckdb_use_lib` links the duckdb
# port's libduckdb; without it the bindings link a prebuilt glibc archive that
# cannot link against musl.
export CGO_ENABLED=1
go build -mod=vendor \
	-tags "no_base postgres sqlite3 libsqlite3 mysql csvq duckdb duckdb_use_lib" \
	-ldflags "-s -w" -o usql
install -Dm755 usql $PKG/usr/bin/usql

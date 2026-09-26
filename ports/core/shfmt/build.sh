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

# THIS TREE IS ~470 build.sh FILES AND preflight ALREADY RUNS `bash -n` OVER
# EVERY ONE. That catches a syntax error and nothing else; shfmt is the same
# gate one level up — it PARSES rather than sources, so it reports the
# inconsistent indentation a syntax check passes.
# The shellcheck port is the level above that: it analyses what the parsed
# script does — the unquoted expansion, the unchecked `cd`.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.version=v$version" -o shfmt ./cmd/shfmt
install -Dm755 shfmt $PKG/usr/bin/shfmt
install -d $PKG/usr/share/man/man1
scdoc < cmd/shfmt/shfmt.1.scd > $PKG/usr/share/man/man1/shfmt.1

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

# THE `wireshark` DEPENDENCY IS A RUNTIME ONE AND IS NOT OPTIONAL. termshark
# does no dissection itself — it drives `tshark -T pdml` and draws the result —
# so installed without it, it starts, presents an interface list and fails on
# the first capture with a message about a missing binary. The two ship
# together or neither is worth having.
#
# What it buys over tshark alone: a capture is a thing you SCROLL and filter
# interactively rather than a wall of text, which on a character-grid desktop
# is exactly the shape the rest of this system already has.
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w" -o termshark ./cmd/termshark
install -Dm755 termshark $PKG/usr/bin/termshark

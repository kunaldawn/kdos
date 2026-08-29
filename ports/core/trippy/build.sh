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

# IT NEEDS CAP_NET_RAW AND IS NOT GIVEN IT HERE, the same as bandwhich: raw
# sockets are a capability, and this ships as an ordinary binary to be run as
# root or granted the capability deliberately. There is no third setuid program
# on this system and a network probe is not the one to add.
#
# What it answers that ping and traceroute cannot: WHICH hop is losing packets
# and by how much, continuously, which is the question on a link that works
# except when it does not.
cargo build --release --frozen --offline
install -Dm755 target/release/trip $PKG/usr/bin/trip

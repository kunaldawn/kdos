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

# IT NEEDS CAP_NET_RAW AND IS NOT GIVEN IT HERE. Reading packet headers off an
# interface is a capability, and this ships as an ordinary binary that must be
# run as root or given the capability deliberately — there is no third setuid
# program on this system, and a network sniffer is not the one to add. Modern
# bandwhich needs no libpcap, which is why it is a port at all.
cargo build --release --frozen --offline
install -Dm755 target/release/bandwhich $PKG/usr/bin/bandwhich

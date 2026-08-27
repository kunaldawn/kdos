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

# DISCOVERY IS mDNS, WHICH IS WHY avahi IS A DEPENDENCY OF AN INSTRUMENT
# LIBRARY. An LXI device announces itself on the local network the same way a
# printer does; avahi is already running here, so `lxi discover` finds a scope
# somebody plugged in without anybody typing an address.
#
# It also speaks VXI-11, which is ONC RPC and therefore needs libtirpc — the
# older protocol a great many instruments still answer on when the newer raw
# socket does not.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

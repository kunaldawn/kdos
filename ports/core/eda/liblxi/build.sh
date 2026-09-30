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
#
# There is no option for avahi: the build probes for avahi-client/client.h and
# leaves mDNS out without a word when it is missing, giving a library whose
# discover call finds nothing. The check after setup turns that into a stop.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release
grep -q -- -DHAVE_AVAHI build/build.ninja || {
	echo 'liblxi: avahi-client/client.h not found, mDNS discovery would be missing' >&2
	exit 1
}
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# THE COMMS LADDER SKIPPED VOICE, WHICH IS WHAT PEOPLE DEFAULT TO UNDER STRESS.
# This tree can already carry text between machines (syncthing, croc), serve
# documents (kiwix, caddy) and stand up a network with no router (hostapd +
# dnsmasq) — and had no way for two people on that network to TALK. With
# Asterisk in a box or a plain peer-to-peer `sip:user@host`, this closes it.
#
# The codecs are the ones already ported for ffmpeg — opus for speech and vp8
# for video — so this adds a protocol stack and no new media dependency.
# Everything graphical is off by rule: baresip's own interface is a terminal
# menu, which is the right shape for this desktop anyway.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSTATIC=OFF
ninja
DESTDIR=$PKG ninja install

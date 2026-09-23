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

# ffmpeg CAN DO THIS AND THESE ARE STILL WORTH HAVING. `opusenc` exposes the
# encoder's own options — bitrate management, framesize, the `--music`/
# `--speech` hint — that ffmpeg's generic wrapper flattens, and `opusinfo`
# reports what is actually in a stream. On a machine archiving voice recordings
# at 16 kbit/s the difference between the two encoders' defaults is audible.
#
# libpcap has no configure switch: an unconditional library search links it
# when present and gives `opusrtp --extract` its pcap reader. It is in
# depends so that reader does not come and go with build order.
./configure --prefix=/usr --disable-static --with-flac
make
make DESTDIR=$PKG install

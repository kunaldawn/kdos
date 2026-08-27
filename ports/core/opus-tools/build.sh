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
./configure --prefix=/usr --disable-static
make
make DESTDIR=$PKG install

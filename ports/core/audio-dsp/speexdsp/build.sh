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

# WIRESHARK MAKES IT REQUIRED, not optional: FindSpeexDSP.cmake is a hard
# find_package, so a capture tool refuses to configure without a resampler.
# What it buys is the RTP player — following a VoIP call in a capture and
# hearing it — which needs the streams resampled to a common rate.
#
# The speex CODEC is a separate tarball and is NOT here: it is a deprecated
# narrowband codec that opus replaced, and nothing in this tree encodes it.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

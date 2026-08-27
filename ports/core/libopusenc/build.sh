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

# libopus ENCODES A FRAME; THIS ENCODES A FILE. The difference is everything
# opus-tools needs and libopus deliberately does not do: Ogg muxing, comment
# headers, the encoder-delay bookkeeping that makes a decoded file line up
# sample-for-sample with the input, and gapless chaining. Without it opusenc
# does not build.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

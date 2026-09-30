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

# THE NEURAL DECODER TOOLS ARE BUILT IN, AND THEIR WEIGHTS SHIP IN THE
# RELEASE ARCHIVE, so nothing is fetched. --enable-deep-plc conceals lost
# packets with a model instead of repeating the last one, --enable-dred
# decodes the deep redundancy a sender may add to a stream, and
# --enable-osce enhances low-bitrate speech. Each is chosen per stream or by
# decoder complexity, so a caller that asks for none of them decodes as it
# otherwise would; the cost is the weights' size in libopus.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doc \
	--disable-extra-programs \
	--enable-deep-plc \
	--enable-dred \
	--enable-osce
make
make DESTDIR=$PKG install

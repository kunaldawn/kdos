#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro

# ONE COPY ON THE MACHINE. mupdf and ghostscript each vendor jbig2dec, and two
# copies means only one of them gets a security fix — which matters more here
# than for most codecs, because JBIG2 is what a scanned page in a PDF is
# encoded with and the decoder is reading somebody else's file.
#
# --with-libpng is for the CLI: the library itself does not need it, but the
# CLI writes PNG and that is the half a scriptable machine uses to look at a
# page it cannot render. The switch only asks — configure still drops PNG
# output quietly if libpng or zlib fails to link, which is why both are
# `depends`.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --with-libpng
make
make DESTDIR=$PKG install

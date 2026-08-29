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
# --without-libpng would drop jbig2dec's own test harness output; the library
# itself does not need it, but the CLI writes PNG and that is the half a
# scriptable machine uses to look at a page it cannot render.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

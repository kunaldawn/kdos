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


# THE DECODERS ARE PYTHON AND THAT IS NOT AN OVERSIGHT. Each protocol — I2C,
# SPI, UART, CAN, USB, one-wire, a hundred more — is a small python module, so
# adding one for a chip nobody has decoded is writing a file rather than
# patching C. The cost is that python3 is a RUNTIME dependency of sigrok-cli,
# which is the trade upstream made and there is no C decoder set to switch to.
# THE SHARED LIBRARY MUST RECORD ITS OWN DEPENDENCY ON libpython. Since 3.8,
# `python3.pc` deliberately carries no -lpython and only the `-embed` variant
# does; configure.ac's probe list stops at python-3.8-embed and then falls
# through to plain python3, so libsigrokdecode.so is linked with no DT_NEEDED
# for libpython at all. It builds and installs — and every consumer then fails
# at link time on PyBytes_AsStringAndSize, naming a library it never mentions.
export LDFLAGS="$LDFLAGS $(pkg-config --libs python3-embed)"

./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

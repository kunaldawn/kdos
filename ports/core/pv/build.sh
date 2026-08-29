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

# THE RELEASE TARBALL, NOT THE TAG ARCHIVE — the fourth port in this tree to
# need saying (bcc, yosys, rizin and here). pv keeps its autoconf inputs under
# `autoconf/` and generates the top-level `configure` at release time, so a tag
# archive has no `configure.ac` where autoreconf looks for one and the build
# stops before it starts.
./configure --prefix=/usr --disable-static
make
make DESTDIR=$PKG install

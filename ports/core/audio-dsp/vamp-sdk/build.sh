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

# The programs are vamp-simple-host and vamp-rdf-template-generator, the two
# command-line tools for running and describing a plugin; they read audio
# through libsndfile.
./configure --prefix=/usr --libdir=/usr/lib --enable-programs

# The makefile builds the SDK, the host SDK and the example plugins from
# shared object lists and races itself under a parallel make.
make -j1
make -j1 DESTDIR=$PKG install

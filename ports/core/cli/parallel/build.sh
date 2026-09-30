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

./configure --prefix=/usr --docdir=/usr/share/doc/parallel
make
make DESTDIR=$PKG install

# `parallel --citation` otherwise blocks the first interactive run on a prompt
# that wants the word "will cite" typed at it, which in a script is a hang with
# no output. The file is what that prompt writes.
install -d $PKG/etc/parallel
: > $PKG/etc/parallel/will-cite

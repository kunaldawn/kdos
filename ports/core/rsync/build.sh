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

# rrsync is a python3 script: an authorized_keys forced command that confines
# a key to rsync within one directory, which is what makes an ssh key safe to
# hand to a backup job. python3 on the depends line is what runs it.
./configure \
    --prefix=/usr \
    --with-included-zlib=no \
    --enable-acl-support \
    --enable-xattr-support \
    --with-rrsync
make
make DESTDIR=$PKG install

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

# THE MINIZIP IN zlib's contrib/, built from the zlib release it ships in: the
# <minizip/unzip.h> and -lminizip that KeePassXC and the like expect. The
# zlib-ng fork is the separate minizip-ng port, which installs under its own
# names so the two do not collide.
#
# Two patches, both Alpine's. install-ints-header.patch adds ints.h to the
# installed headers, because ioapi.h includes it and a consumer's
# #include <minizip/ioapi.h> otherwise fails. zlib-1.2.8-minizip-include.patch
# makes crypt.h include zconf.h for the z_crc_t it uses. The first edits
# Makefile.am, so the build system is regenerated after it.
cd contrib/minizip
patch -p1 -i "$PORT_SRC/install-ints-header.patch"
patch -p1 -i "$PORT_SRC/zlib-1.2.8-minizip-include.patch"
autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install

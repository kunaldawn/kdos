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

# THE PATCH IS A RULE-7 CASE: no flag answers a #error. formats.c reaches into
# the FILE struct to rewind a pipe and knows three libcs' layouts; musl's FILE
# is opaque, so it lands in the #else that stops the build outright. Upstream's
# own comment there gives the answer the patch takes.
patch -p1 -i "$PORT_SRC/musl-rewind-pipe.patch"

# A 2015 autoconf against GCC 15: its conftests are K&R and every one is
# rejected, which configure reports as a broken compiler.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch"
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-distro=KDOS --without-ffmpeg
make
make DESTDIR=$PKG install

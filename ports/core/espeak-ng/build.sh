#!/bin/bash


./autogen.sh

# A SYNTHESISER, NOT A SCREEN READER — the distinction matters because it is
# what BRLTTY is for, and installing this alone gives a machine a voice with
# nothing driving it. What it buys on its own is the corpus read aloud:
# `espeak-ng -f notes.txt` on a machine whose 49 GB of documentation is
# otherwise only readable by somebody whose eyes are free.
#
# --with-mbrola=no keeps the diphone voices out: they are separate downloads
# per language, which on an offline distro is a feature that would ship broken.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-mbrola=no \
	--with-sonic=no \
	--with-pcaudiolib=yes
make
make DESTDIR=$PKG install

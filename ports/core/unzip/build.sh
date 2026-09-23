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

# linux_noasm never runs unix/configure, so nothing is detected: ZIP64 and
# large files, UTF-8 member names and the bzip2 method exist only because they
# are named here. Without them an archive over 4 GB fails and a non-ASCII name
# comes out mangled.
make -f unix/Makefile linux_noasm \
	D_USE_BZ2=-DUSE_BZIP2 L_BZ2=-lbz2 \
	LOCAL_UNZIP="-DBSD -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DLARGE_FILE_SUPPORT -DUNICODE_SUPPORT -DUNICODE_WCHAR -DUTF8_MAYBE_NATIVE -include time.h" \
	prefix=$PKG/usr

make prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 -f unix/Makefile install

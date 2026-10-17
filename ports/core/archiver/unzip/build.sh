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

for p in "$PORT_SRC"/[0-9][0-9]-*.patch; do
	patch -p1 -i "$p"
done

make -f unix/Makefile unzips CC="$CC" LD="$CC" \
	CFLAGS="$CFLAGS" LF2="$LDFLAGS" \
	D_USE_BZ2=-DUSE_BZIP2 L_BZ2=-lbz2 \
	LOCAL_UNZIP="-DBSD -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DLARGE_FILE_SUPPORT -DUNICODE_SUPPORT -DUNICODE_WCHAR -DUTF8_MAYBE_NATIVE -include time.h" \
	prefix=$PKG/usr

make prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 -f unix/Makefile install

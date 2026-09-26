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

# Info-ZIP has made no release since 6.0, so every security fix since 2009
# exists only as a distribution patch: the NN-*.patch series beside this recipe
# is Alpine's, in Alpine's order, which the numbers keep. It carries the
# heap-overflow CVE fixes and the overlapping-entry zip-bomb check, which
# refuses such an archive unless UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE is set.
# Each patch is a diff from the top of the tree and depends on the ones before
# it.
for p in "$PORT_SRC"/[0-9][0-9]-*.patch; do
	patch -p1 -i "$p"
done

# linux_noasm never runs unix/configure, so nothing is detected: ZIP64 and
# large files, UTF-8 member names and the bzip2 method exist only because they
# are named here. Without them an archive over 4 GB fails and a non-ASCII name
# comes out mangled.
make -f unix/Makefile linux_noasm \
	D_USE_BZ2=-DUSE_BZIP2 L_BZ2=-lbz2 \
	LOCAL_UNZIP="-DBSD -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DLARGE_FILE_SUPPORT -DUNICODE_SUPPORT -DUNICODE_WCHAR -DUTF8_MAYBE_NATIVE -include time.h" \
	prefix=$PKG/usr

make prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 -f unix/Makefile install

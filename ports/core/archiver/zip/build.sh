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

ZIP_CC="${CC:-cc} -Wno-implicit-function-declaration"
make -f unix/Makefile flags CC="$ZIP_CC" LOCAL_ZIP="$CFLAGS"
grep -q -- -DBZIP2_SUPPORT flags || {
	echo "zip: bzip2 probe failed — flags lack -DBZIP2_SUPPORT" >&2
	exit 1
}
make -f unix/Makefile generic CC="$ZIP_CC" BIND="$ZIP_CC $LDFLAGS" LOCAL_ZIP="$CFLAGS"

make -f unix/Makefile prefix=$PKG/usr MANDIR=$PKG/usr/share/man/man1 install

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

patch -p1 -i "$PORT_SRC/gsm-64bit.patch"

_ccflags="-c $CFLAGS -fPIC -DNeedFunctionPrototypes=1 -Wall -Wno-comment"
make CC=gcc CCFLAGS="$_ccflags" all
gcc -shared $LDFLAGS -Wl,-soname,libgsm.so.1 -o lib/libgsm.so.$version \
	-Wl,--whole-archive lib/libgsm.a -Wl,--no-whole-archive

install -Dm755 bin/toast "$PKG/usr/bin/toast"
ln -s toast "$PKG/usr/bin/untoast"
ln -s toast "$PKG/usr/bin/tcat"
install -Dm755 lib/libgsm.so.$version "$PKG/usr/lib/libgsm.so.$version"
ln -s libgsm.so.$version "$PKG/usr/lib/libgsm.so.1"
ln -s libgsm.so.$version "$PKG/usr/lib/libgsm.so"

install -Dm644 inc/gsm.h "$PKG/usr/include/gsm/gsm.h"
ln -s gsm/gsm.h "$PKG/usr/include/gsm.h"

install -Dm644 -t "$PKG/usr/share/man/man1" man/toast.1
install -Dm644 -t "$PKG/usr/share/man/man3" man/gsm.3 man/gsm_explode.3 \
	man/gsm_option.3 man/gsm_print.3
ln -s toast.1 "$PKG/usr/share/man/man1/untoast.1"
ln -s toast.1 "$PKG/usr/share/man/man1/tcat.1"

test -x "$PKG/usr/bin/toast"

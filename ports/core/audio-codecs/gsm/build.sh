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

# The coder's state is declared in `long` and assumes 32 bits; on a 64-bit
# target the arithmetic saturates differently and the bitstream is not GSM.
patch -p1 -i "$PORT_SRC/gsm-64bit.patch"

# The makefile's CFLAGS is its own composite and beats the environment's, so
# the tree's flags go in through CCFLAGS. Its compiler name carries -ansi,
# which would contradict the tree's -std=gnu11. -fPIC lets the archive's
# objects make the shared library, which the makefile has no rule for.
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

# Consumers look for both <gsm.h> and <gsm/gsm.h>.
install -Dm644 inc/gsm.h "$PKG/usr/include/gsm/gsm.h"
ln -s gsm/gsm.h "$PKG/usr/include/gsm.h"

install -Dm644 -t "$PKG/usr/share/man/man1" man/toast.1
install -Dm644 -t "$PKG/usr/share/man/man3" man/gsm.3 man/gsm_explode.3 \
	man/gsm_option.3 man/gsm_print.3
ln -s toast.1 "$PKG/usr/share/man/man1/untoast.1"
ln -s toast.1 "$PKG/usr/share/man/man1/tcat.1"

test -x "$PKG/usr/bin/toast"

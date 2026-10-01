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

# PREFIX is baked into the wrapper script at BUILD time, not read at run time,
# so it must be the installed path while DESTDIR carries the staging root.
# ONE RULE IN THE MAKEFILE OMITS -std=c99 — dndblast.c is compiled bare — and
# under GCC 15's default C23 an old-style `extern void cpmx_calc();` declares
# `(void)`, so mltaln.h and functions.h disagree about every function and the
# file fails on thirty "conflicting types". -std=gnu17 restores the meaning of
# an empty parameter list.
#
# CFLAGS IS PASSED ON THE COMMAND LINE, which beats the Makefile's own
# `CFLAGS = -O3`: that line is the whole of it, carrying no -D the source
# reads, so the exported flags go in with -O2 raised to upstream's -O3.
# mxscarna's `CXXFLAGS = -O3` is replaced the same way; its NDEBUG and
# -std=c++98 sit in OFLAGS and are kept. Its link reads LIBS, never LDFLAGS,
# and LIBS holds only -L paths to directories with no library in them, so the
# exported LDFLAGS replace it.
#
# ENABLE_ATOMIC switches the thread counters to C11 atomic_int; the -std=c11
# it adds is overridden by the -std=gnu17 that follows it on the line.
# extensions/ is mxscarnamod, the RNA structural aligner behind --mxscarna,
# Q-INS-i and X-INS-i; without it those strategies stop at run time asking
# for it to be built.
make -C core PREFIX=/usr ENABLE_ATOMIC=-Denableatomic CFLAGS="${CFLAGS/-O2/-O3} -std=gnu17"
make -C core PREFIX=/usr ENABLE_ATOMIC=-Denableatomic DESTDIR=$PKG install
make -C extensions PREFIX=/usr CXXFLAGS="${CXXFLAGS/-O2/-O3}" LIBS="$LDFLAGS"
make -C extensions PREFIX=/usr DESTDIR=$PKG install

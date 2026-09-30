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

# prebuilt/ carries bytecode for the evaluator, psyntax and boot-9 that
# upstream compiled; with it on the load path stage0 would bootstrap from
# objects nobody here built. Removing it makes stage0 compile them through
# the C interpreter, which is most of the build's time. prebuilt/ is not a
# build subdirectory, so nothing asks for the removed files again.
# The base CFLAGS' -std=gnu11 is what libguile's empty-parameter scm_t_subr
# needs: under C23 that declarator means (void) and every subr call fails to
# compile.
# --disable-error-on-warning keeps a newer compiler's new warnings from
# failing the build.
find prebuilt -name '*.go' -delete
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-error-on-warning \
	--disable-nls \
	--disable-static
make
make DESTDIR=$PKG install

# The GDB helper is installed as libguile-3.0.so.<ver>-gdb.scm beside the
# library, where ldconfig takes it for a shared object and warns on every run.
rm -f "$PKG"/usr/lib/libguile-3.0.so.*-gdb.scm

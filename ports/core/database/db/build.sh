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

# atomic.patch renames the library's own __atomic_compare_exchange, a name GCC
# reserves for its builtin; with the original the atomic header does not
# compile.
patch -p1 -i "$PORT_SRC/atomic.patch"

# The 2013 configure probes with implicit declarations and implicit int, which
# GCC 14 and later reject. -fpermissive turns them back into warnings;
# without it a probe that fails to compile reads as a missing feature.
export CFLAGS="$CFLAGS -fpermissive"

# The build runs from build_unix, the directory upstream's configure expects
# to be run from. compat185 installs db_185.h for programs written against the
# 1.85 API; the C++ binding is libdb_cxx.
cd build_unix
../dist/configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--enable-compat185 \
	--enable-cxx
make

# install_docs is left out: it writes 94 MB of HTML, including the Java and C#
# API references for bindings that are not built, under /usr/docs.
make DESTDIR=$PKG install_include install_lib install_utilities
install -Dm644 ../LICENSE "$PKG/usr/share/licenses/db/LICENSE"
test -e "$PKG/usr/include/db.h"
test -e "$PKG/usr/lib/libdb.so"

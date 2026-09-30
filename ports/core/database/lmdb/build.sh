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

# The library lives in one subdirectory of the OpenLDAP tree and has no
# configure step. The patch gives the shared library a soname and links the
# tools against it; without it every consumer records an unversioned
# liblmdb.so and the static archive is installed as well.
cd libraries/liblmdb
patch -p1 -i "$PORT_SRC/lmdb-make.patch"

make CC="$CC" OPT="-O2" XCFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
make DESTDIR="$PKG" prefix=/usr install

# Upstream ships no pkg-config file; nheko and its lmdbxx look for one.
install -d "$PKG/usr/lib/pkgconfig"
cat > "$PKG/usr/lib/pkgconfig/lmdb.pc" <<PC
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: liblmdb
Description: Lightning Memory-mapped key-value database
URL: https://www.symas.com/mdb
Version: $version
Libs: -L\${libdir} -llmdb
Cflags: -I\${includedir}
PC
chmod 644 "$PKG/usr/lib/pkgconfig/lmdb.pc"

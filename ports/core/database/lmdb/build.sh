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

cd libraries/liblmdb
patch -p1 -i "$PORT_SRC/lmdb-make.patch"

make CC="$CC" OPT="-O2" XCFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
make DESTDIR="$PKG" prefix=/usr install

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

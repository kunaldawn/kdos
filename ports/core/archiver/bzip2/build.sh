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

make -f Makefile-libbz2_so CC="gcc $LDFLAGS" CFLAGS="$CFLAGS -fPIC"
make clean
make CC="gcc $LDFLAGS" CFLAGS="$CFLAGS"
make PREFIX=$PKG/usr install

install -Dm755 libbz2.so.$version -t $PKG/usr/lib
ln -sf libbz2.so.$version $PKG/usr/lib/libbz2.so.1.0
ln -sf libbz2.so.$version $PKG/usr/lib/libbz2.so
install -m755 bzip2-shared $PKG/usr/bin/bzip2
ln -sf bzip2 $PKG/usr/bin/bunzip2
ln -sf bzip2 $PKG/usr/bin/bzcat

install -Dm644 /dev/stdin $PKG/usr/lib/pkgconfig/bzip2.pc <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: bzip2
Description: Block-sorting file compression library
Version: $version
Libs: -L\${libdir} -lbz2
Cflags: -I\${includedir}
EOF

install -d $PKG/usr/share
mv $PKG/usr/man $PKG/usr/share/man
for l in bzegrep:bzgrep bzfgrep:bzgrep bzless:bzmore bzcmp:bzdiff; do
	ln -sf ${l#*:} $PKG/usr/bin/${l%:*}
done

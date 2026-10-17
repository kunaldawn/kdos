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


patch -p1 -i "$PORT_SRC/gcc-headers.patch"
patch -p1 -i "$PORT_SRC/reproducible-timestamp.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-java \
	--enable-cxx \
	--disable-python \
	--disable-csharp \
	--disable-examples

make

mkdir jar-tree
( cd jar-tree && unzip -q ../antlr/antlr.jar && find . -type f ! -path ./META-INF/MANIFEST.MF | LC_ALL=C sort > ../jar.list )
rm antlr/antlr.jar
( cd jar-tree && jar --create --file ../antlr/antlr.jar --manifest META-INF/MANIFEST.MF \
	--date="$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ')" @../jar.list )

make prefix=$PKG/usr libdir=$PKG/usr/lib install

rm -rf $PKG/usr/share/doc
rm -f $PKG/usr/share/antlr-$version/antlr.jar

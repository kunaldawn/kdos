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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-man-pdfs \
	--enable-ltdl \
	--disable-ltdl-install \
	--without-included-ltdl \
	--enable-swig=no \
	--enable-sharp=no --enable-d=no --enable-go=no --enable-guile=no \
	--enable-java=no --enable-javascript=no --enable-lua=no \
	--enable-perl=no --enable-php=no --enable-python=no \
	--enable-python3=no --enable-r=no --enable-ruby=no --enable-tcl=no \
	--without-x \
	--with-expat=yes \
	--with-pangocairo=yes \
	--with-freetype2=yes \
	--with-libgd=yes \
	--with-webp=yes \
	--with-rsvg=yes \
	--with-gdk-pixbuf=no \
	--with-poppler=yes \
	--with-ghostscript=no \
	--with-lasi=no \
	--with-gdk=no \
	--with-gtk=no --with-gtkgl=no --with-gtkglext=no --with-glade=no \
	--with-qt=no \
	--with-glut=no \
	--with-smyrna=no \
	--with-gts=yes \
	--with-ann=no \
	--with-devil=no \
	--with-aalib=no \
	--with-sfdp=yes \
	--with-ortho=yes \
	--with-digcola=yes \
	--with-ipsepcola=yes
make
make DESTDIR=$PKG install
for p in core dot_layout neato_layout pango gd rsvg poppler webp; do
	test -e "$PKG/usr/lib/graphviz/libgvplugin_$p.so"
done
readelf -d "$PKG/usr/lib/graphviz/libgvplugin_neato_layout.so" | grep -q 'NEEDED.*\[libgts-'
GVBINDIR="$PKG/usr/lib/graphviz" LD_LIBRARY_PATH="$PKG/usr/lib" \
	"$PKG/usr/bin/dot" -c
test -s "$PKG/usr/lib/graphviz/config8"

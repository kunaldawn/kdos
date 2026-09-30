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

# Every renderer below is a pkg-config probe that drops its plugin without a
# word when the library is missing, even when asked for with =yes. The list
# of plugins checked after the install is what stops such a build.
#
# No language bindings, no GUI: gvedit is a Qt program, smyrna needs GTK and
# GLUT, and the swig bindings for Python, Lua and Tcl have no consumer here.
# No X11, which only adds the -Txlib preview window; the ghostscript plugin
# renders through Xrender, so it goes with it. The gdk plugin is GTK 3 for
# what the gd, rsvg and poppler plugins already load.
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
	--with-gts=no \
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

# THE PLUGIN INDEX IS WRITTEN HERE, not on the target. Upstream's install hook
# runs `dot -c` only when DESTDIR is empty, and without the index dot loads no
# plugin and knows no output format. Every plugin is in this package, so the
# index is fixed at build time; GVBINDIR points dot at the staged plugins.
GVBINDIR="$PKG/usr/lib/graphviz" LD_LIBRARY_PATH="$PKG/usr/lib" \
	"$PKG/usr/bin/dot" -c
test -s "$PKG/usr/lib/graphviz/config8"

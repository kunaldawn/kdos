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

mkdir nons ns
tar xf "$PORT_SRC/$name-$version.tar.bz2" --strip-components=1 -C nons
tar xf "$PORT_SRC/$name-ns-$version.tar.bz2" --strip-components=1 -C ns
patch -d nons -p1 -i "$PORT_SRC/string-subst.patch"
patch -d ns -p1 -i "$PORT_SRC/string-subst.patch"

dest=$PKG/usr/share/xml/docbook/xsl-stylesheets-nons-$version
install -v -m755 -d $dest $dest/webhelp
( cd nons && cp -v -R VERSION assembly common eclipse epub epub3 fo        \
         highlighting html htmlhelp images javahelp lib manpages params    \
         profiling roundtrip slides template tests website xhtml xhtml-1_1 \
         xhtml5 $dest
  cp -v -R webhelp/xsl $dest/webhelp/ )
ln -s VERSION $dest/VERSION.xsl

nsdest=$PKG/usr/share/xml/docbook/xsl-stylesheets-$version
install -v -m755 -d $nsdest $nsdest/webhelp
cp -v -R ns/VERSION ns/VERSION.xsl ns/assembly ns/common ns/eclipse ns/epub \
         ns/epub3 ns/fo ns/highlighting ns/html ns/htmlhelp ns/images      \
         ns/javahelp ns/lib ns/manpages ns/params ns/profiling ns/roundtrip \
         ns/slides ns/template ns/tests ns/website ns/xhtml ns/xhtml-1_1    \
         ns/xhtml5 \
    $nsdest
cp -v -R ns/webhelp/xsl $nsdest/webhelp/

install -v -m755 -d $PKG/etc/xml
xmlcatalog --noout --create $PKG/etc/xml/catalog

xmlcatalog --noout --add "delegatePublic" \
    "-//OASIS//ENTITIES DocBook XML" \
    "file:///etc/xml/docbook" \
    $PKG/etc/xml/catalog

xmlcatalog --noout --add "delegatePublic" \
    "-//OASIS//DTD DocBook XML" \
    "file:///etc/xml/docbook" \
    $PKG/etc/xml/catalog

xmlcatalog --noout --add "delegateSystem" \
    "http://www.oasis-open.org/docbook/" \
    "file:///etc/xml/docbook" \
    $PKG/etc/xml/catalog

xmlcatalog --noout --add "delegateURI" \
    "http://www.oasis-open.org/docbook/" \
    "file:///etc/xml/docbook" \
    $PKG/etc/xml/catalog

for ver in $version current; do
	for base in http://docbook.sourceforge.net/release/xsl \
	            http://cdn.docbook.org/release/xsl-nons \
	            https://cdn.docbook.org/release/xsl-nons; do
		for kind in rewriteSystem rewriteURI; do
			xmlcatalog --noout --add "$kind" \
			    "$base/$ver" \
			    "/usr/share/xml/docbook/xsl-stylesheets-nons-$version" \
			    $PKG/etc/xml/catalog
		done
	done
done
for ver in $version current; do
	for base in http://docbook.sourceforge.net/release/xsl-ns \
	            http://cdn.docbook.org/release/xsl \
	            https://cdn.docbook.org/release/xsl; do
		for kind in rewriteSystem rewriteURI; do
			xmlcatalog --noout --add "$kind" \
			    "$base/$ver" \
			    "/usr/share/xml/docbook/xsl-stylesheets-$version" \
			    $PKG/etc/xml/catalog
		done
	done
done

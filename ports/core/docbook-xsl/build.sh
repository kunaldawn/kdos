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

for i in xsl-stylesheets xsl-stylesheets-nons; do
	install -v -m755 -d $PKG/usr/share/xml/docbook/$i-$version	
	cp -v -R VERSION assembly common eclipse epub epub3 extensions fo        \
	         highlighting html htmlhelp images javahelp lib manpages params  \
	         profiling roundtrip slides template tests tools webhelp website \
	         xhtml xhtml-1_1 xhtml5                                          \
	    $PKG/usr/share/xml/docbook/$i-$version	
	ln -s VERSION $PKG/usr/share/xml/docbook/$i-$version/VERSION.xsl
done

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
	xmlcatalog --noout --add "rewriteSystem" \
	    "http://docbook.sourceforge.net/release/xsl/$ver" \
	    "/usr/share/xml/docbook/xsl-stylesheets-$version" \
	    $PKG/etc/xml/catalog

	xmlcatalog --noout --add "rewriteURI" \
	    "http://docbook.sourceforge.net/release/xsl/$ver" \
	    "/usr/share/xml/docbook/xsl-stylesheets-$version" \
	    $PKG/etc/xml/catalog
done

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

# ONE COPY. The tarball is the nons release, which is what both the
# sourceforge release/xsl URIs and the cdn xsl-nons URIs name, so the catalog
# resolves every one of them into this tree; a second copy under another
# directory would be the same files twice. The namespaced release (xsl-ns,
# cdn release/xsl) is not shipped and has no rewrite.
# extensions/ and tools/ are prebuilt Java jars for saxon, xalan and the
# webhelp indexer, and webhelp's template/ and docs/ are minified jQuery and a
# rendered sample — none of it is read by xsltproc, which only needs the
# stylesheets. webhelp/xsl stays so the stylesheet set is complete.
dest=$PKG/usr/share/xml/docbook/xsl-stylesheets-nons-$version
install -v -m755 -d $dest $dest/webhelp
cp -v -R VERSION assembly common eclipse epub epub3 fo highlighting html  \
         htmlhelp images javahelp lib manpages params profiling roundtrip \
         slides template tests website xhtml xhtml-1_1 xhtml5             \
    $dest
cp -v -R webhelp/xsl $dest/webhelp/
ln -s VERSION $dest/VERSION.xsl

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

# Projects name the stylesheets by either base and either scheme; a URI no
# rewrite matches is fetched from the network, and xsltproc --nonet fails.
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

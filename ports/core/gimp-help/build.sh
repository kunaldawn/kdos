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


# ALL_LINGUAS=en builds the English manual only; every other language is a
# second full DocBook render and its own copy of the screenshots. With en alone
# configure turns translation off, so no gettext tool is needed.
#
# --without-gimp installs into $(datadir)/gimp/3.0/help, the directory GIMP 3
# reads its manual from, without asking pkg-config for a GIMP that need not be
# built first. XSLTFLAGS carries --nonet: the stylesheet URIs resolve through
# docbook-xsl's catalog in /etc/xml, and one that did not would fail the render
# rather than fetch. The screenshots are linked into the tree by a Perl script
# and copied into the package whole by the install.
./configure --prefix=/usr --without-gimp ALL_LINGUAS=en
make
make DESTDIR=$PKG install
[ -f "$PKG/usr/share/gimp/3.0/help/en/index.html" ] ||
	{ echo "gimp-help: the English manual was not rendered" >&2; exit 1; }
[ -n "$(ls -A "$PKG/usr/share/gimp/3.0/help/en/images")" ] ||
	{ echo "gimp-help: the manual was installed without its images" >&2; exit 1; }

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


./configure --prefix=/usr --without-gimp ALL_LINGUAS=en
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/gimp/3.0/help/pdf"
chmod -R u=rwX,go=rX "$PKG/usr/share/gimp"
[ -f "$PKG/usr/share/gimp/3.0/help/en/index.html" ] ||
	{ echo "gimp-help: the English manual was not rendered" >&2; exit 1; }
[ -n "$(ls -A "$PKG/usr/share/gimp/3.0/help/en/images")" ] ||
	{ echo "gimp-help: the manual was installed without its images" >&2; exit 1; }

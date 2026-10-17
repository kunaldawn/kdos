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

cd chemical-mime-data
intltoolize --force --copy --automake
autoreconf -fi
./configure \
	--prefix=/usr \
	--disable-update-database \
	--without-gnome-mime \
	--without-kde-mime \
	--without-hicolor-theme \
	RSVG=true
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/doc"
install -Dm644 icons/hicolor/gnome-mime-chemical.svg \
	"$PKG/usr/share/icons/hicolor/scalable/mimetypes/gnome-mime-chemical.svg"
grep -q 'chemical/x-cml' "$PKG/usr/share/mime/packages/chemical-mime-data.xml"

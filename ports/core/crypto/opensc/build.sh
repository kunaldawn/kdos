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

patch -p1 -i "$PORT_SRC/openssl4.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-openssl \
	--enable-zlib \
	--enable-readline \
	--enable-pcsc \
	--enable-sm \
	--enable-man \
	--disable-doc \
	--disable-notify \
	--disable-openpace \
	--disable-autostart-items \
	--disable-tests \
	--disable-cmocka \
	--with-xsl-stylesheetsdir=/usr/share/xml/docbook/xsl-stylesheets-nons-1.79.2 \
	--with-pcsc-provider=libpcsclite.so.1
make
make DESTDIR=$PKG install
rm "$PKG/usr/share/applications/org.opensc.notify.desktop" \
	"$PKG/usr/share/bash-completion/completions/opensc-notify" \
	"$PKG/usr/share/man/man1/opensc-notify.1"

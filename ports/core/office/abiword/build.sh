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

patch -p1 -i "$PORT_SRC/enchant-2-pkgconfig.patch"
patch -p1 -i "$PORT_SRC/enchant.patch"
patch -p1 -i "$PORT_SRC/musl-1.2.3.patch"
./autogen-common.sh
autoreconf -fi

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-spell \
	--enable-print \
	--enable-plugins="applix bmp clarisworks command docbook eml epub gimp goffice hancom hrtext iscii kword latex loadbindings mht mif mswrite opendocument openwriter openxml opml paint passepartout pdb pdf presentation s5 sdw t602 wml wordperfect wpg xslfo" \
	--with-goffice \
	--with-gio \
	--without-gnomevfs \
	--without-redland \
	--without-evolution-data-server \
	--without-libical \
	--without-champlain \
	--disable-introspection
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/abiword.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=AbiWord
GenericName=Word Processor
Comment=Compose, edit and view documents
TryExec=abiword
Exec=abiword %U
Icon=abiword
Terminal=false
StartupNotify=true
Categories=Office;WordProcessor;GTK;
MimeType=application/x-abiword;
Keywords=word;processor;document;doc;rtf;odt;abiword;
DESKTOP
chmod 644 "$PKG/usr/share/applications/abiword.desktop"

find "$PKG/usr/share/abiword-${version%.*}/strings" -name '*.strings' ! -name 'en*' -delete

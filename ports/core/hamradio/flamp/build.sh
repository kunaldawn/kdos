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

patch -p1 -i "$PORT_SRC/x11-hints-on-x11-only.patch"
patch -p1 -i "$PORT_SRC/icon-pixmap-on-x11-only.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--with-flxmlrpc
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/applications/flamp.desktop"
s=$(magick identify -format %w "data/flamp.xpm")
install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
magick "data/flamp.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/flamp.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/flamp.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flamp
GenericName=Amateur Multicast File Transfer
Comment=Send files to many stations at once over an fldigi link
Exec=flamp
Icon=flamp
Terminal=false
StartupWMClass=flamp
Categories=Network;HamRadio;
Keywords=ham;radio;amp;multicast;file;transfer;fldigi;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flamp.desktop"

test -x "$PKG/usr/bin/flamp"

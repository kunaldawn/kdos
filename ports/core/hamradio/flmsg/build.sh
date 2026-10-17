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
patch -p1 -i "$PORT_SRC/font-browser-pthread.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--with-flxmlrpc
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/applications/flmsg.desktop"
s=$(magick identify -format %w "data/flmsg.xpm")
install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
magick "data/flmsg.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/flmsg.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/flmsg.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flmsg
GenericName=Amateur Radio Message Forms
Comment=Compose and read ICS, Radiogram and other standard message forms
Exec=flmsg
Icon=flmsg
Terminal=false
StartupWMClass=flmsg
Categories=Network;HamRadio;
Keywords=ham;radio;ics;radiogram;message;forms;emcomm;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flmsg.desktop"

test -x "$PKG/usr/bin/flmsg"

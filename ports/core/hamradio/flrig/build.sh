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

patch -p1 -i "$PORT_SRC/no-error-h.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/applications/flrig.desktop"
s=$(magick identify -format %w "data/flrig.xpm")
install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
magick "data/flrig.xpm" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/flrig.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/flrig.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=flrig
GenericName=Amateur Radio Transceiver Control
Comment=Control a transceiver's frequency, mode, filters and power over CAT
Exec=flrig
Icon=flrig
Terminal=false
StartupWMClass=Flrig
Categories=Network;HamRadio;
Keywords=ham;radio;rig;cat;transceiver;control;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flrig.desktop"

test -x "$PKG/usr/bin/flrig"

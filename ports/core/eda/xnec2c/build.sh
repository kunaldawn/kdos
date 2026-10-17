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

export CFLAGS="$CFLAGS -g0"

autoreconf -fi
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-nls
make
make DESTDIR=$PKG install

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/xnec2c.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/xnec2c.png"
done
cat > "$PKG/usr/share/applications/xnec2c.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=xnec2c
GenericName=Antenna Simulator
Comment=Model antennas with NEC2: radiation patterns, impedance and SWR sweeps
Exec=xnec2c %f
Icon=xnec2c
Terminal=false
StartupWMClass=xnec2c
MimeType=application/x-nec2;
Categories=Science;Electronics;HamRadio;
Keywords=nec2;antenna;simulator;swr;radiation;pattern;ham;radio;
DESKTOP
chmod 644 "$PKG/usr/share/applications/xnec2c.desktop"

test -x "$PKG/usr/bin/xnec2c"
test -e "$PKG/usr/share/mime/packages/x-nec2.xml"

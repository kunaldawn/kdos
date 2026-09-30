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

# The release tarball carries no configure script. autoreconf needs autopoint
# for the gettext macros. The linear-algebra backend (OpenBLAS) is dlopened at
# run time when the user picks it, and is installed beside it so that choice
# works.
autoreconf -fi
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-nls
make
make DESTDIR=$PKG install

# THE MENU: the SVG is rasterised for the sizes the panel reads, and the entry
# gains the window class. The window is a plain GtkWindow, so the Wayland
# app_id is the program name. files/x-nec2.xml, installed above, defines the
# type the entry claims.
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

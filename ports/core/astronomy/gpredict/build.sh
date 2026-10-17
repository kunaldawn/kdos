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

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-nls \
	--disable-caches
grep -q '^#define HAS_LIBGPS 1' build-config.h
make
make DESTDIR=$PKG install

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s icons/gpredict.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/gpredict.png"
done

cat > "$PKG/usr/share/applications/gpredict.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Gpredict
GenericName=Satellite Tracker
Comment=Track satellites and predict passes, with Doppler tuning and rotator control
Exec=gpredict
Icon=gpredict
Terminal=false
StartupWMClass=gpredict
Categories=HamRadio;Science;Astronomy;Education;Network;
Keywords=satellite;tracking;iss;pass;prediction;orbit;tle;ham;radio;doppler;
DESKTOP
chmod 644 "$PKG/usr/share/applications/gpredict.desktop"

test -x "$PKG/usr/bin/gpredict"

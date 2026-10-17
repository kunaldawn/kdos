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

autoreconf -fi
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with-libcurl \
	--with-nominatim \
	--with-libproj \
	--with-shapelib \
	--without-ax25 \
	--with-gpsman \
	--without-festival \
	--with-geotiff \
	--with-map-cache \
	--with-graphicsmagick \
	--without-imagemagick \
	--without-postgis \
	--without-mysql \
	--without-libgc
grep -q '^#define HAVE_LIBSHP 1' config.h
grep -q '^#define HAVE_PROJ 1' config.h
grep -q '^#define HAVE_LIBCURL 1' config.h
grep -q '^#define HAVE_CJSON 1' config.h
grep -q '^#define HAVE_GRAPHICSMAGICK 1' config.h
grep -q '^#define HAVE_GPSMAN 1' config.h
grep -q '^#define HAVE_LIBGEOTIFF 1' config.h
grep -q '^#define USE_MAP_CACHE 1' config.h
make
make DESTDIR=$PKG install

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s symbols/icon.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/xastir.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/xastir.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Xastir
GenericName=APRS Client
Comment=Map APRS stations, weather and objects from a TNC, Dire Wolf or APRS-IS
Exec=xastir
Icon=xastir
Terminal=false
StartupWMClass=Xastir
Categories=Network;HamRadio;
Keywords=ham;radio;aprs;map;tnc;ax25;gps;weather;
DESKTOP
chmod 644 "$PKG/usr/share/applications/xastir.desktop"

test -x "$PKG/usr/bin/xastir"

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

# libgps is optional to configure and has no switch, so its absence would
# only drop the GPS ground-station position; the check after configure makes
# it a failure instead. The satellite catalogue in data/satdata is shipped as
# it is: gpredict tracks from it offline and only asks before refreshing it.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-nls \
	--disable-caches
grep -q '^#define HAS_LIBGPS 1' build-config.h
make
make DESTDIR=$PKG install

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s icons/gpredict.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/gpredict.png"
done

# The window is a plain GtkWindow, so the Wayland app_id is the program name.
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

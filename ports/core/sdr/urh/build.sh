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

patch -p1 -i "$PORT_SRC/app-id.patch"

python3 setup.py \
	--with-airspy --with-bladerf --with-hackrf --with-rtlsdr \
	--with-limesdr --with-plutosdr --with-usrp --without-sdrplay \
	bdist_wheel -d dist
python3 -m pip install --ignore-installed --no-deps --no-index --no-build-isolation \
	--root="$PKG" --prefix=/usr dist/urh-*.whl

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/appicon.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/urh.png"
done
install -Dm644 data/icons/appicon.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/urh.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/urh.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Universal Radio Hacker
GenericName=Wireless Protocol Analyser
Comment=Record, demodulate, decode and replay wireless protocols
Exec=urh
Terminal=false
Icon=urh
Categories=HamRadio;Network;Qt;
Keywords=SDR;radio;protocol;demodulation;reverse;IoT;433;
StartupWMClass=urh
DESKTOP
chmod 644 "$PKG/usr/share/applications/urh.desktop"

test -x "$PKG/usr/bin/urh"
for d in airspy bladerf hackrf rtlsdr limesdr plutosdr usrp; do
	test -n "$(find "$PKG"/usr/lib/python3*/site-packages/urh/dev/native/lib -name "$d.*.so")"
done

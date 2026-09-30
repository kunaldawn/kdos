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

# KLog asks klog.xyz for a newer release at every start, sending the station
# callsign; the patch makes that an opt-in the settings page still offers.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DKLOG_COVERAGE=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

# Bundled data is English only: the source strings are English.
rm -rf "$PKG/usr/share/klog/translations"

# THE MENU: the SVG is rasterised for the sizes the panel reads. Qt builds
# the Wayland app_id from the reversed organisation domain and the program
# name, xyz.klog.klog.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s src/klog.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/klog.png"
done
cat > "$PKG/usr/share/applications/io.github.ea4k.klog.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KLog
GenericName=Amateur Radio Logbook
Comment=Log contacts, track DXCC and awards, and exchange ADIF and LoTW files
Exec=klog
Icon=klog
Terminal=false
StartupWMClass=xyz.klog.klog
Categories=Utility;HamRadio;
Keywords=ham;radio;amateur;log;logbook;qso;adif;dxcc;lotw;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.github.ea4k.klog.desktop"

test -x "$PKG/usr/bin/klog"

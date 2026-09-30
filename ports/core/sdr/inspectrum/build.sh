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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

# Upstream ships neither an entry nor an icon. With no organisation domain
# set, Qt's Wayland app_id is the executable's name. The icon is the atlas's
# network-wireless, which the panel draws without a hicolor file.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/inspectrum.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=inspectrum
GenericName=Signal Analyser
Comment=Inspect recorded radio signals as a spectrogram and demodulate them
Exec=inspectrum %f
Terminal=false
Icon=network-wireless
Categories=HamRadio;Science;Qt;
Keywords=SDR;radio;IQ;spectrogram;signal;analysis;cf32;
StartupWMClass=inspectrum
DESKTOP
chmod 644 "$PKG/usr/share/applications/inspectrum.desktop"

test -x "$PKG/usr/bin/inspectrum"

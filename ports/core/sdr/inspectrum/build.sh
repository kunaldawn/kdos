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
	-DCMAKE_INSTALL_PREFIX=/usr
ninja -C build
DESTDIR=$PKG ninja -C build install

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

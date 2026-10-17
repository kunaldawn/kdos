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
	-DUSE_HAMLIB=ON \
	-DUSE_SYSTEM_RTAUDIO=OFF \
	-DUSE_AUDIO_PULSE=ON \
	-DUSE_AUDIO_JACK=OFF \
	-DUSE_AUDIO_ALSA=OFF \
	-DUSE_AUDIO_OSS=OFF \
	-DENABLE_DIGITAL_LAB=OFF \
	-DBUILD_DEB=OFF \
	-DBUNDLE_APP=OFF \
	-DCUSTOM_BUILD=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

install -Dm644 src/CubicSDR.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/CubicSDR.png"
cat > "$PKG/usr/share/applications/CubicSDR.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=CubicSDR
GenericName=Software Defined Radio
Comment=Receive and demodulate radio with a spectrum and waterfall display
Exec=CubicSDR
Terminal=false
Icon=CubicSDR
Categories=Science;HamRadio;DataVisualization;
Keywords=SDR;radio;receiver;waterfall;rtl-sdr;soapysdr;
StartupWMClass=CubicSDR
DESKTOP
chmod 644 "$PKG/usr/share/applications/CubicSDR.desktop"

test -x "$PKG/usr/bin/CubicSDR"
test -d "$PKG/usr/share/cubicsdr/fonts"

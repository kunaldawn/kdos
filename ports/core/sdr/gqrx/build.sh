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

# Qt 6 is forced, so a Qt 5 in the chroot is never picked instead. Audio goes
# out through libpulse, which pipewire-pulse answers. CUSTOM_AIRSPY_KERNELS
# asks a patched gr-osmosdr for filter kernels the released one does not
# have, and is off. CMP0167=NEW lets GNU Radio's exported config find Boost
# through Boost's own package, which knows Boost.System is header-only.
# CCACHE=OFF: upstream wraps the compiler in any ccache it finds on PATH.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFORCE_QT6=ON \
	-DLINUX_AUDIO_BACKEND=Pulseaudio \
	-DCUSTOM_AIRSPY_KERNELS=OFF \
	-DCCACHE=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

install -Dm644 resources/gqrx.1 "$PKG/usr/share/man/man1/gqrx.1"

# The Wayland app_id is Qt's reversed organisation domain plus the
# application name, dk.gqrx.gqrx. The panel reads PNG only, so the SVG is
# rasterised beside it.
for s in 32 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/icons/gqrx.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/gqrx.png"
done
cat > "$PKG/usr/share/applications/dk.gqrx.gqrx.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Gqrx
GenericName=Software Defined Radio
Comment=Receive AM, FM and SSB with a spectrum and waterfall display
Exec=gqrx
Terminal=false
Icon=gqrx
Categories=AudioVideo;Audio;Qt;HamRadio;
Keywords=SDR;Radio;HAM;receiver;waterfall;scanner;rtl-sdr;
StartupWMClass=dk.gqrx.gqrx
DESKTOP
chmod 644 "$PKG/usr/share/applications/dk.gqrx.gqrx.desktop"

test -x "$PKG/usr/bin/gqrx"

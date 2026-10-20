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
	-DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -Wno-error" \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_SKIP_RPATH=ON \
	-DSTLINK_MODPROBED_DIR=/etc/modprobe.d \
	-DSTLINK_UDEV_RULES_DIR=/etc/udev/rules.d
ninja -C build stlink-gui

install -Dm755 build/bin/stlink-gui "$PKG/usr/bin/stlink-gui"
install -Dm644 src/stlink-gui/stlink-gui.ui "$PKG/usr/share/stlink/stlink-gui.ui"
install -Dm644 src/stlink-gui/stlink-gui.desktop "$PKG/usr/share/applications/stlink-gui.desktop"
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s src/stlink-gui/icons/stlink-gui.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/stlink-gui.png"
done

test -z "$(find "$PKG" -name 'libstlink*')"
readelf -d "$PKG/usr/bin/stlink-gui" | grep -q 'NEEDED.*\[libstlink\.so\.'

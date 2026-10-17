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


mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -Wno-error" \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DSTLINK_MODPROBED_DIR=/etc/modprobe.d \
	-DSTLINK_UDEV_RULES_DIR=/etc/udev/rules.d
ninja
DESTDIR=$PKG ninja install

rm -f "$PKG"/etc/udev/rules.d/49-stlink*.rules
rmdir --ignore-fail-on-non-empty "$PKG/etc/udev/rules.d" "$PKG/etc/udev" 2>/dev/null || true

rm -f "$PKG/usr/bin/stlink-gui" "$PKG/usr/share/stlink/stlink-gui.ui" \
	"$PKG/usr/share/applications/stlink-gui.desktop" \
	"$PKG/usr/share/icons/hicolor/scalable/apps/stlink-gui.svg"
rmdir --ignore-fail-on-non-empty "$PKG/usr/share/applications" \
	"$PKG/usr/share/icons/hicolor/scalable/apps" "$PKG/usr/share/icons/hicolor/scalable" \
	"$PKG/usr/share/icons/hicolor" "$PKG/usr/share/icons" 2>/dev/null || true
test ! -e "$PKG/usr/bin/stlink-gui"

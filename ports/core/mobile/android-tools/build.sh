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

export LDFLAGS="$LDFLAGS -Wl,--copy-dt-needed-entries -Wl,--strip-debug"

patch -p1 -d vendor/adb -i "$PORT_SRC/protobuf-36-absl-log-macros.patch"

cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DANDROID_TOOLS_PATCH_VENDOR=OFF \
	-DANDROID_TOOLS_USE_BUNDLED_FMT=OFF \
	-DANDROID_TOOLS_USE_BUNDLED_LIBUSB=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

install -d "$PKG/usr/lib/udev/rules.d"
cat > "$PKG/usr/lib/udev/rules.d/70-android-tools.rules" <<'RULES'
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ENV{ID_USB_INTERFACES}=="*:ff4201:*|*:ff4203:*", GROUP="dialout", MODE="0660"
RULES
chmod 644 "$PKG/usr/lib/udev/rules.d/70-android-tools.rules"

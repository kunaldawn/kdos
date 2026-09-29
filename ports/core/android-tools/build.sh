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

# The generated protobuf code calls into abseil, and the targets the build
# links do not name every absl library it needs. The flag lets the linker
# follow libprotobuf's own DT_NEEDED entries to them; without it adb's link
# can stop on unresolved absl:: symbols.
export LDFLAGS="$LDFLAGS -Wl,--copy-dt-needed-entries"

# protobuf-36-absl-log-macros.patch is Arch's, proposed upstream as
# nmeum/android-tools#213. protobuf's descriptor.h includes absl/log/log.h,
# whose LOG and VLOG replace adb's when a protobuf header comes after
# adb_trace.h, and adb's LOG(DEBUG) then names a severity abseil does not
# have. The patch includes protobuf first, so adb's macros are the last ones
# defined. Its paths are relative to the adb tree.
patch -p1 -d vendor/adb -i "$PORT_SRC/protobuf-36-absl-log-macros.patch"

# -DANDROID_TOOLS_PATCH_VENDOR=OFF: the release tarball carries the vendored
# AOSP trees already patched, and the patch step runs `git submodule` and
# `git am`, which have no repository here to act on.
# The system fmt and libusb are used rather than the bundled copies, so a
# fix to either port reaches adb and fastboot. gtest is a build dependency
# only: libbase and fastboot headers include gtest_prod.h.
cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DANDROID_TOOLS_PATCH_VENDOR=OFF \
	-DANDROID_TOOLS_USE_BUNDLED_FMT=OFF \
	-DANDROID_TOOLS_USE_BUNDLED_LIBUSB=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# adb and fastboot open the phone's raw USB device. Matched on the interface
# rather than a vendor table: class ff, subclass 42, protocol 01 is the adb
# interface and 03 the fastboot one on every Android device, so every phone
# is covered and nothing else is. Without the rule the node is root-only and
# `adb devices` lists nothing for the console user.
install -d "$PKG/usr/lib/udev/rules.d"
cat > "$PKG/usr/lib/udev/rules.d/70-android-tools.rules" <<'RULES'
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ENV{ID_USB_INTERFACES}=="*:ff4201:*|*:ff4203:*", GROUP="dialout", MODE="0660"
RULES
chmod 644 "$PKG/usr/lib/udev/rules.d/70-android-tools.rules"

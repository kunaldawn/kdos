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

# The MTP filesystem: `aft-mtp-mount ~/Phone` puts the phone's storage under a
# directory any file manager reads, through fusermount3, and
# `fusermount3 -u ~/Phone` takes it away. It speaks MTP itself over the
# kernel's usbdevfs rather than through libmtp; libmtp is in depends for
# 69-libmtp.rules, which is what gives the dialout group the phone's USB node.
#
# BUILD_QT_UI=ON is the `android-file-transfer` window, which browses the
# phone's storage and copies to and from it with no mount. DESIRED_QT_VERSION=6
# asks for Qt 6 alone: left to itself the build takes Qt 6 or Qt 5, whichever
# it finds, and with neither it prints a warning and builds no window, so the
# check after install fails the build instead. BUILD_MTPZ=OFF: MTPZ is the
# Zune handshake and needs keys that are not distributable. BUILD_PYTHON and
# BUILD_TAGLIB are off because nothing here imports the module or wants the
# tags written on upload. libmagic, from `file`, gives an uploaded file its
# MIME type. The library is linked statically into all three programs and its
# archive is not shipped.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_QT_UI=ON \
	-DDESIRED_QT_VERSION=6 \
	-DBUILD_FUSE=ON \
	-DBUILD_MTPZ=OFF \
	-DBUILD_PYTHON=OFF \
	-DBUILD_TAGLIB=OFF \
	-DBUILD_SHARED_LIB=OFF
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libmtp-ng-static.a"
[ -x "$PKG/usr/bin/android-file-transfer" ] || {
	echo "android-file-transfer: the Qt window was not built" >&2
	exit 1
}

# Upstream's entry has no StartupWMClass. The program sets the organisation
# domain whoozle.github.io and no desktop file name, so Qt makes the Wayland
# app_id the reversed domain and the program name, which the entry repeats.
# The icon is upstream's 512-pixel PNG in hicolor.
cat > "$PKG/usr/share/applications/android-file-transfer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Android File Transfer
GenericName=Phone File Transfer
Comment=Copy files to and from an Android phone or any MTP device
Exec=android-file-transfer
Icon=android-file-transfer
Terminal=false
StartupWMClass=io.github.whoozle.android-file-transfer
Categories=Qt;Utility;FileTools;
Keywords=android;phone;mtp;usb;transfer;tablet;
DESKTOP
chmod 644 "$PKG/usr/share/applications/android-file-transfer.desktop"

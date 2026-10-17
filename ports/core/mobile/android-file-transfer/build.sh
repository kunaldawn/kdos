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

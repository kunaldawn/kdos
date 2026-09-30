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

# JS8Call asks GitHub for a newer release at every start; the patch makes
# that an opt-in the settings page still offers. The source is a release
# archive with no .git, and GIT_EXECUTABLE is cleared so the version string
# is not taken from whatever repository the build directory sits in.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DGIT_EXECUTABLE=GIT_EXECUTABLE-NOTFOUND \
	-Wno-dev
ninja -C build

# Upstream installs nothing on Linux. The program is installed under a
# lower-case name, which is also Qt's Wayland app_id: no organisation domain
# is set, so the app_id is the executable's name.
install -Dm755 build/JS8Call "$PKG/usr/bin/js8call"
install -Dm644 icons/Unix/js8call_icon.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/js8call.png"
for s in 48 64; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s artwork/icon_128.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/js8call.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/js8call.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=JS8Call
GenericName=Weak-Signal Messaging
Comment=Keyboard-to-keyboard and store-and-forward messaging over HF with the JS8 mode
Exec=js8call
Icon=js8call
Terminal=false
StartupWMClass=js8call
Categories=Network;HamRadio;
Keywords=ham;radio;js8;ft8;weak;signal;hf;messaging;
DESKTOP
chmod 644 "$PKG/usr/share/applications/js8call.desktop"

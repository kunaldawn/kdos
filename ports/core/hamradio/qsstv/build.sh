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

# qmake reads the compiler flags from the environment (qt5-qtbase's cflags
# patch). A release build leaves out the qwt scope, which is debug-only.
# Upstream installs the binary alone; the entry and icon are written below.
/usr/lib/qt5/bin/qmake PREFIX=/usr CONFIG+=release
make
make INSTALL_ROOT="$PKG" install

# The icon is 42 pixels; padded, not scaled, onto the 48 grid the panel reads.
install -d "$PKG/usr/share/icons/hicolor/48x48/apps"
magick icons/qsstv.png -background none -gravity center -extent 48x48 \
	"$PKG/usr/share/icons/hicolor/48x48/apps/qsstv.png"

# No organisation domain is set, so Qt's Wayland app_id is the program name.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/qsstv.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QSSTV
GenericName=Slow-Scan Television
Comment=Receive and send SSTV pictures and digital DRM images over amateur radio
Exec=qsstv
Icon=qsstv
Terminal=false
StartupWMClass=qsstv
Categories=Network;HamRadio;
Keywords=ham;radio;sstv;drm;hamdrm;image;picture;
DESKTOP
chmod 644 "$PKG/usr/share/applications/qsstv.desktop"

test -x "$PKG/usr/bin/qsstv"

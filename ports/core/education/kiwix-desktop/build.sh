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
qmake6 .. \
	PREFIX=/usr \
	CONFIG+=release \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS" \
	-after QMAKE_CXXFLAGS+=-Wno-error
make
make INSTALL_ROOT="$PKG" install
[ -f "$PKG/usr/share/icons/hicolor/48x48/apps/kiwix-desktop.png" ] ||
	{ echo "kiwix-desktop: the hicolor PNG icon was not installed" >&2; exit 1; }

cat > "$PKG/usr/share/applications/org.kiwix.desktop.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kiwix
GenericName=Offline Library
Comment=Read ZIM archives such as Wikipedia with no network
Exec=kiwix-desktop %F
Icon=kiwix-desktop
Terminal=false
StartupWMClass=org.kiwix.desktop
MimeType=application/org.kiwix.desktop.x-zim;
Categories=Qt;Education;
Keywords=zim;wikipedia;offline;encyclopedia;library;kiwix;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kiwix.desktop.desktop"

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

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr
ninja
DESTDIR=$PKG ninja install
cd ..

install -Dm644 "release/other/Freedesktop.org Resources/Fasttracker II clone.png" \
	"$PKG/usr/share/icons/hicolor/512x512/apps/ft2-clone.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ft2-clone.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Fasttracker II
GenericName=Music Tracker
Comment=Compose music in a faithful Fasttracker II clone
TryExec=ft2-clone
Exec=ft2-clone %f
Icon=ft2-clone
Terminal=false
StartupWMClass=ft2-clone
Categories=AudioVideo;Audio;Sequencer;Music;
Keywords=tracker;fasttracker;ft2;module;xm;mod;music;
DESKTOP
chmod 644 "$PKG/usr/share/applications/ft2-clone.desktop"
install -Dm644 release/LICENSES.txt "$PKG/usr/share/licenses/ft2-clone/LICENSES.txt"

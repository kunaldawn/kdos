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
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
ninja
DESTDIR=$PKG ninja install
cd ..

# THE MENU ICON: upstream's 512-pixel PNG, under the program's name.
install -Dm644 "release/other/Freedesktop.org Resources/ProTracker 2 clone.png" \
	"$PKG/usr/share/icons/hicolor/512x512/apps/pt2-clone.png"

# SDL names the window after the program. Module files are left to the
# music player; mimeapps.list chooses defaults.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/pt2-clone.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=ProTracker 2
GenericName=Music Tracker
Comment=Compose Amiga MOD music in a faithful ProTracker 2 clone
TryExec=pt2-clone
Exec=pt2-clone %f
Icon=pt2-clone
Terminal=false
StartupWMClass=pt2-clone
Categories=AudioVideo;Audio;Sequencer;Music;
Keywords=tracker;protracker;amiga;module;mod;music;
DESKTOP
chmod 644 "$PKG/usr/share/applications/pt2-clone.desktop"

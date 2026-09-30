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

# MIDI input is the bundled RtMidi over ALSA, which the project compiles in
# unconditionally. The disk operations walk directories with fts, which musl
# leaves out; musl-fts supplies it and CMake finds libfts by name.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
ninja
DESTDIR=$PKG ninja install
cd ..

# THE MENU ICON: upstream's 512-pixel PNG, under the program's name.
install -Dm644 "release/other/Freedesktop.org Resources/Fasttracker II clone.png" \
	"$PKG/usr/share/icons/hicolor/512x512/apps/ft2-clone.png"

# SDL names the window after the program. Module files are left to the
# music player; mimeapps.list chooses defaults.
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

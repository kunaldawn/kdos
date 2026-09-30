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

# CMake 4 refuses cmake_policy(SET CMP0004 OLD), which the top-level file
# sets for an SDL2 older than 2.0.5; the patch drops the line.
patch -p1 -i "$PORT_SRC/milkytracker-cmake4.patch"

# Audio through ALSA (and so PipeWire) and SDL2; MIDI input through RtMidi.
# JACK stays out even where a libjack is on the build root. LHA and ZIP
# module archives open through lhasa and zziplib; both lookups are required,
# since a probe that fails quietly builds a tracker that cannot open them.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LHASA=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZZIPLIB=ON
ninja
DESTDIR=$PKG ninja install
cd ..

# THE MENU ICON: the carton, upstream's own 128-pixel PNG.
install -Dm644 resources/pictures/carton.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/milkytracker.png"

# SDL names the window after the program. MimeType is left out, since the
# music player owns module files; mimeapps.list chooses defaults.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/milkytracker.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=MilkyTracker
GenericName=Music Tracker
Comment=Compose music in the Fast Tracker II style
TryExec=milkytracker
Exec=milkytracker %f
Icon=milkytracker
Terminal=false
StartupWMClass=milkytracker
Categories=AudioVideo;Audio;Sequencer;Music;
Keywords=tracker;fasttracker;module;xm;mod;music;
DESKTOP
chmod 644 "$PKG/usr/share/applications/milkytracker.desktop"

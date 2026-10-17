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

patch -p1 -i "$PORT_SRC/milkytracker-cmake4.patch"

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LHASA=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZZIPLIB=ON
ninja
DESTDIR=$PKG ninja install
cd ..

install -Dm644 resources/pictures/carton.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/milkytracker.png"

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

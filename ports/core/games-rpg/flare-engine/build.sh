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

# The data directory is compiled in, so the engine and flare-game have to
# agree on it: /usr/share/flare, the program in /usr/bin rather than
# upstream's /usr/games.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBINDIR=bin \
	-DDATADIR=share/flare \
	-DMANDIR=share/man
ninja
DESTDIR=$PKG ninja install
cd ..

# Upstream's entry and scalable icon are replaced: the panel reads PNG only,
# and the entry is written with StartupWMClass, the Wayland app_id being SDL's
# default, the executable's name.
rm -f "$PKG/usr/share/applications/flare.desktop"
rm -rf "$PKG/usr/share/icons/hicolor/scalable"
install -Dm644 distribution/flare_logo_icon.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/flare.png"
cat > "$PKG/usr/share/applications/flare.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Flare
GenericName=Action Role-Playing Game
Comment=Single-player isometric action RPG
Exec=flare
Icon=flare
Terminal=false
StartupWMClass=flare
Categories=Game;RolePlaying;
Keywords=rpg;action;isometric;fantasy;flare;
DESKTOP
chmod 644 "$PKG/usr/share/applications/flare.desktop"

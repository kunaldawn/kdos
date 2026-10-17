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
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUSE_SDL_VERSION=SDL2 \
	-DENABLE_IMAGE=ON \
	-DENABLE_TOOLS=OFF \
	-DGET_HOMM2_DEMO=OFF \
	-DFHEROES2_DATA=share/fheroes2
ninja fheroes2
DESTDIR=$PKG cmake --install .
cd ..

rm -f "$PKG/usr/share/applications/fheroes2.desktop"
cat > "$PKG/usr/share/applications/fheroes2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=fheroes2
GenericName=Turn-based Strategy Game
Comment=Heroes of Might and Magic II, from your own game data
Exec=fheroes2
Icon=fheroes2
Terminal=false
StartupWMClass=fheroes2
Categories=Game;StrategyGame;
Keywords=heroes;homm2;might;magic;strategy;fheroes2;
DESKTOP
chmod 644 "$PKG/usr/share/applications/fheroes2.desktop"

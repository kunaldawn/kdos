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
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DBUILD_TESTING=OFF \
	-DDISABLE_LTO=ON \
	-DDISABLE_ZERO_TIER=ON \
	-DDISABLE_TCP=OFF \
	-DDISCORD_INTEGRATION=OFF \
	-DNONET=OFF \
	-DNOSOUND=OFF \
	-DBUILD_ASSETS_MPQ=OFF \
	-DDEVILUTIONX_SYSTEM_SDL2=ON \
	-DDEVILUTIONX_SYSTEM_SDL_IMAGE=ON \
	-DDEVILUTIONX_SYSTEM_ZLIB=ON \
	-DDEVILUTIONX_SYSTEM_BZIP2=ON \
	-DDEVILUTIONX_SYSTEM_LIBSODIUM=ON \
	-DDEVILUTIONX_SYSTEM_LIBFMT=OFF \
	-DDEVILUTIONX_SYSTEM_SDL_AUDIOLIB=OFF \
	-DDEVILUTIONX_SYSTEM_SIMPLEINI=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

rm -f "$PKG/usr/share/applications/devilutionx.desktop" \
	"$PKG/usr/share/applications/devilutionx-hellfire.desktop"
cat > "$PKG/usr/share/applications/devilutionx.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DevilutionX
GenericName=Action Role-Playing Game
Comment=Play Diablo from your own game data
Exec=devilutionx --diablo
Icon=devilutionx
Terminal=false
StartupWMClass=devilutionx
Categories=Game;RolePlaying;
Keywords=diablo;rpg;dungeon;devilutionx;
DESKTOP
cat > "$PKG/usr/share/applications/devilutionx-hellfire.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DevilutionX Hellfire
GenericName=Action Role-Playing Game
Comment=Play Diablo: Hellfire from your own game data
Exec=devilutionx --hellfire
Icon=devilutionx-hellfire
Terminal=false
StartupWMClass=devilutionx
Categories=Game;RolePlaying;
Keywords=diablo;hellfire;rpg;dungeon;devilutionx;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"devilutionx*.desktop

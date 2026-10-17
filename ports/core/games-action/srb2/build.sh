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
	-DSRB2_SDL2_EXE_NAME=srb2 \
	-DSRB2_CONFIG_SYSTEM_LIBRARIES=ON \
	-DSRB2_CONFIG_STATIC_STDLIB=OFF \
	-DSRB2_CONFIG_EXECINFO=OFF \
	-DSRB2_CONFIG_HWRENDER=ON \
	-DSRB2_CONFIG_STATIC_OPENGL=OFF \
	-DSRB2_CONFIG_USE_GME=ON \
	-DSRB2_CONFIG_ERRORMODE=OFF \
	-DSRB2_CONFIG_DEV_BUILD=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_libopenmpt=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_libgme=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_miniupnpc=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_SDL2_mixer_ext=ON
ninja
cd ..

install -Dm755 build/bin/srb2 "$PKG/usr/bin/srb2"

unzip -q srb2-assets-$version.zip \
	srb2.pk3 zones.pk3 characters.pk3 music.pk3 models.dat 'models/*' \
	LICENSE.txt LICENSE-3RD-PARTY.txt -d assets-full
install -d "$PKG/usr/share/games/SRB2"
cp -R assets-full/srb2.pk3 assets-full/zones.pk3 assets-full/characters.pk3 \
	assets-full/music.pk3 assets-full/models.dat assets-full/models \
	"$PKG/usr/share/games/SRB2/"
install -Dm644 assets-full/LICENSE.txt "$PKG/usr/share/licenses/srb2/LICENSE.txt"
install -Dm644 assets-full/LICENSE-3RD-PARTY.txt \
	"$PKG/usr/share/licenses/srb2/LICENSE-3RD-PARTY.txt"

install -Dm644 srb2.png "$PKG/usr/share/icons/hicolor/256x256/apps/srb2.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/srb2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Sonic Robo Blast 2
GenericName=3D Platform Game
Comment=A 3D Sonic the Hedgehog fan game
Exec=srb2
Icon=srb2
Terminal=false
StartupWMClass=srb2
Categories=Game;ActionGame;
Keywords=sonic;platformer;3d;srb2;
DESKTOP
chmod 644 "$PKG/usr/share/applications/srb2.desktop"

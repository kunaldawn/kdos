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

# Every library is the system's. The standard library links dynamically:
# upstream's static default needs every dependency static too. musl has no
# execinfo, so the crash backtrace is off. GME adds the chiptune formats to
# SDL_mixer's Ogg, libopenmpt the tracker modules (MOD, XM, IT, S3M) and
# miniupnpc the router port mapping of a hosted game. Upstream looks each of
# the three up QUIET and builds without it silently, so each is required by
# name and a missing one fails configure. curl carries the add-on downloads of
# a network game. The
# executable is named outright: left empty, the name is derived from the
# git branch, and a release tarball has no repository to ask.
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

# /usr/share/games/SRB2 is on the program's own data search path.
install -Dm755 build/bin/srb2 "$PKG/usr/bin/srb2"

# The game data is the release's full archive, less its Windows program and
# libraries: the four .pk3 files, models.dat and the model directory.
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

# The Wayland app_id is SDL's default, the executable's name: srb2.
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

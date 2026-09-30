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

# Werror.patch is Alpine's: the compile flags carry an unconditional -Werror,
# which this compiler's newer warnings turn into a failed build.
patch -p1 -i "$PORT_SRC/Werror.patch"

# The objects, title sequences, sound effects and music are fetched sources;
# unpacked into data/, the install copies them with the rest and skips the
# download steps, which are also switched off.
mkdir -p data/object data/sequence
unzip -q -o openrct2-objects-$_objects.zip -d data/object
unzip -q -o openrct2-title-sequences-$_titles.zip -d data/sequence
unzip -q -o openrct2-opensfx-$_sfx.zip -d data
unzip -q -o openrct2-openmusic-$_music.zip -d data

# No version check, no HTTP (the server list and the in-game downloads) and
# no Discord. Multiplayer over a direct address stays; its packets are
# signed with OpenSSL. Scripting is the bundled QuickJS. The game itself
# needs the player's own RollerCoaster Tycoon 2 or RCT Classic data, which
# cannot be shipped; the program asks for its location at first start.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DOPENRCT2_USE_CCACHE=OFF \
	-DWITH_TESTS=OFF \
	-DDOWNLOAD_TITLE_SEQUENCES=OFF \
	-DDOWNLOAD_OBJECTS=OFF \
	-DDOWNLOAD_OPENSFX=OFF \
	-DDOWNLOAD_OPENMUSIC=OFF \
	-DDOWNLOAD_REPLAYS=OFF \
	-DDISABLE_VERSION_CHECKER=ON \
	-DDISABLE_HTTP=ON \
	-DDISABLE_DISCORD_RPC=ON \
	-DDISABLE_GOOGLE_BENCHMARK=ON \
	-DDISABLE_NETWORK=OFF \
	-DDISABLE_TTF=OFF \
	-DDISABLE_FLAC=OFF \
	-DDISABLE_VORBIS=OFF \
	-DDISABLE_OPENGL=OFF \
	-DENABLE_SCRIPTING=ON \
	-DPORTABLE=OFF \
	-DSTATIC=OFF
ninja
DESTDIR=$PKG ninja install
cd ..

# The main entry is replaced for StartupWMClass: the Wayland app_id is SDL's
# default, the executable's name. The three hidden handlers for saves,
# scenarios and openrct2: links stay upstream's.
rm -rf "$PKG/usr/share/icons/hicolor/scalable"
cat > "$PKG/usr/share/applications/io.openrct2.openrct2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OpenRCT2
GenericName=Theme Park Game
Comment=Build and run a theme park, from your own RollerCoaster Tycoon 2 data
Exec=openrct2 %u
Icon=openrct2
Terminal=false
StartupWMClass=openrct2
Categories=Game;Simulation;
Keywords=openrct2;rct2;rct;roller;coaster;tycoon;theme park;simulation;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.openrct2.openrct2.desktop"

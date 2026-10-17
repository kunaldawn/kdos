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

patch -p1 -i "$PORT_SRC/Werror.patch"
patch -p1 -i "$PORT_SRC/gcc-includes.patch"

mkdir -p data/object data/sequence
unzip -q -o openrct2-objects-$_objects.zip -d data/object
unzip -q -o openrct2-title-sequences-$_titles.zip -d data/sequence
unzip -q -o openrct2-opensfx-$_sfx.zip -d data
unzip -q -o openrct2-openmusic-$_music.zip -d data

export CXXFLAGS="$CXXFLAGS -DJSON_HAS_FILESYSTEM=0 -DJSON_HAS_EXPERIMENTAL_FILESYSTEM=0"
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

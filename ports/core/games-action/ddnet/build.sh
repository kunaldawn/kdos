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

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_HOME="$SRC_ROOT/.cargo"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPREFER_BUNDLED_LIBS=OFF \
	-DDOWNLOAD_GTEST=OFF \
	-DAUTOUPDATE=OFF \
	-DINFORM_UPDATE=OFF \
	-DSTEAM=OFF \
	-DDISCORD=OFF \
	-DDISCORD_DYNAMIC=OFF \
	-DMYSQL=OFF \
	-DWEBSOCKETS=ON \
	-DANTIBOT=OFF \
	-DUPNP=ON \
	-DVIDEORECORDER=ON \
	-DVULKAN=ON \
	-DCLIENT=ON \
	-DSERVER=ON \
	-DTOOLS=ON \
	-DIPO=OFF \
	-DDEV=OFF \
	-DPRECOMPILE_HEADERS=ON \
	-DSECURITY_COMPILER_FLAGS=ON
ninja
DESTDIR=$PKG ninja install
cd ..

rm -f "$PKG/usr/share/applications/ddnet.desktop"
cat > "$PKG/usr/share/applications/ddnet.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DDNet
GenericName=Platform Game
Comment=Cooperative racing on Teeworlds maps
Exec=DDNet %u
Icon=ddnet
Terminal=false
StartupWMClass=DDNet
MimeType=x-scheme-handler/ddnet;
Categories=Game;ArcadeGame;
Keywords=game;multiplayer;teeworlds;race;ddrace;
DESKTOP
chmod 644 "$PKG/usr/share/applications/ddnet.desktop"

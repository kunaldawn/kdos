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

# The Rust half (the engine's shared crates) is built by CMake through
# cargo, offline, from the vendored crates. Cargo finds the bundle's
# configuration by walking up from its working directory, the build
# directory below this one, so the bundle is unpacked here.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_HOME="$SRC_ROOT/.cargo"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"

# No update notice, no auto-updater, no Discord or Steam, and no bundled
# libraries: every dependency is the system's, so a missing one fails the
# configure step instead of reaching for ddnet-libs. The Vulkan backend
# compiles its shaders with glslangValidator and spirv-opt. The video
# recorder is FFmpeg's. libwebsockets lets the server and the client speak
# ws:// addresses beside UDP ones.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
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

# Upstream's entry is replaced for StartupWMClass: the Wayland app_id is
# SDL's default, the executable's name, DDNet.
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

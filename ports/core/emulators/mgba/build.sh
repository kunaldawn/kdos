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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED=ON \
	-DBUILD_STATIC=OFF \
	-DBUILD_SDL=ON \
	-DBUILD_QT=OFF \
	-DBUILD_LIBRETRO=OFF \
	-DBUILD_TEST=OFF \
	-DBUILD_SUITE=OFF \
	-DBUILD_GL=ON \
	-DBUILD_GLES2=ON \
	-DBUILD_GLES3=ON \
	-DUSE_EPOXY=ON \
	-DUSE_FFMPEG=OFF \
	-DUSE_LUA=OFF \
	-DENABLE_SCRIPTING=OFF \
	-DUSE_DISCORD_RPC=OFF \
	-DUSE_ZLIB=ON \
	-DUSE_PNG=ON \
	-DUSE_LIBZIP=ON \
	-DUSE_MINIZIP=OFF \
	-DUSE_LZMA=ON \
	-DUSE_SQLITE3=ON \
	-DUSE_ELF=ON \
	-DUSE_EDITLINE=ON \
	-DUSE_GDB_STUB=ON \
	-DSDL_VERSION=2
ninja -C build
DESTDIR=$PKG ninja -C build install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/mgba.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=mGBA
GenericName=Game Boy Advance Emulator
Comment=Play Game Boy Advance, Game Boy and Game Boy Color games
TryExec=mgba
Exec=mgba %f
Icon=io.mgba.mGBA
Terminal=false
NoDisplay=true
StartupWMClass=mgba
Categories=Game;Emulator;
MimeType=application/x-gba-rom;application/x-gameboy-rom;application/x-gameboy-color-rom;
Keywords=emulator;gba;gbc;gb;game boy;advance;mgba;
DESKTOP
chmod 644 "$PKG/usr/share/applications/mgba.desktop"

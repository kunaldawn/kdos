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

# The SDL front end and libmgba, no Qt front end. FFmpeg is off: its only use
# is the recording path of the Qt front end. Lua scripting is off with it, as
# the SDL front end has no way to load a script. Discord presence is off.
# libzip reads zipped ROMs, sqlite the game database, libelf homebrew ELFs,
# and libedit is the command-line debugger. The libretro core is the
# libretro-mgba port, built from the same tarball.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
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

# The SDL front end needs a ROM on its command line, so the entry is an
# open-with handler rather than a menu row; RetroArch is the menu's emulator.
# SDL names the Wayland window after the executable, mgba. The icon is
# upstream's, installed above as io.mgba.mGBA.
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

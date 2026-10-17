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

install -d "$SRC_ROOT/sdl-shim"
cat > "$SRC_ROOT/sdl-shim/sdl2-config" <<'SHIM'
#!/bin/sh
case "$1" in
--cflags) exec pkg-config --cflags sdl2 ;;
--libs) exec pkg-config --libs sdl2 ;;
--version) exec pkg-config --modversion sdl2 ;;
*) exit 1 ;;
esac
SHIM
chmod 755 "$SRC_ROOT/sdl-shim/sdl2-config"
export PATH="$SRC_ROOT/sdl-shim:$PATH"
./configure \
	--prefix=/usr \
	--enable-release \
	--disable-Werror \
	--disable-debug \
	--enable-optimizations \
	--disable-cloud \
	--disable-libcurl \
	--disable-sdlnet \
	--disable-enet \
	--disable-dlc \
	--disable-scummvmdlc \
	--disable-updates \
	--disable-sparkle \
	--disable-discord \
	--disable-libunity \
	--disable-gtk \
	--disable-eventrecorder \
	--enable-tts \
	--disable-translation \
	--enable-alsa \
	--enable-fluidsynth \
	--disable-fluidlite \
	--enable-mt32emu \
	--disable-seq-midi \
	--disable-timidity \
	--disable-sndio \
	--disable-sonivox \
	--enable-ogg \
	--enable-vorbis \
	--disable-tremor \
	--enable-flac \
	--enable-mad \
	--enable-faad \
	--enable-theoradec \
	--enable-vpx \
	--enable-jpeg \
	--enable-png \
	--enable-gif \
	--enable-freetype2 \
	--enable-fribidi \
	--enable-zlib \
	--enable-mpeg2 \
	--enable-a52 \
	--disable-mikmod \
	--enable-openmpt \
	--disable-mpcdec \
	--disable-retrowave \
	--disable-opl2lpt \
	--disable-readline
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 icons/scummvm.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/org.scummvm.scummvm.png"

cat > "$PKG/usr/share/applications/org.scummvm.scummvm.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=ScummVM
GenericName=Adventure Game Engine
Comment=Play classic adventure and role-playing games from their original data files
TryExec=scummvm
Exec=scummvm
Icon=org.scummvm.scummvm
Terminal=false
StartupNotify=false
StartupWMClass=scummvm
Categories=Game;AdventureGame;RolePlaying;
Keywords=adventure;point and click;lucasarts;sierra;monkey island;scumm;game;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.scummvm.scummvm.desktop"

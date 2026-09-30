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

# SDL2 through sdl2-compat, whose drivers are SDL3's. Once pkgconf answers
# for sdl2, configure takes its SDL flags from an sdl2-config found on PATH
# and has no other route, and sdl2-config does not exist here; without one it
# reads pkgconf's own version as SDL's and fails its probe. A build-local
# shim, first on PATH, answers it from pkgconf. The stable engine set, the
# release build, and no -Werror promotion for this compiler's newer warnings.
#
# Offline only: cloud saves, libcurl, the DLC and update downloaders, SDL_net
# and ENet multiplayer, Discord and the Unity launcher are off. Toolkit-free:
# the GTK file dialogs are off and the launcher's own browser is used. Text to
# speech goes through speech-dispatcher, which reads the menus and subtitles
# aloud. Translations are off: bundled data is English only. MIDI is
# FluidSynth or the built-in MT-32 emulator; the OSS sequencer, TiMidity's
# network socket and sndio are off. MPEG-2 and A/52 cutscenes play through
# libmpeg2 and liba52, and tracker modules through OpenMPT; each is named on,
# which skips configure's probe and makes a missing library a build failure.
# MikMod is off: configure uses it only when OpenMPT is absent. Musepack,
# Sonivox and FluidLite have no port here and are off rather than detected.
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

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 icons/scummvm.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/org.scummvm.scummvm.png"

# UPSTREAM'S ENTRY IS REPLACED for its translations. SDL names the Wayland
# window after the executable, scummvm.
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

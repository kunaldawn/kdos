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

# The engine's own pak (menus, default.cfg) is rebuilt from its lumps rather
# than taken from the tree. sdl2-compat carries no sdl2-config, so SDL's flags
# come from pkg-config. USE_CURL=0 removes the add-on browser, which downloads
# from a remote server and has nothing to show offline.
make -C Misc/pak
cp Misc/pak/ironwail.pak Quake/ironwail.pak

make -C Quake \
	SDL_CONFIG='pkg-config sdl2' \
	DO_USERDIRS=1 \
	USE_SDL2=1 \
	USE_CURL=0 \
	USE_CODEC_WAVE=1 \
	USE_CODEC_FLAC=1 \
	USE_CODEC_MP3=1 \
	MP3LIB=mpg123 \
	USE_CODEC_VORBIS=1 \
	VORBISLIB=vorbis \
	USE_CODEC_OPUS=1 \
	USE_CODEC_XMP=1 \
	USE_CODEC_MIKMOD=0 \
	USE_CODEC_MODPLUG=0 \
	USE_CODEC_UMX=1 \
	STRIP=true

# The engine finds ironwail.pak beside the path it was started by, so it is
# started by its full path and /usr/bin holds a script, not a link. The game
# data is not free and is not shipped: a player puts id1/pak0.pak under
# ~/.ironwail, which the engine searches after the working directory.
install -Dm755 Quake/ironwail "$PKG/usr/lib/ironwail/ironwail"
install -Dm644 Quake/ironwail.pak "$PKG/usr/lib/ironwail/ironwail.pak"
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/ironwail" <<'SCRIPT'
#!/bin/sh
exec /usr/lib/ironwail/ironwail "$@"
SCRIPT
chmod 755 "$PKG/usr/bin/ironwail"
install -Dm644 README.md Quakespasm.txt Quakespasm-Music.txt \
	-t "$PKG/usr/share/doc/ironwail"

# The window's app_id is the executable's name.
install -Dm644 Misc/QuakeSpasm_512.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/ironwail.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ironwail.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Ironwail
GenericName=First Person Shooter
Comment=Play Quake; needs the game data in ~/.ironwail/id1
Exec=ironwail
Icon=ironwail
Terminal=false
StartupWMClass=ironwail
Categories=Game;ActionGame;
Keywords=first;person;shooter;quake;quakespasm;
DESKTOP
chmod 644 "$PKG/usr/share/applications/ironwail.desktop"

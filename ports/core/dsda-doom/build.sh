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

# STRICT_FIND turns every optional music and image library into a required
# one, so a missing library fails here instead of silently dropping a format.
cmake -S prboom2 -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_PACKAGING=OFF \
	-DSTRICT_FIND=ON \
	-DWITH_FLUIDSYNTH=ON \
	-DWITH_IMAGE=ON \
	-DWITH_MAD=ON \
	-DWITH_PORTMIDI=ON \
	-DWITH_VORBISFILE=ON \
	-DWITH_XMP=ON
cmake --build build
DESTDIR=$PKG cmake --install build

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
# The window's app_id is the program's name.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 prboom2/ICONS/dsda-doom.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/dsda-doom.png"
cat > "$PKG/usr/share/applications/dsda-doom.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=DSDA-Doom
GenericName=First Person Shooter
Comment=Play Doom and Boom-compatible maps, with demo recording for speedruns
Exec=dsda-doom %F
Icon=dsda-doom
Terminal=false
StartupWMClass=dsda-doom
Categories=Game;ActionGame;
MimeType=application/x-doom-wad;
Keywords=first;person;shooter;doom;boom;mbf;prboom;freedoom;
DESKTOP
chmod 644 "$PKG/usr/share/applications/dsda-doom.desktop"

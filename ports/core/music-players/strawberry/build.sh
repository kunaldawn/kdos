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

# The collection, playlists, tags and playback work with no network. Tidal,
# Qobuz, Spotify and Subsonic are account-backed streaming services and are
# compiled out, as is Discord presence. Song tracking fingerprints the
# collection with Chromaprint, locally, so a moved or renamed file keeps its
# play counts and ratings. The tag fetcher is off: it sends fingerprints to
# AcoustID and MusicBrainz. iPod classic goes through libgpod, whose artwork
# is read and written with gdk-pixbuf. X11 global shortcuts grab
# keys on an X server a Wayland session never gives them; the media keys
# arrive over MPRIS instead. Every component is named so the set is explicit,
# but a component switched on whose library is missing is still dropped, with
# only a line in the configure summary; that summary is where a missing
# dependency shows.
#
# Translations are compiled into the binary when on; bundled data is English
# only, so they are off. The test suite is built whenever GTest is found, so
# the lookup is disabled.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_DISABLE_FIND_PACKAGE_GTest=ON \
	-D BUILD_WERROR=OFF \
	-D ENABLE_ALSA=ON \
	-D ENABLE_PULSE=ON \
	-D ENABLE_DBUS=ON \
	-D ENABLE_MPRIS2=ON \
	-D ENABLE_KGLOBALACCEL_GLOBALSHORTCUTS=ON \
	-D ENABLE_UDISKS2=ON \
	-D ENABLE_GIO=ON \
	-D ENABLE_GIO_UNIX=ON \
	-D ENABLE_AUDIOCD=ON \
	-D ENABLE_MTP=ON \
	-D ENABLE_MOODBAR=ON \
	-D ENABLE_GSTFASTSPECTRUM=ON \
	-D ENABLE_WAVEFORM=ON \
	-D ENABLE_EBUR128=ON \
	-D ENABLE_QPA_QPLATFORMNATIVEINTERFACE=ON \
	-D ENABLE_STREAMTAGREADER=ON \
	-D ENABLE_X11_GLOBALSHORTCUTS=OFF \
	-D ENABLE_CHROMAPRINT=ON \
	-D ENABLE_SONGTRACKING=ON \
	-D ENABLE_TAGFETCHER=OFF \
	-D ENABLE_GPOD=ON \
	-D ENABLE_SUBSONIC=OFF \
	-D ENABLE_TIDAL=OFF \
	-D ENABLE_SPOTIFY=OFF \
	-D ENABLE_QOBUZ=OFF \
	-D ENABLE_DISCORD_RPC=OFF \
	-D ENABLE_TRANSLATIONS=OFF \
	-D INSTALL_TRANSLATIONS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: main() sets the desktop
# file name, so the Wayland app_id is org.strawberrymusicplayer.strawberry,
# not upstream's "strawberry". The tidal: scheme handler goes with the Tidal
# service. The hicolor PNGs it names are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.strawberrymusicplayer.strawberry.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Strawberry
GenericName=Music Player
Comment=Play and organise your music collection
TryExec=strawberry
Exec=strawberry %U
Icon=strawberry
Terminal=false
StartupWMClass=org.strawberrymusicplayer.strawberry
Categories=AudioVideo;Audio;Player;Qt;
Keywords=music;player;audio;playlist;collection;flac;mp3;strawberry;
MimeType=x-content/audio-player;application/ogg;application/x-ogg;application/x-ogm-audio;audio/flac;audio/ogg;audio/vorbis;audio/aac;audio/mp4;audio/mpeg;audio/mpegurl;audio/vnd.rn-realaudio;audio/x-flac;audio/x-oggflac;audio/x-vorbis;audio/x-vorbis+ogg;audio/x-speex;audio/x-wav;audio/x-wavpack;audio/x-ape;audio/x-mp3;audio/x-mpeg;audio/x-mpegurl;audio/x-ms-wma;audio/x-musepack;audio/x-pn-realaudio;audio/x-scpls;
Actions=Play-Pause;Stop;Previous;Next;

[Desktop Action Play-Pause]
Name=Play/Pause
Exec=strawberry --play-pause

[Desktop Action Stop]
Name=Stop
Exec=strawberry --stop

[Desktop Action Previous]
Name=Previous
Exec=strawberry --previous

[Desktop Action Next]
Name=Next
Exec=strawberry --next
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.strawberrymusicplayer.strawberry.desktop"

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

patch -p1 -i "$PORT_SRC/fix-destdir-symlinks.patch"
patch -p1 -i "$PORT_SRC/wayland-platform.patch"

export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_SKIP_RPATH=ON \
	-D FETCHCONTENT_FULLY_DISCONNECTED=ON \
	-D FETCHCONTENT_SOURCE_DIR_MNXDOM="$SRC_ROOT/mnxdom-$_mnxdom" \
	-D FETCHCONTENT_SOURCE_DIR_JSON_SCHEMA_VALIDATOR="$SRC_ROOT/json-schema-validator-$_jsv" \
	-D USE_SYSTEM_NLOHMANN_JSON=ON \
	-D USE_SYSTEM_JSON_SCHEMA_VALIDATOR=OFF \
	-D mnxdom_BUILD_TESTING=OFF \
	-D JSON_VALIDATOR_BUILD_TESTS=OFF \
	-D JSON_VALIDATOR_BUILD_EXAMPLES=OFF \
	-D JSON_VALIDATOR_INSTALL=OFF \
	-D MUSESCORE_BUILD_CONFIGURATION=app \
	-D MUSE_APP_BUILD_MODE=release \
	-D MUSE_ENABLE_UNIT_TESTS=OFF \
	-D MUSE_COMPILE_USE_COMPILER_CACHE=OFF \
	-D MUSE_MODULE_UPDATE=OFF \
	-D MUSE_MODULE_CLOUD=OFF \
	-D MUSE_MODULE_LEARN=OFF \
	-D MUSE_MODULE_MUSESAMPLER=OFF \
	-D MUE_BUILD_MUSESOUNDS_MODULE=OFF \
	-D MUE_BUILD_MUSESOUNDS_QML=ON \
	-D MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT=OFF \
	-D MUSE_MODULE_VST=OFF \
	-D MUSE_MODULE_AUDIO_JACK=OFF \
	-D MUSE_MODULE_AUDIO_PIPEWIRE=ON \
	-D MUE_DOWNLOAD_SOUNDFONT=OFF \
	-D MUE_INSTALL_SOUNDFONT=ON \
	-D MUE_RUN_LRELEASE=OFF \
	-D MUE_COMPILE_USE_SYSTEM_FLAC=ON \
	-D MUE_COMPILE_USE_SYSTEM_OPUS=ON \
	-D MUE_COMPILE_USE_SYSTEM_OPUSENC=ON \
	-D MUE_COMPILE_USE_SYSTEM_LAME=ON \
	-D MUE_COMPILE_USE_SYSTEM_HARFBUZZ=ON \
	-D MUE_COMPILE_USE_SYSTEM_FREETYPE=ON \
	-D MUE_COMPILE_USE_SYSTEM_PUGIXML=ON \
	-D MUE_COMPILE_USE_SYSTEM_UTF8CPP=ON \
	-D MUE_COMPILE_USE_SYSTEM_MNXDOM=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/include" "$PKG/usr/lib/pkgconfig"
find "$PKG/usr/lib" -name '*.a' -delete

find "$PKG/usr/share" -path '*/locale/*.qm' -delete

cat > "$PKG/usr/share/applications/org.musescore.MuseScore.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=MuseScore Studio
GenericName=Music Notation
Comment=Create, play and print sheet music
TryExec=mscore
Exec=mscore %U
Icon=mscore
Terminal=false
StartupNotify=true
StartupWMClass=org.musescore.MuseScore
MimeType=application/x-musescore;application/x-musescore+xml;application/vnd.recordare.musicxml;application/vnd.recordare.musicxml+xml;application/x-mei+xml;application/x-bww;application/x-biab;application/x-capella;audio/x-gtp;application/x-musedata;application/x-overture;audio/x-ptb;application/x-tef;
Categories=AudioVideo;Audio;Midi;Sequencer;Music;Publishing;Qt;
Keywords=music;notation;composition;composing;arranging;sheet music;score;scorewriter;midi;musicxml;musescore;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.musescore.MuseScore.desktop"

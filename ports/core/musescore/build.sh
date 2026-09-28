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

# The install writes the musescore alias links under the real prefix rather
# than under DESTDIR, which puts them in the build root and leaves them out of
# the package. The patch is upstream's (pull request 33819), carried by
# Alpine.
patch -p1 -i "$PORT_SRC/fix-destdir-symlinks.patch"

# The MNX importer's DOM library and its JSON Schema validator are fetched
# by FetchContent at pinned commits; they are sources of this recipe, and
# FETCHCONTENT_SOURCE_DIR_* points each declaration at its unpacked tree, so
# nothing is cloned. JSON comes from the nlohmann-json port. The two build as
# static libraries inside the program, and their install rules are removed
# from the package below.
#
# Offline: the MuseScore.com account and cloud scores, the Learn page, the
# update checker and the Muse Sounds / Muse Sampler integration (proprietary
# instruments delivered online) are replaced by upstream's stubs. The crash
# reporter is off, VST3 needs Steinberg's SDK, and JACK is not on this system;
# audio is PipeWire with ALSA beside it. The MS Basic soundfont ships in the
# release tree and is installed from it, so the download step stays off.
#
# Bundled FLAC, Opus, libopusenc, LAME, HarfBuzz, FreeType, pugixml and
# utf8cpp are replaced by the ports. A missing
# system HarfBuzz would make CMake download upstream's build recipe for one,
# so its port is required. MUE_RUN_LRELEASE=OFF leaves the interface
# catalogues out: bundled data is English only. -D_LARGEFILE64_SOURCE keeps
# the 64-bit file offset names musl provides only under that macro.
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

# The fetched libraries' own install rules add a static archive, headers and
# a pkg-config file nothing on the system links against.
rm -rf "$PKG/usr/include" "$PKG/usr/lib/pkgconfig"
find "$PKG/usr/lib" -name '*.a' -delete

# Qt's own catalogues are copied in from qt6-qttranslations when it is
# installed; bundled data is English only. languages.json stays: the
# preferences page reads it.
find "$PKG/usr/share" -path '*/locale/*.qm' -delete

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass. main() sets
# QT_QPA_PLATFORM=xcb unless MU_QT_QPA_PLATFORM is set, so the window is an
# Xwayland one, and the compositor takes its app_id from the WM_CLASS
# instance, the executable's name: mscore. Run with MU_QT_QPA_PLATFORM=wayland
# the app_id is the desktop file name, which the entry's own name matches.
# audio/midi and the soundfont types are left to the players and
# synthesisers; the score formats stay. The hicolor PNGs it names are
# upstream's, installed above.
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
StartupWMClass=mscore
MimeType=application/x-musescore;application/x-musescore+xml;application/vnd.recordare.musicxml;application/vnd.recordare.musicxml+xml;application/x-mei+xml;application/x-bww;application/x-biab;application/x-capella;audio/x-gtp;application/x-musedata;application/x-overture;audio/x-ptb;application/x-tef;
Categories=AudioVideo;Audio;Midi;Sequencer;Music;Publishing;Qt;
Keywords=music;notation;composition;composing;arranging;sheet music;score;scorewriter;midi;musicxml;musescore;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.musescore.MuseScore.desktop"

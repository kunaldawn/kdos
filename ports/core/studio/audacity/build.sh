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

# DEPENDENCIES. The release tarball carries every dependency source its
# muse_deps engine can fetch, under offline-deps/downloads; EXTDEPS_CACHE
# points the engine there, so nothing is downloaded. EXTDEPS_OVERRIDE_ALL=SYSTEM
# takes each library dependency (expat, the Ogg, Vorbis, FLAC, Opus, LAME,
# mpg123, WavPack and sndfile codecs, PortAudio, libpng, zlib, FreeType,
# HarfBuzz, wxBase) from its port instead of a prebuilt archive, and pugixml
# too. The remaining source dependencies have no system path in the engine
# and are compiled into the program from the tarball's copies: SQLite,
# soxr, SoundTouch, SBSMS, twolame, the LV2 host stack, Nyquist, pffft, the
# VST3 SDK the framework always compiles, and a few header libraries.
# utf8cpp is header-only and is rebuilt from its tarball copy, which a
# blanket SYSTEM would otherwise swap for the utfcpp port's CMake package.
# The engine applies its own source patches with 'git apply', so git is a
# build dependency.
#
# OFFLINE: the audio.com cloud, the usage-info module, the update checker and
# the crash reporter are replaced by upstream's stubs or left out. VST3
# effects are off. FFmpeg import and export is loaded at run time from the
# ffmpeg port. AU_RUN_LRELEASE=OFF leaves the interface catalogues out:
# bundled data is English only.
export EXTDEPS_CACHE="$SRC/offline-deps"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D EXTDEPS_CACHE="$SRC/offline-deps" \
	-D EXTDEPS_OVERRIDE_ALL=SYSTEM \
	-D EXTDEPS_OVERRIDE_UTFCPP=REBUILD \
	-D AU4_BUILD_CONFIGURATION=app \
	-D AU4_BUILD_MODE=release \
	-D MUSE_ENABLE_UNIT_TESTS=OFF \
	-D MUSE_COMPILE_USE_CCACHE=OFF \
	-D MUSE_MODULE_UPDATE=OFF \
	-D MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT=OFF \
	-D AU_BUILD_CLOUD_AUDIOCOM=OFF \
	-D AU_BUILD_USAGEINFO_MODULE=OFF \
	-D AU_USE_LIBCURL=OFF \
	-D AU_MODULE_EFFECTS_VST=OFF \
	-D AU_MODULE_EFFECTS_LV2=ON \
	-D AU_MODULE_EFFECTS_NYQUIST=ON \
	-D AU_USE_SBSMS=ON \
	-D AU_USE_SOUNDTOUCH=ON \
	-D AU_USE_PORTMIXER=ON \
	-D AU_RUN_LRELEASE=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Qt's own catalogues are copied in from qt6-qttranslations when it is
# installed; bundled data is English only. languages.json stays: the
# preferences page reads it.
find "$PKG/usr/share" -path '*/locale/*.qm' -delete

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass. main() sets
# QT_QPA_PLATFORM=xcb unless AU_QT_QPA_PLATFORM is set, so the window is an
# Xwayland one, and the compositor takes its app_id from the WM_CLASS
# instance, the executable's name: audacity. Run with
# AU_QT_QPA_PLATFORM=wayland the app_id is the desktop file name, which the
# entry's own name matches. It claims only its own project type; audio files
# open in the music player. The hicolor PNGs it names are upstream's,
# installed above.
cat > "$PKG/usr/share/applications/org.audacityteam.Audacity.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Audacity
GenericName=Sound Editor
Comment=Record and edit audio files
TryExec=audacity
Exec=audacity %U
Icon=audacity
Terminal=false
StartupNotify=true
StartupWMClass=audacity
MimeType=application/x-audacity;
Categories=AudioVideo;Audio;AudioVideoEditing;Recorder;Qt;
Keywords=sound;audio;editor;record;recorder;multitrack;mixing;noise reduction;wav;flac;mp3;audacity;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.audacityteam.Audacity.desktop"

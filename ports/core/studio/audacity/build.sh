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

patch -p1 -i "$PORT_SRC/sqlite-helpers-cstdint.patch"
patch -p1 -i "$PORT_SRC/beats-formatter-pow10.patch"
patch -p1 -i "$PORT_SRC/vst-stub-include.patch"
patch -p1 -i "$PORT_SRC/wayland-platform.patch"
export EXTDEPS_CACHE="$SRC/offline-deps"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_SKIP_INSTALL_RPATH=ON \
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

find "$PKG/usr/share" -path '*/locale/*.qm' -delete

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
StartupWMClass=org.audacityteam.Audacity
MimeType=application/x-audacity;
Categories=AudioVideo;Audio;AudioVideoEditing;Recorder;Qt;
Keywords=sound;audio;editor;record;recorder;multitrack;mixing;noise reduction;wav;flac;mp3;audacity;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.audacityteam.Audacity.desktop"

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

# Playback and recording through PulseAudio (pipewire-pulse on this system),
# ALSA and Qt Multimedia; OSS is a kernel interface this system does not
# carry. MP3 decodes through libmad with tags through id3lib, and encodes by
# running the lame program. Each codec is REQUIRED once its switch is on, so
# a missing library fails the build rather than dropping the format.
# KF_SKIP_PO_PROCESSING leaves the interface catalogues out, and
# kdoctools_install() still builds every translated handbook, removed after
# the install: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D WITH_DOC=ON \
	-D WITH_ALSA=ON \
	-D WITH_PULSEAUDIO=ON \
	-D WITH_QT_AUDIO=ON \
	-D WITH_OSS=OFF \
	-D WITH_FLAC=ON \
	-D WITH_OGG_OPUS=ON \
	-D WITH_OGG_VORBIS=ON \
	-D WITH_MP3=ON \
	-D DEBUG=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# The application icon is installed as SVG only, which the panel never reads.
for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/org.kde.kwave.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/org.kde.kwave.svg"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.kwave. MimeType is left out: an
# editor claiming audio files would open them in place of the music player.
cat > "$PKG/usr/share/applications/org.kde.kwave.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kwave
GenericName=Sound Editor
Comment=Record, edit and convert sound files
TryExec=kwave
Exec=kwave %F
Icon=org.kde.kwave
Terminal=false
StartupWMClass=org.kde.kwave
X-DBUS-StartupType=Unique
X-DocPath=kwave/index.html
Categories=Qt;KDE;AudioVideo;Audio;AudioVideoEditing;Recorder;
Keywords=sound;audio;editor;wave;record;recorder;cut;kwave;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kwave.desktop"

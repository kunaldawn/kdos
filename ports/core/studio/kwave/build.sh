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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
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

for _size in 32 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	rsvg-convert -w "$_size" -h "$_size" \
		-o "$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/org.kde.kwave.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/org.kde.kwave.svg"
done

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

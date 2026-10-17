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
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_SKIP_BUILD_RPATH=ON \
	-D VCPKG=OFF \
	-D SCCACHE=OFF \
	-D CCACHE=OFF \
	-D BUILD_STATIC_LIBS=OFF \
	-D ID3TAG=ON \
	-D MP3_DECODING=ON \
	-D OGG=ON \
	-D VORBIS=ON \
	-D FLAC=ON \
	-D SBSMS=ON \
	-D SOUNDTOUCH=ON \
	-D VAMP=ON \
	-D LV2=ON \
	-D LADSPA=ON \
	-D VST2=ON \
	-D MIDI=ON \
	-D MATROSKA=ON \
	-D MP2=ON \
	-D FFMPEG=OFF \
	-D PERFORM_CODESIGN=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

for _size in 16 22 24 32 48; do
	_dir="$PKG/usr/share/icons/hicolor/${_size}x${_size}"
	install -d "$_dir/apps"
	mv "$_dir/tenacity.png" "$_dir/apps/tenacity.png"
done

cat > "$PKG/usr/share/applications/tenacity.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Tenacity
GenericName=Sound Editor
Comment=Record and edit audio files
TryExec=tenacity
Exec=tenacity %F
Icon=tenacity
Terminal=false
StartupNotify=false
StartupWMClass=tenacity
Categories=AudioVideo;Audio;AudioVideoEditing;Recorder;
Keywords=audio;editor;sound;wave;record;multitrack;audacity;tenacity;
DESKTOP
chmod 644 "$PKG/usr/share/applications/tenacity.desktop"

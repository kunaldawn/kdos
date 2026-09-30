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

# Dependencies come from the system; vcpkg is off, and it is the default only
# on Windows and macOS. Every codec and plugin host is named ON so the set is
# explicit, but each is a cmake_dependent_option that still turns itself off
# when its library is not found, with only a status line saying so: MIDI
# needs PortMidi and PortSMF, Matroska needs libmatroska over libebml, and
# MP2 export needs twolame. FFmpeg import and export is off: it is loaded at
# run time and knows libavformat up to 61 (FFmpeg 7), which the ffmpeg port
# is newer than, so it would never find a library to load. VST2 hosting
# loads plugin shared objects the user installs; nothing ships with it.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
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

# Bundled data is English only; wxWidgets falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# Upstream installs its PNG icons into hicolor/<size>/ with no apps/ level,
# where no icon lookup finds them; they are moved to hicolor/<size>/apps/.
for _size in 16 22 24 32 48; do
	_dir="$PKG/usr/share/icons/hicolor/${_size}x${_size}"
	install -d "$_dir/apps"
	mv "$_dir/tenacity.png" "$_dir/apps/tenacity.png"
done

# UPSTREAM'S ENTRY IS REPLACED. The Wayland app_id is GLib's program name,
# tenacity, which StartupWMClass has to name. Exec drops upstream's env
# wrapper, a workaround for Ubuntu's Unity menu proxy. MimeType is left out:
# Audacity is the audio editor that claims project and audio files.
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

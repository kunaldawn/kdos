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

export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_SKIP_INSTALL_RPATH=ON \
	-D CMAKE_COMPILE_WARNING_AS_ERROR=OFF \
	-D OBS_VERSION_OVERRIDE=$version \
	-D OBS_COMPILE_DEPRECATION_AS_WARNING=ON \
	-D OpenGL_GL_PREFERENCE=GLVND \
	-D ENABLE_FRONTEND=ON \
	-D ENABLE_WAYLAND=ON \
	-D ENABLE_PIPEWIRE=ON \
	-D ENABLE_PULSEAUDIO=ON \
	-D ENABLE_ALSA=ON \
	-D ENABLE_V4L2=ON \
	-D ENABLE_UDEV=ON \
	-D ENABLE_JACK=ON \
	-D ENABLE_SNDIO=OFF \
	-D ENABLE_LIBFDK=OFF \
	-D ENABLE_HEVC=ON \
	-D ENABLE_FREETYPE=ON \
	-D ENABLE_SPEEXDSP=ON \
	-D ENABLE_RNNOISE=ON \
	-D ENABLE_VST=ON \
	-D ENABLE_SCRIPTING=ON \
	-D ENABLE_SCRIPTING_LUA=ON \
	-D ENABLE_SCRIPTING_PYTHON=ON \
	-D ENABLE_BROWSER=OFF \
	-D ENABLE_WEBSOCKET=OFF \
	-D ENABLE_WHATSNEW=OFF \
	-D ENABLE_SERVICE_UPDATES=OFF \
	-D ENABLE_WEBRTC=OFF \
	-D ENABLE_NEW_MPEGTS_OUTPUT=ON \
	-D ENABLE_VLC=ON \
	-D ENABLE_AJA=OFF \
	-D ENABLE_DECKLINK=OFF \
	-D ENABLE_NVENC=OFF \
	-D ENABLE_FFMPEG_NVENC=OFF \
	-D ENABLE_QSV11=OFF \
	-D ENABLE_TEST_INPUT=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/obs" -path '*/locale/*.ini' ! -name en-US.ini -delete

cat > "$PKG/usr/share/applications/com.obsproject.Studio.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OBS Studio
GenericName=Screen Recorder
Comment=Record the screen and stream live
Exec=obs
Icon=com.obsproject.Studio
Terminal=false
StartupNotify=true
StartupWMClass=com.obsproject.Studio
Categories=AudioVideo;Recorder;
Keywords=recording;screencast;stream;obs;capture;
DESKTOP
chmod 644 "$PKG/usr/share/applications/com.obsproject.Studio.desktop"

for _p in vlc-video linux-jack obs-ffmpeg; do
	test -f "$PKG/usr/lib/obs-plugins/$_p.so"
done
readelf -d "$PKG/usr/lib/obs-plugins/obs-filters.so" | grep -q 'librnnoise'
readelf -d "$PKG/usr/lib/obs-plugins/obs-ffmpeg.so" | grep -q 'libsrt'

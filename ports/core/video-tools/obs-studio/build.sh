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

# Screen capture on Wayland is linux-pipewire through the ScreenCast portal;
# linux-capture (XComposite, XSHM) sees only Xwayland windows. libobs links
# libX11 and xcb unconditionally, and the xcb capture plugin comes with it.
#
# Everything that reaches the network on its own is off: the browser source
# and the What's New panel need CEF, obs-websocket needs websocketpp and
# asio (no ports), and ENABLE_SERVICE_UPDATES would download the streaming
# service list at every start. The crash-log upload runs only when the user
# answers the unclean-shutdown dialog with send.
#
# JACK is not on this system, and sndio, AJA, DeckLink, NVENC and QSV (libvpl)
# have no ports. FDK AAC is off because its licence cannot be combined with
# the GPL of the binary that ships; the FFmpeg AAC encoder takes its place. VLC sources need libvlc and WebRTC needs libdatachannel;
# the SRT and RIST mpegts output needs librist and libsrt. RNNoise is the
# copy in the source tree: with no system library, the filter builds it.
#
# OBS_VERSION_OVERRIDE: the release tarball has no .git, and without it the
# version is read from git and comes out as 0.0.1.
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
	-D ENABLE_JACK=OFF \
	-D ENABLE_SNDIO=OFF \
	-D ENABLE_OSS=OFF \
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
	-D ENABLE_NEW_MPEGTS_OUTPUT=OFF \
	-D ENABLE_VLC=OFF \
	-D ENABLE_AJA=OFF \
	-D ENABLE_DECKLINK=OFF \
	-D ENABLE_NVENC=OFF \
	-D ENABLE_FFMPEG_NVENC=OFF \
	-D ENABLE_QSV11=OFF \
	-D ENABLE_TEST_INPUT=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: every module falls back to its en-US.ini.
find "$PKG/usr/share/obs" -path '*/locale/*.ini' ! -name en-US.ini -delete

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the Wayland app_id is the
# desktop file name OBS sets, com.obsproject.Studio, not "obs". The hicolor
# PNGs of that name are upstream's, installed above.
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

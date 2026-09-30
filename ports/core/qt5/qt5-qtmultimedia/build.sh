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


# GStreamer 1.0 is the playback, recording and camera backend; audio output
# goes to PulseAudio, which pipewire-pulse answers, with ALSA as the fallback.
# gstreamer_gl hands decoded frames to the scene graph as GL textures instead
# of copying them through the CPU; photography is the camera settings API from
# gst-plugins-bad. The good and libav plugins are what QMediaPlayer decodes
# with: GStreamer finds them at run time, and without them nothing plays.
# OpenAL (QSoundEffect's alternative) is not a port.
/usr/lib/qt5/bin/qmake -- \
	-gstreamer 1.0 \
	-pulseaudio \
	-alsa \
	-feature-gstreamer_app \
	-feature-gstreamer_gl \
	-feature-gstreamer_photography \
	-feature-gstreamer_encodingprofiles \
	-feature-linux_v4l \
	-no-feature-openal \
	-no-feature-resourcepolicy
make
make INSTALL_ROOT="$PKG" install

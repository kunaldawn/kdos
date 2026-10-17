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

/usr/lib/qt5/bin/qmake -- \
	-gstreamer 1.0 \
	-pulseaudio \
	-alsa \
	-feature-gstreamer_app \
	-feature-gstreamer_gl \
	-feature-gstreamer_photography \
	-feature-gstreamer_encodingprofiles \
	-feature-linux_v4l \
	-feature-openal \
	-no-feature-resourcepolicy
make
make INSTALL_ROOT="$PKG" install

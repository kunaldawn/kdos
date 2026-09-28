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

# KDE's webcam booth: a Kirigami window over a GStreamer camerabin pipeline
# (gst-plugins-bad), drawn into QML by qml6glsink, the Qt 6 element of
# gst-plugins-good; without that element in the image Kamoso opens a window
# with no picture. Cameras are enumerated through GStreamer's device monitor,
# which lists PipeWire's camera nodes and V4L2 devices. Sharing a picture goes
# through Purpose.
#
# GStreamer here is 1.28, newer than the 1.26.3 fix PATCHED_GSTREAMER stands
# in for, so the option stays off. KF_SKIP_PO_PROCESSING: bundled data is
# English only, and the translated handbooks kdoctools builds are removed.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_DOC=ON \
	-D PATCHED_GSTREAMER=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData derives the
# desktop file name from kde.org and "kamoso", so the Wayland app_id is
# org.kde.kamoso. The hicolor PNGs it names are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.kamoso.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kamoso
GenericName=Camera
Comment=Take pictures and videos with a webcam
TryExec=kamoso
Exec=kamoso
Icon=kamoso
Terminal=false
StartupWMClass=org.kde.kamoso
X-DocPath=kamoso/index.html
Categories=Qt;KDE;AudioVideo;Video;Recorder;
Keywords=camera;webcam;photo;picture;video;record;kamoso;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kamoso.desktop"

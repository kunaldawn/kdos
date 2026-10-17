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
	-D BUILD_DOC=ON \
	-D PATCHED_GSTREAMER=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

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

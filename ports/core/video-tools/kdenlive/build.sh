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
	-D RELEASE_BUILD=ON \
	-D USE_DBUS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D BUILD_FUZZING=OFF \
	-D CRASH_AUTO_TEST=OFF \
	-D BUILD_DESIGNERPLUGIN=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kdenlive.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kdenlive
GenericName=Video Editor
Comment=Edit video on a multi-track timeline
Exec=kdenlive %F
Icon=kdenlive
Terminal=false
StartupWMClass=org.kde.kdenlive
X-DocPath=kdenlive/index.html
MimeType=application/x-kdenlive;
Categories=Qt;KDE;AudioVideo;AudioVideoEditing;
Keywords=editing;video;audio;mlt;timeline;cut;kdenlive;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kdenlive.desktop"

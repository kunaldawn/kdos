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

# qml-box2d, the physics QML module several activities import, is a git
# submodule the release archive carries empty. It is a later source, placed
# where cmake/box2d.cmake builds it as an external project and installs it
# under /usr/lib/qml, which gcompris-qt adds to its import path.
cp -a "$SRC_ROOT/qml-box2d-$_box2d/." external/qml-box2d/

# WITH_DOWNLOAD=OFF removes the downloader, so nothing reaches for the network
# and the settings page offers no download. SKIP_TRANSLATIONS leaves the
# interface catalogues out: bundled data is English only. The teachers'
# server is built only for upstream's own packaging and is never installed,
# so it is not built.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D WITH_DOWNLOAD=OFF \
	-D SKIP_TRANSLATIONS=ON \
	-D BUILD_SERVER=OFF \
	-D COMPILE_DOC=OFF \
	-D QML_BOX2D_MODULE=submodule \
	-D COMPRESSED_AUDIO=ogg \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The word pictures, the English voices and the background music are the
# resource files the downloader would otherwise fetch on first use. Each
# directory's Contents file lists its files with their md5, the format the
# download manager reads to register what is present.
rcc="$PKG/usr/share/gcompris-qt/rcc/data3"
place() {
	install -Dm644 "$2" "$rcc/$1/$2"
	(cd "$rcc/$1" && md5sum "$2" >> Contents)
}
place words words-webp-$_words.rcc
place voices-ogg voices-en_US-$_voices.rcc
place voices-ogg voices-en_GB-$_voices.rcc
place backgroundMusic backgroundMusic-ogg-$_music.rcc
chmod 644 "$rcc"/*/Contents

# The entry is replaced for StartupWMClass: gcompris-qt sets its desktop file
# name, and so its Wayland app_id, to org.kde.gcompris.
cat > "$PKG/usr/share/applications/org.kde.gcompris.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GCompris
GenericName=Educational Game for Children
Comment=Activities for children aged 2 to 10
TryExec=gcompris-qt
Exec=gcompris-qt
Icon=gcompris-qt
Terminal=false
StartupWMClass=org.kde.gcompris
Categories=Qt;KDE;Education;Game;KidsGame;
Keywords=children;kids;learn;school;reading;counting;game;gcompris;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.gcompris.desktop"

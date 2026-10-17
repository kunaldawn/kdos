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

cp -a "$SRC_ROOT/qml-box2d-$_box2d/." external/qml-box2d/

echo "set(QT_NO_QML_PLUGIN_RPATH ON)" > "$SRC_ROOT/no-qml-rpath.cmake"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_TOOLCHAIN_FILE="$SRC_ROOT/no-qml-rpath.cmake" \
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

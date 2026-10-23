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

patch -p1 -i "$PORT_SRC/offline-defaults.patch"

export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=8388608"
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_SKIP_INSTALL_RPATH=ON \
	-D WITH_EPWING_SUPPORT=OFF \
	-D WITH_FFMPEG_PLAYER=OFF \
	-D WITH_QT_MULTIMEDIA=ON \
	-D WITH_ZIM=ON \
	-D WITH_TTS=ON \
	-D WITH_X11=ON \
	-D USE_ALTERNATIVE_NAME=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/goldendict/locale"

install -Dm644 redist/icons/goldendict.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/goldendict.png"

cat > "$PKG/usr/share/applications/io.github.xiaoyifang.goldendict_ng.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GoldenDict-ng
GenericName=Dictionary
Comment=Look words up in local dictionaries
Exec=goldendict %u
Icon=goldendict
Terminal=false
StartupWMClass=io.github.xiaoyifang.goldendict_ng
MimeType=x-scheme-handler/goldendict;x-scheme-handler/dict;
Categories=Qt;Office;Dictionary;Education;
Keywords=dict;dictionary;thesaurus;define;word;translate;stardict;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.github.xiaoyifang.goldendict_ng.desktop"

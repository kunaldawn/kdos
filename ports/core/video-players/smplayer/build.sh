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

patch -p1 -i "$PORT_SRC/offline.patch"

_qmake_opts="DEFINES+=NO_MPLAYER QMAKE_CFLAGS_RELEASE=\"$CFLAGS\" QMAKE_CXXFLAGS_RELEASE=\"$CXXFLAGS\" QMAKE_LFLAGS_RELEASE=\"$LDFLAGS\""
make PREFIX=/usr QMAKE=qmake-qt5 LRELEASE=/usr/lib/qt5/bin/lrelease \
	QMAKE_OPTS="$_qmake_opts" CFLAGS_EXTRA=-Wno-error
make PREFIX=/usr QMAKE=qmake-qt5 LRELEASE=/usr/lib/qt5/bin/lrelease \
	QMAKE_OPTS="$_qmake_opts" CFLAGS_EXTRA=-Wno-error \
	DESTDIR=$PKG install

rm -f "$PKG/usr/bin/simple_web_server"

find "$PKG/usr/share/smplayer/translations" -name '*.qm' ! -name 'smplayer_en*.qm' -delete
find "$PKG/usr/share/doc/packages/smplayer" -mindepth 1 -maxdepth 1 -type d ! -name en -exec rm -rf {} +

gunzip "$PKG/usr/share/man/man1/smplayer.1.gz"

rm -f "$PKG/usr/share/applications/smplayer_enqueue.desktop"
cat > "$PKG/usr/share/applications/smplayer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SMPlayer
GenericName=Media Player
Comment=Play videos and music with mpv, remembering where each file stopped
TryExec=smplayer
Exec=smplayer %U
Icon=smplayer
Terminal=false
StartupWMClass=smplayer
Categories=Qt;AudioVideo;Player;Video;
Keywords=video;movie;player;mpv;subtitles;smplayer;
DESKTOP
chmod 644 "$PKG/usr/share/applications/smplayer.desktop"

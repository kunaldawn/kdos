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

# A Qt front end that runs mpv as a child process and has it draw into its own
# window by X11 window id, so SMPlayer sets QT_QPA_PLATFORM=xcb for itself and
# runs under Xwayland, and the mpv it runs needs its X11 video output.
#
# offline.patch turns off four features that exist only to reach the
# internet: the update checker, the OpenSubtitles search, the YouTube URL
# resolver (it downloads a yt-dlp binary into the home directory) and the
# Chromecast sender with its bundled web server. The project file sets them
# unconditionally; there is no qmake switch for any of them alone.
# DEFINES+=NO_MPLAYER is upstream's switch for the MPlayer backend, which is
# not a port; mpv is the one player it drives.
patch -p1 -i "$PORT_SRC/offline.patch"

# qmake 6 reads no compiler flags from the environment, so they are handed to
# it as QMAKE_* assignments; without them the reproducibility flags never
# reach the binary. DOC_PATH and the other data paths stay upstream's: they
# reach the compiler as quoted string defines through the environment, and a
# command-line value would reach the sub-make unquoted. The top-level `all`
# also builds the Chromecast web server, whose makefile adds -Werror;
# CFLAGS_EXTRA is its own hook and -Wno-error comes after it.
_qmake_opts="DEFINES+=NO_MPLAYER QMAKE_CFLAGS_RELEASE=\"$CFLAGS\" QMAKE_CXXFLAGS_RELEASE=\"$CXXFLAGS\" QMAKE_LFLAGS_RELEASE=\"$LDFLAGS\""
make PREFIX=/usr QMAKE=qmake6 LRELEASE=/usr/lib/qt6/bin/lrelease \
	QMAKE_OPTS="$_qmake_opts" CFLAGS_EXTRA=-Wno-error
make PREFIX=/usr QMAKE=qmake6 LRELEASE=/usr/lib/qt6/bin/lrelease \
	QMAKE_OPTS="$_qmake_opts" CFLAGS_EXTRA=-Wno-error \
	DESTDIR=$PKG install

# The web server only serves the Chromecast sender, which is not built.
rm -f "$PKG/usr/bin/simple_web_server"

# Bundled data is English only: every catalogue but English goes, and the
# manual keeps its English pages.
find "$PKG/usr/share/smplayer/translations" -name '*.qm' ! -name 'smplayer_en*.qm' -delete
find "$PKG/usr/share/doc/packages/smplayer" -mindepth 1 -maxdepth 1 -type d ! -name en -exec rm -rf {} +

# The install gzips the page and gzip stamps the time it ran into the file;
# man reads it uncompressed.
gunzip "$PKG/usr/share/man/man1/smplayer.1.gz"

# UPSTREAM'S ENTRIES ARE REPLACED. Qt's xcb backend sets WM_CLASS from the
# program name, smplayer. MimeType is left out: mpv's entry claims the video
# and audio types, and mimeapps.list is where the default player is chosen.
# The enqueue entry is a file-manager action and not a program of its own.
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

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

qmake6 qdirstat.pro \
	INSTALL_PREFIX=/usr \
	CONFIG+=release \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s src/icons/qdirstat.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/qdirstat.png"
done

cat > "$PKG/usr/share/applications/qdirstat.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QDirStat
GenericName=Directory Statistics
Comment=Show what takes the space on a disk, and clean it up
TryExec=qdirstat
Exec=qdirstat %f
Icon=qdirstat
Terminal=false
StartupWMClass=qdirstat
Categories=Qt;System;Filesystem;
Keywords=directory;tree;size;statistic;disk;space;usage;treemap;
DESKTOP
chmod 644 "$PKG/usr/share/applications/qdirstat.desktop"

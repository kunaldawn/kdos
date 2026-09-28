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

# A qmake project, built in the source tree: the man page rule gzips its pages
# from the directory it runs in and finds nothing from a separate build
# directory. Qt 6's qmake reads no compiler flags from the environment, so the
# tree's flags, and with them the reproducibility maps, are passed in.
# qdirstat-cache-writer is a Perl script that writes the cache files the
# program can load instead of scanning.
qmake6 qdirstat.pro \
	INSTALL_PREFIX=/usr \
	CONFIG+=release \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s src/icons/qdirstat.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/qdirstat.png"
done

# UPSTREAM'S ENTRY IS REPLACED. With no desktop file name and no organisation
# domain set, Qt takes the Wayland app_id from the program name, qdirstat.
# inode/directory is left out of MimeType: claimed here, a folder could open
# in the disk-usage view instead of the file manager.
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

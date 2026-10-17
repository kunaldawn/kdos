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

./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man \
	ac_cv_path_INKSCAPE= \
	--enable-gui \
	--enable-dbus \
	--disable-update-check \
	--disable-precompiled-headers \
	--without-gettext \
	--with-flac \
	--with-dvdread
./drake V=1 -j"$KDOS_JOBS"
./drake DESTDIR=$PKG install

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/mkvtoolnix-gui.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/mkvtoolnix-gui.svg"
done

cat > "$PKG/usr/share/applications/org.bunkus.mkvtoolnix-gui.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=MKVToolNix
GenericName=Matroska Muxer
Comment=Merge, split and edit tracks of Matroska and WebM files
Exec=mkvtoolnix-gui %F
Icon=mkvtoolnix-gui
Terminal=false
StartupWMClass=org.bunkus.mkvtoolnix-gui
MimeType=application/x-mkvtoolnix-gui-settings;
Categories=AudioVideo;AudioVideoEditing;
Keywords=matroska;mkv;webm;mux;remux;subtitles;chapters;tracks;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.bunkus.mkvtoolnix-gui.desktop"

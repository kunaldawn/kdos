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

# The build runs on drake, the rake the tarball carries under rake.d/vendor,
# so ruby is all it needs. --disable-update-check compiles out the daily
# download of the release list from mkvtoolnix.download. --without-gettext
# builds the programs in English with no message catalogues. Manual pages come
# from DocBook through xsltproc with --nonet; translated pages need po4a,
# which is not a port, and are not built.
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man \
	--enable-gui \
	--enable-dbus \
	--disable-update-check \
	--disable-precompiled-headers \
	--without-gettext \
	--with-flac \
	--with-dvdread
./drake V=1 -j"${MAKEFLAGS#-j}"
./drake DESTDIR=$PKG install

# The panel draws application icons from hicolor PNGs only, and upstream
# installs SVGs.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/mkvtoolnix-gui.png" \
		"$PKG/usr/share/icons/hicolor/scalable/apps/mkvtoolnix-gui.svg"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass, which it lacks: Qt builds
# the Wayland app_id from the organisation domain and the program,
# org.bunkus.mkvtoolnix-gui. It claims only its own settings type: the
# Matroska and WebM types belong to the players.
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
